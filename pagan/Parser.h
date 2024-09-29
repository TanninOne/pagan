#pragma once

#include <memory>

#include "ObjectIndexTable.h"
#include "DynObject.h"
#include "TypeRegistry.h"

class Parser
{
public:
  Parser();
  ~Parser();

  void addFileStream(const char* filePath);

  bool hasInputData() const;

  void write(const char* filePath, DynObject& obj) const;

  std::shared_ptr<TypeSpec> getType(const char* name) const;

  DynObject getObject(const std::shared_ptr<TypeSpec>& spec, size_t offset, DataStreamId dataStream = 0);
  std::vector<DynObject> getList(const std::shared_ptr<TypeSpec>& spec, size_t offset, DataStreamId dataStream = 0);

  std::vector<uint8_t> objectIndex() const;
  std::vector<uint8_t> arrayIndex() const;

  std::string getTypeById(uint32_t typeId) const;

  uint32_t numObjects() const { return m_IndexTable.numObjectIndices(); }
  uint32_t numArrays() const { return m_IndexTable.numArrayIndices(); }
  uint32_t numTypes() const { return m_TypeRegistry->numTypes(); }

  template <typename T>
  DynObject createObject(const std::weak_ptr<TypeSpec> &spec, std::initializer_list<T> data);

  std::shared_ptr<TypeSpec> createType(const char *name);
  std::shared_ptr<TypeSpec> createType(const char *name, const std::initializer_list<TypeAttribute> &attributes);

private:

  bool m_HasInputData{ false };

  ObjectIndexTable m_IndexTable;
  StreamRegistry m_StreamRegistry;
  std::shared_ptr<TypeRegistry> m_TypeRegistry;

};

template<typename T>
inline DynObject Parser::createObject(const std::weak_ptr<TypeSpec>& spec, std::initializer_list<T> data) {
  return DynObject(spec, m_StreamRegistry, &m_IndexTable, data);
}
