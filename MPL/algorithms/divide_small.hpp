#ifndef MPL_ALGORITHMS_DIVIDE_SMALL_HPP
#define MPL_ALGORITHMS_DIVIDE_SMALL_HPP

#include "../dyn_buffer.hpp"
#include "compare.hpp"
#include "mult.hpp"
#include "shift.hpp"
#include "utility.hpp"

#include <array>
#include <compare>
#include <csignal>
#include <memory>

namespace mpl {
namespace algorithms {
constexpr void divide_small(not_const auto &&dividend, ull divisor,
                            not_const auto &&quotient) {
  using namespace impl;

  if (divisor == 0) {
#ifdef SIGFPE
    std::raise(SIGFPE);
#else
    volatile auto urgh = 0;
    urgh = 1 / urgh;
    // I hate this
    // TODO: Please fix
#endif
    std::cerr << "Div by zero (small)\n";
    std::terminate();
  }

  auto negate_dividend = false;
  auto negate_quotient = false;

  if (std::is_lt(compare(dividend, std::array<ull, 1>{0}))) {
    negate(dividend);
    negate_dividend = !negate_dividend;
    negate_quotient = !negate_quotient;
  }

  if (divisor < 0) {
    divisor = -divisor;
    negate_quotient = !negate_quotient;
  }

  for (auto quotient_index_signed = static_cast<long>(dividend.size() - 1);
       quotient_index_signed >= 0; quotient_index_signed -= 1) {
    auto quotient_index = static_cast<std::size_t>(quotient_index_signed);

    auto &quotient_quad = quotient[quotient_index];

    auto const dividend_128 =
        dividend[quotient_index] |
        (quotient_index + 1 < dividend.size()
             ? static_cast<__uint128_t>(dividend[quotient_index + 1])
                   << ull_bits
             : 0);

    quotient_quad = dividend_128 / divisor;

    auto const quotient_quad_mult_divisor = mult128(quotient_quad, divisor);

    auto const remainder_128 = dividend_128 - quotient_quad_mult_divisor;

    assert(remainder_128 == static_cast<ull>(remainder_128));

    if (quotient_index + 1 < dividend.size()) {
      dividend[quotient_index + 1] = 0;
    }
    dividend[quotient_index] = remainder_128;
  }

  if (negate_dividend) {
    negate(dividend);
  }

  if (negate_quotient) {
    negate(quotient);
  }
}
} // namespace algorithms
} // namespace mpl

#endif // MPL_ALGORITHMS_DIVIDE_SMALL_HPP
