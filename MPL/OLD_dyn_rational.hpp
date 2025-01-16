#ifndef dyn_rational_hpp
#define dyn_rational_hpp

#include "dyn_int.hpp"

#include <cmath>
#include <concepts>
#include <limits>
#include <string_view>

namespace MPL {
using namespace std::literals;

template <typename T>
concept ieee754 = std::numeric_limits<T>::is_iec559;
template <typename T>
concept radix2 = std::numeric_limits<T>::radix == 2;
template <typename T>
concept sane_float = ieee754<T> && radix2<T>;

template <sane_float T>
constexpr auto mantissa_bits = std::numeric_limits<T>::digits;
template <sane_float T>
constexpr auto exponent_bits = [] {
  using limits = std::numeric_limits<T>;
  auto min_exp = limits::min_exponent;
  auto max_exp = limits::max_exponent;
  auto exp_range = max_exp - min_exp;
  return sizeof(exp_range) * CHAR_BIT - __builtin_clz(exp_range);
}();
template <sane_float T>
constexpr auto exponent_bias = 2 - std::numeric_limits<T>::min_exponent;

static_assert(mantissa_bits<float> == 24);
static_assert(exponent_bits<float> == 8);
static_assert(exponent_bias<float> == 127);
static_assert(mantissa_bits<double> == 53);
static_assert(exponent_bits<double> == 11);
static_assert(exponent_bias<double> == 1023);

struct dyn_rational {
  dyn_int numerator;
  dyn_int denominator;

  void canonicalise() {
    auto factor = gcd(numerator, denominator);
    numerator /= factor;
    denominator /= factor;
  }

  struct skip_canonicalise {};

  dyn_rational(skip_canonicalise, dyn_int numerator, dyn_int denominator)
      : numerator{std::move(numerator)}, denominator{std::move(denominator)} {}

  dyn_rational(dyn_int numerator, dyn_int denominator)
      : dyn_rational{skip_canonicalise{}, std::move(numerator),
                     std::move(denominator)} {
    canonicalise();
  }

  dyn_rational(long long numerator)
      : dyn_rational{skip_canonicalise{}, numerator, 1} {}

  dyn_rational(dyn_int numerator)
      : dyn_rational{skip_canonicalise{}, std::move(numerator), 1} {}

private:
  struct numerator_and_denominator_strings {};
  dyn_rational(numerator_and_denominator_strings,
               std::string_view numerator_str, std::string_view denominator_str,
               unsigned int base = 10)
      : numerator{numerator_str, base}, denominator{denominator_str, base} {}

  struct string_and_separator {};
  dyn_rational(string_and_separator, std::string_view s, std::size_t separator,
               unsigned int base = 10)
      : dyn_rational{
            numerator_and_denominator_strings{}, s.substr(0, separator),
            separator == std::string_view::npos ? "1"sv
                                                : s.substr(separator + 1),
            base} {}

public:
  dyn_rational(std::string_view s, unsigned int base = 10)
      : dyn_rational{string_and_separator{}, s, s.find('/'), base} {}

  dyn_rational(sane_float auto d) {
    int magnitude;
    std::frexp(d, &magnitude);
    auto denom_exp = mantissa_bits<decltype(d)> - magnitude;
    if (denom_exp <= 0) {
      numerator = dyn_int{static_cast<unsigned long long>(std::ldexp(d, denom_exp))} << -denom_exp;
      denominator = 1;
    } else {
      numerator = static_cast<unsigned long long>(std::ldexp(d, denom_exp));
      denominator = 1ull << denom_exp;
      if (d < 0) {
        numerator.negate();
      }
    }
  }

  template <floating_point T = double> auto to_float() const -> T {
    auto numerator_d = numerator.to_float<double>();
    auto denominator_d = denominator.to_float<double>();
    return static_cast<T>(numerator_d / denominator_d);
  }

  auto signum() const -> int {
    return numerator.signum() * denominator.signum();
  };

  auto abs() const & -> dyn_rational {
    return {skip_canonicalise{}, numerator.abs(), denominator.abs()};
  };

  auto abs() && -> dyn_rational {
    return {skip_canonicalise{}, std::move(numerator).abs(),
            std::move(denominator).abs()};
  };

  auto floor() const & -> dyn_int { return numerator / denominator; };

  auto floor() && -> dyn_int {
    return std::move(numerator) / std::move(denominator);
  };

  auto ceiling() const & -> dyn_int {
    auto [div, mod] = div_mod(numerator, dyn_int{denominator});
    if (mod != 0) {
      div += 1;
    }
    return std::move(div);
  };

  auto ceiling() && -> dyn_int {
    auto [div, mod] = div_mod(std::move(numerator), std::move(denominator));
    if (mod != 0) {
      div += 1;
    }
    return std::move(div);
  };

  friend auto operator==(dyn_rational const &, dyn_rational const &)
      -> bool = default;

  friend auto operator<=>(dyn_rational const &lhs, dyn_rational const &rhs)
      -> std::strong_ordering {
    auto res_denominator = lcm(lhs.denominator, rhs.denominator);
    return (lhs.numerator * res_denominator / lhs.denominator) <=>
           (rhs.numerator * res_denominator / rhs.denominator);
  }

  auto operator-() const & -> dyn_rational { return {-numerator, denominator}; }

  auto operator-() && -> dyn_rational {
    return {-std::move(numerator), std::move(denominator)};
  }

  friend auto operator+(dyn_rational const &lhs, dyn_rational const &rhs)
      -> dyn_rational {
    auto res_denominator = lcm(lhs.denominator, rhs.denominator);
    auto res_numerator = lhs.numerator * res_denominator / lhs.denominator +
                         rhs.numerator * res_denominator / rhs.denominator;
    return {res_numerator, res_denominator};
  }

  auto operator+=(dyn_rational const &rhs) -> dyn_rational & {
    auto lhs_denominator =
        std::exchange(denominator, lcm(denominator, rhs.denominator));
    numerator = numerator * denominator / lhs_denominator +
                rhs.numerator * denominator / rhs.denominator;
    return *this;
  }

  friend auto operator-(dyn_rational const &lhs, dyn_rational const &rhs)
      -> dyn_rational {
    auto res_denominator = lcm(lhs.denominator, rhs.denominator);
    auto res_numerator = lhs.numerator * res_denominator / lhs.denominator -
                         rhs.numerator * res_denominator / rhs.denominator;
    return {res_numerator, res_denominator};
  }

  auto operator-=(dyn_rational const &rhs) -> dyn_rational & {
    auto lhs_denominator =
        std::exchange(denominator, lcm(denominator, rhs.denominator));
    numerator = numerator * denominator / lhs_denominator -
                rhs.numerator * denominator / rhs.denominator;
    return *this;
  }

  friend auto operator*(dyn_rational const &lhs, dyn_rational const &rhs)
      -> dyn_rational {
    return {lhs.numerator * rhs.numerator, lhs.denominator * rhs.denominator};
  }

  auto operator*=(dyn_rational const &rhs) -> dyn_rational & {
    numerator *= rhs.numerator;
    denominator *= rhs.denominator;
    return *this;
  }

  auto operator*=(dyn_rational &&rhs) -> dyn_rational & {
    numerator *= std::move(rhs.numerator);
    denominator *= std::move(rhs.denominator);
    return *this;
  }

  friend auto operator/(dyn_rational const &lhs, dyn_rational const &rhs)
      -> dyn_rational {
    return {lhs.numerator * rhs.denominator, lhs.denominator * rhs.numerator};
  }

  auto operator/=(dyn_rational const &rhs) -> dyn_rational & {
    numerator *= rhs.denominator;
    denominator *= rhs.numerator;
    return *this;
  }

  auto operator/=(dyn_rational &&rhs) -> dyn_rational & {
    numerator *= std::move(rhs.denominator);
    denominator *= std::move(rhs.numerator);
    return *this;
  }

  friend auto operator<<(std::ostream &os, dyn_rational const &rhs)
      -> std::ostream & {
    return os << rhs.numerator << " / " << rhs.denominator;
  }
};
} // namespace MPL

#endif // dyn_rational_hpp
