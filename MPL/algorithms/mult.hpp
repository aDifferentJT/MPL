#ifndef MPL_ALGORITHMS_MULT_HPP
#define MPL_ALGORITHMS_MULT_HPP

#include "../dyn_buffer.hpp"
#include "add_sub.hpp"
#include "utility.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstring>
#include <iostream>
#include <iterator>
#include <memory>
#include <ranges>
#include <span>

#include <gmp.h>

// #define BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED
// #include <boost/stacktrace.hpp>

namespace mpl {
namespace algorithms {
namespace impl {
[[nodiscard]] constexpr auto mult128(ull x, ull y) noexcept -> __uint128_t {
  return static_cast<__uint128_t>(x) * static_cast<__uint128_t>(y);
}

template <typename Allocator = std::allocator<ull>>
constexpr void mpl_unsigned_mult_and_add(auto const &x, ull y, auto &&dst,
                                         Allocator allocator = {}) {
  assert(dst.size() >= x.size());
  auto x_it = x.begin();
  auto dst_it = dst.begin();
  for (; x_it != x.end(); ++x_it, ++dst_it) {
    assert(dst_it != dst.end());
    auto wide_tmp = impl::mult128(y, *x_it);
    auto tmp_bottom = static_cast<ull>(wide_tmp);
    auto tmp_top = static_cast<ull>(wide_tmp >> 64);
    auto carry = 0ull;
    auto dst_it2 = dst_it;
    *dst_it2 = __builtin_addcll(*dst_it2, tmp_bottom, carry, &carry);
    if (tmp_top || carry) {
      ++dst_it2;
      assert(dst_it2 != dst.end());
      *dst_it2 = __builtin_addcll(*dst_it2, tmp_top, carry, &carry);
      while (carry) {
        ++dst_it2;
        assert(dst_it2 != dst.end());
        *dst_it2 = __builtin_addcll(*dst_it2, 0, carry, &carry);
      }
    }
  }
}

template <typename Allocator = std::allocator<ull>>
constexpr void mpl_unsigned_mult_and_add(ull x, auto const &y, auto &&dst,
                                         Allocator allocator = {}) {
  mpl_unsigned_mult_and_add(y, x, dst, allocator);
}

template <typename Allocator = std::allocator<ull>>
constexpr void mpl_unsigned_mult_and_add(auto const &x, auto const &y,
                                         not_const auto &&dst,
                                         Allocator allocator = {}) {
  /*
  if (!(dst.size() >= x.size() + y.size())) {
    std::cout << boost::stacktrace::stacktrace();
  }
  */
  assert(dst.size() >= x.size() + y.size());

  if (x.size() == 0 || y.size() == 0) {
  } else if (x.size() == 1) {
    mpl_unsigned_mult_and_add(*x.begin(), y, dst, allocator);
  } else if (y.size() == 1) {
    mpl_unsigned_mult_and_add(x, *y.begin(), dst, allocator);
  } else if (x.size() == 2) {
    auto x_it = x.begin();
    auto dst_it = dst.begin();
    for (; x_it != x.end(); ++x_it, ++dst_it) {
      mpl_unsigned_mult_and_add(*x_it, y, view{dst_it, dst.end()}, allocator);
    }
  } else if (y.size() == 2) {
    mpl_unsigned_mult_and_add(y, x, dst, allocator);
  } else if (x.size() <= y.size()) { // TODO inefficient
    auto x_it = x.begin();
    auto dst_it = dst.begin();
    for (; x_it != x.end(); ++x_it, ++dst_it) {
      mpl_unsigned_mult_and_add(*x_it, y, view{dst_it, dst.end()}, allocator);
    }
  } else if (true) { // TODO inefficient
    mpl_unsigned_mult_and_add(y, x, dst, allocator);
  } else {
    // TODO fix this case
    std::cerr << "Impossible\n";
    std::terminate();

    /*
    auto x_span = std::span{x};
    auto y_span = std::span{y};
    auto dst_span = std::span{dst};

    auto half =
        std::max(x.size(), y.size()) - (std::max(x.size(), y.size()) / 2);

    assert(half < std::max(x.size(), y.size()));
    assert(std::max(x.size(), y.size()) - half < std::max(x.size(), y.size()));

    // x = az + b
    // y = cz + d
    auto a = x_span.subspan(half);
    auto b = x_span.subspan(0, half);
    auto c = y_span.subspan(half);
    auto d = y_span.subspan(0, half);

    {
      std::cerr << "x.size(): " << x.size() << ", y.size(): " << y.size()
                << ", half: " << half << ", a.size(): " << a.size()
                << ", b.size(): " << b.size() << '\n';

      auto apb = dyn_buffer<ull, Allocator>{half, allocator};
      std::cerr << "apb.size(): " << apb.size() << '\n';
      {
        [[maybe_unused]] auto carry = unsigned_ripple_adder(a, b, apb);
        assert(!carry);
      }

      auto cpd = dyn_buffer<ull, Allocator>{half, allocator};
      std::cerr << "cpd.size(): " << cpd.size() << '\n';
      {
        [[maybe_unused]] auto carry = unsigned_ripple_adder(c, d, cpd);
        assert(!carry);
      }

      std::cerr << "Adding done\n";

      // into z += (a+b)(c+d)
      // into z += ac+ad+bc+bd
      assert(std::max(apb.size(), cpd.size()) < std::max(x.size(), y.size()));
      mpl_unsigned_mult_and_add(apb, cpd, dst_span.subspan(half), allocator);
      std::cerr << "z += (a+b)*(c+d) done\n";
    }

    {
      auto ac = dyn_buffer<ull, Allocator>{half * 2, allocator};
      assert(std::max(a.size(), c.size()) < std::max(x.size(), y.size()));
      mpl_unsigned_mult_and_add(a, c, ac, allocator);
      std::cerr << "a * c done\n";

      // into z^2 += ac
      std::cerr << "half: " << half << ", dst_span.size(): " << dst_span.size()
                << ", ac.size(): " << ac.size() << '\n';
      std::cerr << "dst_span.subspan(half * 2).size(): "
                << dst_span.subspan(half * 2).size() << '\n';
      {
        [[maybe_unused]] auto carry = unsigned_ripple_adder(
            dst_span.subspan(half * 2), ac, dst_span.subspan(half * 2));
        assert(!carry);
      }
      std::cerr << "z^2 += ac done\n";

      // into z -= ac
      {
        [[maybe_unused]] auto carry =
            ripple_subber(dst_span.subspan(half), ac, dst_span.subspan(half));
        assert(!carry);
      }
      std::cerr << "z -= ac done\n";
    }

    {
      auto bd = dyn_buffer<ull, Allocator>{half * 2, allocator};
      assert(std::max(b.size(), d.size()) < std::max(x.size(), y.size()));
      mpl_unsigned_mult_and_add(b, d, bd, allocator);
      std::cerr << "b * d done\n";

      // into 1 += bd
      {
        [[maybe_unused]] auto carry =
            unsigned_ripple_adder(dst_span.subspan(0), bd, dst_span.subspan(0));
        assert(!carry);
      }
      std::cerr << "1 += bd done\n";

      // into z -= bd
      {
        [[maybe_unused]] auto carry =
            ripple_subber(dst_span.subspan(half), bd, dst_span.subspan(half));
        assert(!carry);
      }
      std::cerr << "z -= bd done\n";
    }
      */
  }
}

// x >= y
inline void gmp_unsigned_mult5(std::span<mp_limb_t const> x,
                               std::span<mp_limb_t const> y,
                               std::span<mp_limb_t> dst) {
  assert(x.size() > 0);
  assert(y.size() > 0);
  assert(dst.size() >= x.size() + y.size());
  mpn_mul(dst.data(), x.data(), x.size(), y.data(), y.size());
  std::fill(dst.begin() + x.size() + y.size(), dst.end(), 0);
}

// x >= y
inline void gmp_unsigned_mult4(std::span<mp_limb_t const> x,
                               std::span<mp_limb_t const> y,
                               not_const auto &&dst, auto allocator) {
  using it = std::remove_cvref_t<decltype(dst.begin())>;
  if constexpr (mpn_compatible_iterator<it>) {
    gmp_unsigned_mult5(
        x, y,
        std::span{reinterpret_cast<mp_limb_t *>(&*dst.begin()), dst.size()});
  } else {
    auto dst2 = dyn_buffer<mp_limb_t>{
        dst.size() * sizeof(mp_limb_t) /
            sizeof(typename std::iterator_traits<it>::value_type),
        allocator};
    gmp_unsigned_mult5(x, y, dst2);
    static_assert(std::endian::native == std::endian::little,
                  "Only little endian is supported");
    auto dst2_ptr = reinterpret_cast<std::byte *>(dst2.data());
    for (auto &&dst_quad : dst) {
      ull tmp;
      std::memcpy(&tmp, dst2_ptr, sizeof(tmp));
      dst_quad = tmp;
      dst2_ptr += sizeof(dst_quad);
    }
  }
}

// x >= y
template <typename ItY>
inline void gmp_unsigned_mult3(std::span<mp_limb_t const> x, view<ItY> y,
                               not_const auto &&dst, auto allocator) {
  if constexpr (mpn_compatible_iterator<ItY>) {
    gmp_unsigned_mult4(
        x,
        std::span{reinterpret_cast<mp_limb_t const *>(&*y.begin()), y.size()},
        dst, allocator);
  } else {
    auto y2 = dyn_buffer<mp_limb_t>{
        y.size() * sizeof(mp_limb_t) /
            sizeof(typename std::iterator_traits<ItY>::value_type),
        allocator};
    static_assert(std::endian::native == std::endian::little,
                  "Only little endian is supported");
    auto y2_ptr = reinterpret_cast<std::byte *>(y2.data());
    for (ull y_quad : y) {
      std::memcpy(y2_ptr, &y_quad, sizeof(y_quad));
      y2_ptr += sizeof(y_quad);
    }
    gmp_unsigned_mult4(x, y2, dst, allocator);
  }
}

// x >= y
template <typename ItX, typename ItY>
inline void gmp_unsigned_mult2(view<ItX> x, view<ItY> y, not_const auto &&dst,
                               auto allocator) {
  if constexpr (mpn_compatible_iterator<ItX>) {
    gmp_unsigned_mult3(
        std::span{reinterpret_cast<mp_limb_t const *>(&*x.begin()), x.size()},
        y, dst, allocator);
  } else {
    auto x2 = dyn_buffer<mp_limb_t>{
        x.size() * sizeof(mp_limb_t) /
            sizeof(typename std::iterator_traits<ItX>::value_type),
        allocator};
    static_assert(std::endian::native == std::endian::little,
                  "Only little endian is supported");
    auto x2_ptr = reinterpret_cast<std::byte *>(x2.data());
    for (ull x_quad : x) {
      std::memcpy(x2_ptr, &x_quad, sizeof(x_quad));
      x2_ptr += sizeof(x_quad);
    }
    gmp_unsigned_mult3(x2, y, dst, allocator);
  }
}

inline void gmp_unsigned_mult(auto const &x, auto const &y,
                              not_const auto &&dst, auto allocator) {
  static_assert(sizeof(mp_limb_t) == sizeof(ull));

  if (x.size() >= y.size()) {
    return gmp_unsigned_mult2(x, y, dst, allocator);
  } else {
    return gmp_unsigned_mult2(y, x, dst, allocator);
  }
}

inline void gmp_unsigned_mult_1_4(std::span<mp_limb_t const> x, ull y,
                                  std::span<mp_limb_t> dst) {
  assert(dst.size() >= x.size() + 1);

  auto carry = mpn_mul_1(dst.data(), x.data(), x.size(), y);
  dst[x.size()] = carry;
  std::fill(dst.begin() + x.size() + 1, dst.end(), 0);
}

inline void gmp_unsigned_mult_1_3(std::span<mp_limb_t const> x, ull y,
                                  not_const auto &&dst, auto allocator) {
  using it = std::remove_cvref_t<decltype(dst.begin())>;
  if constexpr (mpn_compatible_iterator<it>) {
    gmp_unsigned_mult_1_4(
        x, y,
        std::span{reinterpret_cast<mp_limb_t *>(&*dst.begin()), dst.size()});
  } else {
    auto dst2 = dyn_buffer<mp_limb_t>{
        dst.size() * sizeof(mp_limb_t) /
            sizeof(typename std::iterator_traits<it>::value_type),
        allocator};
    gmp_unsigned_mult_1_4(x, y, dst2);
    static_assert(std::endian::native == std::endian::little,
                  "Only little endian is supported");
    auto dst2_ptr = reinterpret_cast<std::byte *>(dst2.data());
    for (auto &&dst_quad : dst) {
      ull tmp;
      std::memcpy(&tmp, dst2_ptr, sizeof(tmp));
      dst_quad = tmp;
      dst2_ptr += sizeof(dst_quad);
    }
  }
}

template <typename ItX>
inline void gmp_unsigned_mult_1_2(view<ItX> x, ull y, not_const auto &&dst,
                                  auto allocator) {
  if constexpr (mpn_compatible_iterator<ItX>) {
    gmp_unsigned_mult_1_3(
        std::span{reinterpret_cast<mp_limb_t const *>(&*x.begin()), x.size()},
        y, dst, allocator);
  } else {
    auto x2 = dyn_buffer<mp_limb_t>{
        x.size() * sizeof(mp_limb_t) /
            sizeof(typename std::iterator_traits<ItX>::value_type),
        allocator};
    static_assert(std::endian::native == std::endian::little,
                  "Only little endian is supported");
    auto x2_ptr = reinterpret_cast<std::byte *>(x2.data());
    for (ull x_quad : x) {
      std::memcpy(x2_ptr, &x_quad, sizeof(x_quad));
      x2_ptr += sizeof(x_quad);
    }
    gmp_unsigned_mult_1_3(x2, y, dst, allocator);
  }
}

template <typename ItX>
inline void gmp_unsigned_mult(view<ItX> x, ull y, not_const auto &&dst,
                              auto allocator) {
  static_assert(sizeof(mp_limb_t) == sizeof(ull));

  assert(x.size() > 0);

  return gmp_unsigned_mult_1_2(x, y, dst, allocator);
}

template <typename ItX, typename ItY, typename Allocator = std::allocator<ull>>
void unsigned_mult(view<ItX> x, view<ItY> y, not_const auto &&dst,
                   Allocator allocator = {}) {
  if (x.size() == 0 || y.size() == 0) {
    std::fill(dst.begin(), dst.end(), 0);
  } else {
    /*
    std::fill(dst.begin(), dst.end(), 0);
    mpl_unsigned_mult_and_add(x, y, dst, allocator);
    */
    gmp_unsigned_mult(x, y, dst, allocator);
  }
}

template <typename Allocator = std::allocator<ull>>
void unsigned_mult(ull x, auto const &y, not_const auto &&dst,
                   Allocator allocator = {}) {
  unsigned_mult(y, x, static_cast<decltype(dst)>(dst), allocator);
}

template <typename ItX, typename Allocator = std::allocator<ull>>
void unsigned_mult(view<ItX> x, ull y, not_const auto &&dst,
                   Allocator allocator = {}) {
  if (x.size() == 0) {
    std::fill(dst.begin(), dst.end(), 0);
  } else {
    /*
    std::fill(dst.begin(), dst.end(), 0);
    mpl_unsigned_mult_and_add(x, y, dst, allocator);
    */
    gmp_unsigned_mult(x, y, dst, allocator);
  }
}
} // namespace impl

// It is assumed that dst is large enough to contain the result

template <typename Allocator = std::allocator<ull>>
void square(not_const auto &&x, not_const auto &&dst,
            Allocator allocator = {}) {
  // TODO use gmp_sqr
  auto negate_x = is_negative(x);

  if (negate_x) {
    negate(x);
  }

  auto x2 = impl::unsafe_trim_leading_zeros(x);
  impl::unsigned_mult(x2, x2, dst, allocator);

  if (negate_x) {
    negate(x);
  }
}

namespace impl {
template <typename T1, typename T2>
auto heterogenous_eq(T1 const &x, T2 const &y) -> bool {
  if constexpr (std::is_same_v<T1, T2>) {
    return x == y;
  } else {
    return false;
  }
}
} // namespace impl

template <typename Allocator = std::allocator<ull>>
void mult(not_const auto &&x, not_const auto &&y, not_const auto &&dst,
          Allocator allocator = {}) {
  if (impl::heterogenous_eq(x.begin(), y.begin())) {
    square(x, dst, allocator);
  } else {
    auto negate_x = is_negative(x);
    auto negate_y = is_negative(y);

    if (negate_x) {
      negate(x);
    }

    if (negate_y) {
      negate(y);
    }

    impl::unsigned_mult(impl::unsafe_trim_leading_zeros(x),
                        impl::unsafe_trim_leading_zeros(y), dst, allocator);

    if (negate_x != negate_y) {
      negate(dst);
    }

    if (negate_x) {
      negate(x);
    }
    if (negate_y) {
      negate(y);
    }
  }
}
} // namespace algorithms
} // namespace mpl

#endif // MPL_ALGORITHMS_MULT_HPP
