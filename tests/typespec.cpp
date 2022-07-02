#include <catch.hpp>
#include "../pagan/TypeRegistry.h"
#include "../pagan/TypeSpec.h"
#include "../pagan/DynObject.h"

class SimpleFixture {
protected:
  std::shared_ptr<TypeRegistry> registry;
public:
  SimpleFixture()
    : registry(TypeRegistry::init())
  {
  }
};

TEST_CASE_METHOD(SimpleFixture, "can create spec", "[typespec]") {
  auto spec = registry->create("test1");

  REQUIRE(spec->getName() == "test1");

  REQUIRE(spec->getNumProperties() == 0);
  REQUIRE(spec->getStaticSize() == 0);
}

TEST_CASE_METHOD(SimpleFixture, "can add properties", "[typespec]") {
  auto spec = registry->create("test2");

  REQUIRE_NOTHROW([&]() {
    spec->appendProperty("int32", TypeId::int32);
    spec->appendProperty("bytes", TypeId::bytes);
    spec->appendProperty("float32", TypeId::float32);
    spec->appendProperty("string", TypeId::string);
  }());

  REQUIRE(spec->getNumProperties() == 4);
  // 4 each for int32 and float32, 8 each for string and bytes
  REQUIRE(spec->getStaticSize() == 24);
}

TEST_CASE_METHOD(SimpleFixture, "can add parameter", "[typespec]") {
  auto spec = registry->create("params_test_1");

  REQUIRE_NOTHROW([&]() {
    spec->appendParameter("int32", TypeId::int32);
    spec->appendParameter("string", TypeId::string);
  }());

  REQUIRE(spec->getNumParameters() == 2);
}

TEST_CASE_METHOD(SimpleFixture, "can add computed property", "[typespec]") {
  auto spec = registry->create("test3");

  spec->appendProperty("prop1", TypeId::int32);

  ComputeFunc compute = [](const IScriptQuery &obj)->std::any {
    return std::any_cast<int32_t>(obj.getAny("prop1")) * 2;
  };

  REQUIRE_NOTHROW([&]() {
    spec->addComputed("computed", compute);
  }());

  // computed property does not count to num properties
  REQUIRE(spec->getNumProperties() == 1);
  REQUIRE(spec->getStaticSize() == 4);
}

TEST_CASE_METHOD(SimpleFixture, "can add prop with arguments", "[typespec]") {
  auto specInner = registry->create("parames_test_2_inner");

  specInner->appendParameter("len", TypeId::int32);
  specInner->appendProperty("str", TypeId::string)
    .withSize([](const IScriptQuery& obj) -> ObjSize { return flexi_cast<ObjSize>(obj.getAny("len")); });

  auto spec = registry->create("parames_test_2");
  spec->appendProperty("prop1", TypeId::int32);
  spec->appendProperty("prop2", specInner->getId())
    .withArguments({ "prop1" });
}

TEST_CASE_METHOD(SimpleFixture, "spec has sensible defaults", "[typespec]") {
  auto spec = registry->create("test4");
  spec->appendProperty("prop1", TypeId::int32);

  const TypeProperty &prop = spec->getProperty("prop1");
  REQUIRE(prop.typeId == TypeId::int32);
  REQUIRE(prop.isConditional == false);
  REQUIRE(prop.hasSizeFunc == false);
  REQUIRE(prop.isList == false);
  REQUIRE(prop.isSwitch == false);
  REQUIRE(prop.isValidated == false);
  REQUIRE(prop.key == "prop1");
}
