#pragma once

#include "iowrap.h"
#include "types.h"
#include <cstdint>
#include <stdexcept>
#include <memory>

static const uint64_t OFFSET_MASK = (0x01LLU << 63) - 1;
static const uint64_t WRITTEN_BIT = 0x01LLU << 63;

class IncompatibleType : public std::runtime_error {
public:
  IncompatibleType(const char *pos)
    : std::runtime_error(fmt::format("Type is incompatible {}", pos).c_str())
  {
  }
};

// using an index, read the data
template <typename T> T type_read(TypeId type, char *index, std::shared_ptr<IOWrapper> &data, std::shared_ptr<IOWrapper> &write);
// using an index, write data
template <typename T> char *type_write(TypeId type, char *index, std::shared_ptr<IOWrapper> &write, const T &value, const DynObject *obj);
// create an index for data
char *type_index(TypeId type, const SizeFunc &size, char *index, std::shared_ptr<IOWrapper> &data, const DynObject *obj);

char *type_index_obj(char *index, std::shared_ptr<IOWrapper> &data, std::streampos dataPos, ObjSize size, const DynObject *obj);

#define DECL_TYPE(VAL_TYPE) \
template <> VAL_TYPE type_read(TypeId type, char *index, std::shared_ptr<IOWrapper> &data, std::shared_ptr<IOWrapper> &write);\
template <> char *type_write(TypeId type, char *index, std::shared_ptr<IOWrapper> &write, const VAL_TYPE &value, const DynObject *obj);


std::any type_read_any(TypeId type, char *index, std::shared_ptr<IOWrapper> &data, std::shared_ptr<IOWrapper> &write);

DECL_TYPE(int8_t);
DECL_TYPE(int16_t);
DECL_TYPE(int32_t);
DECL_TYPE(int64_t);
DECL_TYPE(uint8_t);
DECL_TYPE(uint16_t);
DECL_TYPE(uint32_t);
DECL_TYPE(uint64_t);
DECL_TYPE(std::string);
DECL_TYPE(float);

