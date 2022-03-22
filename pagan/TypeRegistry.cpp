#include "typeregistry.h"
#include "typespec.h"

static const int INIT_TYPES_LENGTH = 64;

static const char* BaseTypeNames[] = {
  "int8", "int16", "int32", "int64",
  "uint8", "uint16", "uint32", "uint64", "bits",
  "float", "string", "stringz", "bytes", "runtime"
};

TypeRegistry::TypeRegistry()
{
  m_Types.resize(INIT_TYPES_LENGTH);
  for (int i = 0; i < custom; ++i) {
    m_Types[i] = std::shared_ptr<TypeSpec>(new TypeSpec(BaseTypeNames[i], i, this));
  }
}

std::shared_ptr<TypeSpec> TypeRegistry::create(const char *name) {
  auto existing = m_TypeIds.find(name);
  if (existing != m_TypeIds.end()) {
    return m_Types[existing->second];
  }
  uint32_t typeId = nextId();
  std::shared_ptr<TypeSpec> res(new TypeSpec(name, typeId, this));
  if (m_Types.size() <= typeId) {
    m_Types.resize(m_Types.size() * 2);
  }
  m_Types[typeId] = res;
  m_TypeIds[name] = typeId;
  return res;
}

std::shared_ptr<TypeSpec> TypeRegistry::create(const char *name, const std::initializer_list<TypeAttribute>& attributes) {
  std::shared_ptr<TypeSpec> res = create(name);
  LOG_BRACKET_F("create type {0} - {1}", name, res->getId());
  for (auto attr : attributes) {
    res->appendProperty(attr.key, attr.type);
  }
  return res;
}

TypeRegistry::~TypeRegistry()
{
}
