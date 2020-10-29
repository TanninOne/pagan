#include "Parser.h"


Parser::Parser()
  : m_TypeRegistry(TypeRegistry::init())
{
}

Parser::~Parser()
{
}

void Parser::addFileStream(const char * filePath) {
  std::shared_ptr<IOWrapper> ptr(IOWrapper::fromFile(filePath));
  m_StreamRegistry.add(ptr);
}

void Parser::write(const char *filePath, DynObject &obj) const {
  std::shared_ptr<IOWrapper> ptr(IOWrapper::fromFile(filePath, true));
  obj.saveTo(ptr);
}

DynObject Parser::getObject(const std::shared_ptr<TypeSpec> &spec, size_t offset, DataStreamId dataStream) {
  std::shared_ptr<IOWrapper> stream = m_StreamRegistry.get(dataStream);

  ObjectIndex *rootIndex = m_IndexTable.allocateObject(spec, dataStream, offset);
  DynObject res(spec, m_StreamRegistry, &m_IndexTable, rootIndex, nullptr);
  res.writeIndex(offset, stream->size(), true);

  return res;
}

std::shared_ptr<TypeSpec> Parser::getType(const char *name) const {
  return m_TypeRegistry->getByName(name);
}

std::shared_ptr<TypeSpec> Parser::createType(const char * name) {
  return m_TypeRegistry->create(name);
}

std::shared_ptr<TypeSpec> Parser::createType(const char * name, const std::initializer_list<TypeAttribute>& attributes) {
  return m_TypeRegistry->create(name, attributes);
}
