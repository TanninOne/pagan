#include <catch2/catch_test_macros.hpp>
#include <any>
#include <chrono>
#include "../pagan/expr.h"
#include "../pagan/IScriptQuery.h"

using namespace pagan;

class TestQuery : public IScriptQuery {
  std::any mValue;
  mutable int mSetCalls{ 0 };
  mutable int mGetCalls{ 0 };
  std::map<std::string, std::any> mAssignments;
public:
  TestQuery(std::any value)
    : mValue(value)
  {
  }

  int numGetCalls() const { return mGetCalls; }

  int numSetCalls() const { return mSetCalls; }

  bool wasAssigned(const char* key) const {
    return mAssignments.find(key) != mAssignments.end();
  }

  std::any getAny(const char *key) const override {
    ++mGetCalls;
    if (wasAssigned(key)) {
      auto iter = mAssignments.find(key);
      return iter->second;
    }
    return mValue;
  }

  std::any getAny(std::string_view key) const override {
    ++mGetCalls;
    auto iter = mAssignments.find(std::string(key));
    if (iter != mAssignments.end()) {
      return iter->second;
    }
    return mValue;
  }

  std::any getAny(const std::vector<std::string_view>::const_iterator& cur, const std::vector<std::string_view>::const_iterator& end) const override {
    ++mGetCalls;
    auto iter = mAssignments.find(std::string(*cur));
    if (iter != mAssignments.end()) {
      return iter->second;
    }
    return mValue;
  }

  void setAny(const std::vector<std::string_view>::const_iterator& cur, const std::vector<std::string_view>::const_iterator& end, const std::any& value) override {
    mAssignments[std::string(*cur)] = value;
    ++mSetCalls;
  }
};

TEST_CASE("can make getter func", "[expr]") {
  TestQuery query(std::any(2));
  auto yes = makeFunc<bool>("x == 2");
  auto no = makeFunc<bool>("x == 3");

  REQUIRE(yes(query) == true);
  REQUIRE(no(query) == false);
  REQUIRE(query.numGetCalls() == 2);
  REQUIRE(query.numSetCalls() == 0);
}

TEST_CASE("can make setter func", "[expr]") {
  TestQuery query(std::any(2));
  auto setter = makeFuncMutable<bool>("x = 2");
  REQUIRE(setter(query, std::any()) == true);
  REQUIRE(query.wasAssigned("x"));
  REQUIRE(query.numGetCalls() == 0);
  REQUIRE(query.numSetCalls() == 1);
}

long long now() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

/*
TEST_CASE("optimizes static numeric values", "[expr]") {
  static const int ITERATION_COUNT = 1000000;
  TestQuery query(std::any(2));
  auto opt = makeFunc<int>("42");
  auto unopt = makeFunc<int>("42 + 0");

  // TODO: how do we verify it optimizes?

  auto t0 = now();
  for (int i = 0; i < ITERATION_COUNT; ++i) {
    opt(query);
  }

  auto t1 = now();

  for (int i = 0; i < ITERATION_COUNT; ++i) {
    unopt(query);
  }

  auto t2 = now();

  // obviously it's not a proper unit test, looking at the runtime but this should be reliable enough, the optimized function
  // should be around 5x faster than the unoptimized one and expecting it to be at least twice as fast should rule out
  // random fluctuation
  // (When this optimization and test were introduced I used a slower expression library where the factor was around 100x so
  //  the use of this optimization was far more obvious)
  REQUIRE((t1 - t0) * 2 < (t2 - t1));
}
*/

TEST_CASE("language features", "[expr]") {
  TestQuery query(std::any(2));
  // math
  REQUIRE(makeFunc<int>("x + 3")(query) == 5);
  REQUIRE(makeFunc<int>("x * 3")(query) == 6);
  REQUIRE(makeFunc<int>("x / 2")(query) == 1);
  REQUIRE(makeFunc<int>("x % 1")(query) == 0);
  REQUIRE(makeFunc<int>("x * x")(query) == 4);
  REQUIRE(makeFunc<int>("x + (-3)")(query) == -1);

  REQUIRE(makeFunc<int>("(x + 4) / 2")(query) == 3);
  REQUIRE(makeFunc<int>("(x + 4) / 2")(query) == 3);

  // string
  REQUIRE(makeFunc<std::string>("\"foobar\"")(query) == "foobar");

  // bitwise
  REQUIRE(makeFunc<int>("x ^ 7")(query) == 5);
  REQUIRE(makeFunc<int>("0xFF & x")(query) == 2);
  REQUIRE(makeFunc<int>("x << 2")(query) == 8);
  REQUIRE(makeFunc<int>("x >> 1")(query) == 1);

  // boolean
  REQUIRE(makeFunc<bool>("1 || 0")(query) == true);
  REQUIRE(makeFunc<bool>("1 or 0")(query) == true);
  REQUIRE(makeFunc<bool>("1 && 0")(query) == false);
  REQUIRE(makeFunc<bool>("1 and 0")(query) == false);

  REQUIRE(makeFunc<bool>("!(2 == 4)")(query) == true);
  REQUIRE(makeFunc<bool>("!(x == 2)")(query) == false);
  REQUIRE(makeFunc<bool>("app_id != 0")(query) == true);
  REQUIRE(makeFunc<bool>("_.type != 0")(query) == true);

  // ternary
  REQUIRE(makeFunc<int>("(x == 2) ? 42 : 666")(query) == 42);
  REQUIRE(makeFunc<int>("(x == 2) ? (21 * 2) : (22 * 30 + 6)")(query) == 42);
  REQUIRE(makeFunc<int>("5 + ((x == 2) ? (21 * 2) : (22 * 30 + 6))")(query) == 47);
  // complex term from .rar
  REQUIRE(makeFunc<std::string>("(year <= 999 ? (\"0\" + (year <= 99 ? (\"0\" + (year <= 9 ? \"0\" : \"\")) : \"\")) : \"\") + year.to_s")(query) == "0002");
  // same term with line breaks
  REQUIRE(makeFunc<std::string>("(year <= 999 ? (\"0\" +\r\n  (year <= 99 ? (\"0\" +\r\n    (year <= 9 ? \"0\" : \"\")\r\n  ) : \"\")\r\n) : \"\") + year.to_s")(query) == "0002");

  // not implemented
  // REQUIRE(makeFunc<int>("24 * ((x == 2) ? 2 : 28) - 6")(query) == 42);
}

TEST_CASE("supports multi-line statements", "[expr]") {
  TestQuery query(std::any(2));
  REQUIRE(makeFunc<int>("2\n+\nx")(query) == 4);
  REQUIRE(makeFunc<int>("1 + (\n1 + x\n)")(query) == 4);
  REQUIRE(makeFunc<std::string>("(year <= 999 ? (\"0\" +\n\
    (year <= 99 ? (\"0\" +\n\
      (year <= 9 ? \"0\" : \"\")\n\
        ) : \"\")\n\
      ) : \"\") + year.to_s"));
}

TEST_CASE("setter can access block size", "[expr]") {
  TestQuery query(std::string("foobar"));

  auto func = makeFuncMutable<bool>("y = length(foobar)");

  REQUIRE(func(query, std::any()) == true);
  REQUIRE(flexi_cast<int>(query.getAny("y")) == 6);
}

