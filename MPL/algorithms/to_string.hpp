#ifndef MPL_ALGORITHMS_TO_STRING_HPP
#define MPL_ALGORITHMS_TO_STRING_HPP

#include "divide_small.hpp"
#include "utility.hpp"

#include <array>
#include <string>
#include <vector>

namespace mpl {
namespace algorithms {
namespace impl {
[[nodiscard]] constexpr auto parse_digit(ull digit) -> char {
  if (digit >= 0 && digit <= 9) {
    return digit + '0';
  } else if (digit >= 10 && digit <= 36) {
    return digit - 10 + 'a';
  } else {
    return '?';
  }
}
} // namespace impl

[[nodiscard]] auto to_string(not_cvref auto &&x, int base = 10) -> std::string {
  using namespace impl;
  using namespace std::literals;

  auto prefix = [&] {
    switch (signum(x)) {
    case -1:
      negate(x);
      return "-"sv;
    case 0:
      return "0"sv;
    case 1:
      return ""sv;
    default:
      return ""sv;
    }
  }();

  auto str = std::string{};

  auto y = std::remove_cvref_t<decltype(x)>{};
  y.resize(x.size());
  //auto y = x;

  while (!is_zero(x)) {
    divide_small(x, base, y);

    str.push_back(parse_digit(x.front()));

    std::swap(x, y);
  }

  str.append(prefix);

  std::reverse(str.begin(), str.end());

  return str;
}
} // namespace algorithms
} // namespace mpl

#endif // MPL_ALGORITHMS_TO_STRING_HPP
