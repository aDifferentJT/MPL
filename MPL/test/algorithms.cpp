#include "../algorithms.hpp"
#include "../utility.hpp"

#include <gmpxx.h>

#include <compare>
#include <vector>

#include <rapidcheck.h>

using mpl::ull;
using mpl::algorithms::not_cvref;

std::string debug_print(auto const &x) {
  auto ss = std::stringstream{};
  ss << std::hex << std::setfill('0');
  for (auto const y : x) {
    ss << std::setw(16) << y << ' ';
  }
  return std::move(ss).str();
}

struct integer_string : std::string {
  auto mpl() { return mpl::algorithms::from_string<std::vector<ull>>(*this); }
  auto gmp() { return mpz_class{*this}; }
};

namespace rc {
template <> struct Arbitrary<integer_string> {
  static auto arbitrary() -> Gen<integer_string> {
    using namespace std::literals;

    return gen::apply(
        [](std::string sign, char first_digit, std::string tail) {
          return integer_string{sign + first_digit + tail};
        },
        gen::element(""s, "-"s), gen::inRange('1', '9'),
        gen::container<std::string>(gen::inRange('0', '9')));
  }
};
} // namespace rc

auto check_equality(std::vector<ull> x, __int128_t y) -> bool {
  return mpl::algorithms::is_eq(mpl::algorithms::compare(
      x, mpl::algorithms::from_int<std::vector<ull>>(y)));
}

auto check_equality(std::vector<ull> x, mpz_class const &y) -> bool {
  return mpl::algorithms::to_string(std::move(x)) == y.get_str();
}

struct assertion_failed : std::runtime_error {
  using std::runtime_error::runtime_error;
};

void assert_equal(std::vector<ull> x, mpz_class const &y,
                  std::string msg = "") {
  auto x_str = mpl::algorithms::to_string(std::move(x));
  auto y_str = y.get_str();
  if (x_str != y_str) {
    throw assertion_failed{msg + ' ' + x_str + " != " + y_str};
  }
}

template <auto, typename T> using snd = T;

template <std::size_t arity, typename... Args>
auto mpl_gmp_match(auto &&mpl_func, auto &&gmp_func) {
  return [&]<auto... Is>(std::index_sequence<Is...>) {
    return [&](snd<Is, integer_string>... strs, Args... args) {
      return check_equality(
          mpl_func(mpl::algorithms::from_string<std::vector<ull>>(strs)...,
                   args...),
          gmp_func(mpz_class{strs}..., args...));
    };
  }
  (std::make_index_sequence<arity>{});
}

int main() {
  rc::check("to_string(from_string(x)) roundtrips", [](integer_string x) {
    return mpl::algorithms::to_string(x.mpl()) == x;
  });

  rc::check("compare (equality) is reflexive", [](integer_string x) {
    return mpl::algorithms::compare(x.mpl(), x.mpl()) == 0;
  });

  rc::check("compare (inequalities) is antisymmetric",
            [](integer_string x, integer_string y) {
              auto ord1 = mpl::algorithms::compare(x.mpl(), y.mpl());
              auto ord2 = mpl::algorithms::compare(y.mpl(), x.mpl());
              if (ord1 == 0) {
                return ord2 == 0;
              } else if (ord1 < 0) {
                return ord2 > 0;
              } else if (ord1 > 0) {
                return ord2 < 0;
              } else {
                return false;
              }
            });

  rc::check("compare matches GMP", [](integer_string x, integer_string y) {
    return mpl::algorithms::compare(x.mpl(), y.mpl()) ==
           mpl::impl::compare_strong_order_fallback(x.gmp(), y.gmp());
  });

  rc::check(
      "unsigned_compare matches GMP", [](integer_string x, integer_string y) {
        RC_PRE(x.gmp() > 0 && y.gmp() > 0);
        return mpl::algorithms::impl::unsigned_compare(x.mpl(), y.mpl()) ==
               mpl::impl::compare_strong_order_fallback(x.gmp(), y.gmp());
      });

  rc::check("sar ull_bits - 1 fills with the sign bit", [](ull x) {
    return mpl::algorithms::sar(x, mpl::ull_bits - 1) ==
           (static_cast<signed long long>(x) < 0 ? ~0ull : 0ull);
  });

  rc::check(
      "trim_leading_sign_bits doesn't change the value",
      [](std::vector<ull> xs) {
        return mpl::algorithms::compare(
                   xs, mpl::algorithms::impl::trim_leading_sign_bits(xs)) == 0;
      });

  /*
  rc::check(
      "unsigned_ripple_subber doesn't crash when x = y", [](integer_string x) {
        RC_PRE(x.gmp() > 0);
        return mpl::algorithms::impl::unsigned_ripple_subber(
            mpl::algorithms::impl::unsafe_trim_leading_zeros(x.mpl()),
            mpl::algorithms::impl::unsafe_trim_leading_zeros(x.mpl()), x.mpl());
      });

  rc::check("unsigned_ripple_subber doesn't crash when x > y",
            [](integer_string x, integer_string y) {
              RC_PRE(std::is_gteq(mpl::algorithms::compare(x.mpl(), y.mpl())) &&
                     y.gmp() > 0);
              return mpl::algorithms::impl::unsigned_ripple_subber(
                  mpl::algorithms::impl::unsafe_trim_leading_zeros(x.mpl()),
                  mpl::algorithms::impl::unsafe_trim_leading_zeros(y.mpl()),
                  x.mpl());
            });
            */

  rc::check("divide_small matches __int128_t", [](std::array<ull, 2> x, ull y) {
    RC_PRE(y != 0ull);

    auto x2 = std::vector(x.begin(), x.end());
    auto res = std::vector<ull>{};
    res.resize(2);
    mpl::algorithms::divide_small(x2, y, res);

    auto res_128 =
        (x[0] | (static_cast<__int128_t>(x[1]) << mpl::ull_bits)) / y;
    return check_equality(res, res_128);
  });

  rc::check("from_string round trips with long longs", [](long long x) {
    return check_equality(
        mpl::algorithms::from_string<std::vector<ull>>(std::to_string(x)), x);
  });

  rc::check("from_string round trips with unsigned long longs", [](ull x) {
    return check_equality(
        mpl::algorithms::from_string<std::vector<ull>>(std::to_string(x)), x);
  });

  rc::check("to_string on long longs matches", [](long long x) {
    return mpl::algorithms::to_string(
               mpl::algorithms::from_int<std::vector<ull>>(x)) ==
           std::to_string(x);
  });

  rc::check("to_string on unsigned long longs matches", [](ull x) {
    return mpl::algorithms::to_string(
               mpl::algorithms::from_int<std::vector<ull>>(x)) ==
           std::to_string(x);
  });

  rc::check("to_string from_string round trips", [](integer_string str) {
    auto x_mpl = mpl::algorithms::from_string<std::vector<ull>>(str);
    if (mpl::algorithms::to_string(mpl::copy(x_mpl)) != str) {
      std::cerr << mpl::algorithms::to_string(mpl::copy(x_mpl)) << ", " << str
                << ", " << x_mpl[0] << ' ' << x_mpl.size() << '\n';
    }
    return mpl::algorithms::to_string(
               mpl::algorithms::from_string<std::vector<ull>>(str)) == str;
  });

  rc::check("GMP to_string from_string round trips", [](integer_string str) {
    // This is mainly to test the test cases, not looking for bugs in GMP
    return mpz_class{str}.get_str() == str;
  });

  rc::check("Addition on long longs matches", [](long long lhs_int,
                                                 long long rhs_int) {
    auto const lhs_vec = mpl::algorithms::from_int<std::vector<ull>>(lhs_int);
    auto const rhs_vec = mpl::algorithms::from_int<std::vector<ull>>(rhs_int);
    auto dst_vec = std::vector<ull>{};
    dst_vec.resize(std::max(lhs_vec.size(), rhs_vec.size()));
    mpl::algorithms::add(lhs_vec, rhs_vec, dst_vec);

    auto res =
        static_cast<__int128_t>(lhs_int) + static_cast<__int128_t>(rhs_int);

    return check_equality(dst_vec, res);
  });

  rc::check("Addition on unsigned long longs matches", [](ull lhs_int,
                                                          ull rhs_int) {
    auto const lhs_vec = mpl::algorithms::from_int<std::vector<ull>>(lhs_int);
    auto const rhs_vec = mpl::algorithms::from_int<std::vector<ull>>(rhs_int);
    auto dst_vec = std::vector<ull>{};
    dst_vec.resize(std::max(lhs_vec.size(), rhs_vec.size()));
    mpl::algorithms::add(lhs_vec, rhs_vec, dst_vec);

    auto res =
        static_cast<__int128_t>(lhs_int) + static_cast<__int128_t>(rhs_int);

    return check_equality(dst_vec, res);
  });

  rc::check("Addition matches gmp",
            mpl_gmp_match<2>(
                [](auto const &lhs, auto const &rhs) {
                  auto dst = std::vector<ull>{};
                  dst.resize(std::max(lhs.size(), rhs.size()));
                  mpl::algorithms::add(lhs, rhs, dst);
                  return dst;
                },
                [](auto const &lhs, auto const &rhs) {
                  return mpz_class{lhs + rhs};
                }));

  rc::check("Subtraction on long longs matches", [](long long lhs_int,
                                                    long long rhs_int) {
    auto const lhs_vec = mpl::algorithms::from_int<std::vector<ull>>(lhs_int);
    auto const rhs_vec = mpl::algorithms::from_int<std::vector<ull>>(rhs_int);
    auto dst_vec = std::vector<ull>{};
    dst_vec.resize(std::max(lhs_vec.size(), rhs_vec.size()));
    mpl::algorithms::sub(lhs_vec, rhs_vec, dst_vec);

    auto res =
        static_cast<__int128_t>(lhs_int) - static_cast<__int128_t>(rhs_int);

    return check_equality(dst_vec, res);
  });

  rc::check("Subtraction on unsigned long longs matches", [](ull lhs_int,
                                                             ull rhs_int) {
    auto const lhs_vec = mpl::algorithms::from_int<std::vector<ull>>(lhs_int);
    auto const rhs_vec = mpl::algorithms::from_int<std::vector<ull>>(rhs_int);
    auto dst_vec = std::vector<ull>{};
    dst_vec.resize(std::max(lhs_vec.size(), rhs_vec.size()));
    mpl::algorithms::sub(lhs_vec, rhs_vec, dst_vec);

    auto res =
        static_cast<__int128_t>(lhs_int) - static_cast<__int128_t>(rhs_int);

    return check_equality(dst_vec, res);
  });

  rc::check("Subtraction matches gmp",
            mpl_gmp_match<2>(
                [](auto const &lhs, auto const &rhs) {
                  auto dst = std::vector<ull>{};
                  dst.resize(std::max(lhs.size(), rhs.size()));
                  mpl::algorithms::sub(lhs, rhs, dst);
                  return dst;
                },
                [](auto const &lhs, auto const &rhs) {
                  return mpz_class{lhs - rhs};
                }));

  rc::check("In place addition on long longs matches", [](long long lhs_int,
                                                          long long rhs_int) {
    auto lhs_vec = mpl::algorithms::from_int<std::vector<ull>>(lhs_int);
    auto const rhs_vec = mpl::algorithms::from_int<std::vector<ull>>(rhs_int);
    lhs_vec.resize(std::max(lhs_vec.size(), rhs_vec.size()));
    mpl::algorithms::add(lhs_vec, rhs_vec, lhs_vec);

    auto res =
        static_cast<__int128_t>(lhs_int) + static_cast<__int128_t>(rhs_int);

    return check_equality(lhs_vec, res);
  });

  rc::check("In place addition on unsigned long longs matches",
            [](ull lhs_int, ull rhs_int) {
              auto lhs_vec =
                  mpl::algorithms::from_int<std::vector<ull>>(lhs_int);
              auto const rhs_vec =
                  mpl::algorithms::from_int<std::vector<ull>>(rhs_int);
              lhs_vec.resize(std::max(lhs_vec.size(), rhs_vec.size()));
              mpl::algorithms::add(lhs_vec, rhs_vec, lhs_vec);

              auto res = static_cast<__int128_t>(lhs_int) +
                         static_cast<__int128_t>(rhs_int);

              return check_equality(lhs_vec, res);
            });

  rc::check("In place subtraction on long longs matches",
            [](long long lhs_int, long long rhs_int) {
              auto lhs_vec =
                  mpl::algorithms::from_int<std::vector<ull>>(lhs_int);
              auto const rhs_vec =
                  mpl::algorithms::from_int<std::vector<ull>>(rhs_int);
              lhs_vec.resize(std::max(lhs_vec.size(), rhs_vec.size()));
              mpl::algorithms::sub(lhs_vec, rhs_vec, lhs_vec);

              auto res = static_cast<__int128_t>(lhs_int) -
                         static_cast<__int128_t>(rhs_int);

              return check_equality(lhs_vec, res);
            });

  rc::check("In place subtraction on unsigned long longs matches",
            [](ull lhs_int, ull rhs_int) {
              auto lhs_vec =
                  mpl::algorithms::from_int<std::vector<ull>>(lhs_int);
              auto const rhs_vec =
                  mpl::algorithms::from_int<std::vector<ull>>(rhs_int);
              lhs_vec.resize(std::max(lhs_vec.size(), rhs_vec.size()));
              mpl::algorithms::sub(lhs_vec, rhs_vec, lhs_vec);

              auto res = static_cast<__int128_t>(lhs_int) -
                         static_cast<__int128_t>(rhs_int);

              return check_equality(lhs_vec, res);
            });

  rc::check("Multiplication on long longs matches", [](long long lhs_int,
                                                       long long rhs_int) {
    auto lhs_vec = mpl::algorithms::from_int<std::vector<ull>>(lhs_int);
    auto rhs_vec = mpl::algorithms::from_int<std::vector<ull>>(rhs_int);
    auto dst_vec = std::vector<ull>{};
    dst_vec.resize(lhs_vec.size() + rhs_vec.size());
    mpl::algorithms::mult<std::vector<ull>>(
        std::move(lhs_vec), std::move(rhs_vec), dst_vec);

    auto res =
        static_cast<__int128_t>(lhs_int) * static_cast<__int128_t>(rhs_int);

    return check_equality(dst_vec, res);
  });

  rc::check("Multiplication on unsigned long longs matches", [](ull lhs_int,
                                                                ull rhs_int) {
    auto lhs_vec = mpl::algorithms::from_int<std::vector<ull>>(lhs_int);
    auto rhs_vec = mpl::algorithms::from_int<std::vector<ull>>(rhs_int);
    auto dst_vec = std::vector<ull>{};
    dst_vec.resize(lhs_vec.size() + rhs_vec.size());
    mpl::algorithms::mult<std::vector<ull>>(
        std::move(lhs_vec), std::move(rhs_vec), dst_vec);

    auto res =
        static_cast<__uint128_t>(lhs_int) * static_cast<__uint128_t>(rhs_int);

    return mpl::algorithms::is_eq(mpl::algorithms::compare(
        dst_vec, mpl::algorithms::from_int<std::vector<ull>>(res)));
  });

  rc::check("Multiplication matches gmp",
            mpl_gmp_match<2>(
                [](not_cvref auto &&lhs, not_cvref auto &&rhs) {
                  auto dst = std::vector<ull>{};
                  dst.resize(lhs.size() + rhs.size());
                  mpl::algorithms::mult<std::vector<ull>>(
                      std::move(lhs), std::move(rhs), dst);
                  return dst;
                },
                [](auto const &lhs, auto const &rhs) {
                  return mpz_class{lhs * rhs};
                }));

  /*
  rc::check("Division on long longs matches", [](long long dividend_int,
                                                 long long divisor_int) {
    RC_PRE(divisor_int != 0u);

    auto dividend_vec =
        mpl::algorithms::from_int<std::vector<ull>>(dividend_int);
    auto divisor_vec = mpl::algorithms::from_int<std::vector<ull>>(divisor_int);
    auto quotient_vec = std::vector<ull>{};
    quotient_vec.resize(dividend_vec.size());
    mpl::algorithms::divide(dividend_vec, std::move(divisor_vec), quotient_vec);

    auto quotient_int = static_cast<__int128_t>(dividend_int) /
                        static_cast<__int128_t>(divisor_int);

    auto remainder_int = static_cast<__int128_t>(dividend_int) %
                         static_cast<__int128_t>(divisor_int);

    return check_equality(quotient_vec, quotient_int) &&
           check_equality(dividend_vec, remainder_int);
  });

  rc::check("Division on unsigned long longs matches", [](ull dividend_int,
                                                          ull divisor_int) {
    RC_PRE(divisor_int != 0u);

    auto dividend_vec =
        mpl::algorithms::from_int<std::vector<ull>>(dividend_int);
    auto divisor_vec = mpl::algorithms::from_int<std::vector<ull>>(divisor_int);
    auto quotient_vec = std::vector<ull>{};
    quotient_vec.resize(dividend_vec.size());
    mpl::algorithms::divide(dividend_vec, std::move(divisor_vec), quotient_vec);

    auto quotient_int = static_cast<__int128_t>(dividend_int) /
                        static_cast<__int128_t>(divisor_int);

    auto remainder_int = static_cast<__int128_t>(dividend_int) %
                         static_cast<__int128_t>(divisor_int);

    return check_equality(quotient_vec, quotient_int) &&
           check_equality(dividend_vec, remainder_int);
  });
  */

  rc::check("Division matches gmp", [](integer_string dividend,
                                       integer_string divisor) {
    auto cerr_flags = std::cerr.flags();
    RC_PRE(!mpl::algorithms::is_zero(divisor.mpl()));
    RC_PRE(divisor.gmp() != 0);

    auto quotient_gmp = mpz_class{dividend.gmp() / divisor.gmp()};
    auto remainder_gmp = mpz_class{dividend.gmp() % divisor.gmp()};

    auto dividend_vec = dividend.mpl();
    auto quotient_vec = std::vector<ull>{};
    quotient_vec.resize(dividend_vec.size());
    mpl::algorithms::divide(dividend_vec, divisor.mpl(), quotient_vec);

    auto quotient_vec_str = mpl::algorithms::to_string(mpl::copy(quotient_vec));
    auto quotient_gmp_str = quotient_gmp.get_str();
    auto remainder_vec_str =
        mpl::algorithms::to_string(mpl::copy(dividend_vec));
    auto remainder_gmp_str = remainder_gmp.get_str();
    if (quotient_vec_str != quotient_gmp_str ||
        remainder_vec_str != remainder_gmp_str) {
      std::cerr << "MPL: " << dividend.gmp() << " / " << divisor.gmp() << " = "
                << quotient_vec_str << " rem " << remainder_vec_str << '\n';
      std::cerr << "GMP: " << dividend.gmp() << " / " << divisor.gmp() << " = "
                << quotient_gmp_str << " rem " << remainder_gmp_str << '\n';
    }

    assert_equal(quotient_vec, quotient_gmp, "Quotient");
    assert_equal(dividend_vec, remainder_gmp, "Remainder");
    std::cerr.flags(cerr_flags);
  });

  rc::check("GCD matches gmp",
            mpl_gmp_match<2>(
                [](not_cvref auto &&lhs, not_cvref auto &&rhs) {
                  auto dst = std::vector<ull>{};
                  dst.resize(lhs.size() + rhs.size());
                  mpl::algorithms::gcd(std::move(lhs), std::move(rhs), dst);
                  return dst;
                },
                [](auto const &lhs, auto const &rhs) {
                  return mpz_class{gcd(lhs, rhs)};
                }));

  rc::check("LCM matches gmp",
            mpl_gmp_match<2>(
                [](not_cvref auto &&lhs, not_cvref auto &&rhs) {
                  auto dst = std::vector<ull>{};
                  dst.resize(lhs.size() + rhs.size());
                  mpl::algorithms::lcm(std::move(lhs), std::move(rhs), dst);
                  return dst;
                },
                [](auto const &lhs, auto const &rhs) {
                  return mpz_class{lcm(lhs, rhs)};
                }));
}
