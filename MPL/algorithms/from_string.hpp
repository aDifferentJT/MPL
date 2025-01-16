#ifndef MPL_ALGORITHMS_FROM_STRING_HPP
#define MPL_ALGORITHMS_FROM_STRING_HPP

#include "add_sub.hpp"
#include "mult.hpp"
#include "utility.hpp"

#include <array>
#include <cmath>
#include <string>

namespace mpl {
namespace algorithms {
namespace impl {
[[nodiscard]] constexpr auto char_to_int(char c) noexcept -> int {
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'A' && c <= 'Z') {
    return c - 'A' + 10;
  } else if (c >= 'a' && c <= 'z') {
    return c - 'a' + 10;
  } else {
    return 0;
  }
}
} // namespace impl

template <typename T>
[[nodiscard]] constexpr auto from_string(std::string_view str, int base = 10)
    -> T {
  using namespace impl;

  if (base <= 0) {
    if (str.starts_with("0x") || str.starts_with("0X")) {
      base = 16;
      str.remove_prefix(2);
    } else if (str.starts_with("0b") || str.starts_with("0b")) {
      base = 2;
      str.remove_prefix(2);
    } else if (str.starts_with("0")) {
      base = 8;
      str.remove_prefix(1);
    } else {
      base = 10;
    }
  }

  auto is_neg = [&] {
    if (str.front() == '-') {
      str.remove_prefix(1);
      return true;
    } else {
      return false;
    }
  }();

  auto res = T{};
  auto spare = T{};

  // auto length = str.size(); // TODO
  // auto length =
  //    static_cast<std::size_t>((str.size() * std::log2(base) + 1) / ull_bits)
  //    + 1;
  auto length =
      static_cast<std::size_t>(str.size() * std::log2(base) / ull_bits) + 2;
  res.resize(length);
  spare.resize(length);

  while (!str.empty()) {
    auto digit = char_to_int(str.front());
    str.remove_prefix(1);

    unsigned_mult(impl::unsafe_trim_leading_zeros(res), static_cast<ull>(base),
                  spare);
    [[maybe_unused]] auto carry =
        ripple_adder(spare, std::array<ull, 1>{static_cast<ull>(digit)}, res);
    assert(!carry);
  }

  if (is_neg) {
    negate(res);
  }

  if (trim_leading_sign_bits(res).size() < res.size()) {
    res.resize(trim_leading_sign_bits(res).size() + 1);
  }

  return res;
}
} // namespace algorithms
} // namespace mpl

#endif // MPL_ALGORITHMS_FROM_STRING_HPP
