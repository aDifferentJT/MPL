#ifndef MPL_STATIC_FLEXIBLE_WIDTH_HPP
#define MPL_STATIC_FLEXIBLE_WIDTH_HPP

#include "wrapper_crtp.hpp"

#include "algorithms.hpp"
#include "stack_allocator.hpp"
#include "utility.hpp"

#include <array>
#include <cassert>

namespace mpl {
template <std::size_t N>
  requires(N % ull_bits == 0 && N > 0)
class static_flexible_width : public wrapper_crtp<static_flexible_width<N>> {
public:
  using container_t = std::array<ull, N / ull_bits>;
  container_t container;
  bool sign = false;

  constexpr static_flexible_width() {
    std::fill(container.begin(), container.end(), 0);
  }

  constexpr static_flexible_width(std::integral auto x) {
    auto it = container.begin();
    *(it++) = x;
    std::fill(it, container.end(), 0);
  }

  template <std::size_t N2>
  friend auto operator+(static_flexible_width const &lhs,
                        static_flexible_width<N2> const &rhs) {
    auto res = static_flexible_width<(std::max)(N, N2) + ull_bits>{};
    if (lhs.sign == rhs.sign) {
      res.sign = lhs.sign;
      auto carry = algorithms::impl::ripple_adder(lhs.container, rhs.container,
                                                  res.container);
      assert(!carry);
      return res;
    } else if (std::is_gteq(
                   algorithms::compare(lhs.container, rhs.container))) {
      res.sign = lhs.sign;
      auto carry = algorithms::impl::ripple_subber(lhs.container, rhs.container,
                                                   res.container);
      assert(!carry);
      return res;
    } else {
      res.sign = rhs.sign;
      auto carry = algorithms::impl::ripple_subber(rhs.container, lhs.container,
                                                   res.container);
      assert(!carry);
      return res;
    }
  }

  template <std::size_t N2>
  friend auto operator-(static_flexible_width const &lhs,
                        static_flexible_width<N2> const &rhs) {
    auto res = static_flexible_width<(std::max)(N, N2) + ull_bits>{};
    if (lhs.sign != rhs.sign) {
      res.sign = lhs.sign;
      auto carry = algorithms::impl::ripple_adder(lhs.container, rhs.container,
                                                  res.container);
      assert(!carry);
      return res;
    } else if (std::is_gteq(
                   algorithms::compare(lhs.container, rhs.container))) {
      res.sign = lhs.sign;
      auto carry = algorithms::impl::ripple_subber(lhs.container, rhs.container,
                                                   res.container);
      assert(!carry);
      return res;
    } else {
      res.sign = !rhs.sign;
      auto carry = algorithms::impl::ripple_subber(rhs.container, lhs.container,
                                                   res.container);
      assert(!carry);
      return res;
    }
  }

  void negate() { sign = !sign; }

  template <std::size_t N2>
  friend auto operator*(static_flexible_width const &lhs,
                        static_flexible_width<N2> const &rhs) {
    auto res = static_flexible_width<N + N2>{};
    res.sign = lhs.sign != rhs.sign;
    mpl::algorithms::impl::unsigned_mult(lhs.shrunk(), rhs.shrunk(),
                                         res.container);
    return res;
  }

  template <std::size_t N2>
  friend auto operator/(static_flexible_width lhs,
                        static_flexible_width<N2> rhs) {
    auto res = static_flexible_width<N + N2>{};
    res.sign = lhs.sign != rhs.sign;
    auto allocator = mpl::stack_allocator<2 * N + 36>{};
    mpl::algorithms::impl::unsigned_divide(
        lhs.container, std::move(rhs.container), res.container,
        allocator.template ref<ull>());
    return res;
  }

  template <std::size_t N2>
  friend auto gcd(static_flexible_width lhs, static_flexible_width<N2> rhs) {
    auto res = static_flexible_width<(std::min)(N, N2)>{};
    auto allocator = mpl::stack_allocator<2 * N + 36>{};
    mpl::algorithms::impl::positive_gcd(
        algorithms::impl::unsafe_trim_leading_zeros(lhs.container),
        algorithms::impl::unsafe_trim_leading_zeros(rhs.container),
        res.container, allocator.template ref<ull>());
    return res;
  }
};
} // namespace mpl

#endif // MPL_STATIC_FLEXIBLE_WIDTH_HPP
