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

int mainy(int argc, char **argv) {
  try {
    std::shared_ptr<Parser> parser = parserFromKSY("zip.ksy");

    parser->addFileStream(argv[1]);

    std::vector<DynObject> sections = parser->getList(parser->getType("pk_section"), 0);

    for (const DynObject& section : sections) {
      if (section.get<uint16_t>("section_type") == 0x0403) {
        DynObject body = section.get<DynObject>("body");
        DynObject header = body.get<DynObject>("header");
        std::cout
          << "file: " << header.get<std::string>("file_name")
          << " - size: " << header.get<uint32_t>("len_body_uncompressed")
          << " - compression: " << header.get<uint16_t>("compression_method")
          << " - flags: " << std::hex << header.get<uint16_t>("flags") << std::dec
          << std::endl;
      }
      else if (section.get<uint16_t>("section_type") == 0x0201) {
        DynObject body = section.get<DynObject>("body");
        // std::cout << "dir: " << body.get<std::string>("file_name") << " - compression: " << body.get<uint16_t>("compression_method") << std::endl;
      }
      else if (section.get<uint16_t>("section_type") == 0x0605) {
        DynObject body = section.get<DynObject>("body");
        std::cout << "central dir entries: " << body.get<uint16_t>("num_central_dir_entries_total") << std::endl;
      }
    }

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

