#ifndef rational_hpp
#define rational_hpp

#include "static_flexible_width.hpp"
#include "wrapper.hpp"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <limits>
#include <string_view>

#include <source_location> // TODO

namespace mpl {
using namespace std::literals;

template <typename T>
concept floating_point = std::is_floating_point_v<T>;

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

static_assert(sane_float<float>);
static_assert(mantissa_bits<float> == 24);
static_assert(exponent_bits<float> == 8);
static_assert(exponent_bias<float> == 127);
static_assert(sane_float<double>);
static_assert(mantissa_bits<double> == 53);
static_assert(exponent_bits<double> == 11);
static_assert(exponent_bias<double> == 1023);

template <typename Container> struct rational {
private:
  wrapper<Container> numerator;
  wrapper<Container> denominator;

public:
  auto get_numerator() const -> wrapper<Container> const & { return numerator; }
  auto get_denominator() const -> wrapper<Container> const & {
    return denominator;
  }

private:
  static void canonicalise_in_place(wrapper<Container> &numerator,
                                    wrapper<Container> &denominator) {
    if (mpl::algorithms::is_zero(numerator.container)) {
      denominator = 1;
    } else {
      auto sign = numerator.signum() * denominator.signum();
      numerator = std::move(numerator).abs();
      denominator = std::move(denominator).abs();
      auto factor = gcd(numerator, denominator);
      if (factor != 1) {
        if (factor.container.size() == 1) {
          numerator /= factor.container[0];
          denominator /= factor.container[0];
        } else {
          numerator /= factor;
          denominator /= factor;
        }
      }
      if (sign < 0) {
        numerator.negate();
      }
    }
  }

  void canonicalise() { canonicalise_in_place(numerator, denominator); }

  struct skip_canonicalise {};

  rational(skip_canonicalise, wrapper<Container> numerator,
           wrapper<Container> denominator)
      : numerator{std::move(numerator)}, denominator{std::move(denominator)} {}

public:
  rational(wrapper<Container> numerator, wrapper<Container> denominator)
      : numerator{std::move(numerator)}, denominator{std::move(denominator)} {
    canonicalise();
  }

  rational(long long numerator) : numerator{numerator}, denominator{1} {}

  rational(wrapper<Container> numerator)
      : numerator{std::move(numerator)}, denominator{1} {}

private:
  struct numerator_and_denominator_strings {};
  rational(numerator_and_denominator_strings, std::string_view numerator_str,
           std::string_view denominator_str, int base = 10)
      : rational{{numerator_str, base}, {denominator_str, base}} {}

  struct string_and_separator {};
  rational(string_and_separator, std::string_view s, std::size_t separator,
           int base = 10)
      : rational{numerator_and_denominator_strings{}, s.substr(0, separator),
                 separator == std::string_view::npos ? "1"sv
                                                     : s.substr(separator + 1),
                 base} {}

public:
  static auto from_decimal(std::string s, int base = 10) -> rational {
    auto point_pos = s.find('.');
    if (point_pos != std::string::npos) {
      auto denominator_exponent = s.size() - point_pos - 1;
      if (point_pos != std::string::npos) {
        s.erase(point_pos, 1);
      }
      return rational{wrapper<Container>{s, base},
                      pow(wrapper<Container>{base}, denominator_exponent)};
    } else {
      return rational{wrapper<Container>{s, base}};
    }
  }

  rational(std::string_view s, int base = 10)
      : rational{string_and_separator{}, s, s.find('/'), base} {}

  rational(sane_float auto d) {
    int magnitude;
    std::frexp(d, &magnitude);
    auto denom_exp = mantissa_bits<decltype(d)> - magnitude;
    if (denom_exp <= 0) {
      numerator = wrapper<Container>{static_cast<unsigned long long>(
                      std::ldexp(d, denom_exp))}
                  << -denom_exp;
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
    return numerator.template to_float<T>() /
           denominator.template to_float<T>();
  }

  auto signum() const -> int {
    return numerator.signum() * denominator.signum();
  };

  auto abs() const & -> rational {
    return {skip_canonicalise{}, numerator.abs(), denominator.abs()};
  };

  auto abs() && -> rational {
    return {skip_canonicalise{}, std::move(numerator).abs(),
            std::move(denominator).abs()};
  };

  auto floor() const & -> wrapper<Container> {
    if (numerator.is_negative()) {
      auto [div, mod] = div_mod(numerator, wrapper{denominator});
      if (mod != 0) {
        div -= 1;
      }
      return std::move(div);
    } else {
      return numerator / denominator;
    }
  };

  auto floor() && -> wrapper<Container> {
    if (numerator.is_negative()) {
      auto [div, mod] = div_mod(std::move(numerator), std::move(denominator));
      if (mod != 0) {
        div -= 1;
      }
      return std::move(div);
    } else {
      return std::move(numerator) / std::move(denominator);
    }
  };

  auto ceiling() const & -> wrapper<Container> {
    if (numerator.is_negative()) {
      return numerator / denominator;
    } else {
      auto [div, mod] = div_mod(numerator, wrapper{denominator});
      if (mod != 0) {
        div += 1;
      }
      return std::move(div);
    }
  };

  auto ceiling() && -> wrapper<Container> {
    if (numerator.is_negative()) {
      return std::move(numerator) / std::move(denominator);
    } else {
      auto [div, mod] = div_mod(std::move(numerator), std::move(denominator));
      if (mod != 0) {
        div += 1;
      }
      return std::move(div);
    }
  };

  friend auto operator==(rational const &lhs, rational const &rhs) -> bool {
    // canonicalisation is a precondition
    return lhs.numerator == rhs.numerator && lhs.denominator == rhs.denominator;
  }

  friend auto operator<=>(rational const &lhs, rational const &rhs)
      -> std::strong_ordering {
    auto res_denominator = lcm(lhs.denominator, rhs.denominator);
    return (lhs.numerator * res_denominator / lhs.denominator) <=>
           (rhs.numerator * res_denominator / rhs.denominator);
  }

  auto operator-() const & -> rational { return {-numerator, denominator}; }

  auto operator-() && -> rational {
    return {-std::move(numerator), std::move(denominator)};
  }

  friend auto operator+(rational const &lhs, rational const &rhs) -> rational {
    if constexpr (mpl::container_traits<Container>::small_size > 0) {
      if (mpl::container_traits<Container>::is_small(lhs.numerator.container) &&
          mpl::container_traits<Container>::is_small(
              lhs.denominator.container) &&
          mpl::container_traits<Container>::is_small(rhs.numerator.container) &&
          mpl::container_traits<Container>::is_small(
              rhs.denominator.container) &&
          lhs.signum() >= 0 && rhs.signum() >= 0) {
        static constexpr auto size =
            mpl::container_traits<Container>::small_size;
        auto lhs_numerator = static_flexible_width<size>{};
        auto lhs_denominator = static_flexible_width<size>{};
        auto rhs_numerator = static_flexible_width<size>{};
        auto rhs_denominator = static_flexible_width<size>{};
        std::copy(lhs.numerator.container.begin(),
                  lhs.numerator.container.end(),
                  lhs_numerator.container.begin());
        std::copy(lhs.denominator.container.begin(),
                  lhs.denominator.container.end(),
                  lhs_denominator.container.begin());
        std::copy(rhs.numerator.container.begin(),
                  rhs.numerator.container.end(),
                  rhs_numerator.container.begin());
        std::copy(rhs.denominator.container.begin(),
                  rhs.denominator.container.end(),
                  rhs_denominator.container.begin());

        auto gcd_denominator = gcd(lhs_denominator, rhs_denominator);
        if (gcd_denominator == 1) {
          return {skip_canonicalise{},
                  (lhs_numerator * rhs_denominator +
                   rhs_numerator * lhs_denominator)
                      .shrunk(),
                  (lhs_denominator * rhs_denominator).shrunk()};
        } else {
          auto lhs_div_gcd = lhs_denominator / gcd_denominator;
          auto rhs_div_gcd = rhs_denominator / gcd_denominator;
          auto res_numerator =
              lhs_numerator * rhs_div_gcd + rhs_numerator * lhs_div_gcd;
          auto factor = gcd(std::move(gcd_denominator), res_numerator);
          if (factor == 1) {
            return {skip_canonicalise{}, std::move(res_numerator).shrunk(),
                    (lhs_denominator * rhs_div_gcd).shrunk()};
          } else {
            return {skip_canonicalise{},
                    (std::move(res_numerator) / factor).shrunk(),
                    ((lhs_denominator / factor) * rhs_div_gcd).shrunk()};
          }
        }
      }
    }

    auto gcd_denominator = gcd(lhs.denominator, rhs.denominator);
    if (gcd_denominator == 1) {
      return {skip_canonicalise{},
              lhs.numerator * rhs.denominator + rhs.numerator * lhs.denominator,
              lhs.denominator * rhs.denominator};
    } else {
      auto lhs_div_gcd = lhs.denominator / gcd_denominator;
      auto rhs_div_gcd = rhs.denominator / gcd_denominator;
      auto res_numerator =
          lhs.numerator * rhs_div_gcd + rhs.numerator * lhs_div_gcd;
      auto factor = gcd(std::move(gcd_denominator), res_numerator);
      if (factor == 1) {
        return {skip_canonicalise{}, std::move(res_numerator),
                lhs.denominator * rhs_div_gcd};
      } else {
        return {skip_canonicalise{}, std::move(res_numerator) / factor,
                (lhs.denominator / factor) * rhs_div_gcd};
      }
    }
  }

  friend auto operator+(rational const &lhs, rational &rhs) -> rational {
    if constexpr (mpl::container_traits<Container>::small_size > 0) {
      if (mpl::container_traits<Container>::is_small(lhs.numerator.container) &&
          mpl::container_traits<Container>::is_small(
              lhs.denominator.container) &&
          mpl::container_traits<Container>::is_small(rhs.numerator.container) &&
          mpl::container_traits<Container>::is_small(
              rhs.denominator.container) &&
          lhs.signum() >= 0 && rhs.signum() >= 0) {
        static constexpr auto size =
            mpl::container_traits<Container>::small_size;
        auto lhs_numerator = static_flexible_width<size>{};
        auto lhs_denominator = static_flexible_width<size>{};
        auto rhs_numerator = static_flexible_width<size>{};
        auto rhs_denominator = static_flexible_width<size>{};
        std::copy(lhs.numerator.container.begin(),
                  lhs.numerator.container.end(),
                  lhs_numerator.container.begin());
        std::copy(lhs.denominator.container.begin(),
                  lhs.denominator.container.end(),
                  lhs_denominator.container.begin());
        std::copy(rhs.numerator.container.begin(),
                  rhs.numerator.container.end(),
                  rhs_numerator.container.begin());
        std::copy(rhs.denominator.container.begin(),
                  rhs.denominator.container.end(),
                  rhs_denominator.container.begin());

        auto gcd_denominator = gcd(lhs_denominator, rhs_denominator);
        if (gcd_denominator == 1) {
          return {skip_canonicalise{},
                  (lhs_numerator * rhs_denominator +
                   rhs_numerator * lhs_denominator)
                      .shrunk(),
                  (lhs_denominator * rhs_denominator).shrunk()};
        } else {
          auto lhs_div_gcd = lhs_denominator / gcd_denominator;
          auto rhs_div_gcd = rhs_denominator / gcd_denominator;
          auto res_numerator =
              lhs_numerator * rhs_div_gcd + rhs_numerator * lhs_div_gcd;
          auto factor = gcd(std::move(gcd_denominator), res_numerator);
          if (factor == 1) {
            return {skip_canonicalise{}, std::move(res_numerator).shrunk(),
                    (lhs_denominator * rhs_div_gcd).shrunk()};
          } else {
            return {skip_canonicalise{},
                    (std::move(res_numerator) / factor).shrunk(),
                    ((lhs_denominator / factor) * rhs_div_gcd).shrunk()};
          }
        }
      }
    }

    auto gcd_denominator = gcd(lhs.denominator, rhs.denominator);
    if (gcd_denominator == 1) {
      return {skip_canonicalise{},
              lhs.numerator * rhs.denominator + rhs.numerator * lhs.denominator,
              lhs.denominator * rhs.denominator};
    } else {
      auto lhs_div_gcd = lhs.denominator / gcd_denominator;
      auto rhs_div_gcd = rhs.denominator / gcd_denominator;
      auto res_numerator =
          lhs.numerator * rhs_div_gcd + rhs.numerator * lhs_div_gcd;
      auto factor = gcd(std::move(gcd_denominator), res_numerator);
      if (factor == 1) {
        return {skip_canonicalise{}, std::move(res_numerator),
                lhs.denominator * rhs_div_gcd};
      } else {
        return {skip_canonicalise{}, std::move(res_numerator) / factor,
                (lhs.denominator / factor) * rhs_div_gcd};
      }
    }
  }

  friend auto operator+(rational &lhs, rational const &rhs) -> rational {
    if constexpr (mpl::container_traits<Container>::small_size > 0) {
      if (mpl::container_traits<Container>::is_small(lhs.numerator.container) &&
          mpl::container_traits<Container>::is_small(
              lhs.denominator.container) &&
          mpl::container_traits<Container>::is_small(rhs.numerator.container) &&
          mpl::container_traits<Container>::is_small(
              rhs.denominator.container) &&
          lhs.signum() >= 0 && rhs.signum() >= 0) {
        static constexpr auto size =
            mpl::container_traits<Container>::small_size;
        auto lhs_numerator = static_flexible_width<size>{};
        auto lhs_denominator = static_flexible_width<size>{};
        auto rhs_numerator = static_flexible_width<size>{};
        auto rhs_denominator = static_flexible_width<size>{};
        std::copy(lhs.numerator.container.begin(),
                  lhs.numerator.container.end(),
                  lhs_numerator.container.begin());
        std::copy(lhs.denominator.container.begin(),
                  lhs.denominator.container.end(),
                  lhs_denominator.container.begin());
        std::copy(rhs.numerator.container.begin(),
                  rhs.numerator.container.end(),
                  rhs_numerator.container.begin());
        std::copy(rhs.denominator.container.begin(),
                  rhs.denominator.container.end(),
                  rhs_denominator.container.begin());

        auto gcd_denominator = gcd(lhs_denominator, rhs_denominator);
        if (gcd_denominator == 1) {
          return {skip_canonicalise{},
                  (lhs_numerator * rhs_denominator +
                   rhs_numerator * lhs_denominator)
                      .shrunk(),
                  (lhs_denominator * rhs_denominator).shrunk()};
        } else {
          auto lhs_div_gcd = lhs_denominator / gcd_denominator;
          auto rhs_div_gcd = rhs_denominator / gcd_denominator;
          auto res_numerator =
              lhs_numerator * rhs_div_gcd + rhs_numerator * lhs_div_gcd;
          auto factor = gcd(std::move(gcd_denominator), res_numerator);
          if (factor == 1) {
            return {skip_canonicalise{}, std::move(res_numerator).shrunk(),
                    (lhs_denominator * rhs_div_gcd).shrunk()};
          } else {
            return {skip_canonicalise{},
                    (std::move(res_numerator) / factor).shrunk(),
                    ((lhs_denominator / factor) * rhs_div_gcd).shrunk()};
          }
        }
      }
    }

    auto gcd_denominator = gcd(lhs.denominator, rhs.denominator);
    if (gcd_denominator == 1) {
      return {skip_canonicalise{},
              lhs.numerator * rhs.denominator + rhs.numerator * lhs.denominator,
              lhs.denominator * rhs.denominator};
    } else {
      auto lhs_div_gcd = lhs.denominator / gcd_denominator;
      auto rhs_div_gcd = rhs.denominator / gcd_denominator;
      auto res_numerator =
          lhs.numerator * rhs_div_gcd + rhs.numerator * lhs_div_gcd;
      auto factor = gcd(std::move(gcd_denominator), res_numerator);
      if (factor == 1) {
        return {skip_canonicalise{}, std::move(res_numerator),
                lhs.denominator * rhs_div_gcd};
      } else {
        return {skip_canonicalise{}, std::move(res_numerator) / factor,
                (lhs.denominator / factor) * rhs_div_gcd};
      }
    }
  }

  friend auto operator+(rational &lhs, rational &rhs) -> rational {
    if constexpr (mpl::container_traits<Container>::small_size > 0) {
      if (mpl::container_traits<Container>::is_small(lhs.numerator.container) &&
          mpl::container_traits<Container>::is_small(
              lhs.denominator.container) &&
          mpl::container_traits<Container>::is_small(rhs.numerator.container) &&
          mpl::container_traits<Container>::is_small(
              rhs.denominator.container) &&
          lhs.signum() >= 0 && rhs.signum() >= 0) {
        static constexpr auto size =
            mpl::container_traits<Container>::small_size;
        auto lhs_numerator = static_flexible_width<size>{};
        auto lhs_denominator = static_flexible_width<size>{};
        auto rhs_numerator = static_flexible_width<size>{};
        auto rhs_denominator = static_flexible_width<size>{};
        std::copy(lhs.numerator.container.begin(),
                  lhs.numerator.container.end(),
                  lhs_numerator.container.begin());
        std::copy(lhs.denominator.container.begin(),
                  lhs.denominator.container.end(),
                  lhs_denominator.container.begin());
        std::copy(rhs.numerator.container.begin(),
                  rhs.numerator.container.end(),
                  rhs_numerator.container.begin());
        std::copy(rhs.denominator.container.begin(),
                  rhs.denominator.container.end(),
                  rhs_denominator.container.begin());

        auto gcd_denominator = gcd(lhs_denominator, rhs_denominator);
        if (gcd_denominator == 1) {
          return {skip_canonicalise{},
                  (lhs_numerator * rhs_denominator +
                      rhs_numerator * lhs_denominator).shrunk(),
                  (lhs_denominator * rhs_denominator).shrunk()};
        } else {
          auto lhs_div_gcd = lhs_denominator / gcd_denominator;
          auto rhs_div_gcd = rhs_denominator / gcd_denominator;
          auto res_numerator =
              lhs_numerator * rhs_div_gcd + rhs_numerator * lhs_div_gcd;
          auto factor = gcd(std::move(gcd_denominator), res_numerator);
          if (factor == 1) {
            return {skip_canonicalise{}, std::move(res_numerator).shrunk(),
                    (lhs_denominator * rhs_div_gcd).shrunk()};
          } else {
            return {skip_canonicalise{}, (std::move(res_numerator) / factor).shrunk(),
                    ((lhs_denominator / factor) * rhs_div_gcd).shrunk()};
          }
        }
      }
    }

    auto gcd_denominator = gcd(lhs.denominator, rhs.denominator);
    if (gcd_denominator == 1) {
      return {skip_canonicalise{},
              lhs.numerator * rhs.denominator + rhs.numerator * lhs.denominator,
              lhs.denominator * rhs.denominator};
    } else {
      auto lhs_div_gcd = lhs.denominator / gcd_denominator;
      auto rhs_div_gcd = rhs.denominator / gcd_denominator;
      auto res_numerator =
          lhs.numerator * rhs_div_gcd + rhs.numerator * lhs_div_gcd;
      auto factor = gcd(std::move(gcd_denominator), res_numerator);
      if (factor == 1) {
        return {skip_canonicalise{}, std::move(res_numerator),
                lhs.denominator * rhs_div_gcd};
      } else {
        return {skip_canonicalise{}, std::move(res_numerator) / factor,
                (lhs.denominator / factor) * rhs_div_gcd};
      }
    }
  }

  friend auto operator+(rational &&lhs, rational const &rhs) -> rational {
    return lhs + rhs;
  }

  friend auto operator+(rational &&lhs, rational &rhs) -> rational {
    return lhs + rhs;
  }

  friend auto operator+(rational const &lhs, rational &&rhs) -> rational {
    return lhs + rhs;
  }

  friend auto operator+(rational &lhs, rational &&rhs) -> rational {
    return lhs + rhs;
  }

  friend auto operator+(rational &&lhs, rational &&rhs) -> rational {
    return lhs + rhs;
  }

  auto operator+=(rational const &rhs) -> rational & {
    *this = std::move(*this) + rhs;
    return *this;
  }

  auto operator+=(rational &rhs) -> rational & {
    *this = std::move(*this) + rhs;
    return *this;
  }

  auto operator+=(rational &&rhs) -> rational & {
    *this = std::move(*this) + std::move(rhs);
    return *this;
  }

  friend auto operator-(rational const &lhs, rational const &rhs) -> rational {
    auto res_denominator = lcm(lhs.denominator, rhs.denominator);
    auto res_numerator = lhs.numerator * res_denominator / lhs.denominator -
                         rhs.numerator * res_denominator / rhs.denominator;
    return {res_numerator, res_denominator};
  }

  auto operator-=(rational const &rhs) -> rational & {
    auto lhs_denominator =
        std::exchange(denominator, lcm(denominator, rhs.denominator));
    numerator = numerator * denominator / lhs_denominator -
                rhs.numerator * denominator / rhs.denominator;
    canonicalise();
    return *this;
  }

  friend auto operator*(rational const &lhs, rational const &rhs) -> rational {
    return auto{lhs} * auto{rhs};
  }

  friend auto operator*(rational &&lhs, rational const &rhs) -> rational {
    return std::move(lhs) * auto{rhs};
  }

  friend auto operator*(rational const &lhs, rational &&rhs) -> rational {
    return auto{lhs} * std::move(rhs);
  }

  friend auto operator*(rational &&lhs, rational &&rhs) -> rational {
    canonicalise_in_place(lhs.numerator, rhs.denominator);
    canonicalise_in_place(rhs.numerator, lhs.denominator);
    return {skip_canonicalise{},
            std::move(lhs.numerator) * std::move(rhs.numerator),
            std::move(lhs.denominator) * std::move(rhs.denominator)};
  }

  auto operator*=(rational const &rhs) -> rational & {
    return *this *= auto{rhs};
  }

  auto operator*=(rational &&rhs) -> rational & {
    canonicalise_in_place(numerator, rhs.denominator);
    canonicalise_in_place(rhs.numerator, denominator);
    numerator *= std::move(rhs.numerator);
    denominator *= std::move(rhs.denominator);
    return *this;
  }

  friend auto operator/(rational const &lhs, rational const &rhs) -> rational {
    return auto{lhs} / auto{rhs};
  }

  friend auto operator/(rational &&lhs, rational const &rhs) -> rational {
    return std::move(lhs) / auto{rhs};
  }

  friend auto operator/(rational const &lhs, rational &&rhs) -> rational {
    return auto{lhs} / std::move(rhs);
  }

  friend auto operator/(rational &&lhs, rational &&rhs) -> rational {
    canonicalise_in_place(lhs.numerator, rhs.numerator);
    canonicalise_in_place(rhs.denominator, lhs.denominator);
    return {skip_canonicalise{},
            std::move(lhs.numerator) * std::move(rhs.denominator),
            std::move(lhs.denominator) * std::move(rhs.numerator)};
  }

  auto operator/=(rational const &rhs) -> rational & {
    return *this /= auto{rhs};
  }

  auto operator/=(rational &&rhs) -> rational & {
    canonicalise_in_place(numerator, rhs.numerator);
    canonicalise_in_place(rhs.denominator, denominator);
    numerator *= std::move(rhs.denominator);
    denominator *= std::move(rhs.numerator);
    return *this;
  }

  auto to_string(int base = 10) const & -> std::string {
    if (denominator == 1) {
      return numerator.to_string(base);
    } else {
      return numerator.to_string(base) + "/" + denominator.to_string(base);
    }
  }

  auto to_string(int base = 10) && -> std::string {
    if (denominator == 1) {
      return std::move(numerator).to_string(base);
    } else {
      return std::move(numerator).to_string(base) + "/" +
             std::move(denominator).to_string(base);
    }
  }

  friend auto operator<<(std::ostream &os, rational const &rhs)
      -> std::ostream & {
    if (rhs.denominator == 1) {
      return os << rhs.numerator;
    } else {
      return os << rhs.numerator << "/" << rhs.denominator;
    }
  }
};
} // namespace mpl

namespace std {
template <typename Container> struct hash<mpl::rational<Container>> {
  std::size_t operator()(mpl::rational<Container> const &x) const noexcept {
    return std::hash<mpl::wrapper<Container>>{}(x.get_numerator()) ^
           std::hash<mpl::wrapper<Container>>{}(x.get_denominator());
  }
};
} // namespace std

#endif // rational_hpp
