#include "typespec.h"
#include "typeregistry.h"
#include "dynobject.h"
#include "streamregistry.h"
#include "membuf.h"
#include "util.h"
#include "Parser.h"
#include <iostream>
#include <any>

std::shared_ptr<std::iostream> makeBufferStream(const uint8_t *buffer, size_t size) {
  MemoryBuf *mb = new MemoryBuf(reinterpret_cast<const char *>(buffer), size);
  return std::shared_ptr<std::iostream>(new std::iostream(mb));
}

std::string debugVector(const DynObject &vec) {
  float x = vec.get<float>("x");
  float y = vec.get<float>("y");
  float z = vec.get<float>("z");
  return std::format("{0} x {1} x {2}", x, y, z);
}

int main() {
  try {
    Parser parser;

    std::shared_ptr<TypeSpec> vector = parser.createType("vector", {
      { "x", TypeId::float32 },
      { "y", TypeId::float32 },
      { "z", TypeId::float32 },
    });

    // build type declaration
    std::shared_ptr<TypeSpec> spec = parser.createType("spec");
    spec->appendProperty("num", TypeId::int32);
    spec->appendProperty("conditional", TypeId::int32)
      .withCondition([](const DynObject &obj) { return obj.get<int32_t>("num") > 5; });
    spec->appendProperty("unfulfilled", TypeId::int32)
      .withCondition([](const DynObject &obj) { return false; });
    spec->appendProperty("str", TypeId::stringz);
    spec->appendProperty("str2len", TypeId::int8);
    spec->appendProperty("str2", TypeId::string)
      .withSize([](const DynObject &obj) {
        int8_t len = obj.get<int8_t>("str2len");
        std::cout << "got len for str2 " << static_cast<int>(len) << std::endl;
        return static_cast<ObjSize>(len);
      })
      .onAssign([](DynObject &obj, const std::any &value) {
        if (std::any_cast<std::string>(value).length() == 4) {
          throw std::runtime_error("invalid length");
        }
        obj.set("str2len", static_cast<int8_t>(std::any_cast<std::string>(value).length()));
      });

    spec->appendProperty("itemCount", TypeId::int8);
    spec->appendProperty("items", TypeId::int8)
      .withCount([](const DynObject &obj) {
        return static_cast<ObjSize>(obj.get<int8_t>("itemCount"));
      })
      .onAssign([](DynObject &obj, const std::any &value) {
        const std::vector<int8_t> vec = std::any_cast<std::vector<int8_t>>(value);
        obj.set("itemCount", static_cast<int8_t>(vec.size()));
      });
    spec->appendProperty("baseVec", vector->getId());

    spec->appendProperty("vectorCount", TypeId::int8);
    spec->appendProperty("vectors", vector->getId())
      .withCount([](const DynObject &obj) {
        return static_cast<ObjSize>(obj.get<int8_t>("vectorCount"));
      })
      .onAssign([](DynObject &obj, const std::any &value) {
        const std::vector<DynObject> vec = std::any_cast<std::vector<DynObject>>(value);
        obj.set("vectorCount", static_cast<int8_t>(vec.size()));
      });

    // set up data streams
    const uint8_t buf[]{
      0x20, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 'T', 'e', 's', 't', '\0',
      3, 'F', 'o', 'o',
      3, 0x10, 0x30, 0x20,
      0, 0, 0, 0,   0, 0, 0, 0x3f,   0, 0, 0x80, 0x3f,
      2,
      0, 0, 0,    0,   0, 0,    0, 0x40,   0, 0, 0x40, 0x40,
      0, 0, 0, 0x40,   0, 0, 0x40, 0x40,   0, 0, 0, 0x3f,
    };

    parser.addDataStream(makeBufferStream(buf, sizeof(buf)));

    DynObject obj = parser.getObject(spec, 0);

    // test object
    int32_t num = obj.get<int32_t>("num");
    int32_t cond = obj.get<int32_t>("conditional");
    std::cout << "Number: " << std::dec << num << std::endl;
    std::cout << "Conditional: " << std::dec << cond << std::endl;
    try {
      int32_t unfulfilled = obj.get<int32_t>("unfulfilled");
      assert(false && "Should have thrown an exception, unfulfilled isn't available");
    }
    catch (const std::exception &e) {
      std::cout << "Expected exception accessing unfulfilled: " << e.what() << std::endl;
    }
    std::string str = obj.get<std::string>("str");
    std::string str2 = obj.get<std::string>("str2");
    std::cout << "String: " << str << std::endl;
    std::cout << "String2: " << str2 << std::endl;

    {
      std::vector<int8_t> items = obj.getList<int8_t>("items");
      std::cout << "#items: " << items.size() << std::endl;
      assert(items.size() == 3);
      for (int8_t item : items) {
        std::cout << " - " << static_cast<int>(item) << std::endl;
      }
    }

    obj.set("num", 42);
    std::cout << "--- index after change num ---" << std::endl;

    num = obj.get<int32_t>("num");
    str = obj.get<std::string>("str");
    std::cout << "Number after: " << std::dec << num << std::endl;
    std::cout << "String now: " << str << std::endl;

    obj.set("str", std::string("foobar"));

    str = obj.get<std::string>("str");
    std::cout << "String after: " << str << std::endl;

    try {
      obj.set("str2", std::string("narf"));
    }
    catch (const std::exception &e) {
      std::cout << "Expected error: " << e.what() << std::endl;
    }

    obj.set("str2", std::string("foobar"));
    std::cout << "String2 after: " << obj.get<std::string>("str2") << std::endl;
    std::cout << "String2 length after: " << static_cast<int>(obj.get<int8_t>("str2len")) << std::endl;

    {
      std::vector<int8_t> items = obj.getList<int8_t>("items");
      assert(items.size() == 3);
      items.push_back(42);
      obj.setList("items", items);
    }

    {
      std::cout << " -------------- items ----------------- " << std::endl;
      std::vector<int8_t> items = obj.getList<int8_t>("items");
      for (int8_t item : items) {
        std::cout << " - " << static_cast<int>(item) << std::endl;
      }
      int8_t itemsAfter = obj.get<int8_t>("items");
      int8_t itemCount = obj.get<int8_t>("itemCount");
      std::cout << "items after: " << static_cast<int>(itemsAfter) << std::endl;
      std::cout << "itemCount after: " << static_cast<int>(itemCount) << std::endl;
      std::cout << " -------------- /items ----------------- " << std::endl;
    }

    {
      DynObject vec = obj.get<DynObject>("baseVec");

      std::cout << std::format("vector: {0}", debugVector(vec)) << std::endl;
      vec.set("x", 3.14f);
      std::cout << std::format("vector after: {0}", debugVector(vec)) << std::endl;
    }

    {
      DynObject vec = obj.get<DynObject>("baseVec");
      std::cout << std::format("vector 2: {0}", debugVector(vec)) << std::endl;
    }

    {
      std::cout << " -------------- vectors ----------------- " << std::endl;
      std::vector<DynObject> vectors = obj.getList<DynObject>("vectors");
      for (auto vec : vectors) {
        std::cout << std::format("vec: {0}", debugVector(vec)) << std::endl;
      }
      std::cout << " -------------- /vectors ----------------- " << std::endl;
    }

    try {
      DynObject newVec = parser.createObject(vector, { 2.5f, 3.5f, 4.5f });
      std::vector<DynObject> vectors = obj.getList<DynObject>("vectors");
      vectors.push_back(newVec);
      std::cout << "count: " << vectors.size() << std::endl;
      obj.setList("vectors", vectors);
    }
    catch (const std::exception &e) {
      std::cerr << "Failed to assign vector " << e.what() << std::endl;
      return 1;
    }

    {
      std::cout << " -------------- vectors after ----------------- " << std::endl;
      std::vector<DynObject> vectors = obj.getList<DynObject>("vectors");
      for (auto vec : vectors) {
        std::cout << std::format("vec: {0}", debugVector(vec)) << std::endl;
      }
      std::cout << " -------------- /vectors after ----------------- " << std::endl;
    }


    return 0;
  }
  catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}

