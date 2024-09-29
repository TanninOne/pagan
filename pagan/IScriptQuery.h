#pragma once

#include <string_view>
#include <any>
#include <vector>

class IScriptQuery {
public:
  virtual std::any getAny(const char *key) const = 0;
  virtual std::any getAny(std::string_view key) const = 0;
  virtual std::any getAny(const std::vector<std::string_view>::const_iterator &cur, const std::vector<std::string_view>::const_iterator &end) const = 0;

  virtual void setAny(const std::vector<std::string_view>::const_iterator& cur, const std::vector<std::string_view>::const_iterator& end, const std::any& value) = 0;
};
