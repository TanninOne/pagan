#include "typespec.h"
#include "typeregistry.h"
#include "dynobject.h"
#include "streamregistry.h"
#include "membuf.h"
#include "parser.h"
#include "util.h"
#include "expr.h"
#include "parserFromKSY.h"
#include <iostream>
#include <fstream>
#include <any>
#include <memory>

std::function<bool(const DynObject&)> findGRUP(const char *groupName) {
  return [groupName](const DynObject &obj) {
    if (obj.get<std::string>("type") != "GRUP") {
      return false;
    }
    std::vector<uint8_t> groupLabel = obj.get<DynObject>("data").get<DynObject>("header").get<std::vector<uint8_t>>("label");
    return memcmp(groupLabel.data(), groupName, 4) == 0;
  };
}

int main(int argc, char **argv) {
  try {
    // set up data streams
    std::shared_ptr<Parser> parser = parserFromKSY("esp.ksy");

    parser->addFileStream(argv[1]);

    LogBracket::log("create root element");

    // create an object
    DynObject obj = parser->getObject(parser->getType("root"), 0);

    auto recList = obj.getList<DynObject>("root");
    std::cout << "# items at root: " << recList.size() << std::endl;

    auto armorGroup = std::find_if(recList.cbegin(), recList.cend(), findGRUP("ARMO"));
    armorGroup->debug(0);
    armorGroup->get<DynObject>("data").debug(0);
    auto armorRecs = armorGroup->get<DynObject>("data").get<DynObject>("records").getList<DynObject>("entries");

    std::cout << "# armor recs " << armorRecs.size() << std::endl;

    int ctr = 0;

    for (const auto &armor : armorRecs) {
      if (armor.get<std::string>("type") == "ARMO") {
        armor.get<DynObject>("data").debug(1);
        armor.get<DynObject>("data").get<DynObject>("header").debug(2);
        armor.get<DynObject>("data").get<DynObject>("z_record").debug(2);
        armor.get<DynObject>("data").get<DynObject>("z_record").get<DynObject>("value").debug(3);
        auto value = armor.get<DynObject>("data").get<DynObject>("z_record").get<DynObject>("value");
        auto fields = value.getList<DynObject>("fields");
        for (const auto &field : fields) {
          std::string type = field.get<std::string>("type");
          // std::cout << "field type " << type << std::endl;
          if (type == "MOD2") {
//             std::cout << "male model " << field.get<std::string>("data") << std::endl;
          }
          else if (type == "MOD4") {
//             std::cout << "female model " << field.get<std::string>("data") << std::endl;
          }
          else if (type == "ICON") {
            std::cout << "icon " << field.get<std::string>("data") << std::endl;
          }
          else if (type == "EDID") {
//             std::cout << "editor id " << field.get<std::string>("data") << std::endl;
          }
          else if (type == "OBND") {
            auto obnd = field.get<DynObject>("data");
            /*
            std::cout
              << "Bounds: "
              << obnd.get<int16_t>("x1") << "x" << obnd.get<int16_t>("y1") << "x" << obnd.get<int16_t>("z1")
              << " - "
              << obnd.get<int16_t>("x2") << "x" << obnd.get<int16_t>("y2") << "x" << obnd.get<int16_t>("z2")
              << std::endl;
              */
          }
          else {
            // std::cout << "field type " << field.get<std::string>("type") << std::endl;
          }
        }
      }
    }

    return 0;
  }
  catch (const std::exception &e) {
    std::cerr << "TL exception: " << e.what() << std::endl;
    return 1;
  }
}

