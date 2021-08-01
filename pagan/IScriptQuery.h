#pragma once

class IScriptQuery {
public:
  virtual std::any getAny(char *key) const = 0;
  virtual std::any getAny(std::string key) const = 0;
  virtual std::any getAny(const std::vector<std::string>::const_iterator &cur, const std::vector<std::string>::const_iterator &end) const = 0;

  virtual void setAny(const std::vector<std::string>::const_iterator& cur, const std::vector<std::string>::const_iterator& end, const std::any& value) = 0;
};

