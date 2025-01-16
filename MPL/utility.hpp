#ifndef MPL_UTILITY_HPP
#define MPL_UTILITY_HPP

#include <climits>
#include <compare>
#include <exception>
#include <iostream>

namespace mpl {
using ull = unsigned long long;
constexpr auto ull_bits = sizeof(ull) * CHAR_BIT;

// In C++23 this will be just auto{x}
__attribute((noinline)) constexpr auto copy(auto &&x) {
  return std::forward<decltype(x)>(x);
}

namespace impl {
// Should be std
template <typename R> struct reverse_view {
  R &&base;

  auto begin() const -> decltype(auto) { return base.rbegin(); }
  auto end() const -> decltype(auto) { return base.rend(); }
  auto rbegin() const -> decltype(auto) { return base.begin(); }
  auto rend() const -> decltype(auto) { return base.end(); }
  auto cbegin() const -> decltype(auto) { return base.crbegin(); }
  auto cend() const -> decltype(auto) { return base.crend(); }
  auto crbegin() const -> decltype(auto) { return base.cbegin(); }
  auto crend() const -> decltype(auto) { return base.cend(); }
};
template <typename R> reverse_view(R &&) -> reverse_view<R>;

// Should be std
constexpr auto compare_strong_order_fallback(auto const &x, auto const &y)
    -> std::strong_ordering {
  if (x == y) {
    return std::strong_ordering::equal;
  } else if (x > y) {
    return std::strong_ordering::greater;
  } else if (x < y) {
    return std::strong_ordering::less;
  }
  std::cerr << "Incomparable values\n";
  std::terminate();
}
} // namespace impl
} // namespace mpl

#endif // MPL_UTILITY_HPP
