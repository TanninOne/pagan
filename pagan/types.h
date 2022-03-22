#pragma once

#include <functional>
#include <any>
#include <variant>
#include "IScriptQuery.h"
#include "iowrap.h"

enum TypeId {
  int8,
  int16,
  int32,
  int64,
  uint8,
  uint16,
  uint32,
  uint64,
  bits,
  float32_iee754,
  float32 = float32_iee754,
  string,
  stringz,
  bytes,
  runtime,

  custom,
};

class DynObject;

typedef int32_t ObjSize;
typedef std::function<ObjSize(const IScriptQuery &object)> SizeFunc;
typedef std::function<void(IScriptQuery &object, const std::any &value)> AssignCB;
typedef std::function<bool(const IScriptQuery &object)> ConditionFunc;
typedef std::function<bool(const std::any &value)> ValidationFunc;
typedef std::function<std::variant<std::string, int32_t>(const IScriptQuery &object)> SwitchFunc;
typedef std::function<std::any(const IScriptQuery& object)> ComputeFunc;
typedef std::function<uint8_t* (uint8_t*, const DynObject*, uint16_t, std::shared_ptr<IOWrapper>, std::streampos)> IndexFunc;
