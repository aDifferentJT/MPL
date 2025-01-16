#ifndef MPL_ALGORITHMS_SHIFT_HPP
#define MPL_ALGORITHMS_SHIFT_HPP

#include "utility.hpp"

#include <cassert>

namespace mpl {
namespace algorithms {
namespace impl {
// A left shift of positive integers by less than ull_bits bits
constexpr auto unsafe_small_positive_left_shift(auto const &lhs, int rhs,
                                                not_const auto &&dst) noexcept
    -> ull {
  assert(rhs >= 0);
  assert(rhs < static_cast<int>(ull_bits));

  auto full_shifter = [rhs](ull x, ull cin, ull &cout) -> ull {
    cout = rhs == 0 ? 0 : x >> (ull_bits - rhs);
    x <<= rhs;
    x |= cin;
    return x;
  };

  auto carry = 0ull;
  auto lhs_it = lhs.begin();
  auto dst_it = dst.begin();
  for (; lhs_it != lhs.end(); ++lhs_it, ++dst_it) {
    *dst_it = full_shifter(*lhs_it, carry, carry);
  }

  return carry;
}

constexpr auto small_left_shift(auto const &lhs, int rhs,
                                not_const auto &&dst) noexcept -> ull {
  auto carry = unsafe_small_positive_left_shift(lhs, rhs, dst);
  if (is_negative(lhs)) {
    carry |= ~0ull << rhs;
  }
  return carry;
}

constexpr auto small_logical_right_shift(auto const &lhs, int rhs,
                                         not_const auto &&dst) noexcept -> ull {
  assert(rhs >= 0);
  assert(rhs < static_cast<int>(ull_bits));
  assert(dst.size() >= lhs.size());

  auto full_shifter = [rhs](ull x, ull cin, ull &cout) -> ull {
    cout = rhs == 0 ? 0 : x << (ull_bits - rhs);
    x >>= rhs;
    x |= cin;
    return x;
  };

  auto carry = 0ull;
  auto lhs_it = lhs.rbegin();
  auto dst_it = std::reverse_iterator{dst.begin() + lhs.size()};
  for (; lhs_it != lhs.rend(); ++lhs_it, ++dst_it) {
    *dst_it = full_shifter(*lhs_it, carry, carry);
  }

  return carry;
}
} // namespace impl
} // namespace algorithms
} // namespace mpl

#endif // MPL_ALGORITHMS_SHIFT_HPP
