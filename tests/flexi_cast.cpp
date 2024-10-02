#include <catch2/catch_test_macros.hpp>
#include "../pagan/flexi_cast.h"

using namespace pagan;

TEST_CASE( "can cast to correct type", "[flexi_cast]" ) {
  uint8_t u8 = 42;
  uint16_t u16 = 42;
  uint32_t u32 = 42;
  uint64_t u64 = 42;
  int8_t s8 = 42;
  int16_t s16 = 42;
  int32_t s32 = 42;
  int64_t s64 = 42;
  float f = 42.1f;
  std::string str = "narf";

  REQUIRE(flexi_cast<uint8_t>(std::any(u8)) == u8);
  REQUIRE(flexi_cast<uint16_t>(std::any(u16)) == u16);
  REQUIRE(flexi_cast<uint32_t>(std::any(u32)) == u32);
  REQUIRE(flexi_cast<uint64_t>(std::any(u64)) == u64);
  REQUIRE(flexi_cast<int8_t>(std::any(s8)) == s8);
  REQUIRE(flexi_cast<int16_t>(std::any(s16)) == s16);
  REQUIRE(flexi_cast<int32_t>(std::any(s32)) == s32);
  REQUIRE(flexi_cast<int64_t>(std::any(s64)) == s64);
  REQUIRE(flexi_cast<float>(std::any(f)) == f);
  REQUIRE(flexi_cast<std::string>(std::any(str)) == str);
}

TEST_CASE( "can cast to other integral type", "[flexi_cast]" ) {
  uint8_t u8 = 42;
  uint16_t u16 = 42;
  uint32_t u32 = 42;
  uint64_t u64 = 42;
  int8_t s8 = 42;
  int16_t s16 = 42;
  int32_t s32 = 42;
  int64_t s64 = 42;

  REQUIRE(flexi_cast<uint32_t>(std::any(u8)) == 42u);
  REQUIRE(flexi_cast<uint32_t>(std::any(u32)) == 42u);
  REQUIRE(flexi_cast<uint32_t>(std::any(u16)) == 42u);
  REQUIRE(flexi_cast<uint32_t>(std::any(u64)) == 42u);
  REQUIRE(flexi_cast<int32_t>(std::any(s8)) == 42);
  REQUIRE(flexi_cast<int32_t>(std::any(s16)) == 42);
  REQUIRE(flexi_cast<int32_t>(std::any(s32)) == 42);
  REQUIRE(flexi_cast<int32_t>(std::any(s64)) == 42);
}

TEST_CASE("can cast integral type to string", "[flexi_cast]") {
  REQUIRE(flexi_cast<std::string>(std::any(42)) == "42");
  REQUIRE(flexi_cast<std::string>(std::any(42ull)) == "42");
}

