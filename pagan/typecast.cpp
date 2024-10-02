#include "typecast.h"
#include "util.h"
#include "dynobject.h"

#include <cassert>
#include <windows.h>
#include <format>

namespace pagan {

#define DEF_TYPE(VAL_TYPE, TYPE_ID) \
template <> VAL_TYPE type_read(TypeId type, char *index, std::shared_ptr<IOWrapper> &data, std::shared_ptr<IOWrapper> &write, char **indexAfter) { \
  if (type != TYPE_ID) {\
    throw IncompatibleType(std::format("expected type {}, got {}", static_cast<int>(TYPE_ID), static_cast<int>(type)));\
  }\
  VAL_TYPE result;\
  memcpy(reinterpret_cast<char*>(&result), index, sizeof(VAL_TYPE));\
  if (indexAfter != nullptr) {\
    *indexAfter = index + sizeof(VAL_TYPE);\
  }\
  return result;\
}\
template <> char *type_write(TypeId type, char *index, std::shared_ptr<IOWrapper> &write, const VAL_TYPE &value) {\
  if (type != TYPE_ID) {\
    throw IncompatibleType(std::format("expected type {}, got {}", static_cast<int>(TYPE_ID), static_cast<int>(type)));\
  }\
  memcpy(index, reinterpret_cast<const char*>(&value), sizeof(VAL_TYPE));\
  return index + sizeof(VAL_TYPE);\
}

DEF_TYPE(int8_t, TypeId::int8);
DEF_TYPE(int16_t, TypeId::int16);
DEF_TYPE(int32_t, TypeId::int32);
DEF_TYPE(int64_t, TypeId::int64);
DEF_TYPE(uint8_t, TypeId::uint8);
DEF_TYPE(uint16_t, TypeId::uint16);
DEF_TYPE(uint32_t, TypeId::uint32);
DEF_TYPE(uint64_t, TypeId::uint64);
DEF_TYPE(float, TypeId::float32);

template <> std::string type_read(TypeId type, char *index, std::shared_ptr<IOWrapper> &data, std::shared_ptr<IOWrapper> &write, char **indexAfter) {
  if ((type != TypeId::stringz) && (type != TypeId::string)) {
    throw IncompatibleType(std::format("expected string, got {}", static_cast<int>(type)));
  }

  int32_t offset;
  memcpy(reinterpret_cast<char*>(&offset), index, sizeof(int32_t));
  index += sizeof(int32_t);

  const auto &stream = (offset < 0)
    ? write
    : data;

  offset = std::abs(offset);

  std::streampos curPos = stream->tellg();

  int32_t seekOffset = static_cast<int32_t>(curPos) - offset;
  if (seekOffset != 0) {
    stream->seekg(offset);
  }

  std::string result;

  int32_t size;

  if (type == TypeId::string) {
    memcpy(reinterpret_cast<char*>(&size), index, sizeof(int32_t));
    index += sizeof(int32_t);
    result.resize(size);
    stream->read(&result[0], size);
  }
  else {
    int ch;
    size = 0;
    while ((ch = stream->get()) != '\0') {
      result.push_back(static_cast<char>(ch));
      ++size;
    }
  }

  if (indexAfter != nullptr) {
    *indexAfter = index;
  }


  if ((size * -1) != seekOffset) {
    stream->seekg(curPos);
  }

  return result;
}

template <> std::vector<uint8_t> type_read(TypeId type, char *index, std::shared_ptr<IOWrapper> &data, std::shared_ptr<IOWrapper> &write, char **indexAfter) {
  int32_t offset;
  memcpy(reinterpret_cast<char*>(&offset), index, sizeof(int32_t));
  index += sizeof(int32_t);

  const auto &stream = (offset < 0)
    ? write
    : data;

  offset = std::abs(offset);

  std::streampos curPos = stream->tellg();

  int32_t seekOffset = static_cast<int32_t>(curPos) - offset;
  if (seekOffset != 0) {
    stream->seekg(offset);
  }

  int32_t size;
  memcpy(reinterpret_cast<char*>(&size), index, sizeof(int32_t));

  std::vector<uint8_t> result(size);

  index += sizeof(int32_t);
  if (size > 0) {
    stream->read(reinterpret_cast<char*>(&result[0]), size);
  }

  if ((size * -1) != seekOffset) {
    stream->seekg(curPos);
  }

  if (indexAfter != nullptr) {
    *indexAfter = index;
  }

  return result;
}

template<>
char *type_write(TypeId type, char *index, std::shared_ptr<IOWrapper> &write, const std::string &value) {
  write->seekendP();
  int32_t offset = static_cast<int32_t>(write->tellp()) * -1;

  char *pos = index;
  memcpy(pos, reinterpret_cast<char*>(&offset), sizeof(int32_t));
  pos += sizeof(int32_t);
  if (type == TypeId::string) {
    int32_t size = static_cast<int32_t>(value.length());
    memcpy(pos, reinterpret_cast<const char*>(&size), sizeof(int32_t));
    pos += sizeof(int32_t);
  }
  write->write(value.c_str(), value.size() + 1);
  return pos;
}

template<>
char *type_write(TypeId type, char *index, std::shared_ptr<IOWrapper> &write, const std::vector<uint8_t> &value) {
  write->seekendP();
  int32_t offset = static_cast<int32_t>(write->tellp()) * -1;

  char *pos = index;
  memcpy(pos, reinterpret_cast<char*>(&offset), sizeof(int32_t));
  pos += sizeof(int32_t);

  // update size field in index. Currently done independent of type, in the past this was only done for strings
  // and I can't remember why I'd do that. Why didn't you explain, me from the past, why?
  int32_t size = static_cast<int32_t>(value.size());
  memcpy(pos, reinterpret_cast<const char*>(&size), sizeof(int32_t));
  pos += sizeof(int32_t);

  write->write(reinterpret_cast<const char*>(value.data()), value.size() + 1);
  return pos;
}

template <typename T> char *type_index_impl(char *index, std::shared_ptr<IOWrapper> &data, const SizeFunc &size, const DynObject *obj, bool sizeField, const std::string &debug);

template <typename T> char *type_index_num(char *index, std::shared_ptr<IOWrapper> &data, const std::string &debug) {
  try {
    data->read(index, sizeof(T));
    T x = *reinterpret_cast<T*>(index);
    LOG_F("indexed num {} at {}", *reinterpret_cast<T*>(index), data->tellg() - sizeof(T));
    if (!debug.empty()) {
      LOG_F("{}: {}", debug, static_cast<int>(x));
    }
    return index + sizeof(T);
  }
  catch (const std::exception&) {
    throw;
  }
}

char *type_index_obj(char *index, std::shared_ptr<IOWrapper> &data, std::streampos dataPos, ObjSize size, const DynObject *obj) {
  int64_t pos = dataPos;
  memcpy(index, reinterpret_cast<char*>(&pos), sizeof(int64_t));
  data->seekg(dataPos + std::streamoff(size));
  return index + sizeof(int64_t);
}

template <> char *type_index_impl<std::string>(char *index, std::shared_ptr<IOWrapper> &data, const SizeFunc &sizeFunc, const DynObject *obj, bool sizeField, const std::string &debug) {
  auto offset = static_cast<int32_t>(data->tellg());
  if (offset < 0) {
    throw InvalidData("invalid data offset");
  }

  static bool paused = false;

  memcpy(index, &offset, sizeof(int32_t));

  if (sizeField) {
    int32_t size = sizeFunc(*obj);

    data->seekg(static_cast<std::streamoff>(offset) + size);
    memcpy(index + sizeof(int32_t), &size, sizeof(int32_t));
    index += sizeof(int32_t) * 2;
  } else {
    while (data->get() != 0) {
      // skip
    }
    index += sizeof(int32_t);
  }
  if (offset < 100) {
    LOG_F("index string offset {}", offset);
  }
  return index;
}

template <> char *type_index_impl<std::vector<uint8_t>>(char *index, std::shared_ptr<IOWrapper> &data, const SizeFunc &sizeFunc, const DynObject *obj, bool sizeField, const std::string &debug) {
  assert(sizeField == true);
  auto offset = static_cast<int32_t>(data->tellg());
  if (offset < 0) {
    throw InvalidData("invalid data offset");
  }

  memcpy(index, &offset, sizeof(int32_t));

  ObjSize size = sizeFunc(*obj);
  if (size < 0) {
    throw InvalidData("invalid size");
  }
  data->seekg(static_cast<size_t>(offset) + size);
  memcpy(index + sizeof(int32_t), &size, sizeof(int32_t));
  index += sizeof(int32_t) * 2;

  return index;
}

char* type_index_bits(TypeId typeId, uint8_t offset, uint8_t size, char * index, std::shared_ptr<IOWrapper> & data, const DynObject * obj, const std::string & debug) {
  uint32_t mask = ((1 << size) - 1) << offset;
  auto *ptr = reinterpret_cast<uint32_t*>(index);
  *ptr = mask;
  ptr += 1;

  data->read(index + 4, 4);
  int byteOffset = (offset + size) / 8;
  data->seekg(data->tellg() - (4 - byteOffset));
  *ptr &= mask;

  return index + 8;
}


char *type_index(TypeId type, const SizeFunc &size, char *index, std::shared_ptr<IOWrapper> &data, const DynObject *obj, const std::string &debug) {
  switch (type) {
    case TypeId::int8: return type_index_num<int8_t>(index, data, debug);
    case TypeId::int16: return type_index_num<int16_t>(index, data, debug);
    case TypeId::int32: return type_index_num<int32_t>(index, data, debug);
    case TypeId::int64: return type_index_num<int64_t>(index, data, debug);
    case TypeId::uint8: return type_index_num<uint8_t>(index, data, debug);
    case TypeId::uint16: return type_index_num<uint16_t>(index, data, debug);
    case TypeId::uint32: return type_index_num<uint32_t>(index, data, debug);
    case TypeId::uint64: return type_index_num<uint64_t>(index, data, debug);
    case TypeId::bits: throw InvalidData("indexing bitmask not implemented");
    case TypeId::float32_iee754: return type_index_num<float>(index, data, debug);
    case TypeId::stringz: return type_index_impl<std::string>(index, data, size, obj, false, debug);
    case TypeId::string: return type_index_impl<std::string>(index, data, size, obj, true, debug);
    case TypeId::bytes: return type_index_impl<std::vector<uint8_t>>(index, data, size, obj, true, debug);
    case TypeId::custom: return type_index_obj(index, data, data->tellg(), size(*obj), obj);
    default: throw InvalidData("invalid type");
  }
}

std::any type_read_any(TypeId type, char *index, std::shared_ptr<IOWrapper> &data, std::shared_ptr<IOWrapper> &write, char **indexAfter) {
  switch (type) {
    case TypeId::int8: return type_read<int8_t>(type, index, data, write, indexAfter);
    case TypeId::int16: return type_read<int16_t>(type, index, data, write, indexAfter);
    case TypeId::int32: return type_read<int32_t>(type, index, data, write, indexAfter);
    case TypeId::int64: return type_read<int64_t>(type, index, data, write, indexAfter);
    case TypeId::uint8: return type_read<uint8_t>(type, index, data, write, indexAfter);
    case TypeId::uint16: return type_read<uint16_t>(type, index, data, write, indexAfter);
    case TypeId::uint32: return type_read<uint32_t>(type, index, data, write, indexAfter);
    case TypeId::uint64: return type_read<uint64_t>(type, index, data, write, indexAfter);
    case TypeId::bits: throw InvalidData("writing bitmask unsupported");
    case TypeId::float32_iee754: return type_read<float>(type, index, data, write, indexAfter);
    case TypeId::stringz: return type_read<std::string>(type, index, data, write, indexAfter);
    case TypeId::string: return type_read<std::string>(type, index, data, write, indexAfter);
    case TypeId::bytes: return type_read<std::vector<uint8_t>>(type, index, data, write, indexAfter);
    case TypeId::custom: throw InvalidData("not implemented");
    default: return std::any();
  }
}

char *type_write_any(TypeId type, char *index, std::shared_ptr<IOWrapper> &write, const std::any &value) {
  switch (type) {
    case TypeId::int8: return type_write<int8_t>(type, index, write, flexi_cast<int8_t>(value));
    case TypeId::int16: return type_write<int16_t>(type, index, write, flexi_cast<int16_t>(value));
    case TypeId::int32: return type_write<int32_t>(type, index, write, flexi_cast<int32_t>(value));
    case TypeId::int64: return type_write<int64_t>(type, index, write, flexi_cast<int64_t>(value));
    case TypeId::uint8: return type_write<uint8_t>(type, index, write, flexi_cast<uint8_t>(value));
    case TypeId::uint16: return type_write<uint16_t>(type, index, write, flexi_cast<uint16_t>(value));
    case TypeId::uint32: return type_write<uint32_t>(type, index, write, flexi_cast<uint32_t>(value));
    case TypeId::uint64: return type_write<uint64_t>(type, index, write, flexi_cast<uint64_t>(value));
    case TypeId::bits: throw InvalidData("writing bitmask unsupported");
    case TypeId::float32_iee754: return type_write<float>(type, index, write, flexi_cast<float>(value));
    case TypeId::stringz: return type_write<std::string>(type, index, write, flexi_cast<std::string>(value));
    case TypeId::string: return type_write<std::string>(type, index, write, flexi_cast<std::string>(value));
    default: throw InvalidData("not implemented");
  }
}

template <typename T>
void stream_write(const std::shared_ptr<IOWrapper> &output, const T &value);

void stream_write_strz(const std::shared_ptr<IOWrapper> &output, const std::string &value) {
  output->write(value.c_str(), value.length() + 1);
}

void stream_write_str(const std::shared_ptr<IOWrapper> &output, const std::string &value) {
  output->write(value.c_str(), value.length());
}

void stream_write_bytes(const std::shared_ptr<IOWrapper> &output, const std::vector<uint8_t> &value) {
  output->write(reinterpret_cast<const char*>(value.data()), value.size());
}

template <typename T>
void stream_write(const std::shared_ptr<IOWrapper> &output, const T &value) {
  output->write(reinterpret_cast<const char*>(&value), sizeof(T));
}

void type_copy_bytes(const std::shared_ptr<IOWrapper> &output, char *index,
                     const std::shared_ptr<IOWrapper> &data,
                     const std::shared_ptr<IOWrapper> &write,
                     char **indexAfter) {
  int32_t offset;
  memcpy(reinterpret_cast<char *>(&offset), index, sizeof(int32_t));
  index += sizeof(int32_t);

  const auto &stream = (offset < 0) ? write : data;

  offset = std::abs(offset);

  std::streampos curPos = stream->tellg();

  int32_t seekOffset = static_cast<int32_t>(curPos) - offset;
  if (seekOffset != 0) {
    stream->seekg(offset);
  }

  int32_t size;
  memcpy(reinterpret_cast<char *>(&size), index, sizeof(int32_t));

  static char buffer[4096];

  index += sizeof(int32_t);
  int32_t left = size;
  while (left > 0) {
    int32_t chunk = std::min<int32_t>(left, 4096);
    stream->read(buffer, chunk);
    output->write(buffer, chunk);
    left -= chunk;
  }

  if ((size * -1) != seekOffset) {
    stream->seekg(curPos);
  }

  if (indexAfter != nullptr) {
    *indexAfter = index;
  }
}

void type_copy_any(TypeId type, char *index, std::shared_ptr<IOWrapper> &output, std::shared_ptr<IOWrapper> &data, std::shared_ptr<IOWrapper> &write, char **indexAfter) {
  switch (type) {
  case TypeId::int8: stream_write(output, type_read<int8_t>(type, index, data, write, indexAfter)); break;
  case TypeId::int16: stream_write(output, type_read<int16_t >(type, index, data, write, indexAfter)); break;
  case TypeId::int32: stream_write(output, type_read<int32_t >(type, index, data, write, indexAfter)); break;
  case TypeId::int64: stream_write(output, type_read<int64_t >(type, index, data, write, indexAfter)); break;
  case TypeId::uint8: stream_write(output, type_read<uint8_t >(type, index, data, write, indexAfter)); break;
  case TypeId::uint16: stream_write(output, type_read<uint16_t>(type, index, data, write, indexAfter)); break;
  case TypeId::uint32: stream_write(output, type_read<uint32_t>(type, index, data, write, indexAfter)); break;
  case TypeId::uint64: stream_write(output, type_read<uint64_t>(type, index, data, write, indexAfter)); break;
  case TypeId::bits: throw InvalidData("copy type not implemented");
  case TypeId::float32_iee754: stream_write(output, type_read<float>(type, index, data, write, indexAfter)); break;
  case TypeId::stringz: stream_write_strz(output, type_read<std::string>(type, index, data, write, indexAfter)); break;
  case TypeId::string: stream_write_str(output, type_read<std::string>(type, index, data, write, indexAfter)); break;
  case TypeId::bytes: type_copy_bytes(output, index, data, write, indexAfter); break;
  case TypeId::custom: throw InvalidData("not implemented");
  default:
    // nop
  break;
  }
}

} // namespace pagan
