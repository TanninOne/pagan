#pragma once

#include "IScriptQuery.h"
#include "iowrap.h"

#include <functional>
#include <any>
#include <variant>

namespace pagan {

enum TypeId {
  int8 = 0,
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

using ObjSize = int32_t;
using SizeFunc = std::function<ObjSize (const IScriptQuery &)>;
using AssignCB = std::function<void (IScriptQuery &, const std::any &)>;
using ConditionFunc = std::function<bool (const IScriptQuery &)>;
using ValidationFunc = std::function<bool (const std::any &)>;
using SwitchFunc = std::function<std::variant<std::string, int32_t> (const IScriptQuery &)>;
using ComputeFunc = std::function<std::any (const IScriptQuery &)>;
using IndexFunc = std::function<uint8_t *(uint8_t *, const DynObject *, uint16_t, std::shared_ptr<IOWrapper>, std::streampos)>;

} // namespace pagan
