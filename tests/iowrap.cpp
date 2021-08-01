#include <catch.hpp>
#include "../pagan/iowrap.h"

TEST_CASE("can read&write in memory", "[iowrap]") {
  IOWrapper *wrap = IOWrapper::memoryBuffer();
  wrap->write("foonarf", 7);
  wrap->seekp(3);
  wrap->write("bar", 3);

  REQUIRE(wrap->tellp() == 6);

  wrap->seekg(0);
  char buffer[8];
  memset(buffer, 0, 8);
  wrap->read(buffer, 7);
  REQUIRE(memcmp(buffer, "foobarf", 7) == 0);
  wrap->seekg(3);
  REQUIRE(wrap->get() == 'b');
  REQUIRE(wrap->size() == 7);
}

// TODO test file streaming

