#include "TypeSpec.h"
#include "DynObject.h"

TypeSpec::TypeSpec(const char *name, uint32_t typeId, TypeRegistry *registry)
  : m_Name(name), m_Registry(registry), m_Id(typeId), m_StaticSize(0)
{
}

TypeSpec::~TypeSpec()
{
}

ObjSize TypeSpec::indexEOSArray(const TypeProperty& prop, ObjectIndexTable* indexTable, uint8_t* buffer, const DynObject* obj, DataStreamId dataStream, std::shared_ptr<IOWrapper> data, std::streampos streamLimit) {
  // std::vector<uint8_t> tmpArrayBuffer(8 * NUM_STATIC_PROPERTIES);
  // uint8_t *curPos = &tmpArrayBuffer[0];
  // uint8_t *endPos = curPos + tmpArrayBuffer.size();
  uint32_t size = NUM_STATIC_PROPERTIES * 8;
  uint8_t* tmpBuffer = m_BaseBuffer;
  memset(tmpBuffer, 0, 8 * NUM_STATIC_PROPERTIES);
  uint8_t* curPos = tmpBuffer;
  uint8_t* endPos = tmpBuffer + size;

  int j = 0;
  try {
    while (data->tellg() < streamLimit) {
      curPos = prop.index(curPos, obj, dataStream, data, streamLimit);
      if (curPos + 8 >= endPos) {
        const size_t offset = curPos - tmpBuffer;
        // tmpArrayBuffer.resize(tmpArrayBuffer.size() * 2);
        size *= 2;
        uint8_t* newBuffer = new uint8_t[size];
        memcpy(newBuffer, tmpBuffer, offset);
        tmpBuffer = newBuffer;

        curPos = tmpBuffer + offset;
        endPos = tmpBuffer + size;
      }
      ++j;
    }
  }
  catch (const std::exception& e) {
    // TODO: assuming this is an eof exception
    LOG_F("eos loop canceled: {}", e.what());
    // if (tmpBuffer != m_BaseBuffer) { delete [] tmpBuffer; }
    // throw e;
  }

  ObjSize count = j;
  uint32_t arraySize = static_cast<uint32_t>(curPos - tmpBuffer);
  LOG_F("indexed eos array with {} items to {:x}", count, (uint64_t)buffer);
  // std::cout << "array size " << arraySize << std::endl;
  // create a sufficiently sized array index
  ObjSize arrayOffset = indexTable->allocateArray(arraySize);
  // store the array index
  memcpy(indexTable->arrayAddress(arrayOffset), tmpBuffer, arraySize);
  // buffer receives the effective number of items and the offset into the array index
  memcpy(buffer, reinterpret_cast<char*>(&count), sizeof(ObjSize));
  memcpy(buffer + sizeof(ObjSize), reinterpret_cast<char*>(&arrayOffset), sizeof(ObjSize));
  if (tmpBuffer != m_BaseBuffer) {
    delete[] tmpBuffer;
  }

  return count;
}

void TypeSpec::writeIndex(ObjectIndexTable *indexTable, ObjectIndex *objIndex, std::shared_ptr<IOWrapper> data, const StreamRegistry &streams, DynObject *obj, std::streampos streamLimit) {
  // first: base data offset of the object
  // TODO: this should be the id of the data stream 
  DataStreamId dataStream = 0;
  DataOffset dataOffset = data->tellg();
  LOG_BRACKET_F("write index for obj type {} data {} size {}", m_Name, dataOffset, m_IndexSize);
  // auto bracket = LogBracket::create(fmt::format("write index for obj type {} data {} size {}", m_Name, dataOffset, m_IndexSize));

  // second: the bitmask reflecting which attributes are present
  //   for now this is all zeros because we don't actually know yet
  size_t bitsetBytes = (m_Sequence.size() + 7) / 8;
  uint8_t *bitmask = obj->getBitmask();
  memset(bitmask, 0x00, bitsetBytes);

  uint8_t staticBuffer[8 * NUM_STATIC_PROPERTIES];
  uint8_t *buffer = staticBuffer;
  std::unique_ptr<uint8_t[]> dynamicBuffer;
  if (m_Sequence.size() > NUM_STATIC_PROPERTIES) {
    dynamicBuffer.reset(new uint8_t[m_Sequence.size() * 8]);
    buffer = dynamicBuffer.get();
  }
  DataOffset dataMax = dataOffset;
  uint8_t *propertiesEnd = buffer;

  objIndex->properties = buffer;

  // third: create properties index
  for (size_t i = 0; i < m_Sequence.size(); ++i) {
    LOG_F("index seq {0}/{1}", i, m_Sequence.size());
    int imod8 = i % 8;
    // LogBracket::log(fmt::format("seq {0} {1} {2:x} (vs {3:x})", i, m_Sequence[i].key, reinterpret_cast<int64_t>(propertiesEnd), reinterpret_cast<int64_t>(buffer)));
    TypeProperty &prop = m_Sequence[i];

    if (!prop.index) {
      std::string typeName = m_Registry->getById(prop.typeId)->getName();
      LOG_F("make index func for {0} - list: {1}", typeName, prop.isList);
      prop.index = makeIndexFunc(prop, streams, indexTable);
    }

    bool isPresent = !prop.isConditional || prop.condition(*obj);
    if (isPresent) {
      objIndex->bitmask[i / 8] |= 1 << (i % 8);
    }

    if (isPresent) {
      propertiesEnd = readPropToBuffer(prop, indexTable, propertiesEnd, obj, dataStream, data, streamLimit);
    }
    DataOffset dataNow = data->tellg();
    if (dataNow > dataMax) {
      dataMax = dataNow;
    }
  }

  indexTable->setProperties(objIndex, buffer, propertiesEnd - buffer);
}

std::vector<TypeProperty>::const_iterator TypeSpec::paramByKey(ObjectIndex* objIndex, const char* key, int* offset) const {
  if (offset != nullptr) {
    *offset = 0;
  }

  try {
    int off = m_ParamIdx.at(key);
    if (offset != nullptr) {
      *offset = off;
    }
    return m_Params.begin() + off;
  }
  catch (const std::exception &err) {
    return m_Params.cend();
  }
}

std::vector<TypeProperty>::const_iterator TypeSpec::propertyByKey(ObjectIndex *objIndex, const char *key, int *offset) const {
  if (offset != nullptr) {
    *offset = 0;
  }
  int idx = 0;
  return std::find_if(m_Sequence.cbegin(), m_Sequence.cend(), [&idx, objIndex, key, offset, this](const TypeProperty &prop) {
    bool isPresent = isBitSet(objIndex, idx);
    bool res = (prop.key == key) && isPresent;
    if (!res) {
      ++idx;
      if (isPresent && (offset != nullptr)) {
        if (prop.isList) {
          // lists store the item count and offset into the array index here
          *offset += sizeof(ObjSize) * 2;
        }
        else {
          *offset += indexSize(prop.typeId);
        }
      }
    }
    return res;
  });
}

std::tuple<uint32_t, int, int> TypeSpec::getPorP(ObjectIndex* objIndex, const char* key) const {
  int propertyOffset = 0;

  size_t bitsetBytes = (m_Sequence.size() + 7) / 8;
  size_t offset = sizeof(DataStreamId) + sizeof(DataOffset);

  int idx = 0;

  { // param?
    auto iter = paramByKey(objIndex, key, &propertyOffset);
    LOG_F("param by key {} - {}", key, iter != m_Params.cend());

    if (iter != m_Params.cend()) {
      return std::tuple<uint32_t, size_t, size_t>(iter->typeId, -1, propertyOffset);
    }
  }

  { // property?
    auto iter = propertyByKey(objIndex, key, &propertyOffset);

    if (iter != m_Sequence.cend()) {
      if (!isBitSet(objIndex, idx)) {
        throw std::runtime_error(fmt::format("Property not set: {0}", key));
      }

      return std::tuple<uint32_t, size_t, size_t>(iter->typeId, propertyOffset, -1);
    }
  }

  throw std::runtime_error(fmt::format("Property not found: {0}", key));
}

std::tuple<uint32_t, size_t> TypeSpec::get(ObjectIndex * objIndex, const char *key) const {
  int propertyOffset = 0;

  size_t bitsetBytes = (m_Sequence.size() + 7) / 8;
  size_t offset = sizeof(DataStreamId) + sizeof(DataOffset);

  int idx = 0;

  { // property?
    auto iter = propertyByKey(objIndex, key, &propertyOffset);

    if (iter != m_Sequence.cend()) {
      if (!isBitSet(objIndex, idx)) {
        throw std::runtime_error(fmt::format("Property not set: {0}", key));
      }
      return std::tuple<uint32_t, size_t>(iter->typeId, propertyOffset);
    }
  }

  throw std::runtime_error(fmt::format("Property not found: {0}", key));
}

std::tuple<uint32_t, size_t, std::vector<std::string>, bool> TypeSpec::getWithArgs(ObjectIndex * objIndex, const char *key) const {
  int propertyOffset = 0;

  size_t bitsetBytes = (m_Sequence.size() + 7) / 8;
  size_t offset = sizeof(DataStreamId) + sizeof(DataOffset);

  int idx = 0;

  { // property?
    auto iter = propertyByKey(objIndex, key, &propertyOffset);

    if (iter != m_Sequence.cend()) {
      if (!isBitSet(objIndex, idx)) {
        throw std::runtime_error(fmt::format("Property not set: {0}", key));
      }
      return std::tuple<uint32_t, size_t, std::vector<std::string>, bool>(iter->typeId, propertyOffset, iter->argList, iter->isList);
    }
  }

  throw std::runtime_error(fmt::format("Property not found: {0}", key));
}

std::tuple<uint32_t, size_t, SizeFunc, AssignCB> TypeSpec::getFull(ObjectIndex * objIndex, const char * key) const {
  size_t bitsetBytes = (m_Sequence.size() + 7) / 8;
  size_t offset = sizeof(DataStreamId) + sizeof(DataOffset);

  const uint8_t *bitmaskCur = objIndex->bitmask;
  uint8_t bitmaskMask = 0x01;

  int idx = 0;
  int propertyOffset = 0;
  auto iter = propertyByKey(objIndex, key, &propertyOffset);

  offset += bitsetBytes + propertyOffset;

  return std::tuple<uint32_t, size_t, SizeFunc, AssignCB>(iter->typeId, propertyOffset, iter->size, iter->onAssign);
}

uint8_t *TypeSpec::indexCustom(const TypeProperty &prop, uint32_t typeId,
                               const StreamRegistry &streams,
                               ObjectIndexTable *indexTable,
                               uint8_t *index, const DynObject *obj,
                               DataStreamId dataStream, std::shared_ptr<IOWrapper> data, std::streampos streamLimit) {
  std::shared_ptr<TypeSpec> spec = m_Registry->getById(typeId);
  int staticSize = spec->getStaticSize();
  LOG_BRACKET_F("index custom spec {} - {}", spec->getName(), typeId);
  std::streampos dataPos = data->tellg();

  int64_t x = static_cast<int64_t>(streamLimit);
  int64_t y = static_cast<int64_t>(dataPos);
  int64_t remaining = x - y;
  if ((static_cast<int64_t>(streamLimit) != 0) && ((static_cast<int64_t>(streamLimit) - static_cast<int64_t>(dataPos)) < 0)) {
    std::cerr << "end of stream " << x << " - " << y << std::endl;
    throw std::runtime_error("end of stream");
  }

  LOG_F("custom type {0} - static size {1}, num properties {2}", spec->getName(), staticSize, spec->getProperties().size());

  char *res = nullptr;
  if (staticSize >= 0) {
    // static size so the object itself doesn't have to be indexed at this time

    // LogBracket::log(fmt::format("{0} static size {1}", prop.key, staticSize));
    res = type_index_obj(reinterpret_cast<char*>(index), data, dataPos, staticSize, obj);
  }
  else {
    // unknown size. to read past the object we have to index it recursively
    ObjectIndex *propObjIndex = indexTable->allocateObject(spec, dataStream, dataPos);

    DynObject newObj(spec, streams, indexTable, propObjIndex, obj);

    ObjSize size = prop.hasSizeFunc ? prop.size(*obj) : 0;

    if (size < 0) {
      throw std::runtime_error("invalid size");
    }
    std::streampos newLimit = prop.hasSizeFunc
      ? (dataPos + std::streamoff(size))
      : streamLimit;

    if (newLimit != streamLimit) {
      int64_t lim = newLimit;
      if (lim < 0) {
        ObjSize again = prop.size(*obj);
        LOG_F("new limit unknown, determine size (of {}) {}", m_Registry->getById(prop.typeId)->getName(), again);
      }
      LOG_F("new stream limit {} (has size: {})", lim, prop.hasSizeFunc);
      lim += 1;
    }
    newObj.writeIndex(dataPos, newLimit, true);

    std::streamoff dynSize = data->tellg() - dataPos;

    LOG_F("parsed object (type {}) size was {} ({} - {})", getRegistry()->getById(typeId)->getName(), dynSize, data->tellg(), dataPos);

    // type_index expects the data stream to be positioned at the start of the indexed object

    int64_t objPtr = reinterpret_cast<int64_t>(propObjIndex) * -1;
    memcpy(index, reinterpret_cast<char*>(&objPtr), sizeof(int64_t));
    data->seekg(dataPos + dynSize);
    res = reinterpret_cast<char*>(index) + sizeof(int64_t);
    // data->seekg(dataPos);
    // res = type_index_obj(reinterpret_cast<char*>(buffer), data, dataPos, static_cast<ObjSize>(dynSize), obj);
  }

  return reinterpret_cast<uint8_t*>(res);
}

auto TypeSpec::makeIndexFunc(const TypeProperty &prop,
                             const StreamRegistry &streams,
                             ObjectIndexTable *indexTable)
                             -> IndexFunc {
  if (prop.typeId == TypeId::runtime) {
    return [this, prop, indexTable, streams](uint8_t *index, const DynObject *obj, DataStreamId dataStream, std::shared_ptr<IOWrapper> data, std::streampos streamLimit) -> uint8_t* {
      // TODO: currently assumes a runtime type never resolves to bit - which I really hope is true
      LOG_F("reset bitmask offset (1)");
      this->m_BitmaskOffset = 0;
      std::variant<std::string, int32_t> caseId = prop.switchFunc(*obj);
      auto iter = prop.switchCases.find(caseId);
      if (iter == prop.switchCases.end()) {
        iter = prop.switchCases.find("_");
      }
      if (iter == prop.switchCases.end()) {
        // apparently it's ok for there to not be a match, in this case ignore the content, consume nothing
        // if there is no size field
        if (prop.hasSizeFunc) {
          auto size = prop.size(*obj);
          data->seekg(data->tellg() + size);
          std::cout << "unmatched switch size " << size << std::endl;
        }
        return index;
      }
      uint32_t typeId = iter->second;
      LOG_F("index runtime type: \"{}\" - {} at {}", m_Registry->getById(typeId)->getName(), typeId, reinterpret_cast<int64_t>(index));

      memcpy(index, reinterpret_cast<uint8_t*>(&typeId), sizeof(uint32_t));
      index += sizeof(uint32_t);

      if (typeId >= TypeId::custom) {
        auto before = data->tellg();
        auto res = this->indexCustom(prop, typeId, streams, indexTable, index, obj, dataStream, data, streamLimit);
        auto after = data->tellg();
        if ((after - before) == 0) {
          throw std::runtime_error(fmt::format("0 byte custom type \"{}\" could lead to endless loop", m_Registry->getById(typeId)->getName()));
        }
        return res;
      }
      else {
        char *res = type_index(static_cast<TypeId>(typeId), prop.size, reinterpret_cast<char*>(index), data, obj, prop.debug);
        return reinterpret_cast<uint8_t*>(res);
      }
    };
  }
  else if (prop.typeId >= TypeId::custom) {
    // index custom type
    return [this, prop, indexTable, streams](uint8_t* index, const DynObject* obj, DataStreamId dataStream, std::shared_ptr<IOWrapper> data, std::streampos streamLimit) -> uint8_t* {
      LOG_F("reset bitmask offset (2) -> {}", this->m_BitmaskOffset);
      this->m_BitmaskOffset = 0;
      LOG_F("index custom type {}", m_Registry->getById(prop.typeId)->getName());
      return this->indexCustom(prop, prop.typeId, streams, indexTable, index, obj, dataStream, data, streamLimit);
    };
  } else if (prop.typeId == TypeId::bits) {
    return [=](uint8_t* index, const DynObject* obj, DataStreamId dataStream, std::shared_ptr<IOWrapper> data, std::streampos streamLimit) -> uint8_t* {
      uint32_t size = prop.size(*obj);
      LOG_F("index bitmask off {}, size {}", this->m_BitmaskOffset, size);
      if ((static_cast<uint64_t>(this->m_BitmaskOffset) + size) > sizeof(uint32_t) * 8) {
        LOG_F("reset bitmask offset (3)");
        this->m_BitmaskOffset = 0;
      }

      LOG_F("index bitmask {}", data->tellg());
      char *res = type_index_bits(static_cast<TypeId>(prop.typeId), this->m_BitmaskOffset, size, reinterpret_cast<char*>(index), data, obj, prop.debug);
      this->m_BitmaskOffset = (this->m_BitmaskOffset + size) % 8;
      return reinterpret_cast<uint8_t*>(res);
    };
  } else {
    // index pod
    return [=](uint8_t *index, const DynObject *obj, DataStreamId dataStream, std::shared_ptr<IOWrapper> data, std::streampos streamLimit) -> uint8_t* {
      LOG_F("reset bitmask offset (4)");
      this->m_BitmaskOffset = 0;
      LOG_F("index pod type {}", m_Registry->getById(prop.typeId)->getName());
      char *res = type_index(static_cast<TypeId>(prop.typeId), prop.size, reinterpret_cast<char*>(index), data, obj, prop.debug);
      return reinterpret_cast<uint8_t*>(res);
    };
  }
}

TypePropertyBuilder::TypePropertyBuilder(TypeProperty *wrappee, std::function<void()> cb)
  : m_Wrappee(wrappee)
  , m_Callback(cb)
{
}

TypePropertyBuilder &TypePropertyBuilder::withProcessing(const std::string &algorithm) {
  m_Wrappee->processing = algorithm;
  return *this;
}

TypePropertyBuilder &TypePropertyBuilder::withCondition(ConditionFunc func) {
  m_Wrappee->condition = func;
  m_Wrappee->isConditional = true;
  return *this;
}

TypePropertyBuilder& TypePropertyBuilder::withValidation(ValidationFunc func) {
  m_Wrappee->validation = func;
  m_Wrappee->isValidated = true;
  return *this;
}

TypePropertyBuilder& TypePropertyBuilder::withDebug(const std::string &debugMessage) {
  m_Wrappee->debug = debugMessage;
  return *this;
}

TypePropertyBuilder& TypePropertyBuilder::withArguments(const std::vector<std::string>& args) {
  m_Wrappee->argList = args;
  return *this;
}

TypePropertyBuilder &TypePropertyBuilder::withSize(SizeFunc func) {
  m_Wrappee->size = func;
  m_Wrappee->hasSizeFunc = true;
  return *this;
}

TypePropertyBuilder& TypePropertyBuilder::withEnum(const std::string &enumName) {
  m_Wrappee->enumName = enumName;
  m_Wrappee->hasEnum = true;
  return *this;
}

TypePropertyBuilder &TypePropertyBuilder::withRepeatToEOS() {
  m_Wrappee->count = eosCount;
  m_Wrappee->isList = true;
  return *this;
}

TypePropertyBuilder &TypePropertyBuilder::withCount(SizeFunc func) {
  m_Wrappee->count = func;
  m_Wrappee->isList = true;
  return *this;
}

TypePropertyBuilder &TypePropertyBuilder::withTypeSwitch(SwitchFunc func, const std::map<std::variant<std::string, int32_t>, uint32_t> &cases) {
  m_Wrappee->switchFunc = func;
  m_Wrappee->switchCases = cases;
  m_Wrappee->isSwitch = true;
  return *this;
}

TypePropertyBuilder &TypePropertyBuilder::onAssign(AssignCB func) {
  m_Wrappee->onAssign = func;
  return *this;
}
