#ifndef MPL_ALGORITHMS_FROM_INT_HPP
#define MPL_ALGORITHMS_FROM_INT_HPP

#include "utility.hpp"

#include <concepts>

namespace mpl {
namespace algorithms {
template <typename T>
[[nodiscard]] constexpr auto from_int(std::signed_integral auto x) -> T {
  using namespace impl;

  static_assert(
      sizeof(x) <= sizeof(ull),
      "from_int requires the argument to be no bigger than a long long");

  auto res = T{};
  res.resize(1);
  res[0] = static_cast<ull>(x);

  for (auto i = 1; i < res.size(); i += 1) {
    res[i] = x < 0 ? ~0ull : 0ull;
  }

  return res;
}

template <typename T>
[[nodiscard]] constexpr auto from_int(std::unsigned_integral auto x) -> T {
  using namespace impl;

  auto res = T{};

  if (x == 0) {
    res.resize(1);
    res[0] = 0;
  } else {
    auto i = 0;
    while (x) {
      res.resize(i + 1);
      res[i] = static_cast<ull>(x);

      if constexpr (sizeof(x) > sizeof(ull)) {
        x >>= ull_bits;
      } else {
        x = 0;
      }
      i += 1;
    }

    if (res[i - 1] >> (ull_bits - 1)) {
      res.resize(i + 1);
      res[i] = 0;
    }
  }

  return res;
}

template <typename T> [[nodiscard]] constexpr auto from_int(__int128_t x) -> T {
  using namespace impl;

  auto res = T{};
  res.resize(2);
  res[0] = static_cast<ull>(x);
  res[1] = static_cast<ull>(x >> ull_bits);
  return res;
}
} // namespace algorithms
} // namespace mpl

#endif // MPL_ALGORITHMS_FROM_INT_HPP
