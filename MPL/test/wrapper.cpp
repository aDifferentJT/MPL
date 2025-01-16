#include "../wrapper.hpp"
#include "../int_container3.hpp"

#include <gmpxx.h>

#include <compare>
#include <vector>

#include <rapidcheck.h>

using mpl::ull;
using mpl::algorithms::not_cvref;

// using container = std::vector<ull>;
using container = mpl::int_container3<>;
using dyn_int = mpl::wrapper<container>;

struct integer_string : std::string {
  auto mpl() { return dyn_int{*this, 10}; }
  auto gmp() { return mpz_class{*this, 10}; }
};

struct natural_string : integer_string {};

namespace rc {
template <> struct Arbitrary<integer_string> {
  static auto arbitrary() -> Gen<integer_string> {
    using namespace std::literals;

    return gen::apply(
        [](std::string sign, char first_digit, std::string tail) {
          return integer_string{sign + first_digit + tail};
        },
        gen::element(""s, "-"s), gen::inRange('0', '9'),
        gen::container<std::string>(gen::inRange('0', '9')));
  }
};

template <> struct Arbitrary<natural_string> {
  static auto arbitrary() -> Gen<natural_string> {
    using namespace std::literals;

    return gen::apply(
        [](char first_digit, std::string tail) {
          return natural_string{first_digit + tail};
        },
        gen::inRange('0', '9'),
        gen::container<std::string>(gen::inRange('0', '9')));
  }
};
} // namespace rc

auto check_equality(dyn_int x, mpz_class const &y) -> bool {
  auto x_str = std::stringstream{};
  x_str << std::move(x);
  if (x_str.str() != y.get_str()) {
    std::cerr << x_str.str() << " != " << y.get_str() << '\n';
  }
  return std::move(x_str).str() == y.get_str();
}

struct assertion_failed : std::runtime_error {
  using std::runtime_error::runtime_error;
};

template <auto, typename T> using snd = T;

template <std::size_t arity, typename... Args>
auto mpl_gmp_match(auto &&mpl_func, auto &&gmp_func) {
  return [&]<auto... Is>(std::index_sequence<Is...>) {
    return [&](snd<Is, integer_string>... strs, Args... args) {
      return check_equality(mpl_func(strs.mpl()..., args...),
                            gmp_func(strs.gmp()..., args...));
    };
  }(std::make_index_sequence<arity>{});
}

template <std::size_t arity, typename... Args> auto mpl_gmp_match(auto &&func) {
  return mpl_gmp_match<arity, Args...>(func, func);
}

auto operator<<(std::ostream &os, std::strong_ordering ord) -> std::ostream & {
  if (ord == 0) {
    return os << "eq";
  } else if (ord < 0) {
    return os << "lt";
  } else if (ord > 0) {
    return os << "gt";
  } else {
    return os << "??";
  }
}

int main() {
  if (integer_string{"-0"}.mpl().is_zero()) {
    std::cout << "-0 == 0\n\n";
  } else {
    std::cout << "-0 != 0\n\n";
  }

  rc::check("compare (equality) is reflexive",
            [](integer_string x) { return x.mpl() == x.mpl(); });

  rc::check("compare (equality) with leading zeros is reflexive",
            [](natural_string x) {
              return x.mpl() == natural_string{"0" + x}.mpl();
            });

  rc::check("compare (equality) with negatives and leading zeros is reflexive",
            [](natural_string x) {
              return natural_string{"-" + x}.mpl() ==
                     natural_string{"-0" + x}.mpl();
            });

  rc::check("compare (inequalities) is antisymmetric",
            [](integer_string x, integer_string y) {
              auto ord1 = x.mpl() <=> y.mpl();
              auto ord2 = y.mpl() <=> x.mpl();
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

  rc::check("construct from signed int matches GMP", [](signed int x) {
    return check_equality(dyn_int{x}, mpz_class{x});
  });

  rc::check("construct from unsigned int matches GMP", [](unsigned int x) {
    return check_equality(dyn_int{x}, mpz_class{x});
  });

  rc::check("construct from signed long int matches GMP",
            [](signed long int x) {
              return check_equality(dyn_int{x}, mpz_class{x});
            });

  rc::check("construct from unsigned long int matches GMP",
            [](unsigned long int x) {
              return check_equality(dyn_int{x}, mpz_class{x});
            });

  rc::check("compare matches GMP", [](integer_string x, integer_string y) {
    return (x.mpl() <=> y.mpl()) ==
           mpl::impl::compare_strong_order_fallback(x.gmp(), y.gmp());
  });

  rc::check("Addition matches gmp",
            mpl_gmp_match<2>(
                [](auto const &lhs, auto const &rhs) { return lhs + rhs; }));

  rc::check("In-place Addition matches gmp",
            mpl_gmp_match<2>([](auto lhs, auto const &rhs) {
              lhs += rhs;
              return lhs;
            }));

  rc::check("Subtraction matches gmp",
            mpl_gmp_match<2>(
                [](auto const &lhs, auto const &rhs) { return lhs - rhs; }));

  rc::check("In-place Subtraction matches gmp",
            mpl_gmp_match<2>([](auto lhs, auto const &rhs) {
              lhs -= rhs;
              return lhs;
            }));

  rc::check("Multiplication matches gmp",
            mpl_gmp_match<2>(
                [](auto const &lhs, auto const &rhs) { return lhs * rhs; }));

  rc::check("In-place Multiplication matches gmp",
            mpl_gmp_match<2>([](auto lhs, auto const &rhs) {
              lhs *= rhs;
              return lhs;
            }));

  rc::check("Division matches gmp",
            mpl_gmp_match<2>([](auto const &lhs, auto const &rhs) {
              return rhs == 0 ? rhs : lhs / rhs;
            }));

  rc::check("In-place Division matches gmp",
            mpl_gmp_match<2>([](auto lhs, auto const &rhs) {
              if (rhs != 0) {
                lhs /= rhs;
              }
              return lhs;
            }));

  rc::check("to_float matches gmp", [](integer_string x) {
    if (x.mpl() == 0) {
      return true;
    }
    auto ratio = (x.mpl().to_float() / x.gmp().get_d());
    return ratio > 0.999 && ratio < 1.001;
  });

  rc::check("abs const & matches gmp",
            mpl_gmp_match<1>([](auto const &x) { return x.abs(); },
                             [](auto const &x) { return mpz_class{abs(x)}; }));

  rc::check("abs && matches gmp",
            mpl_gmp_match<1>([](auto const &x) { return mpl::copy(x).abs(); },
                             [](auto const &x) { return mpz_class{abs(x)}; }));

  rc::check("negate matches gmp",
            mpl_gmp_match<1>([](auto const &x) { return -x; }));

  rc::check("div_mod div matches gmp",
            mpl_gmp_match<2>(
                [](auto const &lhs, auto const &rhs) {
                  return rhs == 0 ? rhs : std::get<0>(div_mod(lhs, rhs));
                },
                [](auto const &lhs, auto const &rhs) {
                  return rhs == 0 ? rhs : lhs / rhs;
                }));

  rc::check("div_mod mod matches gmp",
            mpl_gmp_match<2>(
                [](auto const &lhs, auto const &rhs) {
                  return rhs == 0 ? rhs : std::get<1>(div_mod(lhs, rhs));
                },
                [](auto const &lhs, auto const &rhs) {
                  return rhs == 0 ? rhs : lhs % rhs;
                }));

  rc::check("bitwise not matches gmp",
            mpl_gmp_match<1>([](auto const &x) { return ~x; }));

  rc::check(
      "bitwise and matches gmp",
      mpl_gmp_match<2>([](auto const &x, auto const &y) { return x & y; }));

  rc::check(
      "bitwise or matches gmp",
      mpl_gmp_match<2>([](auto const &x, auto const &y) { return x | y; }));

  rc::check(
      "bitwise xor matches gmp",
      mpl_gmp_match<2>([](auto const &x, auto const &y) { return x ^ y; }));

  rc::check("shift left matches gmp", [](integer_string x) {
    auto i = *rc::gen::inRange<int>(0, 6 * x.size());
    return check_equality(x.mpl() << i, x.gmp() << i);
  });

  rc::check("set_bit 0 matches gmp", [](integer_string x) {
    auto i = *rc::gen::inRange<int>(0, 6 * x.size());
    auto mpl = x.mpl();
    auto gmp = x.gmp();
    mpl.set_bit(i, 0);
    mpz_clrbit(gmp.get_mpz_t(), i);
    return check_equality(mpl, gmp);
  });

  rc::check("set_bit 1 matches gmp", [](integer_string x) {
    auto i = *rc::gen::inRange<int>(0, 6 * x.size());
    auto mpl = x.mpl();
    auto gmp = x.gmp();
    mpl.set_bit(i, 1);
    mpz_setbit(gmp.get_mpz_t(), i);
    return check_equality(mpl, gmp);
  });

  rc::check("is_bit_set matches gmp", [](integer_string x) {
    auto i = *rc::gen::inRange<int>(0, 6 * x.size());
    auto mpl = x.mpl();
    auto gmp = x.gmp();
    return mpl.bit_is_set(i) == mpz_tstbit(gmp.get_mpz_t(), i);
  });

  rc::check("get_bit_range matches gmp", [](integer_string x) {
    auto low = *rc::gen::inRange<int>(0, 6 * x.size());
    auto bit_count = *rc::gen::inRange<int>(0, 6 * x.size());

    auto gmp = [&] {
      uint32_t high = low + bit_count - 1;
      mpz_class rem, div;
      mpz_fdiv_r_2exp(rem.get_mpz_t(), x.gmp().get_mpz_t(), high + 1);
      mpz_fdiv_q_2exp(div.get_mpz_t(), rem.get_mpz_t(), low);
      return div;
    }();

    return check_equality(x.mpl().get_bit_range(bit_count, low), gmp);
  });

  rc::check("one_extend matches gmp", [](integer_string x) {
    // TODO
    if (x.mpl().is_negative()) {
      return true;
    }

    auto size = *rc::gen::inRange<int>(x.mpl().length(), 2 * x.mpl().length());
    auto amount = *rc::gen::inRange<int>(0, 2 * x.mpl().length());

    auto gmp = [&] {
      mpz_class res = x.gmp();

      for (unsigned i = size; i < size + amount; ++i) {
        mpz_setbit(res.get_mpz_t(), i);
      }
      return res;
    }();

    return check_equality(x.mpl().one_extend(size, amount), gmp);
  });

  rc::check("mod_pow_2 matches gmp", [](integer_string x) {
    auto exp = *rc::gen::inRange<int>(0, 6 * x.size());
    auto gmp = mpz_class{};
    mpz_fdiv_r_2exp(gmp.get_mpz_t(), x.gmp().get_mpz_t(), exp);
    return check_equality(x.mpl().mod_pow_2(exp), gmp);
  });

  rc::check("is_pow_2 return correctly on 2^k", []() {
    auto k = *rc::gen::inRange<int>(0, 1000);
    auto x = mpl::wrapper<std::vector<mpl::ull>>{1} << k;
    return x.is_pow_2() == k + 1;
  });

  rc::check("is_pow_2 return correctly on non powers of 2",
            [](integer_string x) {
              return mpz_popcount(x.gmp().get_mpz_t()) == 1 ||
                     x.mpl().is_pow_2() == 0;
            });

  rc::check("length matches gmp", [](integer_string x) {
    if (x.mpl().signum() == 0) {
      return x.mpl().length() == 1;
    } else {
      return x.mpl().length() == mpz_sizeinbase(x.gmp().get_mpz_t(), 2);
    }
  });

  rc::check("pow matches gmp", [](integer_string x) {
    auto exp = *rc::gen::inRange<int>(0, 50);
    auto gmp = mpz_class{};
    mpz_pow_ui(gmp.get_mpz_t(), x.gmp().get_mpz_t(), exp);
    return check_equality(pow(x.mpl(), exp), gmp);
  });

  rc::check(
      "gcd matches gmp",
      mpl_gmp_match<2>([](auto const &x, auto const &y) { return gcd(x, y); }));

  rc::check(
      "lcm matches gmp",
      mpl_gmp_match<2>([](auto const &x, auto const &y) { return lcm(x, y); }));

  rc::check("mult on lvalues leaves them the same",
            mpl_gmp_match<2>([](auto const &x, auto const &y) {
              auto x2 = x;
              (void)(x2 * y);
              return x2;
            }));
}
