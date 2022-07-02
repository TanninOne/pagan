#pragma once

#include "types.h"

#include <string>
#include <cstdio>
#include <map>
#include <variant>

struct TypeProperty {
  std::string key;
  uint32_t typeId;
  SizeFunc size;
  SizeFunc count;
  ValidationFunc validation;
  ConditionFunc condition;
  AssignCB onAssign;
  bool isList;
  bool isConditional;
  bool isValidated;
  bool hasSizeFunc;
  bool isSwitch;
  std::string debug;
  std::string processing;
  IndexFunc index;
  SwitchFunc switchFunc;
  std::map<std::variant<std::string, int32_t>, uint32_t> switchCases;
  std::vector<std::string> argList;
};
