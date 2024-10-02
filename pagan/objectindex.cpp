#include "ObjectIndex.h"
#include "TypeSpec.h"
#include <cstdint>
#include <memory>


namespace pagan {

bool isBitSet(const ObjectIndex *index, int bits) {
  return index->bitmask[bits / 8] & (1 << (bits % 8));
}

ObjectIndex *initIndex(uint8_t *memory, const std::shared_ptr<TypeSpec> type,
                       uint16_t dataStream, uint64_t dataOffset) {
  ObjectIndex *res = reinterpret_cast<ObjectIndex *>(memory);

  uint16_t numProperties = type->getNumProperties();
  uint8_t numBitmaskBytes = (numProperties + 7) / 8;

  res->typeId = type->getId();
  // memset(res->bitmask, 0, numBitmaskBytes);
  res->size = MIN_OBJECT_INDEX_SIZE + numBitmaskBytes;

  res->dataStream = dataStream;
  res->dataOffset = dataOffset;
  res->properties = nullptr;

  return res;
}

void assignProperies(ObjectIndex *index, uint8_t *buffer, size_t size) {
  memcpy(index->properties, buffer, size);
}

} // namespace pagan
