#pragma once

#include "streamregistry.h"
#include "typecast.h"
#include "typespec.h"
#include "util.h"
#include <cstdint>

class DynObject
{

public:

  DynObject(const std::shared_ptr<TypeSpec> &spec, const StreamRegistry &streams, ObjectIndexTable *indexTable, ObjectIndex *index, const DynObject *parent)
    : m_Spec(spec)
    , m_Streams(streams)
    , m_IndexTable(indexTable)
    , m_ObjectIndex(index)
    , m_Parent(parent)
  {
  }

  template<typename T>
  DynObject(const std::shared_ptr<TypeSpec> &spec, StreamRegistry &streams, ObjectIndexTable *indexTable, const DynObject *parent, std::initializer_list<T> props)
    : m_Spec(spec)
    , m_Streams(streams)
    , m_IndexTable(indexTable)
    , m_Parent(parent)
  {
    m_ObjectIndex = indexTable->allocateObject(spec.lock(), -1, 0);

    std::vector<T> out;
    std::copy(props.begin(), props.end(), std::back_inserter(out));

    indexTable->setProperties(m_ObjectIndex, reinterpret_cast<uint8_t*>(&out[0]), out.size() * sizeof(T));
  }

  DynObject(const DynObject &reference)
    : m_Spec(reference.m_Spec)
    , m_Streams(reference.m_Streams)
    , m_IndexTable(reference.m_IndexTable)
    , m_ObjectIndex(reference.m_ObjectIndex)
    , m_Parent(reference.m_Parent)
  {
  }

  DynObject(DynObject &&) = default;

  ~DynObject() {
  }

  DynObject &operator=(DynObject &&) = default;

  void debug(int indent) const {
    int idx = 0;

    std::string sStr;
    sStr.resize(indent * 2, ' ');

    for (auto prop : getKeys()) {
      size_t offset;
      uint32_t typeId;

      std::tie(typeId, offset) = m_Spec->get(m_ObjectIndex, prop.c_str());
    }
  }

  void writeIndex(size_t dataOffset, std::streampos streamLimit, bool noSeek) {
    std::shared_ptr<IOWrapper> stream = noSeek
      ? m_Streams.get(m_ObjectIndex->dataStream)
      : m_Streams.get(m_ObjectIndex->dataStream, m_ObjectIndex->dataOffset);
    m_Spec->writeIndex(m_IndexTable, m_ObjectIndex,
                              stream,
                              m_Streams, this, streamLimit);
  }

  ObjectIndex *getIndex() const {
    return m_ObjectIndex;
  }

  uint8_t *getBitmask() const {
    return m_ObjectIndex->bitmask;
  }

  uint32_t getTypeId() const {
    return m_Spec->getId();
  }

  std::vector<std::string> getKeys() const {
    std::vector<std::string> res;

    auto props = m_Spec->getProperties();
    for (int i = 0; i < props.size(); ++i) {
      if (isBitSet(m_ObjectIndex, i)) {
        res.push_back(props[i].key);
      }
    }

    return res;
  }

  bool has(const char *key) const {
    auto props = m_Spec->getProperties();
    for (int i = 0; i < props.size(); ++i) {
      if (isBitSet(m_ObjectIndex, i) && (props[i].key == key)) {
        return true;
      }
    }

    return false;
  }

  std::any getAny(char *key) const;

  std::any getAny(const std::vector<std::string>::const_iterator &cur, const std::vector<std::string>::const_iterator &end) const;

  template <typename T> T get(const char *key) const;

  template <typename T> std::vector<T> getList(const char *key) const;

  template <typename T> void set(const char *key, const T &value) {
    LOG_BRACKET_F("set pod {0} to {1}", key, value);
    std::shared_ptr<IOWrapper> write = m_Streams.getWrite();

    // find the index offset for the specified attribute
    uint32_t typeId;
    size_t offset;
    SizeFunc size;
    AssignCB onAssign;
    
    std::tie(typeId, offset, size, onAssign) = m_Spec->getFull(m_ObjectIndex, key);

    if (typeId >= TypeId::custom) {
      throw IncompatibleType("expected POD");
    }

    LOG_F("write at index {0:x} + {1}", (int64_t)m_ObjectIndex->properties, offset);

    onAssign(*this, value);
    type_write(static_cast<TypeId>(typeId), reinterpret_cast<char*>(m_ObjectIndex->properties + offset), write, value, this);
  }

  template <typename T> void setList(const char *key, const std::vector<T> &value);

private:

  DynObject getObjectAtOffset(std::shared_ptr<TypeSpec> type,
                              int64_t objOffset,
                              uint8_t *prop) const {
    if (objOffset < 0) {
      // offset is the index offset for an already-indexed object
      int64_t indexOffset = objOffset * -1;

      return DynObject(type, m_Streams, m_IndexTable, reinterpret_cast<ObjectIndex*>(indexOffset), this);
    }
    else {
      ObjectIndex *objIndex = m_IndexTable->allocateObject(type, m_ObjectIndex->dataStream, objOffset);
      // offset is the data offset for a not-yet-indexed object
      DynObject res(type, m_Streams, m_IndexTable, objIndex, this);
      res.writeIndex(objOffset, 0, false);
      // LOG_F("not indexed, data {0}", objOffset);
      objOffset = reinterpret_cast<int64_t>(objIndex) * -1;

      memcpy(prop, reinterpret_cast<char*>(&objOffset), sizeof(int64_t));
      return res;
    }
  }

private:

  std::shared_ptr<TypeSpec> m_Spec;
  const StreamRegistry &m_Streams;
  ObjectIndexTable *m_IndexTable;
  ObjectIndex *m_ObjectIndex;
  const DynObject *m_Parent;

};

template<>
inline DynObject DynObject::get(const char *key) const {
  if (strcmp(key, "_parent") == 0) {
    if (m_Parent == nullptr) {
      throw std::runtime_error("parent pointer not set");
    }
    return *m_Parent;
  }

  size_t propOffset;
  uint32_t typeId;
  std::tie(typeId, propOffset) = m_Spec->get(m_ObjectIndex, key);

  uint8_t *propBuffer = m_ObjectIndex->properties + propOffset;

  if (typeId == TypeId::runtime) {
    typeId = *reinterpret_cast<uint32_t*>(propBuffer);
    propBuffer += sizeof(uint32_t);
  }

  if (typeId < TypeId::custom) {
    LOG_F("different type stored {0}", typeId);
    throw IncompatibleType(fmt::format("expected custom item, got {}", typeId).c_str());
  }

  // offset - either into the data stream if the object hasn't been cached yet or to
  //   its index
  int64_t objOffset = *reinterpret_cast<const int64_t*>(propBuffer);

  std::shared_ptr<TypeSpec> type(m_Spec->getRegistry()->getById(typeId));

  if (!type) {
    throw IncompatibleType(fmt::format("type id not found in registry {}", typeId).c_str());
  }

  LOG_F("child object {} type {} - offset {}", key, type->getName(), objOffset);

  return getObjectAtOffset(type, objOffset, propBuffer);
}


template<typename T>
inline T DynObject::get(const char *key) const {
  size_t offset;
  uint32_t typeId;

  std::tie(typeId, offset) = m_Spec->get(m_ObjectIndex, key);

  uint8_t *propBuffer = m_ObjectIndex->properties + offset;

  if (typeId == TypeId::runtime) {
    typeId = *reinterpret_cast<uint32_t*>(propBuffer);
    propBuffer += sizeof(uint32_t);
  }

  if (typeId >= TypeId::custom) {
    throw IncompatibleType("expected POD");
  }

  std::shared_ptr<IOWrapper> dataStream = m_Streams.get(m_ObjectIndex->dataStream);
  std::shared_ptr<IOWrapper> writeStream = m_Streams.getWrite();
  
  LOG_F("get {}", key);

  return type_read<T>(static_cast<TypeId>(typeId), reinterpret_cast<char*>(propBuffer), dataStream, writeStream);
}

template<>
inline std::vector<DynObject> DynObject::getList(const char *key) const {
  LOG_BRACKET_F("get list of obj {0}", key);

  size_t offset;
  uint32_t typeId;

  std::tie(typeId, offset) = m_Spec->get(m_ObjectIndex, key);
  LOG_F("(3) key {0} offset {1}", key, offset);
  if (typeId < TypeId::custom) {
    throw IncompatibleType(fmt::format("expected custom list, got {}", typeId).c_str());
  }

  std::shared_ptr<TypeSpec> type(m_Spec->getRegistry()->getById(typeId));

  std::shared_ptr<IOWrapper> writeStream = m_Streams.getWrite();

  // property contains item count and an offset into the array index
  union {
    struct {
      ObjSize count;
      ObjSize offset;
    } arrayProp;
    uint64_t buff;
  };

  buff = *reinterpret_cast<uint64_t*>(m_ObjectIndex->properties + offset);

  uint8_t *arrayData = m_IndexTable->arrayAddress(arrayProp.offset);
  uint8_t *arrayCur = arrayData;

  std::vector<DynObject> res;
  for (int i = 0; i < arrayProp.count; ++i) {
    int64_t objOffset = *reinterpret_cast<int64_t*>(arrayCur);
    res.push_back(getObjectAtOffset(type, objOffset, arrayCur));
    arrayCur += sizeof(int64_t);
  }

  return res;
}

template<typename T>
inline std::vector<T> DynObject::getList(const char *key) const {
  LOG_BRACKET_F("get list of pod {0}", key);

  size_t offset;
  uint32_t typeId;

  std::tie(typeId, offset) = m_Spec.lock()->get(m_ObjectIndex, key);
  LOG_F("(2) key {0} offset {1}", key, offset);
  if (typeId >= TypeId::custom) {
    throw IncompatibleType("Expected POD");
  }

  std::shared_ptr<IOWrapper> dataStream = m_Streams.get(m_ObjectIndex->dataStream);
  std::shared_ptr<IOWrapper> writeStream = m_Streams.getWrite();

  ObjSize count;
  ObjSize arrayOffset;
  char *propPtr = reinterpret_cast<char*>(m_ObjectIndex->properties + offset);
  memcpy(reinterpret_cast<char*>(&count), propPtr, sizeof(ObjSize));
  memcpy(reinterpret_cast<char*>(&arrayOffset), propPtr + sizeof(ObjSize), sizeof(ObjSize));

  LOG_F("(2) array index offset {0} + {1} -> count {2}, array offset {3}", m_ObjectIndex->properties, offset, count, arrayOffset);

  char *arrayPtr = reinterpret_cast<char*>(m_IndexTable->arrayAddress(arrayOffset));

  std::vector<T> res;

  for (int i = 0; i < count; ++i) {
    res.push_back(type_read<T>(static_cast<TypeId>(typeId), arrayPtr, dataStream, writeStream));
  }

  return res;
}

template<>
inline void DynObject::setList(const char *key, const std::vector<DynObject> &value) {
  LOG_BRACKET_F("set list {0}", key);
  std::shared_ptr<IOWrapper> write = m_Streams.getWrite();

  // find the index offset for the specified attribute
  /*
  uint32_t typeId;
  size_t offset;
  SizeFunc size;
  AssignCB onAssign;

  std::tie(typeId, offset, size, onAssign) = m_Spec.lock()->getFull(index, m_Bitmask, key);
  LogBracket::log(fmt::format("key {0} offset {1}", key, offset));

  if (typeId < TypeId::custom) {
    throw IncompatibleType();
  }

  // position the streams
  index->seekg(m_IndexOffset + offset);
  index->seekp(m_IndexOffset + offset);
  write->seekg(0, std::ios::end);

  LogBracket::log(fmt::format("write at index {0} + {1}", m_IndexOffset, offset));

  ObjSize listCount;
  ObjSize listOffset;
  index->read(reinterpret_cast<char*>(&listCount), sizeof(ObjSize));
  index->read(reinterpret_cast<char*>(&listOffset), sizeof(ObjSize));

  // optimization: if the list length remains the same, reuse the same array index.
  //   since all pod indices should be of static size, same count means same index size.
  if (listCount == value.size()) {
    arrayIndex->seekp(listOffset);
  }
  else {
    arrayIndex->seekp(0, std::ios::end);
  }
  listCount = static_cast<ObjSize>(value.size());
  offset = arrayIndex->tellp();

  LogBracket::log(fmt::format("write list info at {0} count {1} offset {2}", index->tellp(), listCount, offset));
  index->write(reinterpret_cast<char*>(&listCount), sizeof(ObjSize));
  index->write(reinterpret_cast<char*>(&offset), sizeof(ObjSize));

  onAssign(*this, value);

  for (int i = 0; i < listCount; ++i) {
    type_write(TypeId::int64, arrayIndex, write, static_cast<int64_t>(value[i].getIndexOffset()) * -1, this);
  }
  */
}

template<typename T>
inline void DynObject::setList(const char *key, const std::vector<T> &value) {
  LOG_BRACKET_F("set list {0}", key);
  std::shared_ptr<IOWrapper> write = m_Streams.getWrite();

  uint32_t typeId;
  size_t offset;
  SizeFunc size;
  AssignCB onAssign;

  std::tie(typeId, offset, size, onAssign) = m_Spec.lock()->getFull(m_ObjectIndex, key);

  if (typeId >= TypeId::custom) {
    throw IncompatibleType("Expected POD");
  }

  // LogBracket::log(fmt::format("write at index {0} + {1}", m_ObjectIndex->properties, offset));

  char *index = reinterpret_cast<char*>(m_ObjectIndex->properties + offset);

  ObjSize listCount;
  ObjSize listOffset;
  memcpy(reinterpret_cast<char*>(&listCount), index, sizeof(ObjSize));
  memcpy(reinterpret_cast<char*>(&listOffset), index + sizeof(ObjSize), sizeof(ObjSize));

  // optimization: if the list length remains the same, reuse the same array index.
  //   since all pod indices should be of static size, same count means same index size.
  ObjSize newListOffset = (listCount == value.size())
    ? listOffset
    : m_IndexTable->allocateArray(value.size());

  ObjSize newListCount = static_cast<ObjSize>(value.size());

  memcpy(index, reinterpret_cast<char*>(&newListCount), sizeof(ObjSize));
  memcpy(index + sizeof(ObjSize), reinterpret_cast<char*>(&newListOffset), sizeof(ObjSize));

  onAssign(*this, value);

  char *arrayPtr = reinterpret_cast<char*>(m_IndexTable->arrayAddress(newListOffset));

  for (int i = 0; i < listCount; ++i) {
    arrayPtr = type_write(static_cast<TypeId>(typeId), arrayPtr, write, value[i], this);
  }
}

