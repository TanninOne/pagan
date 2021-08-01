#include <catch.hpp>
#include "../pagan/TypeRegistry.h"
#include "../pagan/TypeSpec.h"


TEST_CASE("can create registry", "[typeregistry]") {
  std::shared_ptr<TypeRegistry> registry = TypeRegistry::init();

  REQUIRE_NOTHROW([&]() { auto spec = registry->create("__test"); }());
}

TEST_CASE("can get spec by name", "[typeregistry]") {
  std::shared_ptr<TypeRegistry> registry = TypeRegistry::init();
  auto spec = registry->create("__test");

  auto id = spec->getId();
  REQUIRE(registry->getByName("__test") != nullptr);
  REQUIRE(id == registry->getByName("__test")->getId());
}

TEST_CASE("can get spec by id", "[typeregistry]") {
  std::shared_ptr<TypeRegistry> registry = TypeRegistry::init();
  auto spec = registry->create("__test");

  auto id = spec->getId();

  REQUIRE(id == registry->numTypes() - 1);
  REQUIRE(registry->getById(id) != nullptr);
  REQUIRE(id == registry->getById(id)->getId());
}

