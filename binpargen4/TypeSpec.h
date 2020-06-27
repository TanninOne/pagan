#pragma once

#include <vector>
#include <atomic>
#include <algorithm>
#include <tuple>
#include <sstream>
#include <functional>
#include <any>
#include <cassert>
#include "types.h"
#include "typecast.h"
#include "typeregistry.h"
#include "objectindex.h"
#include "streamregistry.h"
#include "util.h"
#include "objectindextable.h"

class DynObject;

// number of properties where we use a static buffer to buffer
// the properties. If an object has more properties, we allocate a buffer
// on the heap
static const int NUM_STATIC_PROPERTIES = 64;

static const ObjSize COUNT_EOS = -2;

typedef std::function<uint8_t* (uint8_t*, DynObject*, DataStreamId, std::shared_ptr<IOWrapper>, std::streampos)> IndexFunc;

struct TypeProperty {
  std::string key;
  uint32_t typeId;
  SizeFunc size;
  SizeFunc count;
  ConditionFunc condition;
  AssignCB onAssign;
  bool isList;
  bool isConditional;
  bool hasSizeFunc;
  bool isSwitch;
  std::string processing;
  IndexFunc index;
  SwitchFunc switchFunc;
  std::map<std::string, uint32_t> switchCases;
};

static SizeFunc nullSize = [] (const DynObject &object) -> ObjSize {
  return -1;
};

static SizeFunc eosCount = [] (const DynObject &object) -> ObjSize {
  return COUNT_EOS;
};

static ConditionFunc trueFunc = [](const DynObject &object) -> bool {
  return true;
};

static AssignCB nop = [] (DynObject &object, const std::any &value) {
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
  TypePropertyBuilder &withTypeSwitch(SwitchFunc func, const std::map<std::string, uint32_t> &cases);
  TypePropertyBuilder &onAssign(AssignCB func);
  TypePropertyBuilder &withProcessing(const std::string &algorithm);

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

  TypeSpec(const char *name, TypeRegistry *registry);
  ~TypeSpec();

  TypeRegistry *getRegistry() const {
    return m_Registry;
  }

  uint16_t getNumProperties() const {
    return static_cast<uint16_t>(m_Sequence.size());
  }

  uint16_t indexSize() const {
    return m_IndexSize;
  }

  TypePropertyBuilder appendProperty(const char *key, uint32_t type) {
    LOG_F("append prop to {0} - {1} size index {2}, size data {3}", m_Id, key, m_IndexSize, m_StaticSize);
    m_Sequence.push_back({ key, type, nullSize, nullSize, trueFunc, nop, false, false, false });
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

  int32_t getStaticSize() const {
    return m_StaticSize;
  }

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
        // unknown number of items

        std::vector<uint8_t> tmpArrayBuffer(8 * NUM_STATIC_PROPERTIES);
        uint8_t *curPos = &tmpArrayBuffer[0];
        uint8_t *endPos = curPos + tmpArrayBuffer.size();

        int j = 0;
        try {
          while (data->tellg() < streamLimit) {
            curPos = prop.index(curPos, obj, dataStream, data, streamLimit);
            if (curPos + 8 >= endPos) {
              const size_t offset = curPos - &tmpArrayBuffer[0];
              tmpArrayBuffer.resize(tmpArrayBuffer.size() * 2);
              curPos = &tmpArrayBuffer[0] + offset;
              endPos = &tmpArrayBuffer[0] + tmpArrayBuffer.size();
            }
            ++j;
          }
        }
        catch (const std::exception &e) {
          // TODO: assuming this is an eof exception
          LOG_F("eos loop canceled: {}", e.what());
          throw e;
        }
        // why the + 1?
        // count = j + 1;
        count = j;
        uint32_t arraySize = static_cast<uint32_t>(curPos - &tmpArrayBuffer[0]);
        ObjSize arrayOffset = indexTable->allocateArray(arraySize);
        memcpy(indexTable->arrayAddress(arrayOffset), &tmpArrayBuffer[0], arraySize);
        memcpy(buffer, reinterpret_cast<char*>(&count), sizeof(ObjSize));
        memcpy(buffer + sizeof(ObjSize), reinterpret_cast<char*>(&arrayOffset), sizeof(ObjSize));
      }
      else {
        // In the case of a static length array we can allocate the target array right away
        // and write directly to that
        ObjSize arrayOffset = indexTable->allocateArray(count * indexSize(prop.typeId));
        memcpy(buffer, reinterpret_cast<char*>(&count), sizeof(ObjSize));
        memcpy(buffer + sizeof(ObjSize), reinterpret_cast<char*>(&arrayOffset), sizeof(ObjSize));

        uint8_t *curPos = indexTable->arrayAddress(arrayOffset);

        for (int j = 0; j < count; ++j) {
          curPos = prop.index(curPos, obj, dataStream, data, streamLimit);
        }
      }
      return buffer + sizeof(ObjSize) * 2;
    }
    else {
      // no list, index single item
      LOG_F("index prop {} (type {}) at {}", prop.key, prop.typeId, data->tellg());
      return prop.index(buffer, obj, dataStream, data, streamLimit);
    }
  }

  void writeIndex(ObjectIndexTable *index, ObjectIndex *objIndex, std::shared_ptr<IOWrapper> data, const StreamRegistry &streams, DynObject *obj, std::streampos streamLimit);

  const std::vector<TypeProperty> &getProperties() const {
    return m_Sequence;
  }

  std::tuple<uint32_t, size_t> get(ObjectIndex *objIndex, const char *key) const;

  std::tuple<uint32_t, size_t, SizeFunc, AssignCB> getFull(ObjectIndex *objIndex, const char *key) const {
    size_t bitsetBytes = (m_Sequence.size() + 7) / 8;
    size_t offset = sizeof(DataStreamId) + sizeof(DataOffset);

    const uint8_t *bitmaskCur = objIndex->bitmask;
    uint8_t bitmaskMask = 0x01;

    int idx = 0;
    int attributeOffset = 0;
    auto iter = std::find_if(m_Sequence.cbegin(), m_Sequence.cend(), [&](const TypeProperty &prop) {
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

    offset += bitsetBytes + attributeOffset;

    return std::tuple<uint32_t, size_t, SizeFunc, AssignCB>(iter->typeId, attributeOffset, iter->size, iter->onAssign);
  }

  uint32_t getId() const {
    return m_Id;
  }

  std::string getName() const {
    return m_Name;
  }

private:

  static uint32_t getNextId() {
    static std::atomic<uint32_t> s_NextId = TypeId::custom;
    return s_NextId++;
  }

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

    if (typeId < TypeId::custom) {
      m_StaticSize += indexSize(typeId);
      LOG_F("pod size {0} now {1}", typeId, m_StaticSize);
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
    uint8_t *index, DynObject *obj,
    DataStreamId dataStream, std::shared_ptr<IOWrapper> data, std::streampos streamLimit);

  auto makeIndexFunc(const TypeProperty &prop,
                     const StreamRegistry &streams,
                     ObjectIndexTable *index)
                     -> IndexFunc;

private:

  std::string m_Name;
  TypeRegistry *m_Registry;
  std::vector<TypeProperty> m_Sequence;
  uint16_t m_IndexSize{0};
  uint32_t m_Id;
  int32_t m_StaticSize;

};
