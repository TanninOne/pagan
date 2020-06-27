#include "DynObject.h"

std::any DynObject::getAny(char *key) const {
  size_t dotOffset = strcspn(key, ".");
  if (key[dotOffset] != '\0') {
    // std::string objKey(key, key + dotOffset);
    key[dotOffset] = '\0';
    DynObject obj = get<DynObject>(key);
    return obj.getAny(&key[dotOffset + 1]);
  }
  // else: this is the "final" or "leaf" key
  size_t offset;
  uint32_t typeId;

  std::tie(typeId, offset) = m_Spec->get(m_ObjectIndex, key);

  if (typeId >= TypeId::custom) {
    throw IncompatibleType("expected POD");
  }

  char *index = reinterpret_cast<char*>(m_ObjectIndex->properties + offset);

  std::shared_ptr<IOWrapper> dataStream = m_Streams.get(m_ObjectIndex->dataStream);
  std::shared_ptr<IOWrapper> writeStream = m_Streams.getWrite();

  return type_read_any(static_cast<TypeId>(typeId), index, dataStream, writeStream);
}

std::any DynObject::getAny(const std::vector<std::string>::const_iterator &cur, const std::vector<std::string>::const_iterator &end) const {
  if (cur + 1 != end) {
    DynObject obj = get<DynObject>(cur->c_str());
    return obj.getAny(cur + 1, end);
  }

  // else: this is the "final" or "leaf" key
  size_t offset;
  uint32_t typeId;

  std::tie(typeId, offset) = m_Spec->get(m_ObjectIndex, cur->c_str());

  if (typeId >= TypeId::custom) {
    throw IncompatibleType("expected POD");
  }

  char *index = reinterpret_cast<char*>(m_ObjectIndex->properties + offset);

  std::shared_ptr<IOWrapper> dataStream = m_Streams.get(m_ObjectIndex->dataStream);
  std::shared_ptr<IOWrapper> writeStream = m_Streams.getWrite();

  return type_read_any(static_cast<TypeId>(typeId), index, dataStream, writeStream);
}

