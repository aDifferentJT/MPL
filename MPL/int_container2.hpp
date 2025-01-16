#ifndef MPL_INT_CONTAINER2_HPP
#define MPL_INT_CONTAINER2_HPP

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

#include "container_traits.hpp"
#include "dyn_buffer.hpp"
#include "utility.hpp"

namespace mpl {
template <ull small_size> class int_container2 {
public:
  using value_type = ull;
  using const_reference = ull;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;

private:
  struct large_t {
  private:
    ull _size;
    dyn_buffer<ull> _data;

  public:
    static constexpr auto discriminator = 1ull << (ull_bits - 1);

    large_t(std::size_t size) : _size{size | discriminator}, _data{size} {
      assert(size != 0);
      assert((size & ~discriminator) != 0);
    }

    large_t(dyn_buffer<ull> buffer, std::size_t size)
        : _size{size | discriminator}, _data{std::move(buffer)} {
      assert(size != 0);
      assert((size & ~discriminator) != 0);
    }

    auto data() -> ull * { return _data.data(); }
    auto data() const -> ull const * { return _data.data(); }

    auto size() const -> std::size_t {
      auto size = _size & ~discriminator;
      assert(size != 0);
      return size;
    }
    void incrementSize() { _size += 1; }
    void setSize(std::size_t size) {
      assert(size != 0);
      assert((size & ~discriminator) != 0);
      _size = size | discriminator;
    }

    auto capacity() const -> std::size_t { return _data.size(); }

    auto operator[](std::size_t i) const -> ull { return _data[i]; }
    auto operator[](std::size_t i) -> ull & { return _data[i]; }

    auto begin() const -> ull const * { return data(); }
    auto begin() -> ull * { return data(); }
    auto end() const -> ull const * { return data() + size(); }
    auto end() -> ull * { return data() + size(); }
  };

  struct small_t {
  private:
    ull _size;
    std::array<ull, small_size> _data = {};

  public:
    small_t(std::size_t size) : _size{size} {
      assert(size != 0);
      assert(size <= small_size);
      assert((size & large_t::discriminator) == 0);
    }

    auto data() -> ull * { return _data.data(); }
    auto data() const -> ull const * { return _data.data(); }

    auto size() const -> std::size_t {
      assert(_size != 0);
      assert(_size <= small_size);
      return _size;
    }
    void incrementSize() {
      _size += 1;
      assert(_size <= small_size);
    }
    void setSize(std::size_t size) {
      assert(size != 0);
      assert(size <= small_size);
      _size = size;
    }

    auto capacity() const -> std::size_t { return _data.size(); }

    auto operator[](std::size_t i) const -> ull { return _data[i]; }
    auto operator[](std::size_t i) -> ull & { return _data[i]; }

    auto begin() const -> ull const * { return data(); }
    auto begin() -> ull * { return data(); }
    auto end() const -> ull const * { return data() + size(); }
    auto end() -> ull * { return data() + size(); }
  };

  static_assert(sizeof(small_t) >= sizeof(large_t),
                "the small representation should be no smaller than the large "
                "representation");

  static_assert(alignof(small_t) <= alignof(ull),
                "small representation should align within a ull");
  static_assert(alignof(large_t) <= alignof(ull),
                "large representation should align within a ull");

  alignas(ull) std::byte storage[sizeof(small_t)]{};

public:
  auto is_large() const -> bool {
    return (*std::launder<ull const>(reinterpret_cast<ull const *>(storage)) &
            large_t::discriminator) != 0;
  }
  auto is_small() const -> bool { return !is_large(); }

private:
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

  auto visit(auto &&visitor) -> decltype(auto) {
    if (is_large()) {
      return std::forward<decltype(visitor)>(visitor)(unsafe_large());
    } else {
      return std::forward<decltype(visitor)>(visitor)(unsafe_small());
    }
  }

  auto visit(auto &&visitor) const -> decltype(auto) {
    if (is_large()) {
      return std::forward<decltype(visitor)>(visitor)(unsafe_large());
    } else {
      return std::forward<decltype(visitor)>(visitor)(unsafe_small());
    }
  }

  int_container2(small_t small) { construct<small_t>(std::move(small)); }
  int_container2(large_t large) { construct<large_t>(std::move(large)); }

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
  // This takes advantage of dyn_buffer being trivially relocatable
  friend void swap(int_container2 &lhs, int_container2 &rhs) {
    std::swap(lhs.storage, rhs.storage);
  }

  int_container2() : int_container2{small_t{1}} {}

  int_container2(int_container2 const &that) {
    that.visit(
        [this]<typename T>(T const &that2) { this->construct<T>(that2); });
  }

  auto operator=(int_container2 const &that) -> int_container2 & {
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

  int_container2(int_container2 &&that) { swap(*this, that); }

  auto operator=(int_container2 &&that) -> int_container2 & {
    auto tmp = std::move(that);
    swap(*this, tmp);
    return *this;
  }

  ~int_container2() {
    visit([]<typename T>(T &x) { x.~T(); });
  }

  int_container2(std::size_t size)
      : int_container2{[size]() -> int_container2 {
          if (size == 0) {
            return small_t{1};
          } else if (size <= small_size) {
            return small_t{size};
          } else {
            return large_t{size};
          }
        }()} {}

  auto size() const -> std::size_t {
    auto res =
        visit([](auto const &this2) -> std::size_t { return this2.size(); });
    assert(res != 0);
    return res;
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
    visit([](auto &this2) { this2.incrementSize(); });
  }

  void resize(std::size_t new_size) {
    auto old_size = size();
    reserve(new_size);
    visit([old_size, new_size](auto &this2) {
      for (auto i = old_size; i < new_size; ++i) {
        // TODO is this necessary?
        this2[i] = 0;
      }
      this2.setSize(new_size);
    });
  }

  void resize(std::size_t new_size, ull value) {
    auto old_size = size();
    resize(new_size);
    for (auto i = old_size; i < new_size; i += 1) {
      (*this)[i] = value;
    }
  }

  auto operator[](std::size_t i) const -> ull {
    decltype(auto) ret = visit([i](auto const &this2) { return this2[i]; });
    return ret;
  }

  using reference = ull &;

  auto operator[](std::size_t i) -> reference {
    decltype(auto) ret =
        visit([i](auto &this2) -> reference { return this2[i]; });
    return ret;
  }

  using const_iterator = ull const *;
  using iterator = ull *;

  static_assert(std::random_access_iterator<const_iterator>,
                "const_iterator should be random access");
  static_assert(std::random_access_iterator<iterator>,
                "iterator should be random access");

  auto begin() const -> const_iterator {
    return visit([](auto const &this2) { return this2.begin(); });
  }
  auto begin() -> iterator {
    return visit([](auto &this2) { return this2.begin(); });
  }
  auto end() const -> const_iterator {
    return visit([](auto const &this2) { return this2.end(); });
  }
  auto end() -> iterator {
    return visit([](auto &this2) { return this2.end(); });
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

  friend auto operator<<(std::ostream &os, int_container2 xs)
      -> std::ostream & {
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
  int_container2(InputIt begin, InputIt end)
      : int_container2{static_cast<std::size_t>(std::distance(begin, end))} {
    std::copy(begin, end, this->begin());
  }

  void push_back(ull x) {
    auto i = size();
    expand();
    (*this)[i] = x;
  }
};

template <std::size_t _small_size>
struct container_traits<int_container2<_small_size>> {
  static constexpr std::size_t small_size = _small_size * ull_bits;

  static auto is_small(int_container2<_small_size> const &xs) {
    return xs.is_small();
  }
};
} // namespace mpl

#endif // MPL_INT_CONTAINER2_HPP
