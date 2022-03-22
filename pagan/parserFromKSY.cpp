#include "parserFromKSY.h"
#include "expr.h"
#include "TypeSpec.h"
#ifndef _NOEXCEPT
#define _NOEXCEPT noexcept
#endif
#include <yaml-cpp/yaml.h>
#include <charconv>

typedef std::map<std::string, uint32_t> NamedTypes;

uint32_t getNamedType(NamedTypes &types, Parser &parser, const std::string &name) {
  auto typeIter = types.find(name);
  if (typeIter == types.end()) {
    // reserve a type id, we don't need to know the exact structure until later
    types[name] = parser.createType(name.c_str())->getId();
  }
  return types[name];
}

std::map<std::variant<std::string, int32_t>, uint32_t> makeCases(const YAML::Node &spec, NamedTypes &types, Parser &parser) {
  if (!spec.IsMap()) {
    throw std::runtime_error("expected a map");
  }

  try {
    std::map<std::variant<std::string, int32_t>, uint32_t> result;

    for (YAML::const_iterator it = spec.begin(); it != spec.end(); ++it) {
      result[it->first.as<int32_t>()] = getNamedType(types, parser, it->second.as<std::string>());
    }

    return result;
  }
  catch (const std::exception&) {
    std::map<std::variant<std::string, int32_t>, uint32_t> result;

    for (YAML::const_iterator it = spec.begin(); it != spec.end(); ++it) {
      std::string key = it->first.as<std::string>();
      if ((key[0] == '"') && (*key.crbegin() == '"')) {
        key = key.substr(1, key.length() - 2);
      }
      result[key] = getNamedType(types, parser, it->second.as<std::string>());
    }

    return result;
  }
}

void addInstances(Parser& parser, std::shared_ptr<TypeSpec> &type, const YAML::Node& spec) {
  if (!spec.IsDefined() || !spec.IsMap()) {
    return;
  }

  for (YAML::const_iterator it = spec.begin(); it != spec.end(); ++it) {
    type->addComputed(it->first.as<std::string>().c_str(), makeFunc<std::any>(it->second["value"].as<std::string>()));
  }
}

void addProperties(Parser &parser, NamedTypes &types, std::shared_ptr<TypeSpec> &type, const YAML::Node &spec) {
  if (!spec.IsDefined() || !spec.IsSequence()) {
    return;
  }

  LOG_BRACKET_F("create type {0} - {1}", type->getName(), type->getId());

  for (size_t i = 0; i < spec.size(); ++i) {
    YAML::Node entry = spec[i];
    YAML::Node idNode = entry["id"];
    std::string name = idNode.IsDefined()
      ? idNode.as<std::string>()
      : std::to_string(i);

    uint32_t typeId;
    uint32_t fixedSize = 0;
    YAML::Node typeNode = entry["type"];
    if (!typeNode.IsDefined()) {
      typeId = TypeId::bytes;
    }
    else if (typeNode.IsMap()) {
      if (typeNode["switch-on"].IsDefined()) {
        typeId = TypeId::runtime;
      }
      else {
        typeId = TypeId::custom;
      }
    }
    else {
      std::string typeIdStr = typeNode.as<std::string>();
      LOG_F("typeid {}", typeIdStr);
      if ((typeIdStr[0] == 'b') && (typeIdStr.length() <= 3)) {
        LOG_F("might be bits?");

        // might be bits but can't be sure yet
        const char* buf = typeIdStr.c_str();
        int bitCount = 0;
        auto [ptr, ec] { std::from_chars(buf + 1, buf + typeIdStr.length(), bitCount) };
        LOG_F("might be bits? {} - {}", ptr, (int)ec);
        if ((ec == std::errc()) && (*ptr == '\0')) {
          LOG_F("yes bits {}", bitCount);
          typeId = bits;
          fixedSize = bitCount;
        }
        else {
          // custom type after all
          typeId = getNamedType(types, parser, typeIdStr);
        }
      }
      else {
        typeId = getNamedType(types, parser, typeIdStr);
      }
    }

    TypePropertyBuilder prop = type->appendProperty(name.c_str(), typeId);
    if (entry["size"].IsDefined()) {
      prop.withSize(makeFunc<ObjSize>(entry["size"].as<std::string>()));
    }
    else if (fixedSize > 0) {
      prop.withSize(makeFunc<ObjSize>(std::to_string(fixedSize)));
    }

    if (entry["debug"].IsDefined()) {
      prop.withDebug(entry["debug"].as<std::string>());
    }

    if (entry["contents"].IsDefined()) {
      auto contents = entry["contents"];
      std::vector<uint8_t> value;
      if (contents.IsSequence()) {
        for (size_t i = 0; i < contents.size(); ++i) {
          std::string item = contents[i].as<std::string>();
          std::size_t pos = 0;
          int numValue = 0;
          try {
            if (item.rfind("0x", 0) == 0) {
              // hex number - presumably
              numValue = std::stoi(item, &pos, 16);
            }
            else {
              numValue = std::stoi(item, &pos, 10);
            }
          }
          catch (const std::invalid_argument&) {
            // ignore, assume input as a string
          }
          if (pos == item.length()) {
            value.push_back(numValue);
          } else {
            // if conversion from number was not correct, assume input was a string.
            value.insert(value.begin(), item.begin(), item.end());
          }
        }
      }
      else {
        std::string temp = contents.as<std::string>();
        value.assign(temp.begin(), temp.end());
      }

      if (!entry["size"].IsDefined()) {
        size_t len = value.size();
        prop.withSize([len](const IScriptQuery &obj) -> ObjSize { return static_cast<ObjSize>(len); });
      }
      prop.withValidation([value](const std::any &in) {
        try {
          auto expected = std::any_cast<std::string>(in);
          if (expected.length() != value.size()) {
            return false;
          }
          return memcmp(value.data(), expected.data(), expected.length()) == 0;
        }
        catch (const std::bad_any_cast&) {
          return false;
        }
      });
    }

    if (entry["assign"].IsDefined()) {
      // TODO return value is just a workaround since makeFunc is written to require one, we neither expect
      // the cb to return something nor do we make use of the return value
      prop.onAssign(makeFuncMutable<bool>(entry["assign"].as<std::string>()));
    }
    if (entry["if"].IsDefined()) {
      prop.withCondition(makeFunc<bool>(entry["if"].as<std::string>()));
    }
    if (entry["process"].IsDefined()) {
      prop.withProcessing(entry["process"].as<std::string>());
    }
    if (entry["repeat"].IsDefined()) {
      auto repeatType = entry["repeat"].as<std::string>();
      if (repeatType == "eos") {
        prop.withRepeatToEOS();
      }
      else if (repeatType == "expr") {
        prop.withCount(makeFunc<int32_t>(entry["repeat-expr"].as<std::string>()));
      }
      else {
        throw std::runtime_error("unsupported repeat function");
      }
    }
    if (typeId == TypeId::runtime) {
      std::map<std::variant<std::string, int32_t>, uint32_t> cases = makeCases(typeNode["cases"], types, parser);

      try {
        // makeCases either has all keys as strings or all as numbers, so it's enough to check the first
        int32_t dummy = std::get<int32_t>(cases.begin()->first);
        prop.withTypeSwitch(makeFunc<int32_t>(typeNode["switch-on"].as<std::string>()), cases);
      }
      catch (const std::bad_variant_access&) {
        prop.withTypeSwitch(makeFunc<std::string>(typeNode["switch-on"].as<std::string>()), cases);
      }
    }
  }
}

void createTypeFromYAML(Parser &parser, NamedTypes &types, const char *name, const YAML::Node &spec);

void addSubTypes(Parser &parser, NamedTypes &types, const YAML::Node &spec) {
  if (!spec.IsDefined()) {
    return;
  }

  if (!spec.IsMap()) {
    throw std::runtime_error("expected a map");
  }

  for (YAML::const_iterator it = spec.begin(); it != spec.end(); ++it) {
    createTypeFromYAML(parser, types, it->first.as<std::string>().c_str(), it->second);
  }
}

typedef std::map<std::string, int32_t> KSYEnum;

std::map<std::string, KSYEnum> enumsFromYAML(const YAML::Node &spec) {
  std::map<std::string, KSYEnum> result;

  if (!spec.IsDefined()) {
    return result;
  }

  if (!spec.IsMap()) {
    throw std::runtime_error("expected a map");
  }

  for (YAML::const_iterator it = spec.begin(); it != spec.end(); ++it) {
    if (!it->second.IsMap()) {
      throw std::runtime_error("expected a map");
    }

    KSYEnum enm;

    for (YAML::const_iterator keys = it->second.begin(); keys != it->second.end(); ++keys) {
      enm[keys->second.as<std::string>()] = keys->first.as<int32_t>();
    }

    result[it->first.as<std::string>()] = enm;
  }
 
  return result;
}

void createTypeFromYAML(Parser &parser,
                        NamedTypes &types,
                        const char *name,
                        const YAML::Node &spec) {
  addSubTypes(parser, types, spec["types"]);

  std::shared_ptr<TypeSpec> type = parser.createType(name);
  types[name] = type->getId();
  addProperties(parser, types, type, spec["seq"]);
  addInstances(parser, type, spec["instances"]);
}

void initBaseTypes(NamedTypes &types) {
  types["str"] = TypeId::string;
  types["strz"] = TypeId::stringz;
  types["u1"] = TypeId::uint8;
  types["u2"] = TypeId::uint16;
  types["u4"] = TypeId::uint32;
  types["u8"] = TypeId::uint64;
  types["s1"] = TypeId::int8;
  types["s2"] = TypeId::int16;
  types["s4"] = TypeId::int32;
  types["s8"] = TypeId::int64;
  types["f4"] = TypeId::float32;
  types["b*"] = TypeId::bits;
  types["bytes"] = TypeId::bytes;
}

std::shared_ptr<Parser> parserFromKSY(const char *specFileName) {
  std::ifstream specStream(specFileName);
  if (!specStream.is_open()) {
    throw std::runtime_error("failed to open spec file");
  }
  YAML::Node spec = YAML::Load(specStream);

  if (!spec.IsDefined()) {
    throw std::runtime_error("spec not found");
  }

  std::shared_ptr<Parser> parser(new Parser());

  NamedTypes namedTypes;
  initBaseTypes(namedTypes);

  std::map<std::string, KSYEnum> enums = enumsFromYAML(spec["enums"]);
  createTypeFromYAML(*parser, namedTypes, "root", spec);

  return parser;
}

