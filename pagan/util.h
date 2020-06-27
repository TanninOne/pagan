#pragma once

#include "format.h"
#include "iowrap.h"
#include <iostream>
#include <memory>
#include <thread>

#ifdef NOLOG
#define LOG(...)
#define LOG_F(...)
#define LOG_BRACKET(...)
#define LOG_BRACKET_F(...)
#else
#define LOG(...) LogBracket::log(__VA_ARGS__)
#define LOG_F(pattern, ...) LogBracket::log(fmt::format(pattern, __VA_ARGS__))
#define LOG_BRACKET(...) LogBracket brack = LogBracket::create(__VA_ARGS__)
#define LOG_BRACKET_F(pattern, ...) LogBracket brack = LogBracket::create(fmt::format(pattern, __VA_ARGS__))
#endif

void debugStream(const std::shared_ptr<IOWrapper> &str);

template <typename T> T read(std::istream &stream) {
  T res;
  stream.read(reinterpret_cast<char *>(&res), sizeof(T));
  return res;
}

template <typename T> void write(std::ostream &stream, const T &val) {
  LOG_F("write at {0}", stream.tellp());
  stream.write(reinterpret_cast<const char*>(&val), sizeof(T));
}

class LogBracket {
public:
  ~LogBracket();

  static LogBracket create(const std::string &message);

  /*
  template <typename ...Args>
  static LogBracket create(const std::string &message, const Args &...args) {
    return LogBracket(fmt::format(message, ...args));
  }
  */

  static void log(const std::string &message);

  /*
  template <typename ...Args>
  static void log(const std::string &message, const Args &...args) {
    std::cout << indent() << fmt::format(message, ...args);
  }
  */

  static int getIndentDepth() {
    return s_Indent;
  }

protected:

  LogBracket(const std::string &message);

  static std::string indent();

private:
  static thread_local int s_Indent;
  std::string m_Message;
};

