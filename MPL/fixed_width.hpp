#ifndef MPL_FIXED_WIDTH_HPP
#define MPL_FIXED_WIDTH_HPP

#include "wrapper_crtp.hpp"

#include "algorithms.hpp"
#include "stack_allocator.hpp"
#include "utility.hpp"

#include <array>
#include <cassert>

namespace mpl {
template <std::size_t N>
  requires(N % ull_bits == 0 && N > 0)
class fixed_width : public wrapper_crtp<fixed_width<N>> {
public:
  using container_t = std::array<ull, N / ull_bits>;
  container_t container;

  constexpr fixed_width() { std::fill(container.begin(), container.end(), 0); }

  constexpr fixed_width(std::integral auto x) {
    auto it = container.begin();
    *(it++) = x;
    std::fill(it, container.end(), 0);
  }

  void operator+=(fixed_width const &rhs) {
    auto carry =
        algorithms::impl::ripple_adder(container, rhs.container, container);
    assert(!carry);
  }

  friend auto operator+(fixed_width const &lhs, fixed_width const &rhs) {
    auto res = fixed_width{};
    auto carry = algorithms::impl::ripple_adder(lhs.container, rhs.container,
                                                res.container);
    assert(!carry);
    return res;
  }

  void operator-=(fixed_width const &rhs) {
    auto carry =
        algorithms::impl::ripple_subber(container, rhs.container, container);
    assert(!carry);
  }

  friend auto operator-(fixed_width const &lhs, fixed_width const &rhs) {
    auto res = fixed_width{};
    auto carry = algorithms::impl::ripple_subber(lhs.container, rhs.container,
                                                 res.container);
    assert(!carry);
    return res;
  }

  void operator+=(long long const rhs) {
    auto carry = algorithms::impl::ripple_adder(
        container, std::array<ull, 1>{static_cast<ull>(rhs)}, container);
    assert(!carry);
  }

  friend auto operator+(fixed_width const &lhs, long long const rhs) {
    auto res = fixed_width{};
    auto carry = algorithms::impl::ripple_adder(
        lhs.container, std::array<ull, 1>{static_cast<ull>(rhs)},
        res.container);
    assert(!carry);
    return res;
  }

  void operator-=(long long const rhs) {
    auto carry = algorithms::impl::ripple_subber(
        container, std::array<ull, 1>{static_cast<ull>(rhs)}, container);
    assert(!carry);
  }

  friend auto operator-(fixed_width const &lhs, long long const rhs) {
    auto res = fixed_width{};
    auto carry = algorithms::impl::ripple_subber(
        lhs.container, std::array<ull, 1>{static_cast<ull>(rhs)},
        res.container);
    assert(!carry);
    return res;
  }

  void negate() {
    algorithms::bitwise_not(container);
    *this += 1;
  }

  friend auto operator>>(fixed_width const &lhs, int rhs) {
    assert(rhs >= 0);

    auto res = fixed_width{};

    mpl::algorithms::impl::small_logical_right_shift(
        mpl::algorithms::impl::view{lhs.container}.subview(
            rhs / ull_bits, lhs.container.size() - rhs / ull_bits),
        rhs % ull_bits, res.container);

    if (rhs % ull_bits != 0 && lhs.is_negative()) {
      res.container[lhs.container.size() - rhs / ull_bits] |=
          ~0ull << (ull_bits - rhs % ull_bits);
    }

    return res;
  }

  friend auto operator*(fixed_width const &lhs, fixed_width const &rhs) {
    // TODO handle negative
    auto res = fixed_width{};
    mpl::algorithms::impl::unsigned_mult(lhs.shrunk(), rhs.shrunk(),
                                         res.container);
    return res;
  }

  void operator*=(fixed_width const &rhs) { *this = std::move(*this) * rhs; }

  friend auto operator/(fixed_width lhs, fixed_width rhs) {
    // TODO handle negative
    auto allocator = mpl::stack_allocator<2 * N + 36>{};
    auto res = fixed_width{};
    mpl::algorithms::impl::unsigned_divide(
        lhs.container, std::move(rhs.container), res.container,
        allocator.template ref<ull>());
    return res;
  }

  void operator/=(fixed_width rhs) { *this = *this / std::move(rhs); }

  friend auto operator%(fixed_width lhs, fixed_width rhs) {
    lhs %= rhs;
    return lhs;
  }

  void operator%=(fixed_width rhs) {
    // TODO handle negative
    auto allocator = mpl::stack_allocator<2 * N + 36>{};
    auto quotient = fixed_width{};
    mpl::algorithms::impl::unsigned_divide(container, std::move(rhs.container),
                                           quotient.container,
                                           allocator.template ref<ull>());
  }

  friend auto div_mod(fixed_width lhs, fixed_width rhs)
      -> std::tuple<fixed_width, fixed_width> {
    // TODO handle negative
    auto allocator = mpl::stack_allocator<2 * N + 36>{};
    auto res = fixed_width{};
    mpl::algorithms::impl::unsigned_divide(
        lhs.container, std::move(rhs.container), res.container,
        allocator.template ref<ull>());
    return {res, lhs};
  }

  template <std::uniform_random_bit_generator Generator>
  static auto random_of_length_at_least(std::size_t const len,
                                        Generator &generator) -> fixed_width {
    static_assert(
        std::is_same_v<typename Generator::result_type, unsigned long long>);
    auto res = fixed_width{};
    std::generate(res.container.begin(), res.container.end(),
                  std::ref(generator));
    res.container.back() >>= 1;
    return res;
  }
};
} // namespace mpl

#endif // MPL_FIXED_WIDTH_HPP
