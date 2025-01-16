#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#include <gmpxx.h>
#pragma clang diagnostic pop

#include <cln/integer.h>
#include <cln/integer_io.h>
#include <cln/rational.h>
#include <cln/rational_io.h>

#include "../int_container.hpp"
#include "../int_container2.hpp"
#include "../int_container3.hpp"
#include "../rational.hpp"
#include "../wrapper.hpp"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <iterator>
#include <random>
#include <span>
#include <vector>

auto random_integer_string(std::size_t size) -> std::string {
  assert(size > 0);
  auto str = std::string{};
  str.reserve(size);
  auto device = std::random_device{};
  str.push_back(std::uniform_int_distribution{static_cast<int>('1'),
                                              static_cast<int>('9')}(device));
  for (auto i = 0; i < size - 1; i += 1) {
    str.push_back(std::uniform_int_distribution{static_cast<int>('0'),
                                                static_cast<int>('9')}(device));
  }
  return str;
}

auto random_rational_string(std::size_t size) -> std::string {
  assert(size > 0);
  return random_integer_string(size) + "/" + random_integer_string(size);
}

template <typename dyn_int>
auto sum_squares(std::vector<dyn_int> &xs) -> dyn_int {
  auto res = dyn_int{1};
  for (auto &x : xs) {
    dyn_int sq = x + x;
    res += sq;
  }
  return res;
}

template <typename dyn_int>
static void BM_sum_squares(benchmark::State &state, std::size_t size) {
  auto xs = std::vector<dyn_int>{};
  xs.reserve(100);
  std::generate_n(std::back_insert_iterator{xs}, 100, [&] {
    auto str = random_integer_string(size);
    if constexpr (requires { dyn_int{str}; }) {
      return dyn_int{str};
    } else if constexpr (requires { dyn_int{str.c_str()}; }) {
      return dyn_int{str.c_str()};
    }
  });

  for ([[maybe_unused]] auto _ : state) {
    benchmark::DoNotOptimize(xs);
    auto res = sum_squares(xs);
    benchmark::DoNotOptimize(res);
  }
}

template <typename dyn_rational>
static void BM_sum_rationals(benchmark::State &state, std::size_t size) {
  auto xs = std::vector<dyn_rational>{};
  xs.reserve(100);
  std::generate_n(std::back_insert_iterator{xs}, 5, [&] {
    auto str = random_rational_string(size);
    if constexpr (requires { dyn_rational{str}; }) {
      return dyn_rational{str};
    } else if constexpr (requires { dyn_rational{str.c_str()}; }) {
      return dyn_rational{str.c_str()};
    }
  });

  for ([[maybe_unused]] auto _ : state) {
    benchmark::DoNotOptimize(xs);
    auto res = sum_squares(xs);
    benchmark::DoNotOptimize(res);
  }
}

template <typename dyn_int>
auto dot_product(std::vector<dyn_int> &xs, std::vector<dyn_int> &ys) -> dyn_int {
  auto res = dyn_int{1};
  auto x = xs.begin();
  auto y = ys.begin();
  for (; x != xs.end() && y != ys.end(); ++x, ++y) {
    dyn_int prod = *x * *y;
    res += prod;
  }
  return res;
}

template <typename dyn_int>
static void BM_int_dot_product(benchmark::State &state, std::size_t size) {
  auto xs = std::vector<dyn_int>{};
  auto ys = std::vector<dyn_int>{};
  xs.reserve(100);
  ys.reserve(100);
  std::generate_n(std::back_insert_iterator{xs}, 100, [&] {
    auto str = random_integer_string(size);
    if constexpr (requires { dyn_int{str}; }) {
      return dyn_int{str};
    } else if constexpr (requires { dyn_int{str.c_str()}; }) {
      return dyn_int{str.c_str()};
    }
  });
  std::generate_n(std::back_insert_iterator{ys}, 100, [&] {
    auto str = random_integer_string(size);
    if constexpr (requires { dyn_int{str}; }) {
      return dyn_int{str};
    } else if constexpr (requires { dyn_int{str.c_str()}; }) {
      return dyn_int{str.c_str()};
    }
  });

  for ([[maybe_unused]] auto _ : state) {
    benchmark::DoNotOptimize(xs);
    benchmark::DoNotOptimize(ys);
    auto res = dot_product(xs, ys);
    benchmark::DoNotOptimize(res);
  }
}

template <typename dyn_rational>
static void BM_rational_dot_product(benchmark::State &state, std::size_t size) {
  auto xs = std::vector<dyn_rational>{};
  auto ys = std::vector<dyn_rational>{};
  xs.reserve(100);
  ys.reserve(100);
  std::generate_n(std::back_insert_iterator{xs}, 5, [&] {
    auto str = random_rational_string(size);
    if constexpr (requires { dyn_rational{str}; }) {
      return dyn_rational{str};
    } else if constexpr (requires { dyn_rational{str.c_str()}; }) {
      return dyn_rational{str.c_str()};
    }
  });
  std::generate_n(std::back_insert_iterator{ys}, 5, [&] {
    auto str = random_rational_string(size);
    if constexpr (requires { dyn_rational{str}; }) {
      return dyn_rational{str};
    } else if constexpr (requires { dyn_rational{str.c_str()}; }) {
      return dyn_rational{str.c_str()};
    }
  });

  for ([[maybe_unused]] auto _ : state) {
    benchmark::DoNotOptimize(xs);
    benchmark::DoNotOptimize(ys);
    auto res = dot_product(xs, ys);
    benchmark::DoNotOptimize(res);
  }
}

int main(int argc, char **argv) {
#ifndef NDEBUG
  std::cerr << "Asserts active\n";
#endif

  for (std::size_t size = 8; size <= 160; size += 8) {
    benchmark::RegisterBenchmark(
        ("Sum pairs integer MPL std::vector " + std::to_string(size)).c_str(),
        BM_sum_squares<mpl::wrapper<std::vector<mpl::ull>>>, size);
    /*
    benchmark::RegisterBenchmark(
        ("Sum pairs integer MPL int_container " + std::to_string(size)).c_str(),
        BM_sum_squares<mpl::wrapper<mpl::int_container>>, size);
        */
    benchmark::RegisterBenchmark(
        ("Sum pairs integer MPL int_container2 " + std::to_string(size)).c_str(),
        BM_sum_squares<mpl::wrapper<mpl::int_container2<6>>>, size);
    benchmark::RegisterBenchmark(
        ("Sum pairs integer MPL int_container3 " + std::to_string(size)).c_str(),
        BM_sum_squares<mpl::wrapper<mpl::int_container3<>>>, size);
    benchmark::RegisterBenchmark(
        ("Sum pairs integer GMP " + std::to_string(size)).c_str(),
        BM_sum_squares<mpz_class>, size);
    benchmark::RegisterBenchmark(
        ("Sum pairs integer CLN " + std::to_string(size)).c_str(),
        BM_sum_squares<cln::cl_I>, size);

    benchmark::RegisterBenchmark(
        ("Dot product integer MPL std::vector " + std::to_string(size)).c_str(),
        BM_int_dot_product<mpl::wrapper<std::vector<mpl::ull>>>, size);
    /*
    benchmark::RegisterBenchmark(
        ("Dot product integer MPL int_container " + std::to_string(size)).c_str(),
        BM_int_dot_product<mpl::wrapper<mpl::int_container>>, size);
        */
    benchmark::RegisterBenchmark(
        ("Dot product integer MPL int_container2 " + std::to_string(size)).c_str(),
        BM_int_dot_product<mpl::wrapper<mpl::int_container2<6>>>, size);
    benchmark::RegisterBenchmark(
        ("Dot product integer MPL int_container3 " + std::to_string(size)).c_str(),
        BM_int_dot_product<mpl::wrapper<mpl::int_container3<>>>, size);
    benchmark::RegisterBenchmark(
        ("Dot product integer GMP " + std::to_string(size)).c_str(),
        BM_int_dot_product<mpz_class>, size);
    benchmark::RegisterBenchmark(
        ("Dot product integer CLN " + std::to_string(size)).c_str(),
        BM_int_dot_product<cln::cl_I>, size);
  }

  for (std::size_t size = 2; size <= 50; size += 2) {
    benchmark::RegisterBenchmark(
        ("Sum pairs rational MPL std::vector " + std::to_string(size))
            .c_str(),
        BM_sum_rationals<mpl::rational<std::vector<mpl::ull>>>, size);
    /*
        benchmark::RegisterBenchmark(
            ("Sum pairs rational MPL int_container " +
       std::to_string(size)).c_str(),
            BM_sum_rationals<mpl::rational<mpl::int_container>>, size);
    */
    benchmark::RegisterBenchmark(
        ("Sum pairs rational MPL int_container2 " + std::to_string(size))
            .c_str(),
        BM_sum_rationals<mpl::rational<mpl::int_container2<6>>>, size);
    benchmark::RegisterBenchmark(
        ("Sum pairs rational MPL int_container3 " + std::to_string(size))
            .c_str(),
        BM_sum_rationals<mpl::rational<mpl::int_container3<>>>, size);
    benchmark::RegisterBenchmark(
        ("Sum pairs rational GMP " + std::to_string(size)).c_str(),
        BM_sum_rationals<mpq_class>, size);
    benchmark::RegisterBenchmark(
        ("Sum pairs rational CLN " + std::to_string(size)).c_str(),
        BM_sum_rationals<cln::cl_RA>, size);

    benchmark::RegisterBenchmark(
        ("Dot product rational MPL std::vector " + std::to_string(size))
            .c_str(),
        BM_rational_dot_product<mpl::rational<std::vector<mpl::ull>>>, size);
    /*
        benchmark::RegisterBenchmark(
            ("Dot product rational MPL int_container " +
       std::to_string(size)).c_str(),
            BM_rational_dot_product<mpl::rational<mpl::int_container>>, size);
    */
    benchmark::RegisterBenchmark(
        ("Dot product rational MPL int_container2 " + std::to_string(size))
            .c_str(),
        BM_rational_dot_product<mpl::rational<mpl::int_container2<6>>>, size);
    benchmark::RegisterBenchmark(
        ("Dot product rational MPL int_container3 " + std::to_string(size))
            .c_str(),
        BM_rational_dot_product<mpl::rational<mpl::int_container3<>>>, size);
    benchmark::RegisterBenchmark(
        ("Dot product rational GMP " + std::to_string(size)).c_str(),
        BM_rational_dot_product<mpq_class>, size);
    benchmark::RegisterBenchmark(
        ("Dot product rational CLN " + std::to_string(size)).c_str(),
        BM_rational_dot_product<cln::cl_RA>, size);
  }

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
}
