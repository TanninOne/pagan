#include "TypeSpec.h"
#include "DynObject.h"

TypeSpec::TypeSpec(const char *name, TypeRegistry *registry)
  : m_Name(name), m_Registry(registry), m_Id(TypeSpec::getNextId()), m_StaticSize(0)
{
}

TypeSpec::~TypeSpec()
{
}

void TypeSpec::writeIndex(ObjectIndexTable *indexTable, ObjectIndex *objIndex, std::shared_ptr<IOWrapper> data, const StreamRegistry &streams, DynObject *obj, std::streampos streamLimit) {
  // first: base data offset of the object
  // TODO: this should be the id of the data stream 
  DataStreamId dataStream = 0;
  DataOffset dataOffset = data->tellg();
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
    int imod8 = i % 8;
    // LogBracket::log(fmt::format("seq {0} {1} {2:x} (vs {3:x})", i, m_Sequence[i].key, reinterpret_cast<int64_t>(propertiesEnd), reinterpret_cast<int64_t>(buffer)));
    TypeProperty &prop = m_Sequence[i];

    if (!prop.index) {
      LOG_F("make index func for {0}", prop.typeId);
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

std::tuple<uint32_t, size_t> TypeSpec::get(ObjectIndex * objIndex, const char *key) const {
  int attributeOffset = 0;

  size_t bitsetBytes = (m_Sequence.size() + 7) / 8;
  size_t offset = sizeof(DataStreamId) + sizeof(DataOffset);

  int idx = 0;

  auto iter = std::find_if(m_Sequence.cbegin(), m_Sequence.cend(), [&idx, objIndex, key, &attributeOffset, this](const TypeProperty &prop) {
    bool isPresent = isBitSet(objIndex, idx);
    bool res = (prop.key == key) && isPresent;
    if (!res) {
      ++idx;
      if (isPresent) {
        if (prop.isList) {
          // lists store the item count and offset into the array index here
          attributeOffset += sizeof(ObjSize) * 2;
        }
        else {
          attributeOffset += indexSize(prop.typeId);
        }
      }
    }
    return res;
    }); 

  if (iter == m_Sequence.cend()) {
    throw std::runtime_error(fmt::format("Property not found: {0}", key));
  }

  if (!isBitSet(objIndex, idx)) {
    throw std::runtime_error(fmt::format("Property not set: {0}", key));
  }

  // offset += bitsetBytes + attributeOffset;

  return std::tuple<uint32_t, size_t>(iter->typeId, attributeOffset);
}

uint8_t *TypeSpec::indexCustom(const TypeProperty &prop, uint32_t typeId,
                               const StreamRegistry &streams,
                               ObjectIndexTable *indexTable,
                               uint8_t *index, DynObject *obj,
                               DataStreamId dataStream, std::shared_ptr<IOWrapper> data, std::streampos streamLimit) {
  std::shared_ptr<TypeSpec> spec = m_Registry->getById(typeId);
  int staticSize = spec->getStaticSize();
  LOG_BRACKET_F("index custom spec {}", spec->getName());
  std::streampos dataPos = data->tellg();

  int64_t x = static_cast<int64_t>(streamLimit);
  int64_t y = static_cast<int64_t>(dataPos);
  int64_t remaining = x - y;
  if ((static_cast<int64_t>(streamLimit) != 0) && ((static_cast<int64_t>(streamLimit) - static_cast<int64_t>(dataPos)) < 0)) {
    std::cerr << "end of stream " << x << " - " << y << std::endl;
    throw std::runtime_error("end of stream");
  }

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
    return [this, prop, indexTable, streams](uint8_t *index, DynObject *obj, DataStreamId dataStream, std::shared_ptr<IOWrapper> data, std::streampos streamLimit) -> uint8_t* {
      std::string caseId = prop.switchFunc(*obj);
      auto iter = prop.switchCases.find(caseId);
      if (iter == prop.switchCases.end()) {
        iter = prop.switchCases.find("_");
      }
      uint32_t typeId = iter->second;
      LOG_F("index runtime type: {}", typeId);

      memcpy(index, reinterpret_cast<uint8_t*>(&typeId), sizeof(uint32_t));
      index += sizeof(uint32_t);

      if (typeId >= TypeId::custom) {
        return this->indexCustom(prop, typeId, streams, indexTable, index, obj, dataStream, data, streamLimit);
      }
      else {
        char *res = type_index(static_cast<TypeId>(typeId), prop.size, reinterpret_cast<char*>(index), data, obj);
        return reinterpret_cast<uint8_t*>(res);
      }
    };
  } else if (prop.typeId >= TypeId::custom) {
    // index custom type
    return [this, prop, indexTable, streams](uint8_t *index, DynObject *obj, DataStreamId dataStream, std::shared_ptr<IOWrapper> data, std::streampos streamLimit) -> uint8_t* {
      LOG_F("index custom type {}", prop.typeId);
      return this->indexCustom(prop, prop.typeId, streams, indexTable, index, obj, dataStream, data, streamLimit);
    };
  }
  else {
    // index pod
    return [=](uint8_t *index, DynObject *obj, DataStreamId dataStream, std::shared_ptr<IOWrapper> data, std::streampos streamLimit) -> uint8_t* {
      LOG_F("index pod type {}", prop.typeId);
      char *res = type_index(static_cast<TypeId>(prop.typeId), prop.size, reinterpret_cast<char*>(index), data, obj);
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

TypePropertyBuilder &TypePropertyBuilder::withSize(SizeFunc func) {
  m_Wrappee->size = func;
  m_Wrappee->hasSizeFunc = true;
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

TypePropertyBuilder &TypePropertyBuilder::withTypeSwitch(SwitchFunc func, const std::map<std::string, uint32_t> &cases) {
  m_Wrappee->switchFunc = func;
  m_Wrappee->switchCases = cases;
  m_Wrappee->isSwitch = true;
  return *this;
}

TypePropertyBuilder &TypePropertyBuilder::onAssign(AssignCB func) {
  m_Wrappee->onAssign = func;
  return *this;
}
