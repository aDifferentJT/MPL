
#include <chrono>
#include <iostream>
#include <ratio>
#include <string>
#include <string_view>

#include "dyn_int.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#include <gmpxx.h>
#pragma clang diagnostic pop

using namespace std::literals;

template <typename Clock = std::chrono::steady_clock>
auto time(auto&& f) -> typename Clock::duration {
  auto start = Clock::now();
  std::forward<decltype(f)>(f)();
  auto end = Clock::now();
  return end - start;
}

template <typename Clock = std::chrono::steady_clock>
auto test_at_size_mpl(size_t size) -> typename Clock::duration {
  auto str = std::string(size, '1');
  auto x = dyn_int{str, 16};
  auto y = dyn_int{str, 16};
  auto t = time([&] {
    for (auto i = 0; i < 100000; i += 1) {
      x += y + y;
    }
  });
  return t;
}

template <typename Clock = std::chrono::steady_clock>
auto test_at_size_gmp(size_t size) -> typename Clock::duration {
  auto str = std::string(size, '1');
  auto x = mpz_class{str, 16};
  auto y = mpz_class{str, 16};
  auto t = time([&] {
    for (auto i = 0; i < 100000; i += 1) {
      x += y + y;
    }
  });
  return t;
}

template <typename Period> constexpr auto suffix = " N/A "sv;
template <> constexpr auto suffix<std::atto> = "a"sv;
template <> constexpr auto suffix<std::femto> = "f"sv;
template <> constexpr auto suffix<std::pico> = "p"sv;
template <> constexpr auto suffix<std::nano> = "n"sv;
template <> constexpr auto suffix<std::micro> = "u"sv;
template <> constexpr auto suffix<std::milli> = "m"sv;
template <> constexpr auto suffix<std::centi> = "c"sv;
template <> constexpr auto suffix<std::deci> = "d"sv;
template <> constexpr auto suffix<std::ratio<1>> = ""sv;
template <> constexpr auto suffix<std::kilo> = "k"sv;
template <> constexpr auto suffix<std::mega> = "M"sv;
template <> constexpr auto suffix<std::giga> = "G"sv;
template <> constexpr auto suffix<std::tera> = "T"sv;
template <> constexpr auto suffix<std::peta> = "P"sv;
template <> constexpr auto suffix<std::exa> = "E"sv;

template <typename Rep, typename Period>
auto print_duration(std::chrono::duration<Rep, Period> t) {
  std::cout << t.count() << suffix<Period> << "s\n";
}

int main(int, char**) {
  std::cout << "MPL:\n";
  for (auto size = 4; size < 100; size += 4) {
    print_duration(test_at_size_mpl(size));
  }
  std::cout << "GMP:\n";
  for (auto size = 4; size < 100; size += 4) {
    print_duration(test_at_size_gmp(size));
  }
  std::cout << "MPL:\n";
  for (auto size = 4; size < 100; size += 4) {
    print_duration(test_at_size_mpl(size));
  }
  std::cout << "GMP:\n";
  for (auto size = 4; size < 100; size += 4) {
    print_duration(test_at_size_gmp(size));
  }
}

