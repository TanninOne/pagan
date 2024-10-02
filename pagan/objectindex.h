#pragma once

#include <memory>

namespace pagan {

class TypeSpec;

struct ObjectIndex {
  // offset into the data stream where the data for this object can be found
  uint64_t dataOffset;
  // reference to the index of the object properties
  uint8_t *properties;
  // specifies the type of object
  uint32_t typeId;
  // specifies which data stream this object is found in
  uint16_t dataStream;
  // total size of this index (including this size field)
  uint8_t size;
  // variable length bitmask specifying which properties are set
  uint8_t bitmask[1];
};

static const int MIN_OBJECT_INDEX_SIZE = sizeof(ObjectIndex) - 1;

bool isBitSet(const ObjectIndex *index, int bits);

ObjectIndex *initIndex(uint8_t *memory, const std::shared_ptr<TypeSpec> type,
                       uint16_t dataStream, uint64_t dataOffset);

void assignProperies(ObjectIndex *index, uint8_t *buffer, size_t size);

} // namespace pagan
