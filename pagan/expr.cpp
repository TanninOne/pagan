#include "expr.h"

#include <vector>
#include <string_view>

/*
std::vector<std::string_view> splitVariable(const std::string_view &input) {
  std::vector<std::string_view> result;

  result.reserve(3);

  size_t offset = 0;

  while (true) {
    size_t dotPos = input.find_first_of('.', offset);
    result.emplace_back(dotPos != -1 ? input.substr(offset, dotPos - offset) : input.substr(offset));
    if (dotPos == std::string::npos) {
      break;
    }
    else {
      offset = dotPos + 1;
    }
  }

  return result;
}
*/

void splitVariable(const std::string_view &input, std::vector<std::string_view> &result) {
  result.clear();

  size_t offset = 0;

  while (true) {
    size_t dotPos = input.find_first_of('.', offset);
    result.emplace_back(dotPos != -1 ? input.substr(offset, dotPos - offset) : input.substr(offset));
    if (dotPos == std::string::npos) {
      break;
    }
    else {
      offset = dotPos + 1;
    }
  }
}
