#ifndef MPL_CONTAINER_TRAITS_HPP
#define MPL_CONTAINER_TRAITS_HPP

#include <vector>

namespace mpl {
template <typename Container> struct container_traits;

template <typename T> struct container_traits<std::vector<T>> {
  static constexpr std::size_t small_size = 0;

  static auto is_small(std::vector<T> const &) { return false; }
};
} // namespace mpl

#endif // MPL_CONTAINER_TRAITS_HPP
