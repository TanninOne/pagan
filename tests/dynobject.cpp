#include <catch.hpp>
#include <numeric>
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

class FixtureWithParameters {
protected:
  std::shared_ptr<TypeRegistry> types;
  StreamRegistry streams;
  ObjectIndexTable indexTable;
  std::shared_ptr<TypeSpec> parentType;
  std::shared_ptr<TypeSpec> testType;
  std::shared_ptr<IOWrapper> testStream;

public:
  FixtureWithParameters()
    : types(TypeRegistry::init())
    , parentType(types->create("parent"))
    , testType(types->create("test"))
  {
    testType->appendParameter("in", TypeId::int32);
    parentType->appendProperty("num", TypeId::int32);
    parentType->appendProperty("test", testType->getId())
      .withArguments(std::vector<std::string> { "num" });

    testStream.reset(IOWrapper::memoryBuffer());

    std::vector<uint8_t> buffer { 0x2A, 0x00, 0x00, 0x00 };
    testStream->write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    streams.add(testStream);
  }
};

class FixtureWithRTPODArray {
protected:
  std::shared_ptr<TypeRegistry> types;
  StreamRegistry streams;
  ObjectIndexTable indexTable;
  std::shared_ptr<TypeSpec> listType;
  std::shared_ptr<IOWrapper> testStream;

public:
  FixtureWithRTPODArray()
    : types(TypeRegistry::init())
    , listType(types->create("list"))
  {
    // typedef std::function<std::variant<std::string, int32_t>(const IScriptQuery &object)> SwitchFunc;
    // TypePropertyBuilder &TypePropertyBuilder::withTypeSwitch(SwitchFunc func, const std::map<std::variant<std::string, int32_t>, uint32_t> &cases) {
    listType->appendProperty("list", TypeId::runtime)
      .withTypeSwitch([](const IScriptQuery& object) { return "_"; }, { { "_", TypeId::uint8 } })
      .withRepeatToEOS()
      ;

    testStream.reset(IOWrapper::memoryBuffer());

    std::vector<uint8_t> buffer { 0x01, 0x02, 0x03, 0x05, 0x08, 0x0D };
    testStream->write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    streams.add(testStream);
  }
};


class FixtureWithRTArray {
protected:
  std::shared_ptr<TypeRegistry> types;
  StreamRegistry streams;
  ObjectIndexTable indexTable;
  std::shared_ptr<TypeSpec> listType;
  std::shared_ptr<TypeSpec> itemType;
  std::shared_ptr<IOWrapper> testStream;

public:
  FixtureWithRTArray()
    : types(TypeRegistry::init())
    , listType(types->create("list"))
    , itemType(types->create("item"))
  {
    itemType->appendProperty("val", TypeId::int8);

    // typedef std::function<std::variant<std::string, int32_t>(const IScriptQuery &object)> SwitchFunc;
    // TypePropertyBuilder &TypePropertyBuilder::withTypeSwitch(SwitchFunc func, const std::map<std::variant<std::string, int32_t>, uint32_t> &cases) {
    listType->appendProperty("list", TypeId::runtime)
      .withTypeSwitch([](const IScriptQuery& object) { return "_"; }, { { "_", itemType->getId() } })
      .withRepeatToEOS()
      ;

    testStream.reset(IOWrapper::memoryBuffer());

    std::vector<uint8_t> buffer { 0x01, 0x02, 0x03, 0x05, 0x08, 0x0D };
    testStream->write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    streams.add(testStream);
  }
};


TEST_CASE_METHOD(SimpleFixture, "can create simple", "[DynObject]") {
  ObjectIndex *index = indexTable.allocateObject(testType, 0, 0);

  DynObject obj(testType, streams, &indexTable, index, nullptr);
  obj.writeIndex(0, testStream->size(), true);

  REQUIRE(obj.get<int32_t>("num") == 42);
}

TEST_CASE_METHOD(SimpleFixture, "can save simple", "[DynObject]") {
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
  ObjectIndex *index = indexTable.allocateObject(testType, 0, 0);

  DynObject obj(testType, streams, &indexTable, index, nullptr);
  REQUIRE_THROWS(obj.get<int32_t>("invalid"));
}

TEST_CASE_METHOD(ComplexFixture, "can save complex", "[DynObject]") {
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

TEST_CASE_METHOD(FixtureWithParameters, "can access parameters", "[DynObject]") {
  ObjectIndex *index = indexTable.allocateObject(parentType, 0, 0);

  DynObject parent(parentType, streams, &indexTable, index, nullptr);
  parent.writeIndex(0, testStream->size(), true);

  DynObject test = parent.get<DynObject>("test");

  REQUIRE(std::any_cast<int>(test.getAny("in")) == 42);
}

TEST_CASE_METHOD(FixtureWithRTArray, "correctly indexes eos sized array of runtime custom types", "[DynObject]") {
  ObjectIndex* index = indexTable.allocateObject(listType, 0, 0);

  DynObject list(listType, streams, &indexTable, index, nullptr);
  list.writeIndex(0, testStream->size(), true);

  std::vector<DynObject> items = list.getList<DynObject>("list");

  REQUIRE(items.size() == 6);
  REQUIRE(items[5].getTypeId() == itemType->getId());
  REQUIRE(items[5].get<int8_t>("val") == 13);
}

TEST_CASE_METHOD(FixtureWithRTPODArray, "correctly indexes eos sized array of runtime pod types", "[DynObject]") {
  ObjectIndex* index = indexTable.allocateObject(listType, 0, 0);

  DynObject list(listType, streams, &indexTable, index, nullptr);
  list.writeIndex(0, testStream->size(), true);

  std::vector<uint8_t> items = list.getList<uint8_t>("list");

  REQUIRE(items.size() == 6);
  REQUIRE(items[5] == 13);
}

