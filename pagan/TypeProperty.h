#pragma once

#include <string>
#include <map>
#include <variant>

#include "types.h"

namespace pagan {

struct TypeProperty {
  std::string key;
  uint32_t typeId;
  SizeFunc size;
  SizeFunc count;
  ConditionFunc repeatCondition;
  ValidationFunc validation;
  ConditionFunc condition;
  AssignCB onAssign;
  bool isList;
  bool isConditional;
  bool isValidated;
  bool hasSizeFunc;
  bool isSwitch;
  bool hasEnum;
  std::string debug;
  std::string processing;
  std::string enumName;
  IndexFunc index;
  SwitchFunc switchFunc;
  std::map<std::variant<std::string, int32_t>, uint32_t> switchCases;
  std::vector<std::string> argList;
};

} // namespace pagan
