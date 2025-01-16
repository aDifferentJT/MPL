#ifndef MPL_ALGORITHMS_COMPARE_HPP
#define MPL_ALGORITHMS_COMPARE_HPP

#include <compare>

#include "utility.hpp"

namespace mpl {
namespace algorithms {
// Should be std
[[nodiscard]] constexpr auto is_eq(auto x) noexcept -> bool { return x == 0; }
[[nodiscard]] constexpr auto is_neq(auto x) noexcept -> bool { return x != 0; }

namespace impl {
[[nodiscard]] constexpr auto unsigned_compare(auto const &lhs,
                                              auto const &rhs) noexcept
    -> std::strong_ordering {
  using namespace impl;

  // TODO it'd be nicer to work from msq to lsq

  auto lhs_it = lhs.begin();
  auto rhs_it = rhs.begin();

  auto res = std::strong_ordering::equal;

  for (; lhs_it != lhs.end() && rhs_it != rhs.end(); ++lhs_it, ++rhs_it) {
    auto quad_res = *lhs_it <=> *rhs_it;
    if (mpl::algorithms::is_neq(quad_res)) {
      res = quad_res;
    }
  }

  if (rhs_it == rhs.end()) {
    for (; lhs_it != lhs.end(); ++lhs_it) {
      if (*lhs_it != 0) {
        return std::strong_ordering::greater;
      }
    }
  } else if (lhs_it == lhs.end()) {
    for (; rhs_it != rhs.end(); ++rhs_it) {
      if (*rhs_it != 0) {
        return std::strong_ordering::less;
      }
    }
  }

  return res;
}

[[nodiscard]] constexpr auto negative_compare(auto const &lhs,
                                              auto const &rhs) noexcept
    -> std::strong_ordering {
  using namespace impl;

  // TODO it'd be nicer to work from msq to lsq

  auto lhs_it = lhs.begin();
  auto rhs_it = rhs.begin();

  auto res = std::strong_ordering::equal;

  for (; lhs_it != lhs.end() && rhs_it != rhs.end(); ++lhs_it, ++rhs_it) {
    auto quad_res = *lhs_it <=> *rhs_it;
    if (mpl::algorithms::is_neq(quad_res)) {
      res = quad_res;
    }
  }

  auto sign_bits = ~0ull;
  if (rhs_it == rhs.end()) {
    for (; lhs_it != lhs.end(); ++lhs_it) {
      if (*lhs_it != sign_bits) {
        res = std::strong_ordering::less;
      }
    }
  } else if (lhs_it == lhs.end()) {
    for (; rhs_it != rhs.end(); ++rhs_it) {
      if (*rhs_it != sign_bits) {
        res = std::strong_ordering::greater;
      }
    }
  }

  return res;
}
} // namespace impl

[[nodiscard]] constexpr auto compare(auto const &lhs, auto const &rhs) noexcept
    -> std::strong_ordering {
  using namespace impl;

  if (is_negative(lhs)) {
    if (is_negative(rhs)) {
      return negative_compare(lhs, rhs);
    } else {
      return std::strong_ordering::less;
    }
  } else {
    if (is_negative(rhs)) {
      return std::strong_ordering::greater;
    } else {
      return unsigned_compare(lhs, rhs);
    }
  }
}
} // namespace algorithms
} // namespace mpl

#endif // MPL_ALGORITHMS_COMPARE_HPP
