#include <catch.hpp>
#include "../pagan/DynObject.h"
#include "../pagan/TypeRegistry.h"
#include "../pagan/TypeSpec.h"

class SimpleFixture {
protected:
  std::shared_ptr<TypeRegistry> types;
  StreamRegistry streams;
  ObjectIndexTable indexTable;
  std::shared_ptr<TypeSpec> testType;
  std::shared_ptr<IOWrapper> testStream;

public:
  SimpleFixture()
    : types(TypeRegistry::init())
    , testType(types->create("test"))
  {
    testType->appendProperty("num", TypeId::int32);
    testStream.reset(IOWrapper::memoryBuffer());

    std::vector<uint8_t> buffer { 0x2A, 0x00, 0x00, 0x00 };
    testStream->write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    streams.add(testStream);
  }
};

class ComplexFixture {
protected:
  std::shared_ptr<TypeRegistry> types;
  StreamRegistry streams;
  ObjectIndexTable indexTable;
  std::shared_ptr<TypeSpec> testType;
  std::shared_ptr<IOWrapper> testStream;

public:
  ComplexFixture()
    : types(TypeRegistry::init())
    , testType(types->create("test"))
  {
    std::shared_ptr<TypeSpec> nestedType = types->create("nested");
    nestedType->appendProperty("num", TypeId::int32);
    nestedType->appendProperty("len", TypeId::uint8);
    nestedType->appendProperty("str", TypeId::string)
      .withSize([](const IScriptQuery &obj) -> ObjSize { return std::any_cast<uint8_t>(obj.getAny("len")); })
      .onAssign([](IScriptQuery& obj, const std::any& value) {
        std::vector<std::string> keyVec{ "len" };
        obj.setAny(keyVec.begin(), keyVec.end(), static_cast<uint8_t>(std::any_cast<std::string>(value).length()));
      });
    nestedType->appendProperty("lst", TypeId::string)
      .withSize([](const IScriptQuery&) { return 3; })
      .withCount([](const IScriptQuery&) { return 3; });
// typedef std::function<void(IScriptQuery &object)> AssignCB;

    testType->appendProperty("nested", nestedType->getId());

    testStream.reset(IOWrapper::memoryBuffer());

    std::vector<uint8_t> buffer { 0x2A, 0x00, 0x00, 0x00, 0x06, 'f', 'o', 'o', 'b', 'a', 'r', 'a', 'a', 'a', 'b', 'b', 'b', 'c', 'c', 'c' };
    testStream->write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    streams.add(testStream);
  }
};

TEST_CASE_METHOD(SimpleFixture, "can create simple", "[DynObject]") {
  uint8_t staticBuffer[8 * NUM_STATIC_PROPERTIES];

  ObjectIndex *index = indexTable.allocateObject(testType, 0, 0);

  DynObject obj(testType, streams, &indexTable, index, nullptr);
  obj.writeIndex(0, testStream->size(), true);

  REQUIRE(obj.get<int32_t>("num") == 42);
}

TEST_CASE_METHOD(SimpleFixture, "can save simple", "[DynObject]") {
  uint8_t staticBuffer[8 * NUM_STATIC_PROPERTIES];

  ObjectIndex *index = indexTable.allocateObject(testType, 0, 0);

  DynObject obj(testType, streams, &indexTable, index, nullptr);
  obj.writeIndex(0, testStream->size(), true);

  std::shared_ptr<IOWrapper> result(IOWrapper::memoryBuffer());
  obj.saveTo(result);

  char output[8];
  result->read(output, 4);

  REQUIRE(*reinterpret_cast<int32_t*>(output) == 42);
}

TEST_CASE_METHOD(SimpleFixture, "can edit simple", "[DynObject]") {
  uint8_t staticBuffer[8 * NUM_STATIC_PROPERTIES];

  ObjectIndex *index = indexTable.allocateObject(testType, 0, 0);

  DynObject obj(testType, streams, &indexTable, index, nullptr);
  obj.writeIndex(0, testStream->size(), true);
  obj.set<int32_t>("num", 69);

  std::shared_ptr<IOWrapper> result(IOWrapper::memoryBuffer());
  obj.saveTo(result);

  char output[8];
  result->read(output, 4);

  REQUIRE(*reinterpret_cast<int32_t*>(output) == 69);
}

TEST_CASE_METHOD(SimpleFixture, "returns reasonable error", "[DynObject]") {
  uint8_t staticBuffer[8 * NUM_STATIC_PROPERTIES];

  ObjectIndex *index = indexTable.allocateObject(testType, 0, 0);

  DynObject obj(testType, streams, &indexTable, index, nullptr);
  REQUIRE_THROWS(obj.get<int32_t>("invalid"));
}

TEST_CASE_METHOD(ComplexFixture, "can save complex", "[DynObject]") {
  uint8_t staticBuffer[8 * NUM_STATIC_PROPERTIES];

  ObjectIndex *index = indexTable.allocateObject(testType, 0, 0);

  DynObject obj(testType, streams, &indexTable, index, nullptr);
  obj.writeIndex(0, testStream->size(), true);

  std::shared_ptr<IOWrapper> result(IOWrapper::memoryBuffer());
  obj.saveTo(result);

  char output[20];
  result->read(output, 20);

  REQUIRE(*reinterpret_cast<int32_t*>(output) == 42);
  REQUIRE(memcmp(output + 5, "foobar", 6) == 0);
}

TEST_CASE_METHOD(ComplexFixture, "can edit complex", "[DynObject]") {
  uint8_t staticBuffer[8 * NUM_STATIC_PROPERTIES];

  ObjectIndex *index = indexTable.allocateObject(testType, 0, 0);

  DynObject obj(testType, streams, &indexTable, index, nullptr);
  obj.writeIndex(0, testStream->size(), true);
  DynObject nested = obj.get<DynObject>("nested");
  nested.set<int32_t>("num", 69);
  nested.set<std::string>("str", "foomore");

  std::shared_ptr<IOWrapper> result(IOWrapper::memoryBuffer());
  obj.saveTo(result);

  char output[20];
  result->read(output, 20);

  REQUIRE(*reinterpret_cast<int32_t*>(output) == 69);
  REQUIRE(memcmp(output + 5, "foomore", 7) == 0);
  // also ensure the length field was updated by the onAssign callback
  REQUIRE(output[4] == 7);
}

TEST_CASE_METHOD(ComplexFixture, "can edit array", "[DynObject]") {
  uint8_t staticBuffer[8 * NUM_STATIC_PROPERTIES];

  ObjectIndex *index = indexTable.allocateObject(testType, 0, 0);

  DynObject obj(testType, streams, &indexTable, index, nullptr);
  obj.writeIndex(0, testStream->size(), true);
  DynObject nested = obj.get<DynObject>("nested");
  std::vector<std::string> list = nested.getList<std::string>("lst");
  list[1] = "ddd";
  nested.setList("lst", list);

  std::shared_ptr<IOWrapper> result(IOWrapper::memoryBuffer());
  obj.saveTo(result);

  char output[30];
  memset(output, '\0', 30);
  result->read(output, 29);

  REQUIRE(memcmp(output + 11, "aaadddccc", 9) == 0);
}

