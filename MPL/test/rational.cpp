#include "../int_container3.hpp"
#include "../rational.hpp"

#include <gmpxx.h>

#include <compare>
#include <vector>

#include <rapidcheck.h>

using mpl::ull;
using mpl::algorithms::not_cvref;

//using container = std::vector<ull>;
using container = mpl::int_container3<>;
using dyn_int = mpl::wrapper<container>;
using dyn_rational = mpl::rational<container>;

struct integer_string : std::string {
  auto mpl() { return dyn_int{*this}; }
  auto gmp() { return mpz_class{*this}; }
};

struct natural_string : std::string {};

struct rational_string : std::string {
  auto mpl() { return dyn_rational{*this}; }
  auto gmp() {
    auto res = mpq_class{*this};
    res.canonicalize();
    return res;
  }
};

struct real_string : std::string {
  auto mpl() { return dyn_rational::from_decimal(*this); }
  auto dbl() -> double { return std::stod(*this); }
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

template <> struct Arbitrary<natural_string> {
  static auto arbitrary() -> Gen<natural_string> {
    using namespace std::literals;

    return gen::apply(
        [](char first_digit, std::string tail) {
          return natural_string{first_digit + tail};
        },
        gen::inRange('1', '9'),
        gen::container<std::string>(gen::inRange('0', '9')));
  }
};

template <> struct Arbitrary<rational_string> {
  static auto arbitrary() -> Gen<rational_string> {
    using namespace std::literals;

    return gen::apply(
        [](integer_string numerator, natural_string denominator) {
          return rational_string{numerator + "/" + denominator};
        },
        gen::arbitrary<integer_string>(), gen::arbitrary<natural_string>());
  }
};

template <> struct Arbitrary<real_string> {
  static auto arbitrary() -> Gen<real_string> {
    using namespace std::literals;

    return gen::apply(
        [](std::string sign, char first_digit, std::string tail,
           std::string fraction, char last_digit) {
          return real_string{sign + first_digit + tail + "." + fraction +
                             last_digit};
        },
        gen::element(""s, "-"s), gen::inRange('1', '9'),
        gen::container<std::string>(gen::inRange('0', '9')),
        gen::container<std::string>(gen::inRange('0', '9')),
        gen::inRange('1', '9'));
  }
};
} // namespace rc

auto check_equality(dyn_rational x, mpq_class const &y)
    -> bool {
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
    return [&](snd<Is, rational_string>... strs, Args... args) {
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
  {
    auto x = rational_string{"2025130727/2000000000"};
    auto y = rational_string{"16109045816000/46578006721"};
    auto res_mpl = x.mpl() * y.mpl();
    std::cout << "MPL: " << res_mpl << '\n';
    std::cout << "GMP: " << x.gmp() * y.gmp() << '\n';
  }

  {
          auto x = rational_string{"-121/144"};
          auto y = x.mpl();
          auto z = y;
          auto sgn_mpl = z.signum();
          std::cout << "MPL: " << sgn_mpl << '\n';
  }

  rc::check("compare (equality) is reflexive",
            [](rational_string x) { return x.mpl() == x.mpl(); });

  rc::check("compare (inequalities) is antisymmetric",
            [](rational_string x, rational_string y) {
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

  rc::check("compare matches GMP", [](rational_string x, rational_string y) {
    return (x.mpl() <=> y.mpl()) ==
           mpl::impl::compare_strong_order_fallback(x.gmp(), y.gmp());
  });

  rc::check("from_decimal matches double", [](real_string x) {
    auto res = x.mpl().to_float() / x.dbl();
    return res > 0.99 && res < 1.01;
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
            mpl_gmp_match<2>(
                [](auto const &lhs, auto const &rhs) { return lhs / rhs; }));

  rc::check("In-place Division matches gmp",
            mpl_gmp_match<2>([](auto lhs, auto const &rhs) {
              lhs /= rhs;
              return lhs;
            }));

  rc::check("to_float matches gmp", [](rational_string x) {
    auto ratio = (x.mpl().to_float() / x.gmp().get_d());
    return ratio > 0.999 && ratio < 1.001;
  });

  rc::check("floor matches gmp",
            mpl_gmp_match<1>([](auto const &x) { return x.floor(); },
                             [](auto const &x) {
                               auto q = mpz_class{};
                               mpz_fdiv_q(q.get_mpz_t(), x.get_num_mpz_t(),
                                          x.get_den_mpz_t());
                               return q;
                             }));

  rc::check("ceiling matches gmp",
            mpl_gmp_match<1>([](auto const &x) { return x.ceiling(); },
                             [](auto const &x) {
                               auto q = mpz_class{};
                               mpz_cdiv_q(q.get_mpz_t(), x.get_num_mpz_t(),
                                          x.get_den_mpz_t());
                               return q;
                             }));
}
