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
#include <ctime>

std::function<bool(const DynObject&)> findGRUP(const char *groupName) {
  return [groupName](const DynObject &obj) {
    if (obj.get<std::string>("type") != "GRUP") {
      return false;
    }
    std::vector<uint8_t> groupLabel = obj.get<DynObject>("data").get<DynObject>("header").get<std::vector<uint8_t>>("label");
    return memcmp(groupLabel.data(), groupName, 4) == 0;
  };
}

int mainx(int argc, char **argv) {
  try {
    // set up data streams
    std::shared_ptr<Parser> parser = parserFromKSY("esp.ksy");

    parser->addFileStream(argv[1]);

    std::cout << "create root element" << std::endl;
    time_t start = time(nullptr);

    // create an object
    DynObject obj = parser->getObject(parser->getType("root"), 0);

    std::cout << "parsing done in " << (int)std::difftime(time(nullptr), start) << " seconds" << std::endl;

    auto recList = obj.getList<DynObject>("root");
    std::cout << "# items at root: " << recList.size() << std::endl;

    auto tes4 = std::find_if(recList.cbegin(), recList.cend(), [](const DynObject &obj) { return obj.get<std::string>("type") == "TES4"; });
    std::cout << "header found: " << (tes4 != recList.cend()) << std::endl;
    auto zrec = tes4->get<DynObject>("data").get<DynObject>("z_record").get<DynObject>("value");
    auto fields = zrec.getList<DynObject>("fields");
    auto cnam = std::find_if(fields.cbegin(), fields.cend(), [](const DynObject &obj) { return obj.get<std::string>("type") == "CNAM"; });
    for (const auto &key : cnam->get<DynObject>("fields").getKeys()) {
      std::cout << "cnam key " << key << std::endl;
    }
    std::cout << "- " << cnam->get<DynObject>("fields").get<std::string>("name") << std::endl;
    // cnam->get<DynObject>("fields").set<std::string>("name", "foo");

    /*
    std::cout << "cnam: " << cnam->get<std::string>("type") << std::endl;

    int32_t numRecords = header->get<int32_t>("num_records");
    std::cout << "# total records: " << numRecords << std::endl;
    */

    auto armorGroup = std::find_if(recList.cbegin(), recList.cend(), findGRUP("ARMO"));
    auto armorRecs = armorGroup->get<DynObject>("data").get<DynObject>("records").getList<DynObject>("entries");

    std::cout << "# armor recs: " << armorRecs.size() << std::endl;

    int ctr = 0;

    for (const auto &armor : armorRecs) {
      if (armor.get<std::string>("type") == "ARMO") {
        // armor.get<DynObject>("data").debug(1);
        // armor.get<DynObject>("data").get<DynObject>("header").debug(2);
        // armor.get<DynObject>("data").get<DynObject>("z_record").debug(2);
        // armor.get<DynObject>("data").get<DynObject>("z_record").get<DynObject>("value").debug(3);
        auto value = armor.get<DynObject>("data").get<DynObject>("z_record").get<DynObject>("value");
        auto fields = value.getList<DynObject>("fields");
        for (auto &field : fields) {
          std::string type = field.get<std::string>("type");
          if (type == "MOD2") {
            std::cout << "male model >" << field.get<std::string>("data") << "< - " << field.get<std::string>("data").length() << std::endl;
            field.set<std::string>("data", std::string("foobar"));
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
          }
        }
      }
    }

    parser->write((std::string(argv[1]) + ".edited").c_str(), obj);

    return 0;
  }
  catch (const std::exception &e) {
    std::cerr << "TL exception: " << e.what() << std::endl;
    return 1;
  }
  catch (...) {
    std::cerr << "unknown exception" << std::endl;
    return 2;
  }
}

