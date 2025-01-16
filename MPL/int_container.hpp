#ifndef MPL_INT_CONTAINER_HPP
#define MPL_INT_CONTAINER_HPP

#include <array>
#include <cassert>
#include <compare>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>

#include "dyn_buffer.hpp"
#include "utility.hpp"

namespace mpl {
class int_container {
public:
  using value_type = ull;
  using const_reference = ull;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;

private:
  struct large_t {
    dyn_buffer<ull> _data;
    ull _size;
    ull padding = 0b10ull << (ull_bits - 2);

    large_t(std::size_t size) : _data{size}, _size{size} {}

    large_t(dyn_buffer<ull> buffer, std::size_t size)
        : _data{std::move(buffer)}, _size{size} {}

    auto data() -> ull * { return _data.data(); }
    auto data() const -> ull const * { return _data.data(); }

    auto size() const -> std::size_t { return _size; }
    auto capacity() const -> std::size_t { return _data.size(); }

    auto operator[](std::size_t i) const -> ull { return _data[i]; }
    auto operator[](std::size_t i) -> ull & { return _data[i]; }

    auto begin() const -> ull const * { return _data.data(); }
    auto begin() -> ull * { return _data.data(); }
    auto end() const -> ull const * { return _data.data() + _size; }
    auto end() -> ull * { return _data.data() + _size; }
  };

  static constexpr auto small_size = 4ul;
  struct small_t : std::array<ull, small_size> {
    auto capacity() const -> std::size_t { return small_size; }
  };

  static_assert(sizeof(small_t) == sizeof(large_t),
                "small and large representations should be the same size");

  static_assert(alignof(small_t) <= alignof(ull),
                "small representation should align within a ull");
  static_assert(alignof(large_t) <= alignof(ull),
                "large representation should align within a ull");

  alignas(ull) std::byte storage[sizeof(small_t)]{};

  auto is_large() const -> bool {
    return (reinterpret_cast<ull const &>(
                storage[offsetof(large_t, padding)]) >>
            (ull_bits - 2)) == 0b10;
  }

  auto unsafe_small() -> small_t & {
    return *std::launder<small_t>(reinterpret_cast<small_t *>(storage));
  }

  auto unsafe_small() const -> small_t const & {
    return *std::launder<small_t const>(
        reinterpret_cast<small_t const *>(storage));
  }

  auto unsafe_large() -> large_t & {
    return *std::launder<large_t>(reinterpret_cast<large_t *>(storage));
  }

  auto unsafe_large() const -> large_t const & {
    return *std::launder<large_t const>(
        reinterpret_cast<large_t const *>(storage));
  }

  template <typename T> void construct(auto &&...args) {
    std::construct_at<T>(reinterpret_cast<T *>(storage),
                         std::forward<decltype(args)>(args)...);
  }

  auto visit(auto &&visitor) {
    if (is_large()) {
      return std::forward<decltype(visitor)>(visitor)(unsafe_large());
    } else {
      return std::forward<decltype(visitor)>(visitor)(unsafe_small());
    }
  }

  auto visit(auto &&visitor) const {
    if (is_large()) {
      return std::forward<decltype(visitor)>(visitor)(unsafe_large());
    } else {
      return std::forward<decltype(visitor)>(visitor)(unsafe_small());
    }
  }

  int_container(small_t small) { construct<small_t>(std::move(small)); }
  int_container(large_t large) { construct<large_t>(std::move(large)); }

  static constexpr auto expand_size(std::size_t size) -> std::size_t {
    // The only contractual requirement here is that the return value is greater
    // than size
    // In practice this should expand in such a way that works well
    // with the allocator Unfortunately the allocator is a mysterious beast This
    // should also be doing some kind of exponential growth

    // This is based on what this says macOS does, hopefully it's good enough
    // for other platforms, or if macOS has changed since then
    // https://www.cocoawithlove.com/2010/05/look-at-how-malloc-works-on-mac.html

    // The strategy I'll use here is basically x + (x / 2)
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

      return round_up(size + (size / 2));
    };

    auto new_size = expand_size_bytes(size * sizeof(ull)) / sizeof(ull);
    assert(new_size > size);
    return new_size;
  }

public:
  // This takes advantage of dyn_buffer being trivially relocatable
  friend void swap(int_container &lhs, int_container &rhs) {
    std::swap(lhs.storage, rhs.storage);
  }

  int_container() : int_container{small_t{}} {}

  int_container(int_container const &that) {
    that.visit(
        [this]<typename T>(T const &that2) { this->construct<T>(that2); });
  }

  auto operator=(int_container const &that) -> int_container & {
    visit([this, &that]<typename T>(T &this2) {
      that.visit([this, &that, &this2]<typename U>(U const &that2) {
        if constexpr (std::is_same_v<T, U>) {
          this2 = that2;
        } else {
          if (this != &that) {
            this2.~T();
            this->construct<U>(that2);
          }
        }
      });
    });
    return *this;
  }

  int_container(int_container &&that) { swap(*this, that); }

  auto operator=(int_container &&that) -> int_container & {
    auto tmp = std::move(that);
    swap(*this, tmp);
    return *this;
  }

  ~int_container() {
    visit([]<typename T>(T &x) { x.~T(); });
  }

  int_container(std::size_t size)
      : int_container{[size]() -> int_container {
          if (size <= small_size) {
            return small_t{};
          } else {
            return large_t{size};
          }
        }()} {}

  auto size() const -> std::size_t {
    return visit([](auto const &this2) -> std::size_t { return this2.size(); });
  }

  auto capacity() const -> std::size_t {
    return visit(
        [](auto const &this2) -> std::size_t { return this2.capacity(); });
  }

  void reserve(std::size_t new_cap) {
    visit([this, new_cap](auto const &this2) {
      if (new_cap > this2.capacity()) {
        auto buffer = dyn_buffer<ull>{new_cap};
        std::copy(this2.begin(), this2.end(), buffer.begin());
        *this = large_t{std::move(buffer), this2.size()};
      }
    });
  }

  void expand() {
    if (size() == capacity()) {
      reserve(expand_size(size()));
    }
    unsafe_large()._size += 1;
  }

  void resize(std::size_t new_size) {
    auto old_size = size();
    if (new_size > old_size) {
      reserve(new_size);
      for (auto i = old_size; i < new_size; ++i) {
        // TODO is this necessary?
        unsafe_large()[i] = 0;
      }
      unsafe_large()._size = new_size;
    } else if (is_large()) {
      unsafe_large()._size = new_size;
    }
  }

  void resize(std::size_t new_size, ull value) {
    auto old_size = size();
    resize(new_size);
    for (auto i = old_size; i < new_size; i += 1) {
      (*this)[i] = value;
    }
  }

  auto operator[](std::size_t i) const -> ull {
    return visit([i](auto const &this2) { return this2[i]; });
  }

  class reference {
  private:
    int_container *container;
    std::size_t index;

    reference(int_container *container, std::size_t index)
        : container{container}, index{index} {}

  public:
    operator ull() const { return std::as_const(*container)[index]; }

    void operator=(ull new_value) {
      assert(container != nullptr);
      assert(index < container->size());

      static constexpr auto discriminator_index =
          offsetof(large_t, padding) / sizeof(ull);

      if (index == discriminator_index && new_value >> (CHAR_BIT - 1) == 0b10) {
        container->expand();
      }
      container->visit([index = index, new_value](auto &container2) {
        container2[index] = new_value;
      });
    }

    friend class int_container;
  };

  auto operator[](std::size_t i) -> reference { return reference{this, i}; }

  template <bool is_const> class basic_iterator {
  private:
    using container_t =
        std::conditional_t<is_const, int_container const, int_container>;
    container_t *container;
    std::size_t index;

    basic_iterator(container_t *container, std::size_t index)
        : container{container}, index{index} {}

  public:
    using value_type = int_container::value_type;
    using reference =
        std::conditional_t<is_const, ull, int_container::reference>;
    using difference_type = int_container::difference_type;
    using iterator_category = std::random_access_iterator_tag;

    basic_iterator() : container{nullptr}, index{0} {}

    friend auto operator==(basic_iterator lhs, basic_iterator rhs)
        -> bool = default;

    friend auto operator<=>(basic_iterator lhs, basic_iterator rhs)
        -> std::strong_ordering = default;

    auto operator*() const -> reference { return (*container)[index]; }

    auto operator++() -> basic_iterator & {
      ++index;
      return *this;
    }

    auto operator++(int) -> basic_iterator {
      auto tmp = *this;
      operator++();
      return tmp;
    }

    auto operator--() -> basic_iterator & {
      --index;
      return *this;
    }

    auto operator--(int) -> basic_iterator {
      auto tmp = *this;
      operator--();
      return tmp;
    }

    auto operator+=(difference_type offset) -> basic_iterator & {
      index += offset;
      return *this;
    }

    auto operator-=(difference_type offset) -> basic_iterator & {
      return *this += -offset;
    }

    friend auto operator+(basic_iterator it, difference_type offset)
        -> basic_iterator {
      return it += offset;
    }

    friend auto operator+(difference_type offset, basic_iterator it)
        -> basic_iterator {
      return it + offset;
    }

    friend auto operator-(basic_iterator it, difference_type offset)
        -> basic_iterator {
      return it + -offset;
    }

    friend auto operator-(difference_type offset, basic_iterator it)
        -> basic_iterator {
      return it - offset;
    }

    friend auto operator-(basic_iterator lhs, basic_iterator rhs)
        -> difference_type {
      return lhs.index - rhs.index;
    }

    auto operator[](difference_type offset) const -> reference {
      return *(*this + offset);
    }

    friend class int_container;
  };

  using const_iterator = basic_iterator<true>;
  using iterator = basic_iterator<false>;

  static_assert(std::random_access_iterator<const_iterator>,
                "const_iterator should be random access");
  static_assert(std::random_access_iterator<iterator>,
                "iterator should be random access");

  auto begin() const -> const_iterator { return {this, 0}; }
  auto begin() -> iterator { return {this, 0}; }
  auto end() const -> const_iterator { return {this, size()}; }
  auto end() -> iterator { return {this, size()}; }

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

  friend auto operator<<(std::ostream &os, int_container xs) -> std::ostream & {
    struct save_flags {
      std::ostream &os;
      std::ostream::fmtflags flags;
      save_flags(std::ostream &os) : os{os}, flags{os.flags()} {}
      ~save_flags() { os.flags(flags); }
    };
    auto _save_flags = save_flags{os};
    os << std::hex << std::noshowbase << std::setfill('0');

    for (auto const &x : impl::reverse_view{xs}) {
      os << std::setw(ull_bits / 4) << x << ' ';
    }
    return os;
  }

  template <typename InputIt>
  int_container(InputIt begin, InputIt end)
      : int_container{static_cast<std::size_t>(std::distance(begin, end))} {
    std::copy(begin, end, this->begin());
  }

  void push_back(ull x) {
    auto i = size();
    expand();
    (*this)[i] = x;
  }
};
} // namespace mpl

#endif // MPL_INT_CONTAINER_HPP
