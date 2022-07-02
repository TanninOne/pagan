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
  std::string funcName;
  std::vector<std::string> args;
  std::tie(funcName, args) = splitTypeName(name);

  auto existing = m_TypeIds.find(funcName);
  if (existing != m_TypeIds.end()) {
    return m_Types[existing->second];
  }
  uint32_t typeId = nextId();

  std::shared_ptr<TypeSpec> res(new TypeSpec(funcName.c_str(), typeId, this));
  if (m_Types.size() <= typeId) {
    m_Types.resize(m_Types.size() * 2);
  }
  m_Types[typeId] = res;
  m_TypeIds[funcName] = typeId;
  return res;
}

std::shared_ptr<TypeSpec> TypeRegistry::create(const char *name, const std::initializer_list<TypeAttribute>& attributes) {
  std::shared_ptr<TypeSpec> res = create(name);
  for (auto attr : attributes) {
    res->appendProperty(attr.key, attr.type);
  }
  return res;
}

std::tuple<std::string, std::vector<std::string>> TypeRegistry::splitTypeName(const char* name) {
  std::string funcName;
  std::vector<std::string> args;

  size_t len = strlen(name);
  const char *bracketPos = strchr(name, '(');
  if (bracketPos != nullptr) {
    size_t namePartLen = bracketPos - name;
    std::string argListString(bracketPos + 1, len - namePartLen - 2);
    funcName = std::string(name, namePartLen);
    std::istringstream sstream(argListString);
    std::string tok;
    while (std::getline(sstream, tok, ',')) {
      args.push_back(tok);
    }
  }
  else {
    funcName = name;
  }
  return std::make_tuple(funcName, args);
}

TypeRegistry::~TypeRegistry()
{
}
