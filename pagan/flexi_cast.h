#pragma once

#include <any>
#include <functional>
#include <typeindex>
#include <iostream>
#include <sstream>
#include "util.h"

template<typename T>
T flexi_cast_impl(const std::any &input, std::false_type) {
  return std::any_cast<T>(input);
}

template<typename T>
struct IntegralCaster {
  static const std::function<T(const std::any &input)> &get(std::type_index type) {
    static const std::unordered_map<std::type_index, std::function<T(const std::any &input)>> casters{
      { typeid(bool), [](const std::any &input) { return static_cast<T>(std::any_cast<bool>(input)); } },
      { typeid(char), [](const std::any &input) { return static_cast<T>(std::any_cast<char>(input)); } },
      { typeid(short), [](const std::any &input) { return static_cast<T>(std::any_cast<short>(input)); } },
      { typeid(int), [](const std::any &input) { return static_cast<T>(std::any_cast<int>(input)); } },
      { typeid(long), [](const std::any &input) { return static_cast<T>(std::any_cast<long>(input)); } },
      { typeid(unsigned char), [](const std::any &input) { return static_cast<T>(std::any_cast<unsigned char>(input)); } },
      { typeid(unsigned short), [](const std::any &input) { return static_cast<T>(std::any_cast<unsigned short>(input)); } },
      { typeid(unsigned int), [](const std::any &input) { return static_cast<T>(std::any_cast<unsigned int>(input)); } },
      { typeid(unsigned long), [](const std::any &input) { return static_cast<T>(std::any_cast<unsigned long>(input)); } },
      { typeid(uint8_t), [](const std::any &input) { return static_cast<T>(std::any_cast<uint8_t>(input)); } },
      { typeid(uint16_t), [](const std::any &input) { return static_cast<T>(std::any_cast<uint16_t>(input)); } },
      { typeid(uint32_t), [](const std::any &input) { return static_cast<T>(std::any_cast<uint32_t>(input)); } },
      { typeid(uint64_t), [](const std::any &input) { return static_cast<T>(std::any_cast<uint64_t>(input)); } },
      { typeid(int8_t), [](const std::any &input) { return static_cast<T>(std::any_cast<int8_t>(input)); } },
      { typeid(int16_t), [](const std::any &input) { return static_cast<T>(std::any_cast<int16_t>(input)); } },
      { typeid(int32_t), [](const std::any &input) { return static_cast<T>(std::any_cast<int32_t>(input)); } },
      { typeid(int64_t), [](const std::any &input) { return static_cast<T>(std::any_cast<int64_t>(input)); } },
    };
    try {
      return casters.at(type);
    }
    catch (const std::exception& e) {
      std::cout << "invalid caster type requested " << type.name() << " - " << e.what() << std::endl;
      printExceptionStack();
      throw;
    }
  }
};

template<typename T>
T flexi_cast_impl(const std::any &input, std::true_type) {
  static const IntegralCaster<T> caster;
  return caster.get(input.type())(input);
}

template<typename T> T flexi_cast(const std::any &input);

template<>
inline std::string flexi_cast<std::string>(const std::any &input) {
  static const IntegralCaster<int64_t> caster;
  if (!input.has_value()) {
    return std::string();
  }
  try {
    return std::any_cast<std::string>(input);
  }
  catch (const std::bad_any_cast&) {
    try {
      std::ostringstream str;
      str << caster.get(input.type())(input);
      return str.str();
    }
    catch (const std::out_of_range&) {
      return "";
    }
  }
}

/**
 * wrapper around any_cast that is a bit more flexible in casting
 * std::any to types "similar" to what it actually contains
 */
template<typename T>
T flexi_cast(const std::any &input) {
  return flexi_cast_impl<T>(input, std::is_integral<T>());
}

template<>
inline std::any flexi_cast(const std::any& input) {
  return input;
}
