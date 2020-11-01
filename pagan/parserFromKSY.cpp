#include "parserFromKSY.h"
#include "expr.h"
#ifndef _NOEXCEPT
#define _NOEXCEPT noexcept
#endif
#include <yaml-cpp/yaml.h>

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
      result[it->first.as<std::string>()] = getNamedType(types, parser, it->second.as<std::string>());
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
    std::string name = entry["id"].as<std::string>();

    uint32_t typeId;
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
      typeId = getNamedType(types, parser, typeIdStr);
    }

    TypePropertyBuilder prop = type->appendProperty(entry["id"].as<std::string>().c_str(), typeId);
    if (entry["size"].IsDefined()) {
      prop.withSize(makeFunc<ObjSize>(entry["size"].as<std::string>()));
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
        prop.withSize([len](const DynObject &obj) -> ObjSize { return static_cast<ObjSize>(len); });
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
      prop.withRepeatToEOS();
    }
    if (typeId == TypeId::runtime) {
      std::map<std::variant<std::string, int32_t>, uint32_t> cases = makeCases(typeNode["cases"], types, parser);

      // makeCases either has all keys as strings or all as numbers, so it's enough to check the first
      try {
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

