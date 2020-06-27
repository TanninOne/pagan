#include "expr.h"

std::vector<std::string> splitVariable(const std::string & input) {
  std::vector<std::string> result;

  size_t offset = 0;

  while (true) {
    size_t dotPos = input.find_first_of('.', offset);
    result.push_back(dotPos != -1 ? input.substr(offset, dotPos - offset) : input.substr(offset));
    if (dotPos == -1) {
      break;
    }
    else {
      offset = dotPos + 1;
    }
  }

  return result;
}
