#ifndef MPL_ALGORITHMS_ADD_SUB_HPP
#define MPL_ALGORITHMS_ADD_SUB_HPP

#include "utility.hpp"

#include <array>
#include <cassert>
#include <optional>
#include <type_traits>

#include <sstream>
#include <string>

namespace mpl {
namespace algorithms {
namespace impl {
template <auto full_adder, bool sign_extend>
[[nodiscard]] auto ripple_add_or_sub(auto const &lhs, auto const &rhs,
                                     auto &dst) noexcept
    -> std::tuple<ull, ull, ull, ull> {
  assert(dst.size() >= std::max(lhs.size(), rhs.size()));

  auto carry = 0ull;

  auto lhs_it = lhs.begin();
  auto rhs_it = rhs.begin();
  auto dst_it = dst.begin();

  auto last_lhs = 0ull;
  auto last_rhs = 0ull;
  auto last_dst = 0ull;

  for (; lhs_it != lhs.end() && rhs_it != rhs.end();
       ++lhs_it, ++rhs_it, ++dst_it) {
    last_lhs = *lhs_it;
    last_rhs = *rhs_it;
    last_dst = full_adder(last_lhs, last_rhs, carry, &carry);
    *dst_it = last_dst;
  }

  if (lhs_it != lhs.end()) {
    if constexpr (sign_extend) {
      last_rhs = sar(last_rhs, ull_bits - 1);
    } else {
      last_rhs = 0;
    }
    for (; lhs_it != lhs.end(); ++lhs_it, ++dst_it) {
      last_lhs = *lhs_it;
      last_dst = full_adder(last_lhs, last_rhs, carry, &carry);
      *dst_it = last_dst;
    }
    if constexpr (sign_extend) {
      last_lhs = sar(last_lhs, ull_bits - 1);
    } else {
      last_lhs = 0;
    }
  } else if (rhs_it != rhs.end()) {
    if constexpr (sign_extend) {
      last_lhs = sar(last_lhs, ull_bits - 1);
    } else {
      last_lhs = 0;
    }
    for (; rhs_it != rhs.end(); ++rhs_it, ++dst_it) {
      last_rhs = *rhs_it;
      last_dst = full_adder(last_lhs, last_rhs, carry, &carry);
      *dst_it = last_dst;
    }
    if constexpr (sign_extend) {
      last_rhs = sar(last_rhs, ull_bits - 1);
    } else {
      last_rhs = 0;
    }
  }

  for (; dst_it != dst.end(); ++dst_it) {
    last_dst = full_adder(last_lhs, last_rhs, carry, &carry);
    *dst_it = last_dst;
  }

  return {last_lhs, last_rhs, last_dst, carry};
}

[[nodiscard]] constexpr auto detect_signed_carry(ull last_lhs, ull last_rhs,
                                                 ull last_dst, ull carry)
    -> std::optional<ull> {
  if ((last_lhs >> (ull_bits - 1)) ^ (last_rhs >> (ull_bits - 1)) ^
      (last_dst >> (ull_bits - 1)) ^ carry) {
    return sar(carry << (ull_bits - 1), ull_bits - 1);
  } else {
    return std::nullopt;
  }
}

[[nodiscard]] constexpr auto ripple_adder(auto const &lhs, auto const &rhs,
                                          not_const auto &&dst) noexcept
    -> std::optional<ull> {
  constexpr auto full_adder = [](ull lhs, ull rhs, ull cin, ull *cout) -> ull {
    return __builtin_addcll(lhs, rhs, cin, cout);
  };

  return std::apply(detect_signed_carry,
                    ripple_add_or_sub<full_adder, true>(lhs, rhs, dst));
}

// Will be member function from C++23
template <typename T>
[[nodiscard]] constexpr auto optional_transform(std::optional<T> x, auto &&f)
    -> std::optional<decltype(std::invoke(std::forward<decltype(f)>(f), *x))> {
  if (x) {
    return std::invoke(std::forward<decltype(f)>(f), *x);
  } else {
    return std::nullopt;
  }
}

[[nodiscard]] constexpr auto ripple_subber(auto const &lhs, auto const &rhs,
                                           not_const auto &&dst) noexcept
    -> std::optional<ull> {
  constexpr auto full_subber = [](ull lhs, ull rhs, ull cin, ull *cout) -> ull {
    return __builtin_subcll(lhs, rhs, cin, cout);
  };

  return optional_transform(
      std::apply(detect_signed_carry,
                 ripple_add_or_sub<full_subber, true>(lhs, rhs, dst)),
      [](ull carry) { return ~carry; });
}

[[nodiscard]] constexpr auto detect_unsigned_carry(
    [[maybe_unused]] ull last_lhs, [[maybe_unused]] ull last_rhs,
    [[maybe_unused]] ull last_dst, ull carry) -> std::optional<ull> {
  if (carry) {
    return carry;
  } else {
    return std::nullopt;
  }
}

[[nodiscard]] constexpr auto
unsigned_ripple_adder(auto const &lhs, auto const &rhs,
                      not_const auto &&dst) noexcept -> std::optional<ull> {
  constexpr auto full_adder = [](ull lhs, ull rhs, ull cin, ull *cout) -> ull {
    return __builtin_addcll(lhs, rhs, cin, cout);
  };

  return std::apply(detect_unsigned_carry,
                    ripple_add_or_sub<full_adder, false>(lhs, rhs, dst));
}

constexpr void unsigned_ripple_subber(auto const &lhs, auto const &rhs,
                                      not_const auto &&dst) noexcept {
  constexpr auto full_subber = [](ull lhs, ull rhs, ull cin, ull *cout) -> ull {
    return __builtin_subcll(lhs, rhs, cin, cout);
  };

  [[maybe_unused]] auto carry =
      std::apply(detect_unsigned_carry,
                 ripple_add_or_sub<full_subber, false>(lhs, rhs, dst));
  assert(!carry);
}
} // namespace impl

constexpr void add(auto const &lhs, auto const &rhs, not_const auto &&dst) {
  auto carry = impl::ripple_adder(lhs, rhs, dst);
  if (carry) {
    dst.push_back(*carry);
  }
}

constexpr void sub(auto const &lhs, auto const &rhs, not_const auto &&dst) {
  auto carry = impl::ripple_subber(lhs, rhs, dst);
  if (carry) {
    dst.push_back(*carry);
  }
}

constexpr void bitwise_not(not_const auto &&x) {
  for (auto &&quad : x) {
    quad = ~quad;
  }
}

constexpr void negate(not_const auto &&x) {
  bitwise_not(x);
  add(x, std::array<ull, 1>{1}, x);
}

constexpr void bitwise_and(auto const &lhs, auto const &rhs,
                           not_const auto &&dst) noexcept {
  constexpr auto kernel = [](ull lhs, ull rhs, [[maybe_unused]] ull cin,
                             [[maybe_unused]] ull *cout) -> ull {
    return lhs & rhs;
  };

  (void)impl::ripple_add_or_sub<kernel, true>(lhs, rhs, dst);
}

constexpr void bitwise_or(auto const &lhs, auto const &rhs,
                          not_const auto &&dst) noexcept {
  constexpr auto kernel = [](ull lhs, ull rhs, [[maybe_unused]] ull cin,
                             [[maybe_unused]] ull *cout) -> ull {
    return lhs | rhs;
  };

  (void)impl::ripple_add_or_sub<kernel, true>(lhs, rhs, dst);
}

constexpr void bitwise_xor(auto const &lhs, auto const &rhs,
                           not_const auto &&dst) noexcept {
  constexpr auto kernel = [](ull lhs, ull rhs, [[maybe_unused]] ull cin,
                             [[maybe_unused]] ull *cout) -> ull {
    return lhs ^ rhs;
  };

  (void)impl::ripple_add_or_sub<kernel, true>(lhs, rhs, dst);
}
} // namespace algorithms
} // namespace mpl

#endif // MPL_ALGORITHMS_ADD_SUB_HPP
