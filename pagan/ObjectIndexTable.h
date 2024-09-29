#pragma once

#include "objectindex.h"
#include "types.h"
#include "streamregistry.h"

#include <vector>
#include <memory>

class TypeSpec;

class ObjectIndexTable
{
public:

  ObjectIndexTable();
  ~ObjectIndexTable();

  ObjectIndex *allocateObject(const std::shared_ptr<TypeSpec> type, DataStreamId dataStream, DataOffset dataOffset);
  void setProperties(ObjectIndex *obj, uint8_t *buffer, size_t size);

  // allocate space for an array of the specified size in bytes
  // The (32bit) return value can be used with "arrayAddress" to get at the concrete address of the array
  ObjSize allocateArray(uint32_t size);

  // used to get the full address of the specified 32bit array
  uint8_t *arrayAddress(ObjSize offset);

  uint32_t numObjectIndices() const { return m_ObjectCount; }
  uint32_t numArrayIndices() const { return m_ArrayCount; }

  /**
   * return the entire object index.
   * This is a slow operation and will consume a fair bit of memory, it is only intended for debugging
   */
  std::vector<uint8_t> getObjectIndex() const;

  /**
   * return the entire array index.
   * This is a slow operation and will consume a fair bit of memory, it is only intended for debugging
   */
  std::vector<uint8_t> getArrayIndex() const;

private:

  void addObjBuffer();
  void addPropBuffer();
  void addArrayBuffer();

private:
  std::vector<std::unique_ptr<uint8_t*>> m_ObjBuffers;
  std::vector<uint32_t> m_ObjBufferSizes;
  uint32_t m_NextFreeObjIndex = {0};
  uint32_t m_ObjectCount = 0;

  std::vector<std::unique_ptr<uint8_t*>> m_PropBuffers;
  uint32_t m_NextFreePropIndex = {0};

  std::vector<std::unique_ptr<uint8_t*>> m_ArrayBuffers;
  uint32_t m_NextFreeArrayIndex = {0};
  uint32_t m_ArrayCount = 0;
};

