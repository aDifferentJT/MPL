#ifndef MPL_ALGORITHMS_UTILITY_HPP
#define MPL_ALGORITHMS_UTILITY_HPP

#include <algorithm>
#include <cassert>
#include <iterator>

#include "../utility.hpp"

#include <gmp.h>

namespace mpl {
namespace algorithms {
template <typename T>
concept not_const = !std::is_const_v<std::remove_reference_t<T>>;

template <typename T>
concept not_cvref =
    !(std::is_const_v<T> || std::is_volatile_v<T> || std::is_reference_v<T>);

[[nodiscard]] constexpr auto sar(ull lhs, int rhs) noexcept -> ull {
  return static_cast<ull>(static_cast<signed long long>(lhs) >> rhs);
}

constexpr auto is_negative(auto const &x) -> bool {
  if (x.size() == 0) {
    return false;
  } else {
    return x.back() >> (ull_bits - 1) != 0;
  }
}

constexpr auto is_zero(auto const &x) -> bool {
  return std::all_of(x.begin(), x.end(), [](ull x2) { return x2 == 0; });
}

constexpr auto signum(auto const &x) -> int {
  if (is_negative(x)) {
    return -1;
  } else {
    if (is_zero(x)) {
      return 0;
    } else {
      return 1;
    }
  }
}

constexpr void set_to_zero(not_const auto &&x) {
  for (auto &&quad : x) {
    quad = 0;
  }
}

namespace impl {
template <typename It> struct view {
  using it = It;

  It _begin;
  It _end;

  view(It begin, It end) : _begin{std::move(begin)}, _end{std::move(end)} {}

  template <typename Container>
  view(Container &container)
      : _begin{std::begin(container)}, _end{std::end(container)} {}

  template <std::contiguous_iterator It2>
    requires(std::is_pointer_v<It>)
  explicit view(view<It2> that) : _begin{&*that._begin}, _end{&*that._end} {}

  auto begin() const { return _begin; }
  auto end() const { return _end; }

  auto rbegin() const { return std::reverse_iterator{end()}; }
  auto rend() const { return std::reverse_iterator{begin()}; }

  auto size() const -> std::size_t { return std::distance(begin(), end()); }

  auto front() const -> decltype(auto) { return *begin(); }
  auto back() const -> decltype(auto) { return *rbegin(); }

  auto operator[](std::size_t i) ->
      typename std::iterator_traits<It>::reference {
    assert(i <= size());
    return *(begin() + i);
  }

  auto subview(std::size_t offset, std::size_t count) -> view {
    assert(offset < size());
    assert(offset + count <= size());
    return {begin() + offset, begin() + offset + count};
  }
};

template <typename Container>
view(Container &) -> view<typename Container::iterator>;
template <typename Container>
view(Container const &) -> view<typename Container::const_iterator>;

auto trim_leading_sign_bits(auto &&x) {
  auto ret = view{x.begin(), x.end()};

  auto can_trim = [&] {
    if (auto last = ret.rbegin(); last == ret.rend()) {
      return false;
    } else {
      if (auto penultimate = std::next(ret.rbegin());
          penultimate == ret.rend()) {
        return false;
      } else {
        return *last == sar(*penultimate, ull_bits - 1);
      }
    }
  };

  while (can_trim()) {
    --ret._end;
  }

  return ret;
}

auto unsafe_trim_leading_zeros(auto &&x) {
  auto ret = view{x.begin(), x.end()};

  while (ret.rbegin() != ret.rend() && *ret.rbegin() == 0) {
    --ret._end;
  }

  return ret;
}
} // namespace impl

template <typename It>
concept mpn_compatible_iterator =
    std::contiguous_iterator<It> &&
    sizeof(typename std::iterator_traits<It>::value_type) == sizeof(mp_limb_t);
} // namespace algorithms
} // namespace mpl

#endif // MPL_ALGORITHMS_UTILITY_HPP
