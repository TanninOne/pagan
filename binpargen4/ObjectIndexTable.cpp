#include "objectindextable.h"
#include "typespec.h"


static const uint32_t CHUNK_SIZE = 64 * 1024;

// TODO: the code can not handle the case where the elements of a single array don't fit into one chunk.
//   e.g. if the chunk size is only 64kb - each item can take up to 8 byte - no array can be larger than
//   8000 items
static const uint8_t ARRAY_CHUNK_SIZE_BITS = 24;
static const uint32_t ARRAY_CHUNK_SIZE = static_cast<uint32_t>((1 << ARRAY_CHUNK_SIZE_BITS) - 1);


ObjectIndexTable::ObjectIndexTable()
{
  addObjBuffer();
  addPropBuffer();
  addArrayBuffer();
}


ObjectIndexTable::~ObjectIndexTable()
{
}


ObjectIndex *ObjectIndexTable::allocateObject(const std::shared_ptr<TypeSpec> type, DataStreamId dataStream, DataOffset dataOffset) {
  int bitsetSize = (type->getNumProperties() + 7) / 8;

  uint32_t indexSize = (MIN_OBJECT_INDEX_SIZE + bitsetSize);

  if ((CHUNK_SIZE - m_NextFreeObjIndex) < indexSize) {
    addObjBuffer();
  }

  uint8_t *target = **m_ObjBuffers.rbegin() + m_NextFreeObjIndex;

  m_NextFreeObjIndex += indexSize;

  return initIndex(target, type, dataStream, dataOffset);
}

void ObjectIndexTable::setProperties(ObjectIndex *obj, uint8_t *buffer, size_t size) {
  if (CHUNK_SIZE - m_NextFreePropIndex < size) {
    addPropBuffer();
  }

  obj->properties = **m_PropBuffers.rbegin() + m_NextFreePropIndex;

  assignProperies(obj, buffer, size);

  m_NextFreePropIndex += static_cast<uint32_t>(size);
}

ObjSize ObjectIndexTable::allocateArray(uint32_t size) {
  if (size > ARRAY_CHUNK_SIZE) {
    throw std::runtime_error("array too long");
  }
  if (ARRAY_CHUNK_SIZE - m_NextFreeArrayIndex < size) {
    addArrayBuffer();
  }

  ObjSize offset = static_cast<ObjSize>(((m_ArrayBuffers.size() - 1) << ARRAY_CHUNK_SIZE_BITS)
    | m_NextFreeArrayIndex);

  m_NextFreeArrayIndex += size;

  return offset;
}

uint8_t *ObjectIndexTable::arrayAddress(ObjSize offset) {
  uint32_t idx = offset & ARRAY_CHUNK_SIZE;
  uint32_t arrayNum = (offset & (0xFFFFFFFF - ARRAY_CHUNK_SIZE)) >> ARRAY_CHUNK_SIZE_BITS;

  return m_ArrayBuffers[arrayNum].get()[0] + idx;
}

void ObjectIndexTable::addObjBuffer() {
  m_ObjBuffers.push_back(std::make_unique<uint8_t*>(new uint8_t[CHUNK_SIZE]));
  m_NextFreeObjIndex = 0;
}

void ObjectIndexTable::addPropBuffer() {
  m_PropBuffers.push_back(std::make_unique<uint8_t*>(new uint8_t[CHUNK_SIZE]));
  m_NextFreePropIndex = 0;
}

void ObjectIndexTable::addArrayBuffer() {
  m_ArrayBuffers.push_back(std::make_unique<uint8_t*>(new uint8_t[ARRAY_CHUNK_SIZE]));
  m_NextFreeArrayIndex = 0;
}

