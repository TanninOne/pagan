#include "TypePropertyBuilder.h"
#include "TypeProperty.h"
#include "constants.h"

namespace pagan {

static const SizeFunc eosCount = [] (const IScriptQuery &object) -> ObjSize {
  return COUNT_EOS;
};

static const SizeFunc moreCount = [] (const IScriptQuery &object) -> ObjSize {
  return COUNT_MORE;
};

TypePropertyBuilder::TypePropertyBuilder(TypeProperty *wrappee,
                                         const std::function<void()> &cb)
    : m_Wrappee(wrappee), m_Callback(cb) {}

TypePropertyBuilder &
TypePropertyBuilder::withProcessing(std::string_view algorithm) {
  m_Wrappee->processing = algorithm;
  return *this;
}

TypePropertyBuilder &TypePropertyBuilder::withCondition(ConditionFunc &&func) {
  m_Wrappee->condition = std::move(func);
  m_Wrappee->isConditional = true;
  return *this;
}

TypePropertyBuilder &
TypePropertyBuilder::withValidation(ValidationFunc &&func) {
  m_Wrappee->validation = func;
  m_Wrappee->isValidated = true;
  return *this;
}

TypePropertyBuilder &
TypePropertyBuilder::withDebug(std::string_view debugMessage) {
  m_Wrappee->debug = debugMessage;
  return *this;
}

TypePropertyBuilder &
TypePropertyBuilder::withArguments(const std::vector<std::string> &args) {
  m_Wrappee->argList = args;
  return *this;
}

TypePropertyBuilder &TypePropertyBuilder::withSize(SizeFunc &&func) {
  m_Wrappee->size = std::move(func);
  m_Wrappee->hasSizeFunc = true;
  return *this;
}

TypePropertyBuilder &TypePropertyBuilder::withEnum(std::string_view enumName) {
  m_Wrappee->enumName = enumName;
  m_Wrappee->hasEnum = true;
  return *this;
}

TypePropertyBuilder &TypePropertyBuilder::withRepeatToEOS() {
  m_Wrappee->count = eosCount;
  m_Wrappee->isList = true;
  return *this;
}

TypePropertyBuilder &TypePropertyBuilder::withCount(SizeFunc &&func) {
  m_Wrappee->count = std::move(func);
  m_Wrappee->isList = true;
  return *this;
}

TypePropertyBuilder &
TypePropertyBuilder::withRepeatCondition(ConditionFunc &&func) {
  m_Wrappee->count = moreCount;
  m_Wrappee->repeatCondition = std::move(func);
  m_Wrappee->isList = true;
  return *this;
}

TypePropertyBuilder &TypePropertyBuilder::withTypeSwitch(
    SwitchFunc &&func,
    std::map<std::variant<std::string, int32_t>, uint32_t> &&cases) {
  m_Wrappee->switchFunc = std::move(func);
  m_Wrappee->switchCases = std::move(cases);
  m_Wrappee->isSwitch = true;
  return *this;
}

TypePropertyBuilder &TypePropertyBuilder::onAssign(const AssignCB &func) {
  m_Wrappee->onAssign = func;
  return *this;
}

} // namespace pagan
