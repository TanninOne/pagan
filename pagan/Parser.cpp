#include "Parser.h"
#include "TypeSpec.h"

namespace pagan {

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
  m_HasInputData = true;
}

bool Parser::hasInputData() const {
  return m_HasInputData;
}

void Parser::write(const char *filePath, DynObject &obj) const {
  std::shared_ptr<IOWrapper> ptr(IOWrapper::fromFile(filePath, true));
  obj.saveTo(ptr);
}

std::vector<uint8_t> Parser::objectIndex() const {
  return m_IndexTable.getObjectIndex();
}

std::vector<uint8_t> Parser::arrayIndex() const {
  return m_IndexTable.getArrayIndex();
}

std::string Parser::getTypeById(uint32_t typeId) const {
  return m_TypeRegistry->getById(typeId)->getName();
}

DynObject Parser::getObject(const std::shared_ptr<TypeSpec> &spec, size_t offset, DataStreamId dataStream) {
  std::shared_ptr<IOWrapper> stream = m_StreamRegistry.get(dataStream);

  ObjectIndex *rootIndex = m_IndexTable.allocateObject(spec, dataStream, offset);
  DynObject res(spec, m_StreamRegistry, &m_IndexTable, rootIndex, nullptr);
  res.writeIndex(offset, stream->size(), true);

  return res;
}

std::vector<DynObject> Parser::getList(const std::shared_ptr<TypeSpec>& spec, size_t offset, DataStreamId dataStream) {
  std::shared_ptr<IOWrapper> stream = m_StreamRegistry.get(dataStream);

  // DynObject(const std::shared_ptr<TypeSpec> &spec, const StreamRegistry &streams, ObjectIndexTable *indexTable, ObjectIndex *index, const DynObject *parent)
  std::shared_ptr<TypeSpec> dummySpec(new TypeSpec("dummy", m_TypeRegistry->nextId(), m_TypeRegistry.get()));
  dummySpec->appendProperty("root", spec->getId())
    .withRepeatToEOS();
  ObjectIndex *rootIndex = m_IndexTable.allocateObject(dummySpec, dataStream, offset);
  DynObject dummy(dummySpec, m_StreamRegistry, &m_IndexTable, rootIndex, nullptr);
  dummy.writeIndex(offset, stream->size(), true);

  return dummy.getList<DynObject>("root");
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

} // namespace pagan
