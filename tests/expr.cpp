#include <catch.hpp>
#include <any>
#include <chrono>
#include "../pagan/expr.h"
#include "../pagan/IScriptQuery.h"

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

  std::any getAny(char* key) const override {
    ++mGetCalls;
    if (wasAssigned(key)) {
      auto iter = mAssignments.find(key);
      return iter->second;
    }
    return mValue;
  }

  std::any getAny(std::string key) const override {
    ++mGetCalls;
    return mValue;
  }

  std::any getAny(const std::vector<std::string>::const_iterator& cur, const std::vector<std::string>::const_iterator& end) const override {
    ++mGetCalls;
    return mValue;
  }

  void setAny(const std::vector<std::string>::const_iterator& cur, const std::vector<std::string>::const_iterator& end, const std::any& value) override {
    mAssignments[*cur] = value;
    ++mSetCalls;
  }
};

TEST_CASE("can make getter func", "[expr]") {
  TestQuery query(std::any(2));
  auto yes = makeFunc<int>("x == 2");
  auto no = makeFunc<int>("x == 3");

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
  // should be around 100x faster than the unoptimized one and expecting it to be at least ten times as fast should rule out
  // random fluctuation
  REQUIRE((t1 - t0) * 10 < (t2 - t1));
}

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
  REQUIRE(makeFunc<bool>("1 && 0")(query) == false);

  REQUIRE(makeFunc<bool>("!(2 == 4)")(query) == true);
  REQUIRE(makeFunc<bool>("!(x == 2)")(query) == false);

  // ternary
  REQUIRE(makeFunc<int>("(x == 2) ? 42 : 666")(query) == 42);
  REQUIRE(makeFunc<int>("(x == 2) ? (21 * 2) : (22 * 30 + 6)")(query) == 42);

  // not implemented
  // REQUIRE(makeFunc<int>("24 * ((x == 2) ? 2 : 28) - 6")(query) == 42);
}

TEST_CASE("setter can access block size", "[expr]") {
  TestQuery query(std::string("foobar"));

  auto func = makeFuncMutable<bool>("y = length(foobar)");

  REQUIRE(func(query, std::any()) == true);
  REQUIRE(flexi_cast<int>(query.getAny("y")) == 6);
}

