#ifndef MPL_INT_CONTAINER3_HPP
#define MPL_INT_CONTAINER3_HPP

#include <array>
#include <bit>
#include <cassert>
#include <climits>
#include <compare>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>

#include "container_traits.hpp"
#include "utility.hpp"

namespace mpl {
template <std::size_t small_size = 3> class int_container3 {
public:
  using value_type = ull;
  using const_reference = ull;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;

private:
  static_assert(small_size > 0, "small_size must be strictly positive");

  ull *_data;
  ull storage[small_size]{};

  static constexpr auto end_ptr_offset() {
    return offsetof(int_container3, storage[0]) -
           offsetof(int_container3, _data);
  }

  /* _data is the data pointer, in small cases this will be storage
   *
   * *reinterpret_cast<ull **>(
   *   reinterpret_cast<std::byte*>(_data)
   *   - end_ptr_offset()
   * ) + small_size
   * is the end pointer
   * in small cases this is just data + small_size
   *
   * in large cases storage[0] is the capacity
   */

public:
  auto is_small() const -> bool { return _data == storage; }
  auto is_large() const -> bool { return !is_small(); }

private:
  static auto make_storage(std::size_t size, std::size_t capacity) -> ull * {
    auto storage = std::malloc(end_ptr_offset() + capacity * sizeof(ull));
    auto data = reinterpret_cast<ull *>(reinterpret_cast<std::byte *>(storage) +
                                        end_ptr_offset());
    *reinterpret_cast<ull **>(storage) = data + size - small_size;
    return data;
  }

  static constexpr auto expand_size(std::size_t size) -> std::size_t {
    // The only contractual requirement here is that the return value is greater
    // than size
    // In practice this should expand in such a way that works well
    // with the allocator Unfortunately the allocator is a mysterious beast This
    // should also be doing some kind of exponential growth

    // This is based on what this says macOS does, hopefully it's good enough
    // for other platforms, or if macOS has changed since then
    // https://www.cocoawithlove.com/2010/05/look-at-how-malloc-works-on-mac.html

    // The strategy I'll use here is basically 2x - (x / 2)
    // to get a 1.5x increase each time

    // Then we make sure that we are making maximum use of the granularity used
    // by the allocator by rounding up, the easiest way to do that is to
    // subtract 1, set the required number of trailing bits to 1, and then add 1

    constexpr auto expand_size_bytes = [](std::size_t size) {
      constexpr auto round_up = [](std::size_t size) {
        constexpr auto mask = [](std::size_t size) {
          if (size <= 992) {
            return 16 - 1;
          } else if (size <= (127 << 10)) {
            return 512 - 1;
          } else {
            return (4 << 10) - 1;
          }
        };

        return ((size - 1) | mask(size)) + 1;
      };

      return round_up((size * 2) - (size / 2));
    };

    auto new_size = expand_size_bytes(size * sizeof(ull)) / sizeof(ull);
    assert(new_size > size);
    return new_size;
  }

public:
  auto operator[](std::size_t i) const -> ull { return _data[i]; }

  using reference = ull &;

  auto operator[](std::size_t i) -> reference { return _data[i]; }

  using const_iterator = ull const *;
  using iterator = ull *;

  static_assert(std::random_access_iterator<const_iterator>,
                "const_iterator should be random access");
  static_assert(std::random_access_iterator<iterator>,
                "iterator should be random access");

  auto begin() const -> const_iterator { return _data; }
  auto begin() -> iterator { return _data; }
  auto end() const -> const_iterator {
    return *reinterpret_cast<ull const *const *>(
               reinterpret_cast<std::byte const *>(_data) - end_ptr_offset()) +
           small_size;
  }
  auto end() -> iterator {
    return *reinterpret_cast<ull *const *>(
               reinterpret_cast<std::byte const *>(_data) - end_ptr_offset()) +
           small_size;
  }

  auto rbegin() const { return std::reverse_iterator{end()}; }
  auto rbegin() { return std::reverse_iterator{end()}; }
  auto rend() const { return std::reverse_iterator{begin()}; }
  auto rend() { return std::reverse_iterator{begin()}; }

  auto cbegin() const { return begin(); }
  auto cend() const { return end(); }
  auto crbegin() const { return rbegin(); }
  auto crend() const { return rend(); }

  auto front() -> reference { return *begin(); }
  auto front() const -> const_reference { return *begin(); }
  auto back() -> reference { return *rbegin(); }
  auto back() const -> const_reference { return *rbegin(); }

  void set_size_and_cap(std::size_t new_size, std::size_t new_capacity,
                        ull value = 0) {
    assert(new_size <= new_capacity);
    if (new_size > small_size) {
      if (is_small()) {
        _data = make_storage(new_size, new_capacity);
        auto it = std::copy(std::begin(storage), std::end(storage), _data);
        std::fill(it, end(), value);
        storage[0] = new_capacity;
      } else {
        auto heap_storage =
            reinterpret_cast<std::byte *>(_data) - end_ptr_offset();
        if (new_size > storage[0]) {
          auto old_size = size();
          heap_storage = reinterpret_cast<std::byte *>(std::realloc(
              heap_storage, end_ptr_offset() + new_capacity * sizeof(ull)));
          _data = reinterpret_cast<ull *>(heap_storage + end_ptr_offset());
          std::fill(_data + old_size, _data + new_size, value);
          storage[0] = new_capacity;
        }
        *reinterpret_cast<ull **>(heap_storage) = _data + new_size - small_size;
      }
    }
  }

  void resize(std::size_t new_size, ull value = 0) {
    set_size_and_cap(new_size, new_size, value);
  }

  void reserve(std::size_t new_capacity) {
    set_size_and_cap(size(), new_capacity);
  }

  void expand() {
    if (size() == capacity()) {
      set_size_and_cap(size() + 1, expand_size(size()));
    } else {
      set_size_and_cap(size() + 1, capacity());
    }
  }

  explicit int_container3(std::size_t size = 0) {
    if (size <= small_size) {
      _data = storage;
    } else {
      _data = make_storage(size, size);
      storage[0] = size;
    }
    std::fill(begin(), end(), 0);
  }

  template <typename InputIt>
  int_container3(InputIt begin, InputIt end)
      : int_container3{static_cast<std::size_t>(std::distance(begin, end))} {
    assert(size() == std::distance(begin, end));
    std::copy(begin, end, this->begin());
  }

  int_container3(int_container3 const &that) : int_container3{that.size()} {
    assert(size() == that.size());
    std::copy(that.begin(), that.end(), begin());
  }

  template <std::size_t that_small_size>
  int_container3(int_container3<that_small_size> const &that)
      : int_container3{that.size()} {
    assert(size() == that.size());
    std::copy(that.begin(), that.end(), begin());
  }

  auto operator=(int_container3 const &that) -> int_container3 & {
    if (that.is_small()) {
      this->~int_container3();
      new (this) int_container3{std::move(that)};
    } else {
      resize(that.size());
      assert(size() == that.size());
      std::copy(that.begin(), that.end(), begin());
    }
    return *this;
  }

  int_container3(int_container3 &&that) noexcept {
    if (that.is_small()) {
      _data = storage;
      std::copy(std::begin(that.storage), std::end(that.storage),
                std::begin(storage));
    } else {
      _data = std::exchange(that._data, that.storage);
      storage[0] = that.storage[0];
    }
  }

  auto operator=(int_container3 &&that) -> int_container3 & {
    this->~int_container3();
    new (this) int_container3{std::move(that)};
    return *this;
  }

  ~int_container3() {
    if (is_large()) {
      std::free(reinterpret_cast<std::byte *>(_data) - end_ptr_offset());
    }
  }

  auto size() const -> std::size_t { return end() - begin(); }

  auto capacity() const -> std::size_t {
    return is_small() ? small_size : storage[0];
  }

  friend auto operator<<(std::ostream &os, int_container3 xs)
      -> std::ostream & {
    struct save_flags {
      std::ostream &os;
      std::ostream::fmtflags flags;
      save_flags(std::ostream &os) : os{os}, flags{os.flags()} {}
      ~save_flags() { os.flags(flags); }
    };
    auto _save_flags = save_flags{os};
    os << std::hex << std::noshowbase << std::setfill('0');

    for (auto const &x : std::ranges::reverse_view{xs}) {
      os << std::setw(ull_bits / 4) << x << ' ';
    }
    return os;
  }

  void push_back(ull x) {
    auto i = size();
    expand();
    (*this)[i] = x;
  }
};

template <std::size_t _small_size>
struct container_traits<int_container3<_small_size>> {
  static constexpr std::size_t small_size = _small_size * ull_bits;

  static auto is_small(int_container3<_small_size> const &xs) {
    return xs.is_small();
  }
};
} // namespace mpl

#endif // MPL_INT_CONTAINER3_HPP
