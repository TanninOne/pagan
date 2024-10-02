#include "objectindextable.h"
#include "typespec.h"

namespace pagan {

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
  ++m_ObjectCount;

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
    throw std::runtime_error(std::format("array too long: {} > {}", size, ARRAY_CHUNK_SIZE));
  }
  if (ARRAY_CHUNK_SIZE - m_NextFreeArrayIndex < size) {
    addArrayBuffer();
  }

  ObjSize offset = static_cast<ObjSize>(((m_ArrayBuffers.size() - 1) << ARRAY_CHUNK_SIZE_BITS)
    | m_NextFreeArrayIndex);

  m_NextFreeArrayIndex += size;
  ++m_ObjectCount;

  return offset;
}

uint8_t *ObjectIndexTable::arrayAddress(ObjSize offset) {
  uint32_t idx = offset & ARRAY_CHUNK_SIZE;
  uint32_t arrayNum = (offset & (0xFFFFFFFF - ARRAY_CHUNK_SIZE)) >> ARRAY_CHUNK_SIZE_BITS;

  return m_ArrayBuffers[arrayNum].get()[0] + idx;
}

void ObjectIndexTable::addObjBuffer() {
  if (m_ObjBuffers.size() > 0) {
    // for debugging purposes we store how much of each chunk is actually used
    m_ObjBufferSizes.push_back(m_NextFreeObjIndex);
  }
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

std::vector<uint8_t> ObjectIndexTable::getObjectIndex() const {
  std::vector<uint8_t> result;
  for (int i = 0; i < m_ObjBuffers.size() - 1; ++i) {
    uint8_t *from = *(m_ObjBuffers[i].get());
    uint8_t *to = from + m_ObjBufferSizes[i];
    result.insert(result.end(), from, to);
  }
  uint8_t *from = *(m_ObjBuffers.rbegin()->get());
  uint8_t* to = from + m_NextFreeObjIndex;
  result.insert(result.end(), from, to);
  return result;
}

std::vector<uint8_t> ObjectIndexTable::getArrayIndex() const {
  std::vector<uint8_t> result;
  for (const auto &iter : m_ArrayBuffers) {
    uint8_t *from = *(iter.get());
    uint8_t *to = from + CHUNK_SIZE;
    result.insert(result.end(), from, to);
  }
  return result;
}

} // namespace pagan
