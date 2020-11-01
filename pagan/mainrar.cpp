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

int main(int argc, char **argv) {
  try {
    std::shared_ptr<Parser> parser = parserFromKSY("rar.ksy");

    parser->addFileStream(argv[1]);

    DynObject obj = parser->getObject(parser->getType("root"), 0);
    auto magic = obj.get<DynObject>("magic");
    auto magic1 = magic.get<std::vector<uint8_t>>("magic1");
    std::cout << "magic 1 " << magic1.data() << std::endl;

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

