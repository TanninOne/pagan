#pragma once

#include <unordered_set>
#include <vector>

#include "shunting_yard.h"
#include "evaluate.h"
#include "token.h"

#include "IScriptQuery.h"
#include "flexi_cast.h"

#include <any>
#include <functional>
#include <map>
#include <typeindex>

namespace pagan {

typedef std::function<std::any(const std::any& args)> AnyFunc;

template <typename T>
bool equal(const std::any &lhs, const std::any &rhs) {
  return flexi_cast<T>(lhs) == flexi_cast<T>(rhs);
}

static bool strEqual(const std::any &lhs, const std::any &rhs) {
  return strcmp(flexi_cast<std::string>(lhs).c_str(), flexi_cast<std::string>(rhs).c_str()) == 0;
}

static const inline std::map<std::type_index, std::function<bool(const std::any &lhs, const std::any &rhs)>> comparators{
  { typeid(char), equal<char> },
  { typeid(short), equal<short> },
  { typeid(int), equal<int> },
  { typeid(long), equal<long> },
  { typeid(int8_t), equal<int8_t> },
  { typeid(int16_t), equal<int16_t> },
  { typeid(int32_t), equal<int32_t> },
  { typeid(int64_t), equal<int64_t> },
  { typeid(uint8_t), equal<uint8_t> },
  { typeid(uint16_t), equal<uint16_t> },
  { typeid(uint32_t), equal<uint32_t> },
  { typeid(uint64_t), equal<uint64_t> },
  { typeid(std::string), strEqual },
};


static inline bool any_equal(const std::any &lhs, const std::any &rhs) {
  try {
    return comparators.at(lhs.type())(lhs, rhs);
  }
  catch (const std::out_of_range &) {
    LOG_F("trying to compare {}", lhs.type().name());
    return false;
  }
  catch (const std::bad_any_cast &) {
    return false;
  }
}

std::vector<std::string_view> splitVariable(const std::string_view &input);
void splitVariable(const std::string_view &input, std::vector<std::string_view> &result);

inline SYP::Token tokenFromAny(const std::any& input)
{
  static std::unordered_set<std::type_index> unsigned_types { typeid(uint64_t), typeid(uint32_t), typeid(uint16_t), typeid(uint8_t) };
  static std::unordered_set<std::type_index> signed_types { typeid(int64_t), typeid(int32_t), typeid(int16_t), typeid(int8_t) };

  auto& inputType { input.type() };
  if (unsigned_types.contains(inputType)) {
    return SYP::Token{ flexi_cast<uint64_t>(input) };
  } else if (signed_types.contains(inputType)) {
    return SYP::Token{ flexi_cast<int64_t>(input) };
  } else {
    auto str = std::any_cast<std::string>(input);
    // TODO: string? variable? function name?
    return SYP::Token{ str, SYP::TokenType::String };
  }
}

inline std::any tokenToAny(const SYP::Token& input)
{
  switch (input.type) {
  case SYP::TokenType::Unsigned: {
    return input.unsignedValue;
  }
  case SYP::TokenType::Signed: {
    return input.signedValue;
  }
  case SYP::TokenType::Float: {
    return input.floatValue;
  }
  case SYP::TokenType::Boolean: {
    return input.boolValue;
  }
  case SYP::TokenType::String: {
    return std::any(input.getVariableName());
  }
  default:
    throw std::runtime_error(
        std::format("can't convert type to any: {}", static_cast<int>(input.type)));
  }
}

inline std::function<SYP::Token(const std::string&)> makeGetAdapter(const IScriptQuery& query)
{
  static thread_local std::vector<std::string_view> keySegments;
  keySegments.reserve(4);
  return [&](const std::string &value) {
    splitVariable(value, keySegments);
    std::any resAny;
    if (*keySegments.rbegin() == "to_s")
    {
      resAny = query.getAny(keySegments.begin(), --keySegments.end());
      // TODO: nonsense, have to to_string based on actual type of the any value
      resAny = std::to_string(flexi_cast<int64_t>(resAny));
    }
    else if (*keySegments.rbegin() == "length")
    {
      return SYP::Token("length", [](const std::vector<SYP::Token>& arguments) -> SYP::Token {
        return arguments.at(0).getVariableName().length();
      });
    }
    else {
      resAny = query.getAny(keySegments.begin(), keySegments.end());
    }
    return tokenFromAny(resAny);
  };
}

inline std::function<void(const std::string&, const SYP::Token&)> makeSetAdapter(IScriptQuery& query)
{
  static thread_local std::vector<std::string_view> keySegments;
  keySegments.reserve(4);
  return [&](const std::string &value, const SYP::Token& token) -> void {
    splitVariable(value, keySegments);
    query.setAny(keySegments.begin(), keySegments.end(), tokenToAny(token));
  };
}

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

template <typename T> T flexi_get(const SYP::Result &input) {
  T res;
  std::visit(overloaded{[&res](int64_t val) { res = flexi_cast<T>(val); },
                        [&res](uint64_t val) { res = flexi_cast<T>(val); },
                        [&res](double val) { res = flexi_cast<T>(val); },
                        [&res](bool val) { res = flexi_cast<T>(val); },
                        [&res](std::string val) { res = flexi_cast<T>(val); }}, input);

  return res;
}

template <typename T>
std::function<T(const IScriptQuery &)> makeFuncImpl(const std::string &code) {
  auto tokenStack = SYP::tokenize(code);

  return [tokenStack, code](const IScriptQuery &obj) -> T {
    auto res = SYP::evaluate(tokenStack, makeGetAdapter(obj));
    return flexi_get<T>(res);
  };
}

template <typename T>
std::function<T(IScriptQuery &, const std::any&)> makeFuncMutableImpl(const std::string &code) {
  auto tokenStack = SYP::tokenize(code);

  return [tokenStack](IScriptQuery &obj, const std::any& value) mutable -> T {
    return flexi_get<T>(SYP::evaluate(tokenStack, makeGetAdapter(obj), makeSetAdapter(obj)));
  };
}

template <typename T>
inline std::function<T(const IScriptQuery &)> makeFunc(const std::string &code) {
  try {
    return makeFuncImpl<T>(code);
  }
  catch (const std::exception& e) {
    throw std::runtime_error(std::format("failed to compile function \"{}\": {}", code, e.what()).c_str());
  }
}

template <>
inline std::function<int32_t(const IScriptQuery &)> makeFunc(const std::string &code) {
  char *endPtr = nullptr;
  long num = std::strtol(code.c_str(), &endPtr, 10);
  if (*endPtr == '\0') {
    return [num](const IScriptQuery &) -> int32_t { return num; };
  }

  try {
    return makeFuncImpl<int32_t>(code);
  }
  catch (const std::exception& e) {
    throw std::runtime_error(std::format("failed to compile function \"{}\": {}", code, e.what()).c_str());
  }
}

template <typename T>
inline std::function<T(IScriptQuery &, const std::any&)> makeFuncMutable(const std::string &code) {
  try {
    return makeFuncMutableImpl<T>(code);
  }
  catch (const std::exception& e) {
    throw std::runtime_error(std::format("failed to compile function \"{}\": {}", code, e.what()).c_str());
  }
}

} // namespace pagan
