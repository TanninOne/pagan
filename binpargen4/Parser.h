#pragma once

#include "ObjectIndexTable.h"
#include "DynObject.h"
#include "iowrap.h"
#include <memory>

class Parser
{
public:
  Parser();
  ~Parser();

  void addFileStream(const char *filePath);

  std::shared_ptr<TypeSpec> getType(const char *name) const;

  DynObject getObject(const std::shared_ptr<TypeSpec> &spec, size_t offset, DataStreamId dataStream = 0);

  template <typename T>
  DynObject createObject(const std::weak_ptr<TypeSpec> &spec, std::initializer_list<T> data);

  std::shared_ptr<TypeSpec> createType(const char *name);
  std::shared_ptr<TypeSpec> createType(const char *name, const std::initializer_list<TypeAttribute> &attributes);

private:

  ObjectIndexTable m_IndexTable;
  StreamRegistry m_StreamRegistry;
  std::shared_ptr<TypeRegistry> m_TypeRegistry;

};

template<typename T>
inline DynObject Parser::createObject(const std::weak_ptr<TypeSpec>& spec, std::initializer_list<T> data) {
  return DynObject(spec, m_StreamRegistry, &m_IndexTable, data);
}
