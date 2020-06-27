#include "iowrap.h"

static const int FILE_BUFFER_SIZE = 128 * 1024;

IOWrapper *IOWrapper::memoryBuffer() {
  return new IOWrapper(new std::stringstream(), -1);
}

IOWrapper *IOWrapper::fromFile(const char *filePath) {
  std::fstream *str = new std::fstream(filePath, std::ios::in | std::ios::binary);
  str->seekg(0, std::ios::end);
  std::streampos fileSize = str->tellg();
  str->seekg(0);
  str->exceptions(std::ios::failbit | std::ios::badbit);
  if (!str->is_open()) {
    throw std::runtime_error(fmt::format("failed to open \"{}\"", filePath));
  }

  return new IOWrapper(str, static_cast<int64_t>(fileSize), FILE_BUFFER_SIZE);
}

IOWrapper::IOWrapper(const IOWrapper &reference)
  : m_Stream(reference.m_Stream)
  , m_Size(reference.m_Size)
  , m_PosG(reference.m_PosG)
  , m_PosP(reference.m_PosP)
  , m_Buffer(reference.m_Buffer)
{
  const_cast<IOWrapper&>(reference).m_Stream = nullptr;
  const_cast<IOWrapper&>(reference).m_Buffer = nullptr;
}

IOWrapper::IOWrapper(std::iostream *stream, int64_t size, int32_t bufferSize)
  : m_Stream(stream)
  , m_Size(size)
  , m_BufferSize(bufferSize)
{
  if (bufferSize != -1) {
    m_Buffer = new char[bufferSize];
    m_BufferPos = -1;
  }
}

IOWrapper::~IOWrapper() {
  if (m_Stream != nullptr) {
    delete m_Stream;
  }
  if (m_Buffer != nullptr) {
    delete m_Buffer;
  }

  std::cout << "seek count " << m_SeekCount << std::endl;
}