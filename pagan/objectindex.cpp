#include "objectindex.h"
#include "typespec.h"
#include <memory>
#include <cstdint>

/*
bool attributePresent(const std::shared_ptr<IOWrapper>& index, int attribute) {
  uint8_t bitset;
  if (attribute > 7) {
    index->seekg(attribute / 8, std::ios::cur);
  }
  index->read(reinterpret_cast<char*>(&bitset), 1);
  int byte = attribute / 8;
  return bitset & (0x01 << (attribute % 8));
}

size_t attributeOffset(const std::shared_ptr<IOWrapper>& index, int attribute) {
  size_t res = 0;
  uint8_t bitset;
  index->read(reinterpret_cast<char*>(&bitset), 1);
  uint8_t mask = 0x01;
  for (int i = 0; i < attribute; ++i) {
    if (bitset & mask) {
      ++res;
    }
    mask <<= 1;
    if (mask == 0) {
      index->read(reinterpret_cast<char*>(&bitset), 1);
      mask = 0x01;
    }
  }
  return res * sizeof(uint64_t);
}
*/

bool isBitSet(const ObjectIndex *index, int bit) {
  return index->bitmask[bit / 8] & (1 << (bit % 8));
}

ObjectIndex *initIndex(uint8_t *memory, const std::shared_ptr<TypeSpec> type, uint16_t dataStream, uint64_t dataOffset) {
  ObjectIndex *res = reinterpret_cast<ObjectIndex*>(memory);

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

