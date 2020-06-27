#pragma once

#include <functional>
#include <any>

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
typedef std::function<void(DynObject &object, const std::any &value)> AssignCB;
typedef std::function<bool(const DynObject &object)> ConditionFunc;
typedef std::function<std::string(const DynObject &object)> SwitchFunc;
