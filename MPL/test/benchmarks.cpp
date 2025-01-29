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

template <typename dyn_int, dyn_int op(dyn_int const &, dyn_int const &)>
auto sum_squares(std::vector<dyn_int> &xs, std::vector<dyn_int> &ys)
    -> dyn_int {
  auto res = dyn_int{1};
  auto x = xs.begin();
  auto y = ys.begin();
  for (; x != xs.end() && y != ys.end(); ++x, ++y) {
    dyn_int sq = op(*x, *y);
    res += std::move(sq);
  }
  return res;
}

template <typename dyn_int, dyn_int op(dyn_int const &, dyn_int const &)>
static void BM_ints(benchmark::State &state, std::size_t size) {
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

  auto ys = std::vector<dyn_int>{};
  ys.reserve(100);
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
    auto res = sum_squares<dyn_int, op>(xs, ys);
    benchmark::DoNotOptimize(res);
  }
}

template <typename dyn_rational,
          dyn_rational op(dyn_rational const &, dyn_rational const &)>
static void BM_rationals(benchmark::State &state, std::size_t size) {
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
    auto res = sum_squares<dyn_rational, op>(xs, xs);
    benchmark::DoNotOptimize(res);
  }
}

template <typename dyn_int>
static void BM_int_add(benchmark::State &state, std::size_t size) {
  BM_ints<dyn_int, [](dyn_int const &x, dyn_int const &y) -> dyn_int {
    return x + y;
  }>(state, size);
}

template <typename dyn_int>
static void BM_int_mult(benchmark::State &state, std::size_t size) {
  BM_ints<dyn_int, [](dyn_int const &x, dyn_int const &y) -> dyn_int {
    return x * y;
  }>(state, size);
}

template <typename dyn_int>
static void BM_int_div(benchmark::State &state, std::size_t size) {
  BM_ints<dyn_int, [](dyn_int const &x, dyn_int const &y) -> dyn_int {
    return x / y;
  }>(state, size);
}

template <>
void BM_int_div<cln::cl_I>(benchmark::State &state, std::size_t size) {
  using dyn_int = cln::cl_I;
  BM_ints<dyn_int, [](dyn_int const &x, dyn_int const &y) -> dyn_int {
    return truncate1(x / y);
  }>(state, size);
}

template <typename dyn_rational>
static void BM_rational_add(benchmark::State &state, std::size_t size) {
  BM_rationals<dyn_rational,
               [](dyn_rational const &x,
                  dyn_rational const &y) -> dyn_rational { return x + y; }>(
      state, size);
}

template <typename dyn_rational>
static void BM_rational_mult(benchmark::State &state, std::size_t size) {
  BM_rationals<dyn_rational,
               [](dyn_rational const &x,
                  dyn_rational const &y) -> dyn_rational { return x * y; }>(
      state, size);
}

template <typename dyn_int, dyn_int op(dyn_int, dyn_int)>
auto sum_squares_copy(std::vector<dyn_int> &xs, std::vector<dyn_int> &ys)
    -> dyn_int {
  auto res = dyn_int{1};
  auto x = xs.begin();
  auto y = ys.begin();
  for (; x != xs.end() && y != ys.end(); ++x, ++y) {
    dyn_int sq = op(*x, *y);
    res += std::move(sq);
  }
  return res;
}

template <typename dyn_int, dyn_int op(dyn_int, dyn_int)>
static void BM_ints_copy(benchmark::State &state, std::size_t size) {
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

  auto ys = std::vector<dyn_int>{};
  ys.reserve(100);
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
    auto res = sum_squares_copy<dyn_int, op>(xs, ys);
    benchmark::DoNotOptimize(res);
  }
}

template <typename dyn_rational,
          dyn_rational op(dyn_rational, dyn_rational)>
static void BM_rationals_copy(benchmark::State &state, std::size_t size) {
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
    auto res = sum_squares_copy<dyn_rational, op>(xs, xs);
    benchmark::DoNotOptimize(res);
  }
}

template <typename dyn_int>
static void BM_int_add_copy(benchmark::State &state, std::size_t size) {
  BM_ints_copy<dyn_int, [](dyn_int x, dyn_int y) -> dyn_int {
    return x + y;
  }>(state, size);
}

template <typename dyn_int>
static void BM_int_mult_copy(benchmark::State &state, std::size_t size) {
  BM_ints_copy<dyn_int, [](dyn_int x, dyn_int y) -> dyn_int {
    return x * y;
  }>(state, size);
}

template <typename dyn_int>
static void BM_int_div_copy(benchmark::State &state, std::size_t size) {
  BM_ints_copy<dyn_int, [](dyn_int x, dyn_int y) -> dyn_int {
    return x / y;
  }>(state, size);
}

template <>
void BM_int_div_copy<cln::cl_I>(benchmark::State &state, std::size_t size) {
  using dyn_int = cln::cl_I;
  BM_ints_copy<dyn_int, [](dyn_int x, dyn_int y) -> dyn_int {
    return truncate1(x / y);
  }>(state, size);
}

template <typename dyn_rational>
static void BM_rational_add_copy(benchmark::State &state, std::size_t size) {
  BM_rationals_copy<dyn_rational,
               [](dyn_rational x,
                  dyn_rational y) -> dyn_rational { return x + y; }>(
      state, size);
}

template <typename dyn_rational>
static void BM_rational_mult_copy(benchmark::State &state, std::size_t size) {
  BM_rationals_copy<dyn_rational,
               [](dyn_rational x,
                  dyn_rational y) -> dyn_rational { return x * y; }>(
      state, size);
}

template <typename dyn_int>
auto dot_product(std::vector<dyn_int> &xs, std::vector<dyn_int> &ys)
    -> dyn_int {
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

template <typename dyn_int>
void register_benchmarks_for_int_of_size(std::string type, std::size_t size) {
  benchmark::RegisterBenchmark(
      ("Sum pairs integer " + type + " " + std::to_string(size)).c_str(),
      BM_int_add<dyn_int>, size);

  benchmark::RegisterBenchmark(
      ("Sum mult integer " + type + " " + std::to_string(size)).c_str(),
      BM_int_mult<dyn_int>, size);

  benchmark::RegisterBenchmark(
      ("Sum ratio integer " + type + " " + std::to_string(size)).c_str(),
      BM_int_div<dyn_int>, size);

  benchmark::RegisterBenchmark(
      ("Sum pairs integer copy " + type + " " + std::to_string(size)).c_str(),
      BM_int_add_copy<dyn_int>, size);

  benchmark::RegisterBenchmark(
      ("Sum mult integer copy " + type + " " + std::to_string(size)).c_str(),
      BM_int_mult_copy<dyn_int>, size);

  benchmark::RegisterBenchmark(
      ("Sum ratio integer copy " + type + " " + std::to_string(size)).c_str(),
      BM_int_div_copy<dyn_int>, size);

  /*
  benchmark::RegisterBenchmark(
      ("Dot product integer " + type + " " + std::to_string(size)).c_str(),
      BM_int_dot_product<dyn_int>, size);
      */
}

void register_benchmarks_for_int_of_size(std::size_t size) {
  register_benchmarks_for_int_of_size<mpl::wrapper<std::vector<mpl::ull>>>(
      "MPL std::vector", size);
  // register_benchmarks_for_int_of_size<mpl::wrapper<mpl::int_container>>("MPL
  // int_container", size);
  register_benchmarks_for_int_of_size<mpl::wrapper<mpl::int_container2<6>>>(
      "MPL int_container2", size);
  register_benchmarks_for_int_of_size<mpl::wrapper<mpl::int_container3<>>>(
      "MPL int_container3", size);
  register_benchmarks_for_int_of_size<mpz_class>("GMP", size);
  register_benchmarks_for_int_of_size<cln::cl_I>("CLN", size);
}

template <typename dyn_rational>
void register_benchmarks_for_rational_of_size(std::string type,
                                              std::size_t size) {
  benchmark::RegisterBenchmark(
      ("Sum pairs rational " + type + " " + std::to_string(size)).c_str(),
      BM_rational_add<dyn_rational>, size);

  benchmark::RegisterBenchmark(
      ("Sum mult rational " + type + " " + std::to_string(size)).c_str(),
      BM_rational_mult<dyn_rational>, size);

  benchmark::RegisterBenchmark(
      ("Sum pairs rational copy " + type + " " + std::to_string(size)).c_str(),
      BM_rational_add_copy<dyn_rational>, size);

  benchmark::RegisterBenchmark(
      ("Sum mult rational copy " + type + " " + std::to_string(size)).c_str(),
      BM_rational_mult_copy<dyn_rational>, size);
}

void register_benchmarks_for_rational_of_size(std::size_t size) {
  register_benchmarks_for_rational_of_size<
      mpl::rational<std::vector<mpl::ull>>>("MPL std::vector", size);
  // register_benchmarks_for_rational_of_size<mpl::rational<mpl::int_container>>("MPL
  // int_container", size);
  register_benchmarks_for_rational_of_size<
      mpl::rational<mpl::int_container2<6>>>("MPL int_container2", size);
  register_benchmarks_for_rational_of_size<
      mpl::rational<mpl::int_container3<>>>("MPL int_container3", size);
  register_benchmarks_for_rational_of_size<mpq_class>("GMP", size);
  register_benchmarks_for_rational_of_size<cln::cl_RA>("CLN", size);
}

int main(int argc, char **argv) {
#ifndef NDEBUG
  std::cerr << "Asserts active\n";
#endif

  for (std::size_t size = 8; size <= 160; size += 8) {
    register_benchmarks_for_int_of_size(size);
  }

  for (std::size_t size = 2; size <= 50; size += 2) {
    register_benchmarks_for_rational_of_size(size);
  }

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
}
