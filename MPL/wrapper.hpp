#ifndef MPL_WRAPPER_HPP
#define MPL_WRAPPER_HPP

#include "wrapper_crtp.hpp"

#include "algorithms.hpp"
#include "utility.hpp"

#include <concepts>
#include <numeric>
#include <optional>

namespace mpl {
template <typename Container>
class wrapper : public wrapper_crtp<wrapper<Container>> {
public:
  Container container;

private:
  void unconditional_resize(std::size_t size) {
    container.resize(
        size, container.size() == 0
                  ? 0
                  : mpl::algorithms::sar(container.back(), ull_bits - 1));
  }

  void grow_to(std::size_t size) {
    if (container.size() < size) {
      unconditional_resize(size);
    }
  }

public:
  wrapper() = default;
  wrapper(wrapper const &) = default;
  wrapper &operator=(wrapper const &) = default;
  wrapper(wrapper &&) = default;
  wrapper &operator=(wrapper &&) = default;
  wrapper(std::integral auto x)
      : container{mpl::algorithms::from_int<Container>(x)} {}

  template <typename It> wrapper(algorithms::impl::view<It> that) {
    unconditional_resize(that.size());
    std::copy(that.begin(), that.end(), container.begin());
  }

  wrapper(std::string_view x, int base = 10)
      : container{mpl::algorithms::from_string<Container>(x, base)} {}

  void negate() { algorithms::negate(container); }

  void operator+=(wrapper const &rhs) {
    grow_to(rhs.container.size());
    mpl::algorithms::add(container, rhs.container, container);
  }

  friend auto operator+(wrapper const &lhs, wrapper const &rhs) {
    auto res = wrapper{};
    res.container.resize(std::max(lhs.container.size(), rhs.container.size()));
    mpl::algorithms::add(lhs.container, rhs.container, res.container);
    return res;
  }

  void operator-=(wrapper const &rhs) {
    grow_to(rhs.container.size());
    mpl::algorithms::sub(container, rhs.container, container);
  }

  friend auto operator-(wrapper const &lhs, wrapper const &rhs) {
    auto res = wrapper{};
    res.container.resize(std::max(lhs.container.size(), rhs.container.size()));
    mpl::algorithms::sub(lhs.container, rhs.container, res.container);
    return res;
  }

  friend auto operator*(wrapper const &lhs, wrapper const &rhs) {
    if (lhs.is_negative() || rhs.is_negative()) {
      auto lhs2 = lhs;
      auto rhs2 = rhs;
      auto lhs3 = lhs2.shrunk();
      auto rhs3 = rhs2.shrunk();
      auto res = wrapper<Container>{};
      res.container.resize(lhs3.size() + rhs3.size() + 1);
      mpl::algorithms::mult(lhs2.container, rhs2.container, res.container);
      return res;
    } else {
      auto lhs2 = lhs.shrunk();
      auto rhs2 = rhs.shrunk();
      auto res = wrapper<Container>{};
      res.container.resize(lhs2.size() + rhs2.size() + 1);
      mpl::algorithms::impl::unsigned_mult(lhs2, rhs2, res.container);
      return res;
    }
  }

  friend auto operator*(wrapper &lhs, wrapper const &rhs) {
    if (lhs.is_negative() || rhs.is_negative()) {
      auto rhs2 = rhs;
      auto lhs3 = lhs.shrunk();
      auto rhs3 = rhs2.shrunk();
      auto res = wrapper<Container>{};
      res.container.resize(lhs3.size() + rhs3.size() + 1);
      mpl::algorithms::mult(lhs.container, rhs2.container, res.container);
      return res;
    } else {
      auto lhs2 = lhs.shrunk();
      auto rhs2 = rhs.shrunk();
      auto res = wrapper<Container>{};
      res.container.resize(lhs2.size() + rhs2.size() + 1);
      mpl::algorithms::impl::unsigned_mult(lhs2, rhs2, res.container);
      return res;
    }
  }

  friend auto operator*(wrapper const &lhs, wrapper &rhs) {
    if (lhs.is_negative() || rhs.is_negative()) {
      auto lhs2 = lhs;
      auto lhs3 = lhs2.shrunk();
      auto rhs3 = rhs.shrunk();
      auto res = wrapper<Container>{};
      res.container.resize(lhs3.size() + rhs3.size() + 1);
      mpl::algorithms::mult(lhs2.container, rhs.container, res.container);
      return res;
    } else {
      auto lhs2 = lhs.shrunk();
      auto rhs2 = rhs.shrunk();
      auto res = wrapper<Container>{};
      res.container.resize(lhs2.size() + rhs2.size() + 1);
      mpl::algorithms::impl::unsigned_mult(lhs2, rhs2, res.container);
      return res;
    }
  }

  friend auto operator*(wrapper &lhs, wrapper &rhs) {
    if (lhs.is_negative() || rhs.is_negative()) {
      auto lhs2 = lhs.shrunk();
      auto rhs2 = rhs.shrunk();
      auto res = wrapper<Container>{};
      res.container.resize(lhs2.size() + rhs2.size() + 1);
      mpl::algorithms::mult(lhs.container, rhs.container, res.container);
      return res;
    } else {
      auto lhs2 = lhs.shrunk();
      auto rhs2 = rhs.shrunk();
      auto res = wrapper<Container>{};
      res.container.resize(lhs2.size() + rhs2.size() + 1);
      mpl::algorithms::impl::unsigned_mult(lhs2, rhs2, res.container);
      return res;
    }
  }

  friend auto operator*(wrapper &&lhs, wrapper const &rhs) { return lhs * rhs; }

  friend auto operator*(wrapper &&lhs, wrapper &rhs) { return lhs * rhs; }

  friend auto operator*(wrapper const &lhs, wrapper &&rhs) { return lhs * rhs; }

  friend auto operator*(wrapper &lhs, wrapper &&rhs) { return lhs * rhs; }

  friend auto operator*(wrapper &&lhs, wrapper &&rhs) { return lhs * rhs; }

  void operator*=(wrapper const &rhs) { *this = std::move(*this) * rhs; }
  void operator*=(wrapper &&rhs) { *this = std::move(*this) * std::move(rhs); }

  friend auto operator/(wrapper lhs, wrapper rhs) {
    auto res = wrapper{};
    res.grow_to(lhs.container.size());
    mpl::algorithms::divide(lhs.container, std::move(rhs.container),
                            res.container);
    return res;
  }

  void operator/=(wrapper rhs) { *this = *this / std::move(rhs); }

  friend auto operator/(wrapper lhs, ull rhs) {
    auto quotient = wrapper{};
    quotient.grow_to(lhs.container.size());
    auto rem = mpl::algorithms::divide(std::move(lhs.container), rhs,
                                       quotient.container);
    return quotient;
  }

  void operator/=(ull rhs) { *this = *this / rhs; }

  friend auto operator%(wrapper lhs, wrapper rhs) {
    lhs %= rhs;
    return lhs;
  }

  void operator%=(wrapper rhs) {
    auto quotient = wrapper{};
    quotient.grow_to(container.size());
    mpl::algorithms::divide(container, std::move(rhs.container),
                            quotient.container);
  }

  friend auto operator%(wrapper lhs, ull rhs) -> ull {
    auto quotient = wrapper{};
    quotient.grow_to(lhs.container.size());
    auto rem = mpl::algorithms::divide(std::move(lhs.container), rhs,
                                       quotient.container);
    return rem;
  }

  void operator%=(ull rhs) { *this = std::move(*this) % rhs; }

  friend auto div_mod(wrapper lhs, wrapper rhs)
      -> std::tuple<wrapper, wrapper> {
    auto res = wrapper{};
    res.grow_to(lhs.container.size());
    mpl::algorithms::divide(lhs.container, std::move(rhs.container),
                            res.container);
    return {res, lhs};
  }

  auto operator~() && {
    algorithms::bitwise_not(container);
    return std::move(*this);
  }

  auto operator~() const & { return ~copy(*this); }

  friend auto operator&(wrapper const &lhs, wrapper const &rhs) {
    auto res = wrapper{};
    res.container.resize(std::max(lhs.container.size(), rhs.container.size()));
    mpl::algorithms::bitwise_and(lhs.container, rhs.container, res.container);
    return res;
  }

  friend auto operator|(wrapper const &lhs, wrapper const &rhs) {
    auto res = wrapper{};
    res.container.resize(std::max(lhs.container.size(), rhs.container.size()));
    mpl::algorithms::bitwise_or(lhs.container, rhs.container, res.container);
    return res;
  }

  friend auto operator^(wrapper const &lhs, wrapper const &rhs) {
    auto res = wrapper{};
    res.container.resize(std::max(lhs.container.size(), rhs.container.size()));
    mpl::algorithms::bitwise_xor(lhs.container, rhs.container, res.container);
    return res;
  }

  friend auto operator<<(wrapper const &lhs, int rhs) {
    assert(rhs >= 0);

    auto res = wrapper{};
    res.grow_to(lhs.container.size() + ((rhs + ull_bits - 1) / ull_bits));

    auto carry = mpl::algorithms::impl::small_left_shift(
        lhs.container, rhs % ull_bits,
        mpl::algorithms::impl::view{res.container}.subview(
            rhs / ull_bits, lhs.container.size()));

    if (rhs % ull_bits != 0) {
      res.container[lhs.container.size() + rhs / ull_bits] = carry;
    }

    return res;
  }

  friend auto operator>>(wrapper const &lhs, int rhs) {
    assert(rhs >= 0);

    auto res = wrapper{};
    res.grow_to(lhs.container.size() - rhs / ull_bits);

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

  void set_bit(uint32_t i, bool value) {
    // TODO this should not change the sign
    grow_to((i + 2 - 1) / ull_bits + 1);
    auto mask = 1ull << (i % ull_bits);
    if (value) {
      container[i / ull_bits] |= mask;
    } else {
      container[i / ull_bits] &= ~mask;
    }
  }

  auto get_bit_range(std::size_t bit_count,
                     std::size_t low) const & -> wrapper {
    return wrapper{*this}.get_bit_range(bit_count, low);
  }

  auto get_bit_range(std::size_t bit_count, std::size_t low) && -> wrapper {
    if (bit_count > 0) {
      auto high_ull = (low + bit_count - 1) / ull_bits + 1;
      grow_to(high_ull);
      auto size = high_ull - low / ull_bits;
      auto res = wrapper{};
      res.unconditional_resize(size);
      small_logical_right_shift(
          algorithms::impl::view{container}.subview(low / ull_bits, size),
          low % ull_bits, res.container);
      return res.mod_pow_2(bit_count);
    } else {
      return {};
    }
  }

  auto one_extend(std::size_t size, std::size_t amount) const & -> wrapper {
    // TODO
    return wrapper{*this}.one_extend(size, amount);
  }

  auto one_extend(std::size_t size, std::size_t amount) && -> wrapper {
    // TODO
    auto container_size = (size + amount) / sizeof(ull) + 1;
    container.resize(container_size);
    auto i = size;
    for (; i < size + amount; i += 1) {
      set_bit(i, 1);
    }
    for (; i < container_size * sizeof(ull); i += 1) {
      set_bit(i, 0);
    }
    return std::move(*this);
  }

  auto lsq() const { return container[0]; }

  auto mod_pow_2(int const exp) && -> wrapper {
    if (exp == 0) {
      return {};
    } else {
      unconditional_resize(exp / ull_bits + 1);
      container[exp / ull_bits] &= (1ull << exp % ull_bits) - 1;
      std::fill(container.begin() + exp / ull_bits + 1, container.end(), 0);
      return std::move(*this);
    }
  }

  auto mod_pow_2(int const exp) const & -> wrapper {
    return wrapper{*this}.mod_pow_2(exp);
  }

  friend auto pow(wrapper const &base, unsigned int exponent) -> wrapper {
    auto res = wrapper{1};
    for (int i = sizeof(exponent) * CHAR_BIT - 1 - __builtin_clz(exponent);
         i >= 0; i -= 1) {
      res = res * res;
      if ((exponent >> i) & 1) {
        res *= base;
      }
    }
    return res;
  }

  friend auto gcd(wrapper const &lhs, wrapper const &rhs) {
    auto res = wrapper{};
    res.container.resize(std::max(lhs.container.size(), rhs.container.size()));
    mpl::algorithms::gcd(auto{lhs.container}, auto{rhs.container},
                         res.container);
    return res;
  }

  friend auto gcd(wrapper &&lhs, wrapper const &rhs) {
    auto res = wrapper{};
    res.container.resize(std::max(lhs.container.size(), rhs.container.size()));
    mpl::algorithms::gcd(std::move(lhs.container), auto{rhs.container},
                         res.container);
    return res;
  }

  friend auto gcd(wrapper const &lhs, wrapper &&rhs) {
    auto res = wrapper{};
    res.container.resize(std::max(lhs.container.size(), rhs.container.size()));
    mpl::algorithms::gcd(auto{lhs.container}, std::move(rhs.container),
                         res.container);
    return res;
  }

  friend auto gcd(wrapper &&lhs, wrapper &&rhs) {
    auto res = wrapper{};
    res.container.resize(std::max(lhs.container.size(), rhs.container.size()));
    mpl::algorithms::gcd(std::move(lhs.container), std::move(rhs.container),
                         res.container);
    return res;
  }

  friend void extended_gcd(wrapper const &lhs, wrapper const &rhs, wrapper &dst,
                           wrapper &a, wrapper &b) {
    dst.container.resize(std::max(lhs.container.size(), rhs.container.size()));
    a.container.resize(std::max(lhs.container.size(), rhs.container.size()));
    b.container.resize(std::max(lhs.container.size(), rhs.container.size()));
    mpl::algorithms::extended_gcd(copy(lhs.container), copy(rhs.container),
                                  dst.container, a.container, b.container);
  }

  friend auto lcm(wrapper const &lhs, wrapper const &rhs) {
    auto res = wrapper{};
    res.container.resize(lhs.container.size() + rhs.container.size());
    mpl::algorithms::lcm(copy(lhs.container), copy(rhs.container),
                         res.container);
    return res;
  }

  template <std::uniform_random_bit_generator Generator>
  static auto random_of_length_at_least(std::size_t const len,
                                        Generator &generator) -> wrapper {
    static_assert(
        std::is_same_v<typename Generator::result_type, unsigned long long>);
    auto res = wrapper{};
    res.container.resize(
        len / (sizeof(typename Generator::result_type) * CHAR_BIT) + 1);
    std::generate(res.container.begin(), res.container.end(),
                  std::ref(generator));
    res.container.back() >>= 1;
    return res;
  }
};
} // namespace mpl

namespace std {
template <typename Container> struct hash<mpl::wrapper<Container>> {
  std::size_t operator()(mpl::wrapper<Container> const &x) const noexcept {
    auto res = 0ull;
    for (auto quad : x.container) {
      res ^=
          quad ^ (quad >> (mpl::ull_bits / 2)) ^ (quad << (mpl::ull_bits / 2));
    }
    return std::hash<mpl::ull>{}(res);
  }
};
} // namespace std

#endif // MPL_WRAPPER_HPP
