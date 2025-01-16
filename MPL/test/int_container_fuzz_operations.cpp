
#include "../int_container.hpp"
#include <iostream>

#include <algorithm>
#include <vector>

#include <rapidcheck.h>

using mpl::ull;

namespace rc {
template <> struct Arbitrary<mpl::int_container> {
  static auto arbitrary() -> Gen<mpl::int_container> {
    return gen::container<mpl::int_container>(gen::arbitrary<ull>());
  }
};
} // namespace rc

auto make_operation(auto op) { return make_operation(op, op); }

auto make_operation(
    auto(*mpl_op)(mpl::int_container &&, auto...)->mpl::int_container,
    auto(*gmp_op)(mpz_class, auto...))
    -> std::tuple<auto(*)(mpl::int_container &&)->mpl::int_container,
                  auto(*)(mpz_class)->mpz_class> {
                          return {};
                  }

static constexpr auto operations = std::array

    int
    main() {
  rc::check("Writing into a small int_container reads out the same",
            [](mpl::int_container xs, ull x) {
              auto i = *rc::gen::inRange<std::size_t>(0, xs.size());
              xs[i] = x;
              return xs[i] == x;
            });

  rc::check("int_container constructed from vector is at least the size",
            [](std::vector<ull> xs) {
              auto ys = mpl::int_container{xs.begin(), xs.end()};
              return ys.size() >= xs.size();
            });

  rc::check("int_container constructed from vector compares equal in prefix",
            [](std::vector<ull> xs) {
              auto ys = mpl::int_container{xs.begin(), xs.end()};
              return std::equal(xs.begin(), xs.end(), ys.begin());
            });

  rc::check("int_container constructed from vector is zero in suffix",
            [](std::vector<ull> xs) {
              auto ys = mpl::int_container{xs.begin(), xs.end()};
              return std::all_of(ys.begin() + xs.size(), ys.end(),
                                 [](ull x) { return x == 0; });
            });

  rc::check("int_container expand is bigger", [](mpl::int_container xs) {
    auto size1 = xs.size();
    xs.expand();
    auto size2 = xs.size();
    return size2 > size1;
  });

  rc::check("int_container push_back is bigger",
            [](mpl::int_container xs, ull x) {
              auto size1 = xs.size();
              xs.push_back(x);
              auto size2 = xs.size();
              return size2 > size1;
            });

  rc::check("int_container push_back has that at the end",
            [](mpl::int_container xs, ull x) {
              xs.push_back(x);
              return xs[xs.size() - 1] == x;
            });

  rc::check("int_container push_back leaves current elements the same",
            [](mpl::int_container xs, ull x) {
              auto ys = xs;
              ys.push_back(x);
              return std::equal(xs.begin(), xs.end(), ys.begin());
            });
}
