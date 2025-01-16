#ifndef MPL_ALGORITHMS_GCD_HPP
#define MPL_ALGORITHMS_GCD_HPP

#include "divide.hpp"
#include "mult.hpp"
#include "utility.hpp"

#include "to_string.hpp"

#include <vector>

namespace mpl {
namespace algorithms {
namespace impl {
template <typename ItX, typename ItY>
void mpl_positive_gcd(view<ItX> x, view<ItY> y, not_const auto &&dst) {
  while (true) {
    if (is_lt(compare(x, y))) {
      using std::swap;
      swap(x, y);
    }

    // x %= y;
    // auto quotient = dyn_buffer<ull>(x.size());
    auto quotient = std::vector<ull>{};
    quotient.resize(x.size());
    // divide(x, std::move(y), quotient);
    divide(x, auto{y}, quotient);

    if (is_zero(x)) {
      auto y2 = mpl::algorithms::impl::trim_leading_sign_bits(y);
      std::copy(y2.begin(), y2.end(), dst.begin());
      return;
    }
  }
}

inline void gmp_positive_gcd4(std::span<mp_limb_t> x, std::span<mp_limb_t> y,
                              std::span<mp_limb_t> dst) {
  auto written_size =
      mpn_gcd(dst.data(), x.data(), x.size(), y.data(), y.size());
  std::fill(dst.begin() + written_size, dst.end(), 0);
}

inline void gmp_positive_gcd3(std::span<mp_limb_t> x, std::span<mp_limb_t> y,
                              not_const auto &&dst, auto allocator) {
  using it = std::remove_cvref_t<decltype(dst.begin())>;
  if constexpr (mpn_compatible_iterator<it>) {
    gmp_positive_gcd4(
        x, y,
        std::span{reinterpret_cast<mp_limb_t *>(&*dst.begin()), dst.size()});
  } else {
  }
}

template <typename ItY>
inline void gmp_positive_gcd2(std::span<mp_limb_t> x, view<ItY> y,
                              not_const auto &&dst, auto allocator) {
  if constexpr (mpn_compatible_iterator<ItY>) {
    gmp_positive_gcd3(
        x, std::span{reinterpret_cast<mp_limb_t *>(&*y.begin()), y.size()}, dst,
        allocator);
  } else {
  }
}

template <typename ItX, typename ItY>
void gmp_positive_gcd(view<ItX> x, view<ItY> y, not_const auto &&dst,
                      auto allocator) {
  if constexpr (mpn_compatible_iterator<ItX>) {
    gmp_positive_gcd2(
        std::span{reinterpret_cast<mp_limb_t *>(&*x.begin()), x.size()}, y, dst,
        allocator);
  } else {
  }
}

void positive_gcd(not_cvref auto &&x, not_cvref auto &&y, not_const auto &&dst,
                  auto allocator) {
  if (is_zero(x)) {
    auto y2 = impl::trim_leading_sign_bits(y);
    std::copy(y2.begin(), y2.end(), dst.begin());
    return;
  }

  if (is_zero(y)) {
    auto x2 = impl::trim_leading_sign_bits(x);
    std::copy(x2.begin(), x2.end(), dst.begin());
    return;
  }

  auto x2 = impl::unsafe_trim_leading_zeros(x);
  auto y2 = impl::unsafe_trim_leading_zeros(y);

  while (x2.begin() != x2.end() && y2.begin() != y2.end() && *x2.begin() == 0 &&
         *y2.begin() == 0) {
    ++x2._begin;
    ++y2._begin;
  }

  auto shift =
      (std::min)(__builtin_ctz(*x2.begin()), __builtin_ctz(*y2.begin()));
  impl::small_logical_right_shift(x2, shift, x2);
  impl::small_logical_right_shift(y2, shift, y2);

  auto x3 = impl::unsafe_trim_leading_zeros(x2);
  auto y3 = impl::unsafe_trim_leading_zeros(y2);

  if (x3.size() >= y3.size()) {
    gmp_positive_gcd(x3, y3, dst, allocator);
  } else {
    gmp_positive_gcd(y3, x3, dst, allocator);
  }

  impl::small_left_shift(dst, shift, dst);
}
} // namespace impl

void gcd(not_cvref auto &&x, not_cvref auto &&y, not_const auto &&dst) {
  assert(dst.size() >= (std::min)(x.size(), y.size()));

  if (is_negative(x)) {
    negate(x);
  }
  if (is_negative(y)) {
    negate(y);
  }

  impl::positive_gcd(std::move(x), std::move(y), dst,
                     std::allocator<mp_limb_t>{});
}

// (old_x, x) := (x, old_x - quotient * x)
void extended_gcd_helper(not_const auto &&x, not_const auto &&old_x,
                         auto const &quotient, not_const auto &&tmp) {
  // tmp := quotient * x
  mult<std::vector<ull>>(quotient, x, tmp);

  auto tmp2 = std::span{tmp.data(), x.size()};

  // tmp := old_x - quotient * x
  auto carry = impl::ripple_subber(old_x, tmp2, tmp2);
  assert(!carry);

  // old_x := x
  std::copy(x.cbegin(), x.cend(), old_x.begin());

  // x := old_x - quotient * x
  // C++23 adds cbegin cend
  std::copy(tmp2.begin(), tmp2.end(), x.begin());
}

// TODO this doesn't seem quite right
void extended_gcd(auto const &x, auto const &y, not_const auto &&dst,
                  not_const auto &&dst_a, not_const auto &&dst_b) {
  auto size = std::max(x.size(), y.size());
  auto old_r = std::vector<ull>{x.cbegin(), x.cend()};
  auto r = std::vector<ull>{y.cbegin(), y.cend()};
  auto old_s = from_int<std::vector<ull>>(1);
  auto s = std::vector<ull>{};
  auto old_t = std::vector<ull>{};
  auto t = from_int<std::vector<ull>>(1);
  auto quotient = std::vector<ull>{};
  auto tmp = std::vector<ull>{};
  old_r.resize(size, sar(old_r.back(), ull_bits - 1));
  r.resize(size, sar(r.back(), ull_bits - 1));
  old_s.resize(size);
  s.resize(size);
  old_t.resize(size);
  t.resize(size);
  quotient.resize(size);
  tmp.resize(size * 2);

  while (!is_zero(r)) {
    divide(auto{old_r}, auto{r}, quotient);
    extended_gcd_helper(r, old_r, quotient, tmp);
    extended_gcd_helper(s, old_s, quotient, tmp);
    extended_gcd_helper(t, old_t, quotient, tmp);
  }

  std::copy(old_r.cbegin(), old_r.cend(), dst.begin());
  std::copy(old_s.cbegin(), old_s.cend(), dst_a.begin());
  std::copy(old_t.cbegin(), old_t.cend(), dst_b.begin());

  if (is_negative(dst)) {
    negate(dst);
    negate(dst_a);
    negate(dst_b);
  }

  /*
  // TODO fill in dst_a and dst_b
assert(dst.size() >= (std::min)(x.size(), y.size()));

if (is_zero(x)) {
if (is_negative(y)) {
negate(y);
}
auto y2 = impl::trim_leading_sign_bits(y);
std::copy(y2.begin(), y2.end(), dst.begin());
return;
}

if (is_zero(y)) {
if (is_negative(x)) {
negate(x);
}
auto x2 = impl::trim_leading_sign_bits(x);
std::copy(x2.begin(), x2.end(), dst.begin());
return;
}

auto old_s = from_int<std::vector<ull>>(1);
auto s = std::vector<ull>{};
auto old_t = std::vector<ull>{};
auto t = from_int<std::vector<ull>>(1);
old_s.resize(x.size());
s.resize(x.size());
old_t.resize(x.size());
t.resize(x.size());

while (true) {
if (is_lt(compare(x, y))) {
swap(x, y);
}

// x %= y;
auto quotient = std::vector<ull>{};
quotient.resize(x.size());
divide(x, std::move(y), quotient);

swap(old_s, s);
auto quotient_mul_old_s = std::vector<ull>{};
quotient_mul_old_s.resize(x.size());
mult<std::vector<ull>>(quotient, old_s, quotient_mul_old_s);
sub(quotient_mul_old_s, s, s);

swap(old_t, t);
auto quotient_mul_old_t = std::vector<ull>{};
quotient_mul_old_t.resize(x.size());
mult<std::vector<ull>>(quotient, old_t, quotient_mul_old_t);
sub(quotient_mul_old_t, t, t);

if (is_zero(x)) {
auto y2 = mpl::algorithms::impl::trim_leading_sign_bits(y);
std::copy(y2.begin(), y2.end(), dst.begin());
std::copy(s.begin(), s.end(), dst_a.begin());
std::copy(t.begin(), t.end(), dst_b.begin());
return;
}
}
*/
}

/*
void mod_inverse(auto const & a, auto const & n) {
    auto t := 0;
    auto newt := 1
    auto r := n;
    auto newr := a

    while (!is_zero(newr)) do
        quotient := r div newr
        (t, newt) := (newt, t − quotient × newt)
        (r, newr) := (newr, r − quotient × newr)

    if r > 1 then
        return "a is not invertible"
    if t < 0 then
        t := t + n

    return t
}
*/

void lcm(not_cvref auto &&x, not_cvref auto &&y, not_const auto &&dst) {
  // TODO Container

  if (is_zero(x) || is_zero(y)) {
    std::fill(dst.begin(), dst.end(), 0);
  } else {
    if (is_negative(x)) {
      negate(x);
    }
    if (is_negative(y)) {
      negate(y);
    }

    auto gcd_res = std::vector<ull>{};
    gcd_res.resize((std::min)(x.size(), y.size()));
    mpl::algorithms::gcd(auto{x}, auto{y}, gcd_res);

    auto x_div_gcd = std::vector<ull>{};
    x_div_gcd.resize(x.size());
    divide(x, std::move(gcd_res), x_div_gcd);
    mult(x_div_gcd, y, dst);
  }

  /*
  auto prod = std::vector<ull>{};
  prod.resize(x.size() + y.size());
  mult<std::vector<ull>>(x, y, prod);
  auto sign = signum(prod);
  if (sign == 0) {
    std::fill(dst.begin(), dst.end(), 0);
  } else {
    if (signum(prod) < 0) {
      negate(prod);
    }
    auto gcd_res = std::vector<ull>{};
    gcd_res.resize((std::min)(x.size(), y.size()));
    mpl::algorithms::gcd(std::move(x), std::move(y), gcd_res);
    divide(prod, std::move(gcd_res), dst);
  }
  */
}
} // namespace algorithms
} // namespace mpl

#endif // MPL_ALGORITHMS_GCD_HPP
