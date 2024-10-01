#include "DynObject.h"
#include "TypeSpec.h"
#include <numeric>

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
        // If we could rely on an EOS array actually ending on the last byte of the stream,
        // we could copy it blindly
        const TypeProperty& prop = getProperty(key.c_str());

        std::shared_ptr<IOWrapper> data = getDataStream();
        uint64_t arrayDataPos;
        uint64_t streamLimit;
        memcpy(reinterpret_cast<char*>(&arrayDataPos), arrayData, sizeof(uint64_t));
        memcpy(reinterpret_cast<char*>(&streamLimit), arrayData + sizeof(uint64_t), sizeof(uint64_t));
        data->seekg(arrayDataPos);

        LOG_F("parse array {0} -> {1}", arrayDataPos, streamLimit);

        std::function<bool(uint8_t*)> repeatCondition;
        if (arrayProp.count == COUNT_MORE) {
          repeatCondition = [&](uint8_t* pos) -> bool {
            LOG_F("repeat condition {0}, ({1})", m_Spec->getId(), m_Spec->getProperties().size());
            DynObject tmp(m_Spec, m_Streams, m_IndexTable, reinterpret_cast<ObjectIndex*>(pos), this);
            return prop.repeatCondition(tmp);
          };
        }

        m_Spec->indexEOSArray(prop, m_IndexTable, m_ObjectIndex->properties + offset,
                              this, m_ObjectIndex->dataStream, data, streamLimit, repeatCondition);
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
      throw IncompatibleType(std::format("type id not found in registry {}", typeId).c_str());
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

  for (const auto& prop : getKeys()) {
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
        throw IncompatibleType(std::format("type id not found in registry {}", typeId).c_str());
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

  return res;
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

std::tuple<uint32_t, uint8_t*, std::vector<std::string>> DynObject::getEffectiveType(std::string_view key) const {
  size_t offset;
  uint32_t typeId;
  std::vector<std::string> args;
  bool isList;

  std::tie(typeId, offset, args, isList) = m_Spec->getWithArgs(m_ObjectIndex, key);

  uint8_t* propBuffer = m_ObjectIndex->properties + offset;


  // for lists, the concrete type is stored with the individual items so the list type is still actually "runtime"
  if ((typeId == TypeId::runtime) && !isList) {
    typeId = *reinterpret_cast<uint32_t*>(propBuffer);
    propBuffer += sizeof(uint32_t);
  }

  return std::make_tuple(typeId, propBuffer, args);
}

const TypeProperty& DynObject::getChildType(const char* key) const {
  return m_Spec->getProperty(key);
}

std::any DynObject::getAny(const char *key) const {
  if (size_t dotOffset = strcspn(key, "."); key[dotOffset] != '\0') {
    std::string_view objKey(key, key + dotOffset);
    DynObject obj = get<DynObject>(key);
    return obj.getAny(&key[dotOffset + 1]);
  }

  if (m_Spec->hasComputed(key)) {
    return m_Spec->compute(key, this);
  }

  // else: this is the "final" or "leaf" key
  int offsetProp;
  int offsetParam;
  uint32_t typeId;

  std::tie(typeId, offsetProp, offsetParam) = m_Spec->getPorP(m_ObjectIndex, key);

  if (offsetProp != -1) {
    uint8_t* propBuffer = m_ObjectIndex->properties + offsetProp;

    if (typeId == TypeId::runtime) {
      typeId = *reinterpret_cast<uint32_t*>(propBuffer);
      propBuffer += sizeof(uint32_t);
    }

    if (typeId >= TypeId::custom) {
      throw IncompatibleType(std::format("expected POD for key {}, got {}", key, typeId));
    }

    auto* index = reinterpret_cast<char*>(propBuffer);

    std::shared_ptr<IOWrapper> dataStream = m_Streams.get(m_ObjectIndex->dataStream);
    std::shared_ptr<IOWrapper> writeStream = m_Streams.getWrite();

    std::any result = type_read_any(static_cast<TypeId>(typeId), index, dataStream, writeStream);

    // std::shared_ptr<TypeSpec> type(m_Spec->getRegistry()->getById(typeId));

    if (TypeProperty prop = m_Spec->getProperty(key); prop.hasEnum) {
      return resolveEnum(prop.enumName, flexi_cast<int32_t>(result));
    }

    return result;
  }
  else if (offsetParam != -1) {
    LOG_F("get parameter {0} - {1}", offsetParam, m_Parameters.size());
    return m_Parameters.at(offsetParam);
  }
  else {
    throw std::runtime_error("invalid paramater");
  }
}

std::any DynObject::getAny(const std::vector<std::string_view>::const_iterator &cur, const std::vector<std::string_view>::const_iterator &end) const {
  if (cur + 1 != end) {
    DynObject obj = get<DynObject>(*cur);
    return obj.getAny(cur + 1, end);
  }

  if (m_Spec->hasComputed(*cur)) {
    return m_Spec->compute(*cur, this);
  }

  // else: this is the "final" or "leaf" key
  int offsetParam;
  int offsetProp;
  uint32_t typeId;

  std::tie(typeId, offsetProp, offsetParam) = m_Spec->getPorP(m_ObjectIndex, *cur);

  LOG_F("getAny({}) found: param {} - prop {}", *cur, offsetParam, offsetProp);

  if (offsetProp != -1) {
    uint8_t* propBuffer = m_ObjectIndex->properties + offsetProp;

    if (typeId == TypeId::runtime) {
      typeId = *reinterpret_cast<uint32_t*>(propBuffer);
      propBuffer += sizeof(uint32_t);
    }

    if (typeId >= TypeId::custom) {
      throw IncompatibleType(std::format("expected POD for key {}, got {}", *cur, typeId));
    }

    auto* index = reinterpret_cast<char*>(propBuffer);

    std::shared_ptr<IOWrapper> dataStream = m_Streams.get(m_ObjectIndex->dataStream);
    std::shared_ptr<IOWrapper> writeStream = m_Streams.getWrite();

    std::any result = type_read_any(static_cast<TypeId>(typeId), index, dataStream, writeStream);
    LOG_F("getAny({}) type: {}", *cur, result.type().name());

    const TypeProperty &prop = m_Spec->getProperty(*cur);

    if (prop.hasEnum) {
      return resolveEnum(prop.enumName, flexi_cast<int32_t>(result));
    }

    return result;
  }
  else if (offsetParam != -1) {
    return m_Parameters.at(offsetParam);
  }
  else {
    throw std::runtime_error("invalid paramater");
  }
}

inline std::string DynObject::resolveEnum(const std::string& enumName, int32_t value) const {
  try {
    const KSYEnum& enumMap = m_Spec->getEnumByName(enumName);
    auto enumValue = enumMap.find(value);
    if (enumValue == enumMap.end()) {
      throw std::runtime_error(std::format("invalid enum value {0} -> {1}", enumName, value));
    }
    return std::format("{0}::{1}", enumName, enumValue->second);
  }
  catch (const std::exception& e) {
    if (m_Parent != nullptr) {
      return m_Parent->resolveEnum(enumName, value);
    }
    else {
      throw;
    }
  }
}

void DynObject::setAny(const std::vector<std::string_view>::const_iterator &cur, const std::vector<std::string_view>::const_iterator &end, const std::any &value) {
  std::string cur_str{ *cur };
  if (cur + 1 != end) {
    DynObject obj = get<DynObject>(cur_str);
    return obj.setAny(cur + 1, end, value);
  }

  // else: this is the "final" or "leaf" key
  size_t offset;
  uint32_t typeId;

  std::tie(typeId, offset) = m_Spec->get(m_ObjectIndex, cur_str);

  if (typeId >= TypeId::custom) {
    throw IncompatibleType(std::format("expected POD for key {}, got {}", cur_str, typeId));
  }

  auto *index = reinterpret_cast<char*>(m_ObjectIndex->properties + offset);

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

bool DynObject::hasComputed(std::string_view key) const {
  return m_Spec->hasComputed(key);
}

std::any DynObject::compute(std::string_view key, const DynObject* obj) const {
  return m_Spec->compute(key, obj);
}

std::tuple<uint32_t, size_t> DynObject::getSpec(std::string_view key) const {
  return m_Spec->get(m_ObjectIndex, key);
}

std::tuple<uint32_t, size_t, SizeFunc, AssignCB> DynObject::getFullSpec(const char* key) const {
  return m_Spec->getFull(m_ObjectIndex, key);
}

const TypeProperty& DynObject::getProperty(std::string_view key) const {
  return m_Spec->getProperty(key);
}

DynObject DynObject::getObject(std::string_view key) const {
  if (key == "_") {
    return *this;
  } else if (key == "_parent") {
    if (m_Parent == nullptr) {
      throw std::runtime_error("parent pointer not set");
    }
    return *m_Parent;
  }
  else if (key == "_root") {
    const DynObject* iter = this;
    while (iter->m_Parent != nullptr) {
      iter = iter->m_Parent;
    }
    return *iter;
  }

  uint8_t* propBuffer;
  uint32_t typeId;
  std::vector<std::string> argList;
  std::tie(typeId, propBuffer, argList) = getEffectiveType(key);

  if (typeId < TypeId::custom) {
    LOG_F("different type stored {0}", typeId);
    throw IncompatibleType(std::format("expected custom item for key {}, got {}", key, typeId));
  }

  // offset - either into the data stream if the object hasn't been cached yet or to
  //   its index
  int64_t objOffset = *reinterpret_cast<const int64_t*>(propBuffer);

  std::shared_ptr<TypeSpec> type(m_Spec->getRegistry()->getById(typeId));

  if (!type) {
    throw IncompatibleType(std::format("type id {} for key {} not found in registry", typeId, key));
  }

  DynObject res = getObjectAtOffset(type, objOffset, propBuffer);
  std::vector<std::any> args;
  std::transform(
      argList.begin(), argList.end(), std::back_inserter(args),
      [this](const std::string &key) { return getAny(key.c_str()); });
  res.setParameters(args);
  return res;
}

std::vector<DynObject> DynObject::getListOfObjects(std::string_view key) const {
  auto [arrayCur, count, typeId] = accessArrayIndex(key);

  std::vector<DynObject> res;
  for (int i = 0; i < count; ++i) {
    uint32_t itemType = typeId;
    if (itemType == TypeId::runtime) {
      itemType = *reinterpret_cast<uint32_t*>(arrayCur);
      arrayCur += sizeof(uint32_t);
      if (itemType < TypeId::custom) {
        throw WrongTypeRequestedError();
      }
    }
    int64_t objOffset = *reinterpret_cast<int64_t*>(arrayCur);

    std::shared_ptr<TypeSpec> type(m_Spec->getRegistry()->getById(itemType));
    res.push_back(getObjectAtOffset(type, objOffset, arrayCur));
    arrayCur += sizeof(int64_t);
  }

  return res;
}

std::tuple<uint8_t*, ObjSize, uint32_t> DynObject::accessArrayIndex(std::string_view key) const {
  LOG_BRACKET_F("get list of obj {0}", key);

  size_t offset;
  uint32_t typeId;

  std::tie(typeId, offset) = getSpec(key);
  LOG_F("(3) key {0}  offset {1} -> {2}", key, offset, (uint64_t)(m_ObjectIndex->properties + offset), typeId);

  // if it's a runtime type the concrete type is stored with each item individually
  // so we can't currently determine if the type at runtime is actually valid
  if ((typeId < TypeId::custom) && (typeId != TypeId::runtime)) {
    throw IncompatibleType(std::format("expected custom list for key {}, got {}", key, typeId));
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

  if ((arrayProp.count == COUNT_EOS) || (arrayProp.count == COUNT_MORE)) {
    // with a dynamic length array we have to to index the objects to know the length of the array
    const TypeProperty& prop = getProperty(key);
    std::shared_ptr<IOWrapper> data = getDataStream();
    uint64_t arrayDataPos;
    uint64_t streamLimit;
    memcpy(reinterpret_cast<char*>(&arrayDataPos), arrayData, sizeof(uint64_t));
    memcpy(reinterpret_cast<char*>(&streamLimit), arrayData + sizeof(uint64_t), sizeof(uint64_t));
    data->seekg(arrayDataPos);
    std::function<bool(uint8_t*)> repeatCondition;
    if (arrayProp.count == COUNT_MORE) {
      std::shared_ptr<TypeSpec> itemType(m_Spec->getRegistry()->getById(prop.typeId));
      LOG_F("repeat-until getList");
      repeatCondition = [&](uint8_t* pos) -> bool {
        // we need the DynObject to correctly evaluate the loop condition but at this point, the object index is only stored in a temporary
        // location, identified by pos
        // TODO: probably need handling for runtime types
        int64_t objIndex = *reinterpret_cast<int64_t*>(pos);
        DynObject tmp(itemType, m_Streams, m_IndexTable, reinterpret_cast<ObjectIndex*>(objIndex * -1), this);

        auto keys = tmp.getKeys();
        std::string joined = std::accumulate(keys.begin(), keys.end(), std::string(), [](std::string res, const std::string& iter) {
          return std::move(res) + "," + iter;
          });

        return prop.repeatCondition(tmp);
      };
    }

    m_Spec->indexEOSArray(prop, m_IndexTable, m_ObjectIndex->properties + offset,
                          this, m_ObjectIndex->dataStream, data, streamLimit, repeatCondition);

    // update the array properties, now with the actual count filled in
    buff = *reinterpret_cast<uint64_t*>(m_ObjectIndex->properties + offset);
    arrayData = m_IndexTable->arrayAddress(arrayProp.offset);
  }

  uint8_t* arrayCur = arrayData;

  LOG_F("array count 2 {} - {}", arrayProp.count, typeId);

  return std::make_tuple(arrayCur, arrayProp.count, typeId);
}

DynObject DynObject::getArrayItem(uint32_t typeId, uint8_t **arrayCur) const {
  uint32_t itemType = typeId;
  // in a "regular" array the item type will always be the same but if it's
  // a runtime type (switch/case) each item may be a different type and the actual
  // object is prefixed by it's real type
  if (itemType == TypeId::runtime) {
    itemType = *reinterpret_cast<uint32_t*>(*arrayCur);
    *arrayCur += sizeof(uint32_t);
  }
  int64_t objOffset = *reinterpret_cast<int64_t*>(*arrayCur);

  std::shared_ptr<TypeSpec> type(m_Spec->getRegistry()->getById(itemType));

  *arrayCur += sizeof(int64_t);
  return getObjectAtOffset(type, objOffset, *arrayCur);
}

std::vector<std::any> DynObject::getListOfAny(std::string_view key) const {
  auto [arrayCur, count, typeId] = accessArrayIndex(key);
  std::shared_ptr<IOWrapper> dataStream = m_Streams.get(m_ObjectIndex->dataStream);
  std::shared_ptr<IOWrapper> writeStream = m_Streams.getWrite();

  std::vector<std::any> res;

  for (int i = 0; i < count; ++i) {
    uint32_t actualTypeId = typeId;
    if (actualTypeId == TypeId::runtime) {
      actualTypeId = *reinterpret_cast<uint32_t*>(arrayCur);
      arrayCur += sizeof(uint32_t);
      if (actualTypeId < TypeId::custom) {
        throw WrongTypeRequestedError();
      }
    }
    res.push_back(type_read_any(static_cast<TypeId>(actualTypeId), reinterpret_cast<char*>(arrayCur), dataStream, writeStream));
  }

  return res;
}

std::tuple<uint32_t, uint8_t*, AssignCB> DynObject::resolveTypeAtKey(std::string_view key, bool resolveRTT) const {
  uint32_t typeId;
  size_t offset;
  SizeFunc size;
  AssignCB onAssign;

  std::tie(typeId, offset, size, onAssign) =
      m_Spec->getFull(m_ObjectIndex, key);
  uint8_t *propBuffer = m_ObjectIndex->properties + offset;

  if ((typeId == TypeId::runtime) && resolveRTT) {
    typeId = *reinterpret_cast<uint32_t *>(propBuffer);
    propBuffer += sizeof(uint32_t);
  }

  if (typeId >= TypeId::custom) {
    throw IncompatibleType(std::format("expected POD for key {}, got {}", key, typeId));
  }
  return std::make_tuple(typeId, propBuffer, onAssign);
}

void DynObject::indexRepeatUntilArray(
    const TypeProperty &prop, uint8_t *propBuffer,
    const std::shared_ptr<IOWrapper> &data, uint64_t streamLimit,
    const std::function<bool(uint8_t*)> &repeatCondition) const {
  m_Spec->indexEOSArray(prop, m_IndexTable, propBuffer, this,
                        m_ObjectIndex->dataStream, data, streamLimit,
                        repeatCondition);
}
