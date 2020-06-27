#include "util.h"
#include "iowrap.h"
#include <algorithm>
#include <iomanip>

thread_local int LogBracket::s_Indent = 0;

static const int MAX_DEPTH = 2;

void debugStream(const std::shared_ptr<IOWrapper>& str) {
  std::streamoff posG = str->tellg();
  /*
  std::streamoff posP = str->tellp();
  std::streamoff pos = std::max(posG, posP);
  */
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
    std::cout << indent() << "+++" << message << std::endl;
  }
  else {
    // std::cout << "+++ " << s_Indent << " - " << message << std::endl;
  }
  ++s_Indent;
}

LogBracket::~LogBracket() {
  --s_Indent;
  if (s_Indent <= MAX_DEPTH) {
    std::cout << indent() << "---" << m_Message << std::endl;
  }
  else {
    // std::cout << "--- " << s_Indent << " - " << m_Message << std::endl;
  }
}

LogBracket LogBracket::create(const std::string & message) {
  return LogBracket(message);
}

void LogBracket::log(const std::string & message) {
  if (s_Indent <= MAX_DEPTH) {
    std::cout << indent() << message << std::endl;
  }
}

std::string LogBracket::indent() {
  std::string xStr;
  std::string sStr;
  int xCount = s_Indent / 10;
  int sCount = s_Indent % 10;
  xStr.resize(xCount * 2, 'X');
  sStr.resize(sCount * 2, ' ');
  return xStr + sStr;
}