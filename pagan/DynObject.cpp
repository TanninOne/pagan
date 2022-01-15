#include "DynObject.h"
#include "TypeSpec.h"

void DynObject::saveTo(std::shared_ptr<IOWrapper> file) {
  LOG_BRACKET_F("save object idx {0} to {1}", (uint64_t)m_ObjectIndex, file->tellp());
  const std::vector<TypeProperty>& props = m_Spec->getProperties();
  // for (auto key : getKeys()) {
  for (int i = 0; i < props.size(); ++i) {
    if (!isBitSet(m_ObjectIndex, i)) {
      continue;
    }
    const std::string &key = props[i].key;
    int propertyOffset;
    auto iter = m_Spec->propertyByKey(m_ObjectIndex, key.c_str(), &propertyOffset);

    uint8_t* propBuffer = m_ObjectIndex->properties + propertyOffset;
    LOG_F("save prop {0} - index {1}", key, (uint64_t)propBuffer);
    uint32_t typeId = iter->typeId;

    if (typeId == TypeId::runtime) {
      typeId = *reinterpret_cast<uint32_t*>(propBuffer);
      propBuffer += sizeof(uint32_t);
    }

    LOG_F("is list: {0}", iter->isList);
    if (!iter->isList) {
      savePropTo(file, typeId, propBuffer);
    }
    else {
      size_t offset;
      uint32_t typeId;

      std::tie(typeId, offset) = m_Spec->get(m_ObjectIndex, key.c_str());

      union {
        struct {
          ObjSize count;
          ObjSize offset;
        } arrayProp;
        uint64_t buff;
      };

      buff = *reinterpret_cast<uint64_t*>(m_ObjectIndex->properties + offset);
      uint8_t* arrayData = m_IndexTable->arrayAddress(arrayProp.offset);

      LOG_F("array size: {0}", arrayProp.count);

      // arrayCur = savePropTo(file, typeId, arrayCur);

      if (arrayProp.count == COUNT_EOS) {
        // previously unindexed array. Do we actually have to do this?
        // If we can rely on an EOS array actually ending on the last byte of the stream,
        // we can copy it blindly
        const TypeProperty& prop = getProperty(key.c_str());

        std::shared_ptr<IOWrapper> data = getDataStream();
        uint64_t arrayDataPos;
        uint64_t streamLimit;
        memcpy(reinterpret_cast<char*>(&arrayDataPos), arrayData, sizeof(uint64_t));
        memcpy(reinterpret_cast<char*>(&streamLimit), arrayData + sizeof(uint64_t), sizeof(uint64_t));
        data->seekg(arrayDataPos);

        LOG_F("parse array {0} -> {1}", arrayDataPos, streamLimit);

        m_Spec->indexEOSArray(prop, m_IndexTable, m_ObjectIndex->properties + offset,
                              this, m_ObjectIndex->dataStream, data, streamLimit);
        buff = *reinterpret_cast<uint64_t*>(m_ObjectIndex->properties + offset);
        arrayData = m_IndexTable->arrayAddress(arrayProp.offset);
        LOG_F("#items: {0}", arrayProp.count);
      }

      uint8_t* arrayCur = arrayData;

      for (int i = 0; i < arrayProp.count; ++i) {
        LOG_F("save arr ele {0} / {1} -> {2} - {3}", i, arrayProp.count, (uint64_t)arrayCur, (uint64_t)file->tellp());
        arrayCur = savePropTo(file, typeId, arrayCur);
      }
    }
  }
}

uint8_t *DynObject::savePropTo(std::shared_ptr<IOWrapper> file, uint32_t typeId, uint8_t* propBuffer) {
  if (typeId >= TypeId::custom) {
    LOG_F("save prop type {0} @ {1}", m_Spec->getRegistry()->getById(typeId)->getName(), file->tellp());
    int64_t objOffset = *reinterpret_cast<const int64_t*>(propBuffer);
    std::shared_ptr<TypeSpec> type(m_Spec->getRegistry()->getById(typeId));

    if (!type) {
      throw IncompatibleType(fmt::format("type id not found in registry {}", typeId).c_str());
    }

    DynObject obj = getObjectAtOffset(type, objOffset, propBuffer);
    LOG_F("saving obj at {0} to {1}", objOffset, file->tellp());
    obj.saveTo(file);
    LOG_F("saved obj ends @ {0}, index {1}", file->tellp(), (uint64_t)propBuffer);
    return propBuffer + sizeof(int64_t);
  }
  else {
    LOG_F("save prop type {0} @ {1}", typeId, file->tellp());
    std::shared_ptr<IOWrapper> dataStream = m_Streams.get(m_ObjectIndex->dataStream);
    std::shared_ptr<IOWrapper> writeStream = m_Streams.getWrite();
    char* after;
    type_copy_any(static_cast<TypeId>(typeId), reinterpret_cast<char*>(propBuffer), file, dataStream, writeStream, &after);
    LOG_F("saved pod ends @ {0}, index: {1} -> {2}", file->tellp(), (uint64_t)propBuffer, (uint64_t)after);
    return reinterpret_cast<uint8_t*>(after);
  }
}

inline void DynObject::debug(size_t indent) const {
  int idx = 0;

  std::string sStr;
  sStr.resize(indent * 2, ' ');

  for (auto prop : getKeys()) {
    int propertyOffset;
    auto iter = m_Spec->propertyByKey(m_ObjectIndex, prop.c_str(), &propertyOffset);
    std::cout << "attribute offset " << propertyOffset << std::endl;

    uint8_t* propBuffer = m_ObjectIndex->properties + propertyOffset;
    uint32_t typeId = iter->typeId;

    std::cout << "list: " << iter->isList << " - " << typeId << std::endl;

    if (iter->isList) {
      std::cout << sStr << " " << prop << "..." << std::endl;
      continue;
    }

    if (typeId == TypeId::runtime) {
      typeId = *reinterpret_cast<uint32_t*>(propBuffer);
      propBuffer += sizeof(uint32_t);
    }

    if (typeId >= TypeId::custom) {
      int64_t objOffset = *reinterpret_cast<const int64_t*>(propBuffer);
      std::shared_ptr<TypeSpec> type(m_Spec->getRegistry()->getById(typeId));

      if (!type) {
        throw IncompatibleType(fmt::format("type id not found in registry {}", typeId).c_str());
      }

      getObjectAtOffset(type, objOffset, propBuffer).debug(indent + 1);
    }
    else {
      std::cout << "pod " << m_ObjectIndex->dataStream << " - " << std::endl;
      std::shared_ptr<IOWrapper> dataStream = m_Streams.get(m_ObjectIndex->dataStream);
      std::shared_ptr<IOWrapper> writeStream = m_Streams.getWrite();

      std::any val = type_read_any(static_cast<TypeId>(typeId), reinterpret_cast<char*>(propBuffer), dataStream, writeStream);

      try {
        std::cout << sStr << " " << prop << " = " << flexi_cast<std::string>(val) << std::endl;
      }
      catch (const std::bad_any_cast&) {
        std::cout << sStr << " " << prop << " = unknown" << std::endl;
      }
    }
  }
}

void DynObject::writeIndex(size_t dataOffset, std::streampos streamLimit, bool noSeek) {
  std::shared_ptr<IOWrapper> stream = noSeek
    ? m_Streams.get(m_ObjectIndex->dataStream)
    : m_Streams.get(m_ObjectIndex->dataStream, m_ObjectIndex->dataOffset);
  m_Spec->writeIndex(m_IndexTable, m_ObjectIndex,
    stream,
    m_Streams, this, streamLimit);
}

uint8_t* DynObject::getBitmask() const {
  return m_ObjectIndex->bitmask;
}

std::shared_ptr<IOWrapper> DynObject::getDataStream() const {
  return m_Streams.get(m_ObjectIndex->dataStream);
}

uint32_t DynObject::getTypeId() const {
  return m_Spec->getId();
}

std::vector<std::string> DynObject::getKeys() const {
  const std::vector<TypeProperty>& props = m_Spec->getProperties();
  std::vector<std::string> res;
  res.reserve(props.size());

  for (int i = 0; i < props.size(); ++i) {
    if (isBitSet(m_ObjectIndex, i)) {
      res.push_back(props[i].key);
    }
  }

  return std::move(res);
}

bool DynObject::has(const char* key) const {
  const std::vector<TypeProperty>& props = m_Spec->getProperties();
  for (int i = 0; i < props.size(); ++i) {
    if (isBitSet(m_ObjectIndex, i) && (props[i].key == key)) {
      return true;
    }
  }

  return false;
}

std::pair<uint32_t, uint8_t*> DynObject::getEffectiveType(const char* key) const {
  size_t offset;
  uint32_t typeId;

  std::tie(typeId, offset) = m_Spec->get(m_ObjectIndex, key);

  uint8_t* propBuffer = m_ObjectIndex->properties + offset;

  if (typeId == TypeId::runtime) {
    typeId = *reinterpret_cast<uint32_t*>(propBuffer);
    propBuffer += sizeof(uint32_t);
  }

  return std::make_pair(typeId, propBuffer);
}

const TypeProperty& DynObject::getChildType(const char* key) const {
  return m_Spec->getProperty(key);
}

std::any DynObject::getAny(char *key) const {
  size_t dotOffset = strcspn(key, ".");
  if (key[dotOffset] != '\0') {
    // std::string objKey(key, key + dotOffset);
    key[dotOffset] = '\0';
    DynObject obj = get<DynObject>(key);
    return obj.getAny(&key[dotOffset + 1]);
  }

  if (m_Spec->hasComputed(key)) {
    return m_Spec->compute(key, this);
  }

  // else: this is the "final" or "leaf" key
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

  char *index = reinterpret_cast<char*>(propBuffer);

  std::shared_ptr<IOWrapper> dataStream = m_Streams.get(m_ObjectIndex->dataStream);
  std::shared_ptr<IOWrapper> writeStream = m_Streams.getWrite();

  return type_read_any(static_cast<TypeId>(typeId), index, dataStream, writeStream);
}

std::any DynObject::getAny(const std::vector<std::string>::const_iterator &cur, const std::vector<std::string>::const_iterator &end) const {
  if (cur + 1 != end) {
    DynObject obj = get<DynObject>(cur->c_str());
    return obj.getAny(cur + 1, end);
  }

  if (m_Spec->hasComputed(cur->c_str())) {
    return m_Spec->compute(cur->c_str(), this);
  }

  // else: this is the "final" or "leaf" key
  size_t offset;
  uint32_t typeId;

  std::tie(typeId, offset) = m_Spec->get(m_ObjectIndex, cur->c_str());

  uint8_t *propBuffer = m_ObjectIndex->properties + offset;

  if (typeId == TypeId::runtime) {
    typeId = *reinterpret_cast<uint32_t*>(propBuffer);
    propBuffer += sizeof(uint32_t);
  }

  if (typeId >= TypeId::custom) {
    throw IncompatibleType("expected POD");
  }

  char *index = reinterpret_cast<char*>(propBuffer);

  std::shared_ptr<IOWrapper> dataStream = m_Streams.get(m_ObjectIndex->dataStream);
  std::shared_ptr<IOWrapper> writeStream = m_Streams.getWrite();

  return type_read_any(static_cast<TypeId>(typeId), index, dataStream, writeStream);
}

void DynObject::setAny(const std::vector<std::string>::const_iterator &cur, const std::vector<std::string>::const_iterator &end, const std::any &value) {
  if (cur + 1 != end) {
    DynObject obj = get<DynObject>(cur->c_str());
    return obj.setAny(cur + 1, end, value);
  }

  // else: this is the "final" or "leaf" key
  size_t offset;
  uint32_t typeId;

  std::tie(typeId, offset) = m_Spec->get(m_ObjectIndex, cur->c_str());

  if (typeId >= TypeId::custom) {
    throw IncompatibleType("expected POD");
  }

  char *index = reinterpret_cast<char*>(m_ObjectIndex->properties + offset);

  std::shared_ptr<IOWrapper> dataStream = m_Streams.get(m_ObjectIndex->dataStream);
  std::shared_ptr<IOWrapper> writeStream = m_Streams.getWrite();

  type_write_any(static_cast<TypeId>(typeId), index, writeStream, value);
}

DynObject DynObject::getObjectAtOffset(std::shared_ptr<TypeSpec> type, int64_t objOffset, uint8_t* prop) const {
  if (objOffset < 0) {
    // offset is the index offset for an already-indexed object
    int64_t indexOffset = objOffset * -1;

    return DynObject(type, m_Streams, m_IndexTable, reinterpret_cast<ObjectIndex*>(indexOffset), this);
  }
  else {
    ObjectIndex* objIndex = m_IndexTable->allocateObject(type, m_ObjectIndex->dataStream, objOffset);
    // offset is the data offset for a not-yet-indexed object
    DynObject res(type, m_Streams, m_IndexTable, objIndex, this);
    res.writeIndex(objOffset, 0, false);
    // LOG_F("not indexed, data {0}", objOffset);
    objOffset = reinterpret_cast<int64_t>(objIndex) * -1;

    memcpy(prop, reinterpret_cast<char*>(&objOffset), sizeof(int64_t));
    return res;
  }
}

bool DynObject::hasComputed(const char* key) const {
  return m_Spec->hasComputed(key);
}

std::any DynObject::compute(const char* key, const DynObject* obj) const {
  return m_Spec->compute(key, obj);
}

std::tuple<uint32_t, size_t> DynObject::getSpec(const char* key) const {
  return m_Spec->get(m_ObjectIndex, key);
}

std::tuple<uint32_t, size_t, SizeFunc, AssignCB> DynObject::getFullSpec(const char* key) const {
  return m_Spec->getFull(m_ObjectIndex, key);
}

const TypeProperty& DynObject::getProperty(const char* key) const {
  return m_Spec->getProperty(key);
}

DynObject DynObject::getObject(const char* key) const {
  if (strcmp(key, "_parent") == 0) {
    if (m_Parent == nullptr) {
      throw std::runtime_error("parent pointer not set");
    }
    return *m_Parent;
  }

  uint8_t* propBuffer;
  uint32_t typeId;
  std::tie(typeId, propBuffer) = getEffectiveType(key);

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

  // LOG_F("child object {} type {} - offset {}", key, type->getName(), objOffset);

  return getObjectAtOffset(type, objOffset, propBuffer);
}

std::vector<DynObject> DynObject::getListOfObjects(const char* key) const {
  LOG_BRACKET_F("get list of obj {0}", key);

  size_t offset;
  uint32_t typeId;

  std::tie(typeId, offset) = getSpec(key);
  LOG_F("(3) key {0} offset {1}", key, offset);

  // if it's a runtime type the concrete type is stored with each item individually
  // so we can't currently determine if the type at runtime is actually valid
  if ((typeId < TypeId::custom) && (typeId != TypeId::runtime)) {
    throw IncompatibleType(fmt::format("expected custom list, got {}", typeId).c_str());
  }

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
  uint8_t* arrayData = m_IndexTable->arrayAddress(arrayProp.offset);

  if (arrayProp.count == COUNT_EOS) {
    // eos array hasn't been loaded yet
    const TypeProperty& prop = getProperty(key);
    std::shared_ptr<IOWrapper> data = getDataStream();
    uint64_t arrayDataPos;
    uint64_t streamLimit;
    memcpy(reinterpret_cast<char*>(&arrayDataPos), arrayData, sizeof(uint64_t));
    memcpy(reinterpret_cast<char*>(&streamLimit), arrayData + sizeof(uint64_t), sizeof(uint64_t));
    data->seekg(arrayDataPos);
    m_Spec->indexEOSArray(prop, m_IndexTable, m_ObjectIndex->properties + offset,
                          this, m_ObjectIndex->dataStream, data, streamLimit);

    buff = *reinterpret_cast<uint64_t*>(m_ObjectIndex->properties + offset);
    arrayData = m_IndexTable->arrayAddress(arrayProp.offset);
  }

  uint8_t* arrayCur = arrayData;

  std::vector<DynObject> res;
  for (int i = 0; i < arrayProp.count; ++i) {
    uint32_t itemType = typeId;
    if (itemType == TypeId::runtime) {
      itemType = *reinterpret_cast<uint32_t*>(arrayCur);
      arrayCur += sizeof(uint32_t);
    }
    int64_t objOffset = *reinterpret_cast<int64_t*>(arrayCur);

    std::shared_ptr<TypeSpec> type(m_Spec->getRegistry()->getById(itemType));
    res.push_back(getObjectAtOffset(type, objOffset, arrayCur));
    arrayCur += sizeof(int64_t);
  }

  return res;
}

std::vector<std::any> DynObject::getListOfAny(const char* key) const {
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
  char* propPtr = reinterpret_cast<char*>(m_ObjectIndex->properties + offset);
  memcpy(reinterpret_cast<char*>(&count), propPtr, sizeof(ObjSize));
  memcpy(reinterpret_cast<char*>(&arrayOffset), propPtr + sizeof(ObjSize), sizeof(ObjSize));

  LOG_F("(2) array index offset {0} + {1} -> count {2}, array offset {3}", m_ObjectIndex->properties, offset, count, arrayOffset);

  char* arrayPtr = reinterpret_cast<char*>(m_IndexTable->arrayAddress(arrayOffset));

  std::vector<std::any> res;

  for (int i = 0; i < count; ++i) {
    res.push_back(type_read_any(static_cast<TypeId>(typeId), arrayPtr, dataStream, writeStream));
  }

  return res;
}

