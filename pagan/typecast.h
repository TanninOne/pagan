#pragma once

#include "iowrap.h"
#include "types.h"

#include <cstdint>
#include <stdexcept>
#include <memory>

namespace  pagan {

static const uint64_t OFFSET_MASK = 0x7FFFFFFFFFFFFFFFull;
static const uint64_t WRITTEN_BIT = 0x01LLU << 63;

class IncompatibleType : public std::runtime_error {
public:
  explicit IncompatibleType(std::string_view pos)
    : std::runtime_error(std::format("Type is incompatible: \"{}\"", pos))
  {
  }
};

class InvalidData : public std::runtime_error {
public:
  explicit InvalidData(std::string_view pos)
    : std::runtime_error(std::format("Invalid data: \"{}\"", pos))
  {
  }
};

// using an index, read the data
template <typename T> T type_read(TypeId type, char *index, std::shared_ptr<IOWrapper> &data, std::shared_ptr<IOWrapper> &write, char **indexAfter);
// using an index, write data
template <typename T> char *type_write(TypeId type, char *index, std::shared_ptr<IOWrapper> &write, const T &value);
// create an index for data
char *type_index(TypeId type, const SizeFunc &size, char *index, std::shared_ptr<IOWrapper> &data, const DynObject *obj, const std::string &debug);

char *type_index_obj(char *index, std::shared_ptr<IOWrapper> &data, std::streampos dataPos, ObjSize size, const DynObject *obj);

#define DECL_TYPE(VAL_TYPE) \
template <> VAL_TYPE type_read(TypeId type, char *index, std::shared_ptr<IOWrapper> &data, std::shared_ptr<IOWrapper> &write, char **indexAfter);\
template <> char *type_write(TypeId type, char *index, std::shared_ptr<IOWrapper> &write, const VAL_TYPE &value);

char* type_index_bits(TypeId typeId, uint8_t offset, uint8_t size, char* index, std::shared_ptr<IOWrapper>& data, const DynObject* obj, const std::string& debug);

std::any type_read_any(TypeId type, char *index, std::shared_ptr<IOWrapper> &data, std::shared_ptr<IOWrapper> &write, char **indexAfter = nullptr);

char *type_write_any(TypeId type, char *index, std::shared_ptr<IOWrapper> &write, const std::any &value);

void type_copy_any(TypeId type, char *index, std::shared_ptr<IOWrapper> &output, std::shared_ptr<IOWrapper> &data, std::shared_ptr<IOWrapper> &write, char **indexAfter = nullptr);

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

} // namespace pagan
