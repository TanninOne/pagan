#pragma once

#include "format.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

/**
 * stream-like wrapper around memory sections or files.
 * This reads entire blocks from the underlying storage and then serves reads from that cache
 * when possible.
 * It also delays seeks and then only actually does them on the file object when that is necessary
 * to fill the cache.
 * This frees the caller from doing those optimizations
 */
class IOWrapper {
public:

  static IOWrapper *memoryBuffer();

  static IOWrapper *fromFile(const char *filePath, bool out = false);

  IOWrapper() = delete;
  IOWrapper(const IOWrapper &reference);

  ~IOWrapper();

  IOWrapper &operator=(const IOWrapper &reference) = delete;

  void seekg(std::streamoff pos) {
    if (pos != m_PosG) {
      if ((m_Size != -1) && (pos > m_Size)) {
        throw std::ios::failure(fmt::format("seek beyond stream end: {}/{}", pos, m_Size));
      }
      m_PosG = pos;
      m_SeekGPending = true;
    }
  }

  void seekp(std::streamoff pos) {
    if (pos != m_PosP) {
      m_PosP = pos;
      m_SeekPPending = true;
    }
  }

  void seekendP() {
    m_Stream->seekp(0, std::ios::end);
    m_PosP = size();
  }

  std::streamsize size() {
    if (m_Size != -1) {
      return m_Size;
    }

    m_Stream->seekg(0, std::ios::end);
    std::streampos end = m_Stream->tellg();
    m_Stream->seekg(m_PosG);

    return end - std::streamoff(0);
  }

  std::streamoff tellg() {
    return m_PosG;
  }

  std::streamoff tellp() {
    return m_PosP;
  }

  void write(const char *data, std::streamsize count) {
    commitSeekP();
    m_Stream->write(data, count);
    m_PosP += count;
  }

  void read(char *target, std::streamsize count) {
    static int numReads = 0;
    static int numCalls = 0;

    ++numCalls;
    if (m_BufferSize == -1) {
      // unbuffered reading
      commitSeekG();
      m_Stream->read(target, count);
      m_PosG += count;

      return;
    }

    // buffered reading
    if ((m_BufferPos == -1)
        || (m_PosG < m_BufferPos)
        || ((m_PosG + count) > (m_BufferPos + m_BufferSize))) {
      int64_t oldPos = m_BufferPos;
      if (m_BufferSize < count) {
        m_BufferSize = count * 2;
        delete [] m_Buffer;
        m_Buffer = new char[m_BufferSize];
      }
      // we fill the entire buffer, the range actually requested will be in the middle of the buffer so that we
      // can also fulfill future requests before and after the requested range
      int64_t padding = m_BufferSize - count;
      m_BufferPos = std::max(0LL, m_PosG - (padding / 2));
      if ((m_Size != -1) && (m_BufferPos + m_BufferSize > m_Size)) {
        // don't buffer beyond the file
        m_BufferSize = static_cast<int32_t>(m_Size - m_BufferPos);
      }
      m_Stream->seekg(m_BufferPos);
      try {
        ++numReads;
        m_Stream->read(m_Buffer, m_BufferSize);
      }
      catch (const std::ios::failure&) {
        m_Stream->clear();
      }
    }
    if ((m_Size != -1) && (m_PosG + count > m_Size)) {
      // if the requested read went beyond the file, we should still generate
      // an eof exception
      throw std::ios::failure("end of stream");
    }

    int64_t buffOffset = m_PosG - m_BufferPos;
    memcpy(target, m_Buffer + buffOffset, count);
    m_PosG += count;
  }

  int get() {
    commitSeekG();
    m_PosG += 1;
    return m_Stream->get();
  }

private:

  IOWrapper(std::iostream *stream, int64_t size, int32_t bufferSize = -1);

  void commitSeekG() {
    if (m_SeekGPending) {
      ++m_SeekCount;
      m_Stream->seekg(m_PosG, std::ios::beg);
      m_SeekGPending = false;
    }
  }

  void commitSeekP() {
    if (m_SeekPPending) {
      m_Stream->seekp(m_PosP, std::ios::beg);
      m_SeekPPending = false;
    }
  }

private:

  std::iostream *m_Stream;
  int32_t m_SeekCount{ 0 };
  int64_t m_Size;
  std::streamoff m_PosG { 0 };
  std::streamoff m_PosP { 0 };
  bool m_SeekGPending{ false };
  bool m_SeekPPending{ false };

  std::streamsize m_BufferSize{ -1 };
  std::streamoff m_BufferPos;
  char *m_Buffer{ nullptr };

};

