
#include <functional>
#include <iostream>
#include <random>
#include <ranges>
#include <sstream>
#include <string>

#include "dyn_int.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#include <gmpxx.h>
#pragma clang diagnostic pop

#include <cln/integer.h>
#include <cln/integer_io.h>

#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch2/catch.hpp>

using namespace std::literals;

auto to_string(auto x) -> std::string {
  auto ss = std::stringstream{};
  ss << x;
  return std::move(ss).str();
}

void test(auto &&f, std::string const & x, std::string const & y) {
  auto res_di = f(dyn_int{x, 16}, dyn_int{y, 16});
  auto res_gmp = mpz_class{f(mpz_class{x, 16}, mpz_class{y, 16})};
  auto res_cln = f(cln::read_integer (16, 0, x.data(), 0, x.size()), cln::read_integer (16, 0, y.data(), 0, y.size()));
  REQUIRE(res_di.to_string() == to_string(res_gmp));
  REQUIRE(res_di.to_string() == to_string(res_cln));
}

static auto gen = std::mt19937{std::random_device{}()};

auto random_hex_digit() -> char {
  auto dist = std::uniform_int_distribution(0, 15);
  auto ss = std::stringstream{};
  ss << std::hex;
  ss << dist(gen);
  return std::move(ss).str()[0];
}

auto random_hex_string(int size) {
  auto res = ""s;
  res.resize(size);
  std::generate(res.begin(), res.end(), random_hex_digit);
  return res;
}

void auto_test(auto &&f) {
  for (auto size = 1; size < 1024; size += 1) {
    for (auto i = 0; i < 10; i += 1) {
      test(f, random_hex_string(size).c_str(), random_hex_string(size).c_str());
    }
  }
}

TEST_CASE("plus", "[auto]") { auto_test(std::plus{}); }

TEST_CASE("1 + 1", "[compare]") { test(std::plus{}, "1", "2"); }

TEST_CASE("1 + 1 = 2", "[mpl]") { REQUIRE(1_di + 1_di == 2_di); }

TEST_CASE("-1 + -1 = -2", "[mpl]") { REQUIRE(-1_di + -1_di == -2_di); }

TEST_CASE("2 * 3 = 6", "[mpl]") { REQUIRE(2_di * 3_di == 6_di); }

TEST_CASE("7 / 3 = 2", "[mpl]") { REQUIRE(7_di / 3_di == 2_di); }

TEST_CASE("7 % 3 = 1", "[mpl]") { REQUIRE(7_di % 3_di == 1_di); }

TEST_CASE("-7 < 0", "[mpl]") { REQUIRE(-7_di < 0_di); }

TEST_CASE("-7 / 3 = -2", "[mpl]") {
  REQUIRE(-7 / 3 == -2);
  REQUIRE(-7_di / 3_di == -2_di);
}

TEST_CASE("-7 % 3 = -1", "[mpl]") {
  REQUIRE(-7 % 3 == -1);
  REQUIRE(-7_di % 3_di == -1_di);
}

/*
TEST_CASE("42 = 42", "[mpl]") {
    REQUIRE("42"_di == 42_di);
}
*/

/*
TEST_CASE("MPL perf", "[mpl_perf]") {
    BENCHMARK("MPL constructor") {
        return 42_di;
    };

    BENCHMARK("MPL 1+1") {
        return 1_di + 1_di;
    };

    BENCHMARK("MPL 2*3") {
        return 2_di * 3_di;
    };

//    BENCHMARK("MPL big constructor") {
//        return
"123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678"_di;
//    };

    BENCHMARK("MPL perf 1") {
        auto x = 42_di;
        //auto x = "123456781234567812345678123456781234567812345678"_di;
        //auto x =
"123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678"_di;
        auto y = 42_di;
        for (auto i = 0; i < 100; i += 1) {
            x += y;
            x += y;
        }
        return x;
    };

    BENCHMARK("MPL perf 2") {
        auto x = 42_di;
        auto y = 42_di;
        for (auto i = 0; i < 100; i += 1) {
            x += y + y;
        }
        return x;
    };
}
*/

/*
TEST_CASE("GMP perf", "[gmp_perf]") {
    BENCHMARK("GMP constructor") {
        return 42_mpz;
    };

    BENCHMARK("GMP 1+1") {
        return 1_mpz + 1_mpz;
    };

    BENCHMARK("GMP 2*3") {
        return 2_mpz * 3_mpz;
    };

//    BENCHMARK("GMP big constructor") {
//        return
mpz_class{"123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678",
16};
//    };

    BENCHMARK("GMP perf 1") {
        //auto x = mpz_class{"123456781234567812345678123456781234567812345678",
16}; auto x = 42_mpz; auto y = 42_mpz; for (auto i = 0; i < 100; i += 1) { x +=
y; x += y;
        }
        return x;
    };

    BENCHMARK("GMP perf 2") {
        //auto x = mpz_class{"123456781234567812345678123456781234567812345678",
16}; auto x = 42_mpz; auto y = 42_mpz; for (auto i = 0; i < 100; i += 1) { x +=
y + y;
        }
        return x;
    };
}
*/

/*
TEST_CASE("CLN perf", "[cln_perf]") {
    BENCHMARK("CLN constructor") {
        return cln::cl_I{42};
    };

    BENCHMARK("CLN 1+1") {
        return cln::cl_I{1} + cln::cl_I{1};
    };

    BENCHMARK("CLN 2*3") {
        return cln::cl_I{2} * cln::cl_I{3};
    };

    BENCHMARK("CLN perf 1") {
        auto x = cln::cl_I{42};
        auto y = cln::cl_I{42};
        for (auto i = 0; i < 100; i += 1) {
            x += y;
            x += y;
        }
        return x;
    };

    BENCHMARK("CLN perf 2") {
        auto x = cln::cl_I{42};
        auto y = cln::cl_I{42};
        for (auto i = 0; i < 100; i += 1) {
            x += y + y;
        }
        return x;
    };
}
*/
