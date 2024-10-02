#pragma once

#include "types.h"

#include <memory>
#include <map>

namespace pagan {

class TypeSpec;

struct TypeAttribute {
  const char *key;
  uint32_t type;
};

class TypeRegistry {
public:
  static std::shared_ptr<TypeRegistry> init() {
    return std::shared_ptr<TypeRegistry>(new TypeRegistry());
  }

  std::shared_ptr<TypeSpec> create(const char *name);

  std::shared_ptr<TypeSpec>
  create(const char *name,
         const std::initializer_list<TypeAttribute> &attributes);

  std::shared_ptr<TypeSpec> getById(uint32_t id) { return m_Types[id]; }

  std::shared_ptr<TypeSpec> getByName(const char *name) {
    return m_Types[m_TypeIds[name]];
  }

  uint32_t numTypes() const { return m_NextId; }

  uint32_t nextId() { return m_NextId++; }

  static std::tuple<std::string, std::vector<std::string>>
  splitTypeName(const char *name);

  ~TypeRegistry();

private:
  TypeRegistry();

private:
  uint32_t m_NextId{TypeId::custom};
  std::map<std::string, uint32_t> m_TypeIds;
  std::vector<std::shared_ptr<TypeSpec>> m_Types;
  // std::map<uint32_t, std::shared_ptr<TypeSpec>> m_Types;
};

} // namespace pagan
