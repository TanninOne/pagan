#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <functional>
#include <map>

#include "types.h"

namespace pagan {

class TypeProperty;

class TypePropertyBuilder {
public:
  TypePropertyBuilder(TypeProperty *wrappee, const std::function<void()> &cb);
  ~TypePropertyBuilder() { m_Callback(); }

  TypePropertyBuilder &withCondition(ConditionFunc &&func);
  TypePropertyBuilder &withSize(SizeFunc &&func);
  TypePropertyBuilder &withEnum(std::string_view enumName);
  TypePropertyBuilder &withRepeatToEOS();
  TypePropertyBuilder &withCount(SizeFunc &&func);
  TypePropertyBuilder &withRepeatCondition(ConditionFunc &&func);
  TypePropertyBuilder &withTypeSwitch(
      SwitchFunc &&func,
      std::map<std::variant<std::string, int32_t>, uint32_t> &&cases);
  TypePropertyBuilder &onAssign(const AssignCB &func);
  TypePropertyBuilder &withProcessing(std::string_view algorithm);
  TypePropertyBuilder &withValidation(ValidationFunc &&func);
  TypePropertyBuilder &withDebug(std::string_view debugMessage);
  TypePropertyBuilder &withArguments(const std::vector<std::string> &args);

private:
  TypeProperty *m_Wrappee;
  std::function<void()> m_Callback;
};

} // namespace pagan
