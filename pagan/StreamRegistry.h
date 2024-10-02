#pragma once

#include "iowrap.h"

#include <vector>
#include <memory>

namespace pagan {

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

  std::shared_ptr<IOWrapper> get(DataStreamId id) const;
  std::shared_ptr<IOWrapper> get(DataStreamId id, DataOffset offset) const;

private:

  std::shared_ptr<IOWrapper> m_Write;

  std::vector<std::shared_ptr<IOWrapper>> m_Streams;

};

} // namespace pagan
