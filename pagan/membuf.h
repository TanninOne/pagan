# pragma once

#include <cstring>
#include <vector>
#include <iostream>
#include <streambuf>
#include <algorithm>

class MemoryBuf : public std::streambuf
{
  std::vector<uint8_t> m_Buffer;

public:

  MemoryBuf(const char *input, size_t size) {
    m_Buffer.resize(size);
    char *buf = reinterpret_cast<char*>(&m_Buffer[0]);
    memcpy(buf, input, size);
    setg(buf, buf, buf + size);
    setp(buf, buf + size);
  }

  MemoryBuf(std::istream &input, size_t size) {
    m_Buffer.resize(size);
    char *buf = reinterpret_cast<char*>(&m_Buffer[0]);
    input.read(buf, size);
    setg(buf, buf, buf + size);
    setp(buf, buf + size);
  }

  const std::vector<uint8_t> &getBuffer() const {
    return m_Buffer;
  }

  virtual pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in) override
  {
    char *begin = reinterpret_cast<char*>(&m_Buffer[0]);
    char *end = begin + m_Buffer.size();

    if (dir == std::ios_base::cur) {
      gbump(static_cast<int>(off));
    }
    else if (dir == std::ios_base::end) {
      setg(begin, end + off, end);
    }
    else if (dir == std::ios_base::beg) {
      setg(begin, begin + off, end);
    }

    return gptr() - eback();
  }

  virtual pos_type seekpos(pos_type pos, std::ios_base::openmode mode) override
  {
    return seekoff(pos - pos_type(off_type(0)), std::ios_base::beg, mode);
  }
};
