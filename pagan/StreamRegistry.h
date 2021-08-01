#pragma once

#include "iowrap.h"

#include <vector>
#include <iostream>
#include <memory>
#include <sstream>
#include <memory>

typedef uint16_t DataStreamId;
typedef uint64_t DataOffset;

class StreamRegistry
{
public:
  StreamRegistry();

  ~StreamRegistry() {}

  int add(std::shared_ptr<IOWrapper> stream);

  std::shared_ptr<IOWrapper> getWrite() const {
    return m_Write;
  }

  std::shared_ptr<IOWrapper> get(DataStreamId id) const {
    if (m_Streams.size() < id) {
      throw std::runtime_error("invalid stream id " + std::to_string(id));
    }
    return m_Streams[id];
  }

  std::shared_ptr<IOWrapper> get(DataStreamId id, DataOffset offset) const;

private:

  std::shared_ptr<IOWrapper> m_Write;

  std::vector<std::shared_ptr<IOWrapper>> m_Streams;

};
