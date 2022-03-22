#include "util.h"
#include "iowrap.h"
#include <algorithm>
#include <iomanip>

thread_local int LogBracket::s_Indent = 0;
thread_local RingLog LogBracket::s_RingLog;

static const int MAX_DEPTH = 15;

void debugStream(const std::shared_ptr<IOWrapper>& str) {
  std::streamoff posG = str->tellg();
  std::streamoff pos = str->size();
  str->seekg(0);
  std::cout << "Debugging stream up to: " << pos << std::endl << std::hex;
  for (std::streamoff i = 0; i < pos; ++i) {
    char buf;
    str->read(&buf, 1);
    std::cout << std::setw(2) << std::right << std::setfill('0') << (int)(unsigned char)buf
      << ((i % 16 == 15) ? "\n" : " ");
  }
  std::cout << std::dec << std::endl;
  str->seekg(posG);
}

LogBracket::LogBracket(const std::string & message)
  : m_Message(message) {
  if (s_Indent <= MAX_DEPTH) {
    s_RingLog.log(indent() + "+++" + message);
    // std::cout << indent() << "+++" << message << std::endl;
  }
  ++s_Indent;
}

LogBracket::~LogBracket() {
  --s_Indent;
  if (s_Indent <= MAX_DEPTH) {
    s_RingLog.log(indent() + "---" + m_Message);
    // std::cout << indent() << "---" << m_Message << std::endl;
  }
}

LogBracket LogBracket::create(const std::string & message) {
  return LogBracket(message);
}

void LogBracket::log(const std::string & message) {
#ifdef RING_LOG
  s_RingLog.log(indent() + message);
#else
  if (s_Indent <= MAX_DEPTH) {
    std::cout << indent() << message << std::endl;
  }
#endif
}

void LogBracket::print() {
  for (const std::string& line : s_RingLog.lines()) {
    std::cout << line << "\n";
  }
  std::cout << std::endl;
}

std::string LogBracket::indent() {
  std::string xStr;
  std::string sStr;
  int xCount = s_Indent / 10;
  int sCount = s_Indent % 10;
  xStr.resize(static_cast<size_t>(xCount) * 2, 'X');
  sStr.resize(static_cast<size_t>(sCount) * 2, ' ');
  return xStr + sStr;
}
