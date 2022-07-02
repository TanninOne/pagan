#pragma once

#include "streamregistry.h"
#include "typecast.h"
#include "util.h"
#include "flexi_cast.h"
#include "IScriptQuery.h"
#include "TypeProperty.h"
#include "constants.h"
#include <cstdint>
#include <iostream>

class TypeSpec;
class ObjectIndex;
class ObjectIndexTable;

class DynObject : public IScriptQuery
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
    m_ObjectIndex = indexTable->allocateObject(spec, -1, 0);

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

  DynObject& operator=(const DynObject& reference) {
    if (this != &reference) {
      m_Spec = reference.m_Spec;
      m_IndexTable = reference.m_IndexTable;
      m_ObjectIndex = reference.m_ObjectIndex;
      m_Parent = reference.m_Parent;
    }
    return *this;
  }

  DynObject &operator=(DynObject &&) = default;

  void setParameters(const std::vector<std::any>& params) {
    m_Parameters = params;
  }

  void saveTo(std::shared_ptr<IOWrapper> file);

  uint8_t *savePropTo(std::shared_ptr<IOWrapper> file, uint32_t typeId, uint8_t* propBuffer);

  void debug(size_t indent) const;

  void writeIndex(size_t dataOffset, std::streampos streamLimit, bool noSeek);

  ObjectIndex *getIndex() const {
    return m_ObjectIndex;
  }

  uint8_t* getBitmask() const;

  std::shared_ptr<TypeSpec> getSpec() const {
    return m_Spec;
  }

  std::shared_ptr<IOWrapper> getDataStream() const;

  std::shared_ptr<IOWrapper> getWriteStream() const {
    return m_Streams.getWrite();
  }

  uint32_t getTypeId() const;

  std::vector<std::string> getKeys() const;

  bool has(const char* key) const;

  std::tuple<uint32_t, uint8_t*, std::vector<std::string>> getEffectiveType(const char* key) const;

  bool isCustom(const char *key) const {
    uint8_t *propBuffer;
    uint32_t typeId;
    std::vector<std::string> args;

    std::tie(typeId, propBuffer, args) = getEffectiveType(key);

    return typeId >= TypeId::custom;
  }

  const TypeProperty& getChildType(const char* key) const;

  // get the value at the specified key
  // note: the key gets modified in the process!
  std::any getAny(char *key) const;

  std::any getAny(std::string key) const {
    return getAny(&key[0]);
  }

  std::any getAny(const std::vector<std::string>::const_iterator &cur, const std::vector<std::string>::const_iterator &end) const;

  void setAny(const std::vector<std::string>::const_iterator &cur,
              const std::vector<std::string>::const_iterator &end,
              const std::any &value);

  template <typename T> T get(const char* key) const;

  template <typename T> std::vector<T> getList(const char *key) const;

  template <typename T> void set(const char *key, const T &value) {
    LOG_BRACKET_F("set pod {0}", key);
    std::shared_ptr<IOWrapper> write = m_Streams.getWrite();

    // find the index offset for the specified attribute
    uint32_t typeId;
    size_t offset;
    SizeFunc size;
    AssignCB onAssign;
    
    std::tie(typeId, offset, size, onAssign) = m_Spec->getFull(m_ObjectIndex, key);
    uint8_t *propBuffer = m_ObjectIndex->properties + offset;

    if (typeId == TypeId::runtime) {
      typeId = *reinterpret_cast<uint32_t*>(propBuffer);
      propBuffer += sizeof(uint32_t);
    }

    if (typeId >= TypeId::custom) {
      throw IncompatibleType("expected POD");
    }

    LOG_F("write at index {0:x} + {1}", (int64_t)m_ObjectIndex->properties, offset);

    type_write(static_cast<TypeId>(typeId), reinterpret_cast<char*>(propBuffer), write, value);
    onAssign(*this, std::any(value));
  }

  template <typename T> void setList(const char *key, const std::vector<T> &value);

private:

  DynObject getObjectAtOffset(std::shared_ptr<TypeSpec> type,
                              int64_t objOffset,
                              uint8_t* prop) const;

  bool hasComputed(const char* key) const;

  std::any compute(const char* key, const DynObject* obj) const;

  std::tuple<uint32_t, size_t> getSpec(const char* key) const;
  std::tuple<uint32_t, size_t, SizeFunc, AssignCB> getFullSpec(const char* key) const;
  const TypeProperty& getProperty(const char* key) const;

  DynObject getObject(const char* key) const;

  std::vector<DynObject> getListOfObjects(const char* key) const;
  std::vector<std::any> getListOfAny(const char* key) const;

private:

  std::shared_ptr<TypeSpec> m_Spec;
  const StreamRegistry &m_Streams;
  ObjectIndexTable *m_IndexTable;
  ObjectIndex *m_ObjectIndex;
  const DynObject *m_Parent;
  std::vector<std::any> m_Parameters;

};

template<>
inline DynObject DynObject::get(const char* key) const {
  return getObject(key);
}

template<typename T>
inline T DynObject::get(const char* key) const {
  if (hasComputed(key)) {
    return flexi_cast<T>(compute(key, this));
  }

  uint32_t typeId;
  uint8_t* propBuffer;
  std::vector<std::string> args;
  std::tie(typeId, propBuffer, args) = getEffectiveType(key);

  if (typeId >= TypeId::custom) {
    throw IncompatibleType("expected POD");
  }

  std::shared_ptr<IOWrapper> dataStream = m_Streams.get(m_ObjectIndex->dataStream);
  std::shared_ptr<IOWrapper> writeStream = m_Streams.getWrite();

  return type_read<T>(static_cast<TypeId>(typeId), reinterpret_cast<char*>(propBuffer), dataStream, writeStream, nullptr);
}

template<>
inline std::vector<DynObject> DynObject::getList(const char *key) const {
  return getListOfObjects(key);
}

template<>
inline std::vector<std::any> DynObject::getList(const char* key) const {
  return getListOfAny(key);
}

template<typename T>
inline std::vector<T> DynObject::getList(const char *key) const {
  LOG_BRACKET_F("get list of pod {0}", key);

  size_t offset;
  uint32_t typeId;

  std::tie(typeId, offset) = m_Spec->get(m_ObjectIndex, key);
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
    res.push_back(type_read<T>(static_cast<TypeId>(typeId), arrayPtr, dataStream, writeStream, &arrayPtr));
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

  std::tie(typeId, offset, size, onAssign) = getFullSpec(key);

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
    arrayPtr = type_write(static_cast<TypeId>(typeId), arrayPtr, write, value[i]);
  }
}

