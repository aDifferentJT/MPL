#ifndef MPL_ALGORITHMS_DIVIDE_HPP
#define MPL_ALGORITHMS_DIVIDE_HPP

#include "../dyn_buffer.hpp"
#include "compare.hpp"
#include "mult.hpp"
#include "shift.hpp"
#include "utility.hpp"

#include "to_string.hpp"

#include <gmp.h>

#include <array>
#include <compare>
#include <csignal>
#include <iomanip>
#include <memory>

namespace mpl {
namespace algorithms {
namespace impl {
template <typename Allocator = std::allocator<ull>>
void mpl_unsigned_divide(not_const auto &&dividend, not_cvref auto &&divisor,
                         not_const auto &&quotient, Allocator allocator = {}) {
  using namespace impl;

  // TODO use custom allocator

  if (is_zero(divisor)) {
#ifdef SIGFPE
    std::raise(SIGFPE);
#else
    volatile auto urgh = 0;
    urgh = 1 / urgh;
    // I hate this
    // TODO: Please fix
#endif
    std::cerr << "Div by zero\n";
    std::terminate();
  }

  std::fill(quotient.begin(), quotient.end(), 0);

  // Quotient index start is being calculated based on the original dividend
  // size, pre-shifting as that should be sufficient for the non zero quotient.
  auto const initial_quotient_index =
      static_cast<long>(unsafe_trim_leading_zeros(dividend).size() -
                        unsafe_trim_leading_zeros(divisor).size());

  auto unsigned_divisor = unsafe_trim_leading_zeros(divisor);
  auto const normalisation_shift = __builtin_clzll(unsigned_divisor.back());
  {
    [[maybe_unused]] auto carry = unsafe_small_positive_left_shift(
        unsigned_divisor, normalisation_shift, unsigned_divisor);
    assert(carry == 0);
  }

  auto unsigned_dividend_storage =
      dyn_buffer<ull, Allocator>{nullptr, allocator};
  auto unsigned_dividend = [&]() {
    auto carry = unsafe_small_positive_left_shift(dividend, normalisation_shift,
                                                  dividend);
    if (carry == 0) {
      return view<ull *>{unsafe_trim_leading_zeros(dividend)};
    } else {
      unsigned_dividend_storage =
          dyn_buffer<ull, Allocator>{dividend.size() + 1, allocator};
      auto it = std::copy(dividend.begin(), dividend.end(),
                          unsigned_dividend_storage.begin());
      *it = carry;
      return view{unsigned_dividend_storage};
    }
  }();

  for (auto quotient_index = initial_quotient_index; quotient_index >= 0;
       quotient_index -= 1) {
    auto &quotient_quad = quotient[static_cast<std::size_t>(quotient_index)];
    auto remainder_span = unsigned_dividend.subview(
        quotient_index, std::min(unsigned_divisor.size() + 1,
                                 unsigned_dividend.size() - quotient_index));

    assert(remainder_span.size() >= unsigned_divisor.size());

    quotient_quad = (remainder_span.size() < unsigned_divisor.size() + 1
                         ? static_cast<__uint128_t>(
                               remainder_span[unsigned_divisor.size() - 1])
                         : remainder_span[unsigned_divisor.size() - 1] |
                               (static_cast<__uint128_t>(
                                    remainder_span[unsigned_divisor.size()])
                                << ull_bits)) /
                    unsigned_divisor.back();
    // TODO add the quick check from D3 in Knuth

    auto quotient_quad_mult_divisor = dyn_buffer<unsigned long long, Allocator>{
        divisor.size() + 1, allocator};
    quotient_quad += 1; // Offset the first decrement
    do {
      // This assert fails when the initial guess for quotient_quad is
      // std::numeric_limits<ull>::max
      // assert(quotient_quad > 0);
      quotient_quad -= 1;
      // TODO this could be optimised as just a subtraction from the last result
      std::fill(quotient_quad_mult_divisor.begin(),
                quotient_quad_mult_divisor.end(), 0);
      unsigned_mult_and_add(quotient_quad, divisor, quotient_quad_mult_divisor,
                            allocator); // TODO allocator
    } while (std::is_gt(
        unsigned_compare(quotient_quad_mult_divisor, remainder_span)));

    if (quotient_quad != 0) {
      auto unsigned_quotient_quad_mult_divisor_view =
          unsafe_trim_leading_zeros(quotient_quad_mult_divisor);

      assert(std::is_lteq(
          unsigned_compare(quotient_quad_mult_divisor, remainder_span)));
      assert(std::is_lteq(unsigned_compare(
          unsigned_quotient_quad_mult_divisor_view, remainder_span)));

      unsigned_ripple_subber(remainder_span,
                             unsigned_quotient_quad_mult_divisor_view,
                             remainder_span);
    }
  }

  {
    [[maybe_unused]] auto carry = small_logical_right_shift(
        unsigned_divisor, normalisation_shift, unsigned_divisor);
    assert(carry == 0);
  }
  {
    [[maybe_unused]] auto carry = small_logical_right_shift(
        unsigned_dividend, normalisation_shift, unsigned_dividend);
    assert(carry == 0);
  }

  if (unsigned_dividend_storage != nullptr) {
    std::copy(unsigned_dividend_storage.begin(),
              unsigned_dividend_storage.begin() + dividend.size(),
              dividend.begin());
  }
}

inline void gmp_unsigned_divide5(std::span<mp_limb_t> dividend,
                                 std::span<mp_limb_t const> divisor,
                                 std::span<mp_limb_t> quotient) {
  assert(dividend.size() >= divisor.size());
  assert(quotient.size() >= dividend.size() - divisor.size() + 1);
  mpn_tdiv_qr(quotient.data(), dividend.data(), 0, dividend.data(),
              dividend.size(), divisor.data(), divisor.size());
  std::fill(dividend.begin() + divisor.size(), dividend.end(), 0);
  std::fill(quotient.begin() + dividend.size() - divisor.size() + 1,
            quotient.end(), 0);
}

inline void gmp_unsigned_divide4(std::span<mp_limb_t> dividend,
                                 std::span<mp_limb_t const> divisor,
                                 not_const auto &&quotient, auto allocator) {
  using it = std::remove_cvref_t<decltype(quotient.begin())>;
  if constexpr (mpn_compatible_iterator<it>) {
    gmp_unsigned_divide5(
        dividend, divisor,
        std::span{reinterpret_cast<mp_limb_t *>(&*quotient.begin()),
                  quotient.size()});
  } else {
    auto quotient2 = dyn_buffer<mp_limb_t>{
        quotient.size() * sizeof(mp_limb_t) /
            sizeof(typename std::iterator_traits<it>::value_type),
        allocator};
    gmp_unsigned_divide5(dividend, divisor, quotient2);
    static_assert(std::endian::native == std::endian::little,
                  "Only little endian is supported");
    auto quotient2_ptr = reinterpret_cast<std::byte *>(quotient2.data());
    for (auto &quotient_quad : quotient) {
      std::memcpy(&quotient_quad, quotient2_ptr, sizeof(quotient_quad));
      quotient2_ptr += sizeof(quotient_quad);
    }
  }
}

template <typename ItDivisor>
inline void gmp_unsigned_divide3(std::span<mp_limb_t> dividend,
                                 view<ItDivisor> divisor,
                                 not_const auto &&quotient, auto allocator) {
  if constexpr (mpn_compatible_iterator<ItDivisor>) {
    gmp_unsigned_divide4(
        dividend,
        std::span{reinterpret_cast<mp_limb_t const *>(&*divisor.begin()),
                  divisor.size()},
        quotient, allocator);
  } else {
    auto divisor2 = dyn_buffer<mp_limb_t>{
        divisor.size() * sizeof(mp_limb_t) /
            sizeof(typename std::iterator_traits<ItDivisor>::value_type),
        allocator};
    static_assert(std::endian::native == std::endian::little,
                  "Only little endian is supported");
    auto divisor2_ptr = reinterpret_cast<std::byte *>(divisor2.data());
    for (auto divisor_quad : divisor) {
      std::memcpy(divisor2_ptr, &divisor_quad, sizeof(divisor_quad));
      divisor2_ptr += sizeof(divisor_quad);
    }
    gmp_unsigned_divide4(dividend, divisor2, quotient, allocator);
  }
}

template <typename ItDividend, typename ItDivisor>
inline void gmp_unsigned_divide2(view<ItDividend> dividend,
                                 view<ItDivisor> divisor,
                                 not_const auto &&quotient, auto allocator) {
  if constexpr (mpn_compatible_iterator<ItDividend>) {
    gmp_unsigned_divide3(
        std::span{reinterpret_cast<mp_limb_t *>(&*dividend.begin()),
                  dividend.size()},
        divisor, quotient, allocator);
  } else {
    auto dividend2 = dyn_buffer<mp_limb_t>{
        dividend.size() * sizeof(mp_limb_t) /
            sizeof(typename std::iterator_traits<ItDividend>::value_type),
        allocator};
    static_assert(std::endian::native == std::endian::little,
                  "Only little endian is supported");
    {
      auto dividend2_ptr = reinterpret_cast<std::byte *>(dividend2.data());
      for (auto dividend_quad : dividend) {
        std::memcpy(dividend2_ptr, &dividend_quad, sizeof(dividend_quad));
        dividend2_ptr += sizeof(dividend_quad);
      }
    }
    gmp_unsigned_divide3(dividend2, divisor, quotient, allocator);
    {
      auto dividend2_ptr = reinterpret_cast<std::byte *>(dividend2.data());
      for (auto &dividend_quad : dividend) {
        std::memcpy(&dividend_quad, dividend2_ptr, sizeof(dividend_quad));
        dividend2_ptr += sizeof(dividend_quad);
      }
    }
  }
}

template <typename Allocator = std::allocator<ull>>
inline void gmp_unsigned_divide(not_const auto &&dividend, auto const &divisor,
                                not_const auto &&quotient,
                                Allocator allocator = {}) {
  gmp_unsigned_divide2(view{dividend}, unsafe_trim_leading_zeros(divisor),
                       quotient, allocator);
}

template <typename Allocator = std::allocator<ull>>
void unsigned_divide(not_const auto &&dividend, not_cvref auto &&divisor,
                     not_const auto &&quotient, Allocator allocator = {}) {
  gmp_unsigned_divide(dividend, divisor, quotient);
}
} // namespace impl

template <typename Allocator = std::allocator<ull>>
void divide(not_const auto &&dividend, not_cvref auto &&divisor,
            not_const auto &&quotient, Allocator allocator = {}) {
  using namespace impl;

  auto negate_dividend = false;
  auto negate_quotient = false;

  if (is_negative(dividend)) {
    negate(dividend);
    negate_dividend = !negate_dividend;
    negate_quotient = !negate_quotient;
  }

  if (is_negative(divisor)) {
    negate(divisor);
    negate_quotient = !negate_quotient;
  }

  if (std::is_lt(compare(dividend, divisor))) {
    std::fill(quotient.begin(), quotient.end(), 0);
    if (negate_dividend) {
      negate(dividend);
    }
    return;
  }

  unsigned_divide(dividend, trim_leading_sign_bits(divisor), quotient,
                  std::move(allocator));

  assert(!is_negative(dividend));
  assert(!is_negative(quotient));

  if (negate_dividend) {
    negate(dividend);
  }

  if (negate_quotient) {
    negate(quotient);
  }
}

namespace impl {
inline auto gmp_unsigned_divide4(std::span<mp_limb_t> dividend, ull divisor,
                                 std::span<mp_limb_t> quotient) -> ull {
  assert(quotient.size() >= dividend.size());
  auto rem = mpn_divrem_1(quotient.data(), 0, dividend.data(), dividend.size(),
                          divisor);
  std::fill(quotient.begin() + dividend.size(), quotient.end(), 0);
  return rem;
}

inline auto gmp_unsigned_divide3(std::span<mp_limb_t> dividend, ull divisor,
                                 not_const auto &&quotient, auto allocator)
    -> ull {
  using it = std::remove_cvref_t<decltype(quotient.begin())>;
  if constexpr (mpn_compatible_iterator<it>) {
    return gmp_unsigned_divide4(
        dividend, divisor,
        std::span{reinterpret_cast<mp_limb_t *>(&*quotient.begin()),
                  quotient.size()});
  } else {
    auto quotient2 = dyn_buffer<mp_limb_t>{
        quotient.size() * sizeof(mp_limb_t) /
            sizeof(typename std::iterator_traits<it>::value_type),
        allocator};
    auto rem = gmp_unsigned_divide4(dividend, divisor, quotient2);
    static_assert(std::endian::native == std::endian::little,
                  "Only little endian is supported");
    auto quotient2_ptr = reinterpret_cast<std::byte *>(quotient2.data());
    for (auto &quotient_quad : quotient) {
      std::memcpy(&quotient_quad, quotient2_ptr, sizeof(quotient_quad));
      quotient2_ptr += sizeof(quotient_quad);
    }
    return rem;
  }
}

template <typename ItDividend>
inline auto gmp_unsigned_divide2(view<ItDividend> dividend, ull divisor,
                                 not_const auto &&quotient, auto allocator)
    -> ull {
  if constexpr (mpn_compatible_iterator<ItDividend>) {
    return gmp_unsigned_divide3(
        std::span{reinterpret_cast<mp_limb_t *>(&*dividend.begin()),
                  dividend.size()},
        divisor, quotient, allocator);
  } else {
    auto dividend2 = dyn_buffer<mp_limb_t>{
        dividend.size() * sizeof(mp_limb_t) /
            sizeof(typename std::iterator_traits<ItDividend>::value_type),
        allocator};
    static_assert(std::endian::native == std::endian::little,
                  "Only little endian is supported");
    {
      auto dividend2_ptr = reinterpret_cast<std::byte *>(dividend2.data());
      for (auto dividend_quad : dividend) {
        std::memcpy(dividend2_ptr, &dividend_quad, sizeof(dividend_quad));
        dividend2_ptr += sizeof(dividend_quad);
      }
    }
    return gmp_unsigned_divide3(dividend2, divisor, quotient, allocator);
  }
}

template <typename Allocator = std::allocator<ull>>
inline auto gmp_unsigned_divide(not_cvref auto &&dividend, ull divisor,
                                not_const auto &&quotient,
                                Allocator allocator = {}) -> ull {
  return gmp_unsigned_divide2(unsafe_trim_leading_zeros(dividend), divisor,
                              quotient, allocator);
}

template <typename Allocator = std::allocator<ull>>
auto unsigned_divide(not_cvref auto &&dividend, ull divisor,
                     not_const auto &&quotient, Allocator allocator = {})
    -> ull {
  return gmp_unsigned_divide(std::move(dividend), divisor, quotient, allocator);
}
} // namespace impl

template <typename Allocator = std::allocator<ull>>
auto divide(not_cvref auto &&dividend, ull divisor, not_const auto &&quotient,
            Allocator allocator = {}) -> ull {
  using namespace impl;

  auto negate_dividend = false;
  auto negate_quotient = false;

  if (is_negative(dividend)) {
    negate(dividend);
    negate_dividend = !negate_dividend;
    negate_quotient = !negate_quotient;
  }

  if (std::is_lt(compare(dividend, std::array<ull, 1>{divisor}))) {
    std::fill(quotient.begin(), quotient.end(), 0);
    if (negate_dividend) {
      negate(dividend);
    }
    return dividend[0];
  }

  auto rem = unsigned_divide(std::move(dividend), divisor, quotient, std::move(allocator));

  assert(rem >= 0);
  assert(!is_negative(quotient));

  if (negate_dividend) {
    rem = -rem;
  }

  if (negate_quotient) {
    negate(quotient);
  }

  return rem;
}
} // namespace algorithms
} // namespace mpl

#endif // MPL_ALGORITHMS_DIVIDE_HPP
