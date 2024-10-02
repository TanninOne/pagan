#include "StreamRegistry.h"

#include <string>

namespace pagan {

StreamRegistry::StreamRegistry() {
  m_Write.reset(IOWrapper::memoryBuffer());
  // offsets into the write stream are marked by being negative values
  // but 0 * -1 is still 0 so we can't actually reference the first byte
  // in the write stream.
  char dummy = static_cast<char>(0x00);
  m_Write->write(&dummy, 1);
}

int StreamRegistry::add(std::shared_ptr<IOWrapper> stream) {
  size_t pos = m_Streams.size();
  m_Streams.push_back(stream);
  return static_cast<int>(pos);
}

std::shared_ptr<IOWrapper> StreamRegistry::get(DataStreamId id) const {
  if (m_Streams.size() < id) {
    throw std::runtime_error("invalid stream id " + std::to_string(id));
  }
  return m_Streams[id];
}

std::shared_ptr<IOWrapper> StreamRegistry::get(DataStreamId id,
                                               DataOffset offset) const {
  m_Streams[id]->seekg(offset);
  return m_Streams[id];
}

} // namespace pagan
