
#include "../int_container3.hpp"
#include <iostream>

#include <algorithm>
#include <vector>

#include <rapidcheck.h>

using mpl::ull;

namespace rc {
template <std::size_t small_size> struct Arbitrary<mpl::int_container3<small_size>> {
  static auto arbitrary() -> Gen<mpl::int_container3<small_size>> {
    return gen::container<mpl::int_container3<small_size>>(gen::arbitrary<ull>());
  }
};
} // namespace rc

int main() {
  rc::check("Writing into a small int_container3 reads out the same",
            [](mpl::int_container3<> xs, ull x) {
              auto i = *rc::gen::inRange<std::size_t>(0, xs.size());
              xs[i] = x;
              return xs[i] == x;
            });

  rc::check("int_container3 constructed from vector is at least the size",
            [](std::vector<ull> xs) {
              auto ys = mpl::int_container3{xs.begin(), xs.end()};
              return ys.size() >= xs.size();
            });

  rc::check("int_container3 constructed from vector compares equal in prefix",
            [](std::vector<ull> xs) {
              auto ys = mpl::int_container3{xs.begin(), xs.end()};
              return std::equal(xs.begin(), xs.end(), ys.begin());
            });

  rc::check("int_container3 constructed from vector is zero in suffix",
            [](std::vector<ull> xs) {
              auto ys = mpl::int_container3{xs.begin(), xs.end()};
              return std::all_of(ys.begin() + xs.size(), ys.end(),
                                 [](ull x) { return x == 0; });
            });

  rc::check("int_container3 expand is bigger",
            [](mpl::int_container3<> xs) {
              auto size1 = xs.size();
              xs.expand();
              auto size2 = xs.size();
              return size2 > size1;
            });

  rc::check("int_container3 push_back is bigger",
            [](mpl::int_container3<> xs, ull x) {
              auto size1 = xs.size();
              xs.push_back(x);
              auto size2 = xs.size();
              return size2 > size1;
            });

  rc::check("int_container3 push_back has that at the end",
            [](mpl::int_container3<> xs, ull x) {
              xs.push_back(x);
              return xs[xs.size() - 1] == x;
            });

  rc::check("int_container3 push_back leaves current elements the same",
            [](mpl::int_container3<> xs, ull x) {
              auto ys = xs;
              ys.push_back(x);
              return std::equal(xs.begin(), xs.end(), ys.begin());
            });
}
