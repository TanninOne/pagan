#pragma once

#include <vector>
#include <atomic>
#include <algorithm>
#include <tuple>
#include <sstream>
#include <functional>
#include <any>
#include <cassert>
#include <variant>
#include "types.h"
#include "typecast.h"
#include "typeregistry.h"
#include "objectindex.h"
#include "streamregistry.h"
#include "util.h"
#include "objectindextable.h"
#include "dynobject.h"
#include "constants.h"
#include "typeproperty.h"

// number of properties where we use a static buffer to buffer
// the properties. If an object has more properties, we allocate a buffer
// on the heap
static const int NUM_STATIC_PROPERTIES = 64;

/*
struct TypeProperty {
  std::string key;
  uint32_t typeId;
  SizeFunc size;
  SizeFunc count;
  ValidationFunc validation;
  ConditionFunc condition;
  AssignCB onAssign;
  bool isList;
  bool isConditional;
  bool isValidated;
  bool hasSizeFunc;
  bool isSwitch;
  std::string debug;
  std::string processing;
  IndexFunc index;
  SwitchFunc switchFunc;
  std::map<std::variant<std::string, int32_t>, uint32_t> switchCases;
};
*/

static SizeFunc nullSize = [] (const IScriptQuery &object) -> ObjSize {
  return -1;
};

static SizeFunc eosCount = [] (const IScriptQuery &object) -> ObjSize {
  return COUNT_EOS;
};

static ConditionFunc trueFunc = [](const IScriptQuery &object) -> bool {
  return true;
};

static ValidationFunc validFunc = [](const std::any& value) -> bool {
  return true;
};

static AssignCB nop = [] (IScriptQuery &object, const std::any& value) {
};

class TypePropertyBuilder {
public:
  TypePropertyBuilder(TypeProperty *wrappee, std::function<void()> cb);
  ~TypePropertyBuilder() {
    m_Callback();
  }

  TypePropertyBuilder &withCondition(ConditionFunc func);
  TypePropertyBuilder &withSize(SizeFunc func);
  TypePropertyBuilder &withRepeatToEOS();
  TypePropertyBuilder &withCount(SizeFunc func);
  TypePropertyBuilder &withTypeSwitch(SwitchFunc func, const std::map<std::variant<std::string, int32_t>, uint32_t> &cases);
  TypePropertyBuilder &onAssign(AssignCB func);
  TypePropertyBuilder &withProcessing(const std::string &algorithm);
  TypePropertyBuilder &withValidation(ValidationFunc func);
  TypePropertyBuilder &withDebug(const std::string &debugMessage);
  TypePropertyBuilder &withArguments(const std::vector<std::string> &args);

private:
  TypeProperty *m_Wrappee;
  std::function<void()> m_Callback;
};

// TypePropertyBuilder makeProperty(const char *key, uint32_t type);

inline bool isBitSet(const uint8_t *bitmask, uint32_t idx) {
  int bitsetByte = idx / 8;
  int bitsetBit = idx % 8;

  return (bitmask[bitsetByte] & (1 << bitsetBit)) != 0;
}


class TypeSpec
{

public:

  TypeSpec(const char *name, uint32_t id, TypeRegistry *registry);
  ~TypeSpec();

  TypeRegistry *getRegistry() const {
    return m_Registry;
  }

  uint16_t getNumParameters() const {
    return static_cast<uint16_t>(m_Params.size());
  }

  uint16_t getNumProperties() const {
    return static_cast<uint16_t>(m_Sequence.size());
  }

  uint16_t indexSize() const {
    return m_IndexSize;
  }

  TypeProperty getParameter(int index) const {
    return m_Params[index];
  }

  void appendParameter(const char* key, uint32_t type) {
    m_ParamIdx[key] = m_Params.size();
    m_Params.push_back({ key, type, nullSize, nullSize, validFunc, trueFunc, nop, false, false, false, false });
  }

  TypePropertyBuilder appendProperty(const char *key, uint32_t type) {
    LOG_F("append prop to {0} - {1} size index {2}, size data {3}", m_Id, key, m_IndexSize, m_StaticSize);
    m_SequenceIdx[key] = m_Sequence.size();
    m_Sequence.push_back({ key, type, nullSize, nullSize, validFunc, trueFunc, nop, false, false, false, false });
    TypeProperty *prop = &*m_Sequence.rbegin();
    return TypePropertyBuilder(prop, [this, type, prop]() {
      m_IndexSize += prop->isList ? (sizeof(ObjSize) * 2) : indexSize(type);
      if (prop->isConditional || prop->isList || prop->hasSizeFunc || prop->isSwitch || (prop->typeId == TypeId::stringz)) {
        m_StaticSize = -1;
      }
      else {
        addStaticSize(type);
      }
    });
    LOG_F("size after append {0}", m_StaticSize);
  }

  TypePropertyBuilder appendProperty(const char *key, TypeId type) {
    return appendProperty(key, static_cast<uint32_t>(type));
  }

  void addComputed(const char* key, ComputeFunc func) {
    LOG_F("add computed {0} - {1}", m_Id, key);
    m_Computed[key] = func;
  }

  int32_t getStaticSize() const {
    return m_StaticSize;
  }

  ObjSize indexEOSArray(const TypeProperty &prop,
                        ObjectIndexTable *indexTable,
                        uint8_t *buffer,
                        const DynObject *obj,
                        DataStreamId dataStream,
                        std::shared_ptr<IOWrapper> data,
    std::streampos streamLimit);

  /**
    * index the specified property for this object to the buffer.
    * After this call the read pointer of the data stream has to be positioned after the
    * property data
    */
  uint8_t *readPropToBuffer(const TypeProperty &prop,
                            ObjectIndexTable *indexTable,
                            uint8_t *buffer,
                            DynObject *obj,
                            DataStreamId dataStream,
                            std::shared_ptr<IOWrapper> data,
                            std::streampos streamLimit) {
    if (prop.processing != "") {
      throw std::runtime_error("processing not supported");
    }
    // LOG_F("read prop to buffer {} type {} (list: {})", prop.key, prop.typeId, prop.isList);
    if (prop.isList) {
      ObjSize count = prop.count(*obj);

      LOG_BRACKET("indexing array");

      if (count == COUNT_EOS) {
        // unknown number of items but we know the total size of items.
        // we use the array index to store start and size in the data stream, so that it can later be
        // lazy loaded easily
        uint64_t dataOffset = data->tellg();
        ObjSize arrayOffset = indexTable->allocateArray(16);
        uint8_t* curPos = indexTable->arrayAddress(arrayOffset);
        memcpy(curPos, reinterpret_cast<char*>(&dataOffset), sizeof(uint64_t));
        memcpy(curPos + sizeof(uint64_t), reinterpret_cast<char*>(&streamLimit), sizeof(uint64_t));

        memcpy(buffer, reinterpret_cast<char*>(&count), sizeof(ObjSize));
        memcpy(buffer + sizeof(ObjSize), reinterpret_cast<char*>(&arrayOffset), sizeof(ObjSize));
        data->seekg(streamLimit);
      }
      else {
        // In the case of a static length array we can allocate the target array right away
        // and write directly to that
        ObjSize arrayOffset = indexTable->allocateArray(count * indexSize(prop.typeId));
        memcpy(buffer, reinterpret_cast<char*>(&count), sizeof(ObjSize));
        memcpy(buffer + sizeof(ObjSize), reinterpret_cast<char*>(&arrayOffset), sizeof(ObjSize));

        uint8_t *curPos = indexTable->arrayAddress(arrayOffset);

        LOG_F("index array of items {}", count);
        for (int j = 0; j < count; ++j) {
          LOG_F("index array item {}/{}", j, count);
          curPos = prop.index(curPos, obj, dataStream, data, streamLimit);
        }
      }
      return buffer + sizeof(ObjSize) * 2;
    }
    else {
      // no list, index single item
      LOG_F("index prop {} (type {}) at {}", prop.key, m_Registry->getById(prop.typeId)->getName(), data->tellg());
      return prop.index(buffer, obj, dataStream, data, streamLimit);
    }
  }

  void writeIndex(ObjectIndexTable *index, ObjectIndex *objIndex, std::shared_ptr<IOWrapper> data, const StreamRegistry &streams, DynObject *obj, std::streampos streamLimit);

  const std::vector<TypeProperty> &getProperties() const {
    return m_Sequence;
  }

  const TypeProperty& getProperty(const char* key) const {
    auto iter = m_SequenceIdx.find(key);
    if (iter == m_SequenceIdx.end()) {
      throw std::runtime_error(fmt::format("invalid property requested: {0}", key));
    }
    return m_Sequence[iter->second];
  }

  bool hasComputed(const char* key) const {
    return m_Computed.find(key) != m_Computed.end();
  }

  std::any compute(const char* key, const IScriptQuery* obj) const {
    return m_Computed.at(key)(*obj);
  }

  std::tuple<uint32_t, size_t> get(ObjectIndex *objIndex, const char *key) const;
  std::tuple<uint32_t, size_t, std::vector<std::string>> getWithArgs(ObjectIndex *objIndex, const char *key) const;

  std::tuple<uint32_t, int, int> getPorP(ObjectIndex* objIndex, const char* key) const;

  std::tuple<uint32_t, size_t, SizeFunc, AssignCB> getFull(ObjectIndex *objIndex, const char *key) const;

  std::vector<TypeProperty>::const_iterator paramByKey(ObjectIndex* objIndex, const char* key, int* offset) const;
  std::vector<TypeProperty>::const_iterator propertyByKey(ObjectIndex *objIndex, const char *key, int *offset = nullptr) const;

  uint32_t getId() const {
    return m_Id;
  }

  std::string getName() const {
    return m_Name;
  }

private:

  /*
  static uint32_t getNextId() {
    static std::atomic<uint32_t> s_NextId = TypeId::custom;
    return s_NextId++;
  }
  */

private:

  uint8_t indexSize(uint32_t type) const {
    // maximum size any index will take - apart from runtime types which will have the
    // typeid plus this size
    static const int MAX_INDEX_SIZE = sizeof(int64_t);

    if (type >= TypeId::custom) {
      // custom types are stored as offset into the data stream while they aren't
      // indexed yet or as an offset into the index stream when they are.
      // if they are indexed the number is negative
      return sizeof(int64_t);
    }

    if (type == TypeId::runtime) {
      // store switch/case as the type id as determined at runtime plus the index
      // of the effective type. Since we don't know the type until runtime we have
      // to reserve the maximum size it could be. Which should be ok, usually it
      // will be a custom type anyway
      return sizeof(uint32_t) + MAX_INDEX_SIZE;
    }

    switch (static_cast<TypeId>(type)) {
      // numerical values stored as copies, to be modified in-place
      case TypeId::int8: return sizeof(int8_t);
      case TypeId::int16: return sizeof(uint16_t);
      case TypeId::int32: return sizeof(int32_t);
      case TypeId::int64: return sizeof(int64_t);
      case TypeId::uint8: return sizeof(uint8_t);
      case TypeId::uint16: return sizeof(uint16_t);
      case TypeId::uint32: return sizeof(uint32_t);
      case TypeId::uint64: return sizeof(uint64_t);
      case TypeId::bits: return sizeof(uint64_t);
      case TypeId::float32_iee754: return sizeof(float);
      // string stored as offset in the data stream from the beginning of the object
      case TypeId::stringz: return sizeof(int32_t);
      // string stored as offset in the data stream and its size
      case TypeId::string: return 2 * sizeof(int32_t);
      // untyped byte array is stored in the same way as a string
      case TypeId::bytes: return 2 * sizeof(int32_t);
    }
    throw std::runtime_error("invalid type id");
  }

  void addStaticSize(uint32_t typeId) {
    if (m_StaticSize < 0) {
      // the size can already not be determined statically
      return;
    }

    if (typeId == TypeId::bits) {
      m_StaticSize = -1;
      return;
    }

    if (typeId < TypeId::custom) {
      m_StaticSize += indexSize(typeId);
    }
    else {
      int32_t size = m_Registry->getById(typeId)->getStaticSize();
      if (size < 0) {
        m_StaticSize = -1;
      }
      else {
        m_StaticSize += size;
      }
      LOG_F("obj size {0} - {1} now {2}", typeId, size, m_StaticSize);
    }
  }

  uint8_t *indexCustom(const TypeProperty &prop, uint32_t typeId,
    const StreamRegistry &streams,
    ObjectIndexTable *indexTable,
    uint8_t *index, const DynObject *obj,
    DataStreamId dataStream, std::shared_ptr<IOWrapper> data, std::streampos streamLimit);

  auto makeIndexFunc(const TypeProperty &prop,
                     const StreamRegistry &streams,
                     ObjectIndexTable *index)
                     -> IndexFunc;

private:

  std::string m_Name;
  TypeRegistry *m_Registry;
  std::vector<TypeProperty> m_Params;
  std::map<std::string, int> m_ParamIdx;
  std::vector<TypeProperty> m_Sequence;
  std::map<std::string, int> m_SequenceIdx;
  std::map<std::string, ComputeFunc> m_Computed;
  uint16_t m_IndexSize{0};
  uint32_t m_Id;
  int32_t m_StaticSize;

  uint32_t m_BitmaskOffset{0};

  uint8_t m_BaseBuffer[8 * NUM_STATIC_PROPERTIES];

};

