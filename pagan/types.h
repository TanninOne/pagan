#pragma once

#include <functional>
#include <any>
#include <variant>

enum TypeId {
  int8,
  int16,
  int32,
  int64,
  uint8,
  uint16,
  uint32,
  uint64,
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
typedef std::function<ObjSize(const DynObject &object)> SizeFunc;
typedef std::function<void(DynObject &object)> AssignCB;
typedef std::function<bool(const DynObject &object)> ConditionFunc;
typedef std::function<bool(const std::any &value)> ValidationFunc;
typedef std::function<std::variant<std::string, int32_t>(const DynObject &object)> SwitchFunc;
typedef std::function<std::any(const DynObject& object)> ComputeFunc;
