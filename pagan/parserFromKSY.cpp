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

std::map<std::string, uint32_t> makeCases(const YAML::Node &spec, NamedTypes &types, Parser &parser) {
  std::map<std::string, uint32_t> result;

  if (!spec.IsMap()) {
    throw std::runtime_error("expected a map");
  }

  for (YAML::const_iterator it = spec.begin(); it != spec.end(); ++it) {
    result[it->first.as<std::string>()] = getNamedType(types, parser, it->second.as<std::string>());
  }

  return result;
}

void addProperties(Parser &parser, NamedTypes &types, std::shared_ptr<TypeSpec> &type, const YAML::Node &spec) {
  if (!spec.IsSequence()) {
    throw std::runtime_error("expected a sequence");
  }

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
      prop.withTypeSwitch(makeFunc<std::string>(typeNode["switch-on"].as<std::string>()),
                          makeCases(typeNode["cases"], types, parser));
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

void createTypeFromYAML(Parser &parser,
                        NamedTypes &types,
                        const char *name,
                        const YAML::Node &spec) {
  addSubTypes(parser, types, spec["types"]);

  std::shared_ptr<TypeSpec> type = parser.createType(name);
  types[name] = type->getId();
  addProperties(parser, types, type, spec["seq"]);
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

  createTypeFromYAML(*parser, namedTypes, "root", spec);

  return parser;
}

