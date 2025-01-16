#ifndef MPL_HPP
#define MPL_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <concepts>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <ios>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

template <typename T> auto steal(T &x) -> T { return std::exchange(x, {}); }

template <typename T> struct equals {
  T const &x;

  auto operator()(T const &y) { return x == y; }
};

template <typename T> equals(T const &) -> equals<T>;

template <typename T> auto to_signed(T x) {
  return static_cast<std::make_signed_t<T>>(x);
}

template <typename T> auto to_unsigned(T x) {
  return static_cast<std::make_unsigned_t<T>>(x);
}

// Should be std
template <typename T>
concept integral = std::is_integral_v<T>;

// Should be std
template <typename T>
concept floating_point = std::is_floating_point_v<T>;

// Should be std
template <class T>
requires(!std::is_array_v<T>) auto make_unique_for_overwrite()
    -> std::unique_ptr<T> {
  return std::unique_ptr<T>(new T);
}

// Should be std
template <class T>
requires(std::is_unbounded_array_v<T>) auto make_unique_for_overwrite(
    std::size_t n) -> std::unique_ptr<T> {
  return std::unique_ptr<T>(new std::remove_extent_t<T>[n]);
}

// Should be std
template <class T>
requires(std::is_bounded_array_v<T>) void make_unique_for_overwrite(
    auto &&...) = delete;

// C++23 P0211
template <typename T> auto allocator_new(auto allocator, auto &&...args) {
  auto x = allocator.allocate(1);
  new (x) T{std::forward<decltype(args)>(args)...};
  return x;
}

template <typename T> void allocator_delete(auto allocator, T *p) {
  p->~T();
  allocator.deallocate(p);
}

template <typename T, typename Allocator>
requires(!std::is_bounded_array_v<T>) struct allocation_deleter
    : private Allocator {
  void operator()(T *p) {
    if constexpr (std::is_unbounded_array_v<T>) {
      static_assert(std::is_trivially_destructible_v<std::remove_extent_t<T>>,
                    "delete[] isn't really possible");
    } else {
      allocator_delete(*this, p);
    }
  }
};

template <typename T, typename Allocator>
requires(!std::is_array_v<T>) auto allocate_unique(Allocator allocator,
                                                   auto &&...args) {
  auto x = allocator_new<T>(allocator, std::forward<decltype(args)>(args)...);
  return std::unique_ptr{
      x, allocation_deleter<T, Allocator>{std::move(allocator)}};
}

template <typename T, typename Allocator>
requires(std::is_unbounded_array_v<T>) auto allocate_unique(Allocator allocator,
                                                            std::size_t n) {
  auto x = allocator.allocate(n);
  new (x) std::remove_extent_t<T>[ n ]();
  return std::unique_ptr{
      x, allocation_deleter<T, Allocator>{std::move(allocator)}};
}

template <typename T, typename Allocator>
requires(std::is_bounded_array_v<T>) auto allocate_unique(
    Allocator allocator, auto &&...args) = delete;

template <typename T, typename Allocator>
requires(!std::is_array_v<T>) auto allocate_unique_for_overwrite(
    Allocator allocator) {
  auto x = allocator.allocate(1);
  new (x) T;
  return std::unique_ptr{
      x, allocation_deleter<T, Allocator>{std::move(allocator)}};
}

template <typename T, typename Allocator>
requires(std::is_unbounded_array_v<T>) auto allocate_unique_for_overwrite(
    Allocator allocator, std::size_t n) {
  auto x = allocator.allocate(n);
  new (x) std::remove_extent_t<T>[ n ];
  return std::unique_ptr{
      x, allocation_deleter<T, Allocator>{std::move(allocator)}};
}

template <typename T, typename Allocator>
requires(std::is_bounded_array_v<T>) auto allocate_unique_for_overwrite(
    Allocator allocator, auto &&...args) = delete;

static_assert(sizeof(unsigned long long *) <= sizeof(unsigned long long),
              "ptr doesn't fit in unsigned long long");

static_assert(alignof(unsigned long long *) <= alignof(unsigned long long),
              "ptr aligns more than unsigned long long");

template <typename Container> class stack_allocator_storage {
private:
  Container buf;
  typename Container::pointer sp;

public:
  constexpr stack_allocator_storage(stack_allocator_storage &) = delete;
  constexpr stack_allocator_storage(stack_allocator_storage const &) = delete;
  constexpr auto operator=(stack_allocator_storage const &)
      -> stack_allocator_storage & = delete;
  constexpr stack_allocator_storage(stack_allocator_storage &&) = delete;
  constexpr auto operator=(stack_allocator_storage &&)
      -> stack_allocator_storage & = delete;

  constexpr stack_allocator_storage(auto &&...args)
      : buf{std::forward<decltype(args)>(args)...}, sp{buf.begin()} {}

  constexpr auto allocate(std::size_t n) -> typename Container::pointer {
    if (sp + n <= &*buf.end()) {
      return std::exchange(sp, sp + n);
    } else {
      std::terminate();
    }
  }

  constexpr void deallocate(typename Container::pointer p, std::size_t n) {
    if (p + n == sp) {
      sp = p;
    } else {
      std::terminate();
    }
  }
};

template <typename Container> class stack_allocator {
private:
  stack_allocator_storage<Container> &allocator;

public:
  constexpr stack_allocator(stack_allocator_storage<Container> &allocator)
      : allocator{allocator} {}

  constexpr stack_allocator(stack_allocator_storage<Container> &&allocator = {})
      : stack_allocator{allocator} {}

  constexpr auto allocate(std::size_t n) { return allocator.allocate(n); }

  constexpr void deallocate(typename Container::pointer p, std::size_t n) {
    allocator.deallocate(p, n);
  }
};

template <typename T, typename Allocator = std::allocator<T>>
class dyn_buffer : private Allocator {
private:
  T *_data;
  std::size_t _size;

public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = value_type &;
  using const_reference = const value_type &;
  using pointer = value_type *;
  using const_pointer = const value_type *;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  auto data() -> T * { return _data; }
  auto data() const -> T const * { return _data; }

  auto size() const { return _size; }

  auto begin() -> T * { return _data; }
  auto begin() const -> T const * { return _data; }
  auto end() -> T * { return _data + _size; }
  auto end() const -> T const * { return _data + _size; }

  auto operator[](std::size_t i) -> T & { return _data[i]; }
  auto operator[](std::size_t i) const -> T const & { return _data[i]; }

  operator std::span<T>() { return {data(), size()}; }
  operator std::span<T const>() const { return {data(), size()}; }

  dyn_buffer(std::nullptr_t) : _data{nullptr}, _size{0} {}

  explicit dyn_buffer(std::size_t size, Allocator allocator = {})
      : Allocator{std::move(allocator)}, _data{this->allocate(size)},
        _size{size} {
    new (_data) T[size]{}; // TODO: This is value initialising for safety
  }

  friend void swap(dyn_buffer &lhs, dyn_buffer &rhs) {
    using std::swap;
    swap(lhs._data, rhs._data);
    swap(lhs._size, rhs._size);
  }

  dyn_buffer(dyn_buffer const &) = delete;
  auto operator=(dyn_buffer const &) -> dyn_buffer & = delete;

  dyn_buffer(dyn_buffer &&that)
      : _data{steal(that._data)}, _size{steal(that._size)} {}

  auto operator=(dyn_buffer &&that) -> dyn_buffer & {
    auto tmp = std::move(that);
    swap(*this, tmp);
    return *this;
  }

  ~dyn_buffer() {
    for (auto &x : *this) {
      x.~T();
    }
    this->deallocate(_data, _size);
  }
};

class dyn_int_const_view;
class dyn_int_view;
class dyn_int;

template <typename Allocator = std::allocator<unsigned long long>>
void mult_and_add(dyn_int_const_view x, dyn_int_const_view y, dyn_int_view into,
                  Allocator allocator = {});

template <typename Allocator = std::allocator<unsigned long long>>
void divide(dyn_int &x, dyn_int_const_view divisor, dyn_int &quotient,
            Allocator allocator = {});

template <typename CRTP> struct dyn_int_const_crtp {
private:
  auto view() const -> decltype(auto) {
    return static_cast<CRTP const &>(*this).view();
  }

public:
  auto lsq() const { return view().front(); }
  auto msq() const { return view().back(); }

  auto get_bit_range(std::size_t count, std::size_t pos) const;

  auto signum() const -> int {
    auto view = this->view();
    if (to_signed(msq()) < 0) {
      return -1;
    } else if (to_signed(msq()) > 0) {
      return +1;
    } else if (view == 0) {
      return 0;
    } else {
      return +1;
    }
  }

  template <floating_point T = double> auto to_float() const -> T {
    auto view = this->view();
    auto size = view.size();
    auto quad_to_double = [&](std::size_t i) {
      return ldexp(static_cast<T>(view[i]),
                   i * sizeof(unsigned long long) * CHAR_BIT);
    };
    return quad_to_double(size - 1) + quad_to_double(size - 2);
  }

  auto is_set(size_t n) const {
    auto quad = view()[n / (sizeof(unsigned long long) * CHAR_BIT)];
    return (quad >> (n % (sizeof(unsigned long long) * CHAR_BIT)) & 1);
  }

  // Return k if n == 2^(k-1), otherwise return 0
  auto is_pow_2() const -> size_t {
    auto k = 0u;
    auto it = view().begin();
    auto end = view().end();
    while (it != end && *it == 0) {
      k += sizeof(unsigned long long) * CHAR_BIT;
      ++it;
    }
    if (it == end) {
      return 0;
    } else {
      k += __builtin_clzll(*it);
      if (__builtin_popcountll(*it) != 1) {
        k = 0;
      }
    }
    ++it;
    while (it != end) {
      if (*it != 0) {
        return 0;
      }
      ++it;
    }
    return k;
  }

  auto length() const {
    // TODO fix
    auto tmp = view();
    tmp.trim_leading_zeros();
    return (tmp.view().end() - tmp.view().begin()) *
           sizeof(unsigned long long) * CHAR_BIT;
  }

  auto to_string(int base = 10);

  friend auto operator<<(std::ostream &os, dyn_int_const_crtp const &rhs)
      -> std::ostream & {
    auto flags = os.flags();
    os << std::hex << std::noshowbase << std::setfill('0');
    auto is_nonnegative = (*rhs.view().rbegin() >>
                           (sizeof(unsigned long long) * CHAR_BIT - 1)) == 0;
    if (!is_nonnegative) {
      os << '-';
    }
    auto drop_carry_point =
        std::find_if_not(rhs.view().begin(), rhs.view().end(), equals{0ull});
    auto carry = 0ull;
    for (auto it = std::reverse_iterator{rhs.view().end()};
         it != std::reverse_iterator{rhs.view().begin()}; ++it) {
      // TODO: urgh
      if (&*it == &*drop_carry_point) {
        carry = 1ull;
      }
      os << std::setw(sizeof(unsigned long long) * 2)
         << (is_nonnegative ? *it : ~*it + carry) << ' ';
    }
    os.flags(flags);
    return os;
  }

  template <typename> friend struct dyn_int_crtp;
};

template <typename CRTP> struct dyn_int_crtp : dyn_int_const_crtp<CRTP> {
private:
  using dyn_int_const_crtp<CRTP>::view;
  auto view() -> decltype(auto) { return static_cast<CRTP &>(*this).view(); }

public:
  auto set_bit(size_t n, bool val) {
    auto &quad = view()[n / (sizeof(unsigned long long) * CHAR_BIT)];
    if (val) {
      quad |= 1 << (n % (sizeof(unsigned long long) * CHAR_BIT));
    } else {
      quad &= ~(1 << (n % (sizeof(unsigned long long) * CHAR_BIT)));
    }
  }

  void bitwise_not() {
    for (auto &x : view()) {
      x = ~x;
    }
  }
};

class dyn_int_const_view : private std::span<unsigned long long const>,
                           public dyn_int_const_crtp<dyn_int_const_view> {

  dyn_int_const_view(std::span<unsigned long long const> data)
      : std::span<unsigned long long const>{data} {}

  dyn_int_const_view(std::span<unsigned long long> data)
      : std::span<unsigned long long const>{data} {}

  template <std::size_t N>
  dyn_int_const_view(std::array<unsigned long long, N> const &data)
      : std::span<unsigned long long const>{data} {}

  template <typename Allocator>
  dyn_int_const_view(dyn_buffer<unsigned long long, Allocator> const &data)
      : std::span<unsigned long long const>{data} {}

public:
  dyn_int_const_view(__uint128_t const &data)
      : std::span<unsigned long long const>{
            reinterpret_cast<unsigned long long const *>(&data),
            reinterpret_cast<unsigned long long const *>(&data + 1)} {}

  auto view() const -> dyn_int_const_view { return *this; }

  void trim_leading_zeros() {
    while (back() == 0) {
      *this = first(size() - 1);
    }
  }

  auto subspan(std::size_t offset,
               std::size_t count = std::dynamic_extent) const
      -> dyn_int_const_view {
    return static_cast<std::span<unsigned long long const> const *>(this)
        ->subspan(offset, count);
  }

  friend class dyn_int;
  friend class dyn_int_view;
  template <typename> friend struct dyn_int_const_crtp;
  template <typename> friend struct dyn_int_crtp;

  friend auto operator==(dyn_int_const_view lhs, dyn_int_const_view rhs)
      -> bool;

  friend auto operator<=>(dyn_int_const_view lhs, dyn_int_const_view rhs)
      -> std::strong_ordering;

  friend struct std::hash<dyn_int_const_view>;

  friend auto ripple_add_or_sub(auto fullAdder, dyn_int_const_view lhs,
                                dyn_int_const_view rhs, dyn_int_view dest)
      -> std::optional<unsigned long long>;

  friend auto bitwise_op(auto op, dyn_int_const_view lhs,
                         dyn_int_const_view rhs, dyn_int_view dest) -> void;

  friend auto left_shift(dyn_int_const_view src, int by, dyn_int_view dest)
      -> std::optional<unsigned long long>;

  friend void right_arithmetic_shift(dyn_int_const_view src, int by,
                                     dyn_int_view dest);

  friend void mult_and_add(unsigned long long x, dyn_int_const_view y,
                           dyn_int_view into);
  friend void mult_and_add(dyn_int_const_view x, unsigned long long y,
                           dyn_int_view into);

  template <typename Allocator>
  friend void mult_and_add(dyn_int_const_view x, dyn_int_const_view y,
                           dyn_int_view into, Allocator allocator);

  template <typename Allocator>
  friend auto mult_and_sub_borrow(dyn_int_const_view x, dyn_int_const_view y,
                                  dyn_int_view into, Allocator allocator)
      -> std::optional<unsigned long long>;

  template <typename Allocator>
  friend void divide(dyn_int &x, dyn_int_const_view divisor, dyn_int &quotient,
                     Allocator allocator);
};

inline auto operator==(dyn_int_const_view lhs, dyn_int_const_view rhs) -> bool {
  auto lhs_size = lhs.size();
  auto rhs_size = rhs.size();

  auto lhs_data = lhs.data();
  auto rhs_data = rhs.data();

  auto i = 0ul;
  for (; i < lhs_size && i < rhs_size; i += 1) {
    if (lhs_data[i] != rhs_data[i]) {
      return false;
    }
  }

  for (; i < lhs_size; i += 1) {
    auto const expected_quad =
        rhs.msq() >> (sizeof(unsigned long long) * CHAR_BIT - 1);
    if (lhs_data[i] != expected_quad) {
      return false;
    }
  }
  for (; i < rhs_size; i += 1) {
    auto const expected_quad =
        lhs.msq() >> (sizeof(unsigned long long) * CHAR_BIT - 1);
    if (rhs_data[i] != expected_quad) {
      return false;
    }
    i += 1;
  }

  return true;
}

inline auto operator<=>(dyn_int_const_view lhs, dyn_int_const_view rhs)
    -> std::strong_ordering {
  // TODO iterating wrong way
  switch (lhs.signum()) {
  case 0:
    switch (rhs.signum()) {
    case 0:
      return std::strong_ordering::equal;
    case +1:
      return std::strong_ordering::less;
    case -1:
      return std::strong_ordering::equal;
    }
  case +1:
    switch (rhs.signum()) {
    case 0:
      return std::strong_ordering::greater;
    case -1:
      return std::strong_ordering::greater;
    case +1: {
      auto lhs_size = lhs.size();
      auto rhs_size = rhs.size();

      auto lhs_data = lhs.data();
      auto rhs_data = rhs.data();

      auto i = 0l;

      if (lhs_size >= rhs_size) {
        i = lhs_size - 1;
        for (; i >= rhs_size; i -= 1) {
          if (lhs_data[i] != 0) {
            return std::strong_ordering::greater;
          }
        }
      } else {
        i = rhs_size - 1;
        for (; i >= lhs_size; i -= 1) {
          if (rhs_data[i] != 0) {
            return std::strong_ordering::less;
          }
          i += 1;
        }
      }

      for (; i >= 0; i -= 1) {
        auto ord = lhs_data[i] <=> rhs_data[i];
        if (is_neq(ord)) {
          return ord;
        }
      }

      return std::strong_ordering::equal;
    }
    }
  case -1:
    switch (rhs.signum()) {
    case 0:
      return std::strong_ordering::less;
    case +1:
      return std::strong_ordering::less;
    case -1: {
      auto lhs_size = lhs.size();
      auto rhs_size = rhs.size();

      auto lhs_data = lhs.data();
      auto rhs_data = rhs.data();

      auto i = 0l;

      if (lhs_size >= rhs_size) {
        i = lhs_size - 1;
        for (; i >= rhs_size; i -= 1) {
          if (lhs_data[i] != 0) {
            return std::strong_ordering::less;
          }
        }
      } else {
        i = rhs_size - 1;
        for (; i >= lhs_size; i -= 1) {
          if (rhs_data[i] != 0) {
            return std::strong_ordering::greater;
          }
          i += 1;
        }
      }

      for (; i >= 0; i -= 1) {
        auto ord = lhs_data[i] <=> rhs_data[i];
        if (is_neq(ord)) {
          return ord;
        }
      }

      return std::strong_ordering::equal;
    }
    }
  }
  std::cerr << "Impossible signum\n";
  std::terminate();
}

template <> struct std::hash<dyn_int_const_view> {
  auto operator()(dyn_int_const_view x) const noexcept -> std::size_t {
    auto hash_combine = [](std::size_t lhs, std::size_t rhs) -> std::size_t {
      return lhs ^ (rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2));
    };

    return std::accumulate(x.begin(), x.end(), 0, hash_combine);
  }
};

class dyn_int_view : private std::span<unsigned long long>,
                     public dyn_int_crtp<dyn_int_view> {
private:
  dyn_int_view(std::span<unsigned long long> data)
      : std::span<unsigned long long>{data} {}

  template <std::size_t N>
  dyn_int_view(std::array<unsigned long long, N> &data)
      : std::span<unsigned long long>{data} {}

  template <typename Allocator>
  dyn_int_view(dyn_buffer<unsigned long long, Allocator> &data)
      : std::span<unsigned long long>{data} {}

public:
  operator dyn_int_const_view() {
    return std::span<unsigned long long const>{*this};
  }

  auto view() -> dyn_int_view { return *this; }
  auto view() const -> dyn_int_const_view { return *this; }

  void trim_leading_zeros() {
    while (back() == 0) {
      *this = first(size() - 1);
    }
  }

  auto subspan(std::size_t offset,
               std::size_t count = std::dynamic_extent) const -> dyn_int_view {
    return static_cast<std::span<unsigned long long> const *>(this)->subspan(
        offset, count);
  }

  friend class dyn_int;
  template <typename> friend struct dyn_int_const_crtp;
  template <typename> friend struct dyn_int_crtp;

  friend auto ripple_add_or_sub(auto fullAdder, dyn_int_const_view lhs,
                                dyn_int_const_view rhs, dyn_int_view dest)
      -> std::optional<unsigned long long>;

  friend auto bitwise_op(auto op, dyn_int_const_view lhs,
                         dyn_int_const_view rhs, dyn_int_view dest) -> void;

  friend auto left_shift(dyn_int_const_view src, int by, dyn_int_view dest)
      -> std::optional<unsigned long long>;

  friend void right_arithmetic_shift(dyn_int_const_view src, int by,
                                     dyn_int_view dest);

  friend void mult_and_add(unsigned long long x, dyn_int_const_view y,
                           dyn_int_view into);
  friend void mult_and_add(dyn_int_const_view x, unsigned long long y,
                           dyn_int_view into);

  template <typename Allocator>
  friend void mult_and_add(dyn_int_const_view x, dyn_int_const_view y,
                           dyn_int_view into, Allocator allocator);

  template <typename Allocator>
  friend auto mult_and_sub_borrow(dyn_int_const_view x, dyn_int_const_view y,
                                  dyn_int_view into, Allocator allocator)
      -> std::optional<unsigned long long>;

  template <typename Allocator>
  friend void divide(dyn_int &x, dyn_int_const_view divisor, dyn_int &quotient,
                     Allocator allocator);
};

inline auto sar(unsigned long long lhs, int rhs) -> unsigned long long {
  return static_cast<unsigned long long>(static_cast<signed long long>(lhs) >>
                                         rhs);
}

// dest may overlap with lhs or rhs
auto ripple_add_or_sub(auto fullAdder, dyn_int_const_view lhs,
                       dyn_int_const_view rhs, dyn_int_view dest)
    -> std::optional<unsigned long long> {
  auto output = [&](std::size_t i, unsigned long long x) {
    if (i < dest.size()) {
      dest[i] = x;
    } else if (x != 0) {
      // TODO negative numbers
      std::cerr << "Rippling non zero value out\n";
      std::terminate();
    }
  };

  auto carry = 0ull;
  auto i = 0ul;
  auto last_lhs = 0ull;
  auto last_rhs = 0ull;
  for (; i < lhs.size() && i < rhs.size(); i += 1) {
    last_lhs = lhs[i];
    last_rhs = rhs[i];
    output(i, fullAdder(last_lhs, last_rhs, carry, &carry));
  }
  if (i < lhs.size()) {
    last_rhs = sar(last_rhs, sizeof(unsigned long long) * CHAR_BIT - 1);
    for (; i < lhs.size(); i += 1) {
      last_lhs = lhs[i];
      output(i, fullAdder(last_lhs, last_rhs, carry, &carry));
    }
  } else if (i < rhs.size()) {
    last_lhs = sar(last_lhs, sizeof(unsigned long long) * CHAR_BIT - 1);
    for (; i < rhs.size(); i += 1) {
      last_rhs = rhs[i];
      output(i, fullAdder(last_lhs, last_rhs, carry, &carry));
    }
  }

  if (i - 1 < dest.size()) {
    if ((last_lhs >> (sizeof(unsigned long long) * CHAR_BIT - 1)) ^
        (last_rhs >> (sizeof(unsigned long long) * CHAR_BIT - 1)) ^
        (dest[i - 1] >> (sizeof(unsigned long long) * CHAR_BIT - 1)) ^ carry) {
      return carry;
    } else {
      return std::nullopt;
    }
  } else {
    return std::nullopt;
  }
}

inline auto ripple_adder(dyn_int_const_view lhs, dyn_int_const_view rhs,
                         dyn_int_view dest) {
  return ripple_add_or_sub(
      [](unsigned long long lhs, unsigned long long rhs, unsigned long long cin,
         unsigned long long *cout) -> unsigned long long {
        return __builtin_addcll(lhs, rhs, cin, cout);
      },
      lhs, rhs, dest);
}

inline auto ripple_subber(dyn_int_const_view lhs, dyn_int_const_view rhs,
                          dyn_int_view dest) {
  return ripple_add_or_sub(
      [](unsigned long long lhs, unsigned long long rhs, unsigned long long cin,
         unsigned long long *cout) -> unsigned long long {
        return __builtin_subcll(lhs, rhs, cin, cout);
      },
      lhs, rhs, dest);
}

// dest may overlap with lhs or rhs
void bitwise_op(auto op, dyn_int_const_view lhs, dyn_int_const_view rhs,
                dyn_int_view dest) {
  auto i = 0ul;
  auto last_lhs = 0ull;
  auto last_rhs = 0ull;
  for (; i < lhs.size() && i < rhs.size(); i += 1) {
    last_lhs = lhs[i];
    last_rhs = rhs[i];
    dest[i] = op(last_lhs, last_rhs);
  }
  if (i < lhs.size()) {
    last_rhs = sar(last_rhs, sizeof(unsigned long long) * CHAR_BIT - 1);
    for (; i < lhs.size(); i += 1) {
      last_lhs = lhs[i];
      dest[i] = op(last_lhs, last_rhs);
    }
  } else if (i < rhs.size()) {
    last_lhs = sar(last_lhs, sizeof(unsigned long long) * CHAR_BIT - 1);
    for (; i < rhs.size(); i += 1) {
      last_rhs = rhs[i];
      dest[i] = op(last_lhs, last_rhs);
    }
  }
}

inline auto bitwise_and(dyn_int_const_view lhs, dyn_int_const_view rhs,
                        dyn_int_view dest) {
  return bitwise_op(
      [](unsigned long long lhs, unsigned long long rhs) -> unsigned long long {
        return lhs & rhs;
      },
      lhs, rhs, dest);
}

inline auto bitwise_or(dyn_int_const_view lhs, dyn_int_const_view rhs,
                       dyn_int_view dest) {
  return bitwise_op(
      [](unsigned long long lhs, unsigned long long rhs) -> unsigned long long {
        return lhs | rhs;
      },
      lhs, rhs, dest);
}

inline auto bitwise_xor(dyn_int_const_view lhs, dyn_int_const_view rhs,
                        dyn_int_view dest) {
  return bitwise_op(
      [](unsigned long long lhs, unsigned long long rhs) -> unsigned long long {
        return lhs ^ rhs;
      },
      lhs, rhs, dest);
}

// dest may overlap with src
inline auto left_shift(dyn_int_const_view src, int by, dyn_int_view dest)
    -> std::optional<unsigned long long> {

  if (by >= sizeof(unsigned long long) * CHAR_BIT) {
    std::cerr << "Left shift that's too large\n";
    std::terminate();
  }

  auto carry = 0ull;
  auto last_src = 0ull;
  for (auto i = 0ul; i < src.size(); i += 1) {
    last_src = src[i];
    dest[i] = last_src << by;
    dest[i] |= carry;
    carry = last_src >> (sizeof(unsigned long long) * CHAR_BIT -
                         static_cast<unsigned int>(by));
  }

  // Check if there are enough redundant sign bits not to need to carry out
  if (__builtin_clrsbll(static_cast<signed long long>(last_src)) >= by) {
    return std::nullopt;
  } else {
    return static_cast<signed long long>(last_src) >>
           (sizeof(unsigned long long) * CHAR_BIT -
            static_cast<unsigned int>(by));
  }
}

// dest may overlap with src
inline void right_arithmetic_shift(dyn_int_const_view src, int by,
                                   dyn_int_view dest) {

  if (by >= sizeof(unsigned long long) * CHAR_BIT) {
    std::cerr << "Right shift that's too large\n";
    std::terminate();
  }

  auto i = to_signed(src.size()) - 1;

  auto last_src = src[static_cast<std::size_t>(i)];
  dest[static_cast<std::size_t>(i)] = static_cast<unsigned long long>(
      static_cast<signed long long>(last_src) >> by);
  auto carry = last_src << (sizeof(unsigned long long) * CHAR_BIT -
                            static_cast<unsigned int>(by));

  i -= 1;

  for (; i >= 0; i -= 1) {
    last_src = src[static_cast<std::size_t>(i)];
    dest[static_cast<std::size_t>(i)] = last_src >> by;
    dest[static_cast<std::size_t>(i)] |= carry;
    carry = last_src << (sizeof(unsigned long long) * CHAR_BIT -
                         static_cast<unsigned int>(by));
  }
}

constexpr auto mult128(unsigned long long x, unsigned long long y)
    -> __uint128_t {
  return static_cast<__uint128_t>(x) * static_cast<__uint128_t>(y);
}

constexpr auto concat(unsigned long long lsq, unsigned long long msq)
    -> __uint128_t {
  return lsq | (static_cast<__uint128_t>(msq)
                << (sizeof(unsigned long long) * CHAR_BIT));
}

inline void mult_and_add(unsigned long long x, dyn_int_const_view y,
                         dyn_int_view into) {
  for (auto i = 0u; i < y.size(); i += 1) {
    auto wide_tmp = mult128(x, y[i]);
    auto tmp_bottom = static_cast<unsigned long long>(wide_tmp);
    auto tmp_top = static_cast<unsigned long long>(wide_tmp >> 64);
    auto carry = 0ull;
    auto j = i;
    into[j] = __builtin_addcll(into[j], tmp_bottom, carry, &carry);
    j += 1;
    into[j] = __builtin_addcll(into[j], tmp_top, carry, &carry);
    j += 1;
    while (carry) {
      into[j] = __builtin_addcll(into[j], 0, carry, &carry);
      j += 1;
    }
  }
}

inline void mult_and_add(dyn_int_const_view x, unsigned long long y,
                         dyn_int_view into) {
  mult_and_add(y, x, into);
}

// into must always be at least size x + y
template <typename Allocator>
void mult_and_add(dyn_int_const_view x, dyn_int_const_view y, dyn_int_view into,
                  Allocator allocator) {
  std::cerr << "mult_and_add sizes: " << x.size() << ',' << y.size() << ','
            << into.size() << '\n';
  if (x.size() == 1) {
    mult_and_add(x[0], y, into);
  } else if (y.size() == 1) {
    mult_and_add(x, y[0], into);
  } else if (x.size() <= 4 && y.size() <= 4) {
    auto x2 = std::array<unsigned long long, 4>{};
    std::copy(x.begin(), x.end(), x2.begin());
    auto y2 = std::array<unsigned long long, 4>{};
    std::copy(y.begin(), y.end(), y2.begin());

    auto grid = std::array<std::array<__uint128_t, 4>, 4>{};
    for (auto i = 0ul; i < 4; i += 1) {
      for (auto j = 0ul; j < 4; j += 1) {
        grid[i][j] = mult128(x2[i], y2[j]);
      }
    }

    ripple_adder(into.subspan(0), grid[0][0], into.subspan(0));

    ripple_adder(into.subspan(1), grid[0][1], into.subspan(1));
    ripple_adder(into.subspan(1), grid[1][0], into.subspan(1));

    ripple_adder(into.subspan(2), grid[0][2], into.subspan(2));
    ripple_adder(into.subspan(2), grid[1][1], into.subspan(2));
    ripple_adder(into.subspan(2), grid[2][0], into.subspan(2));

    ripple_adder(into.subspan(3), grid[0][3], into.subspan(3));
    ripple_adder(into.subspan(3), grid[2][1], into.subspan(3));
    ripple_adder(into.subspan(3), grid[1][2], into.subspan(3));
    ripple_adder(into.subspan(3), grid[3][0], into.subspan(3));

    ripple_adder(into.subspan(4), grid[3][1], into.subspan(4));
    ripple_adder(into.subspan(4), grid[2][2], into.subspan(4));
    ripple_adder(into.subspan(4), grid[1][3], into.subspan(4));

    ripple_adder(into.subspan(5), grid[3][2], into.subspan(5));
    ripple_adder(into.subspan(5), grid[2][3], into.subspan(5));

    ripple_adder(into.subspan(6), grid[3][3], into.subspan(6));
  } else {
    auto half =
        std::max(x.size(), y.size()) - (std::max(x.size(), y.size()) / 2);
    // x = az + b
    // y = cz + d
    auto a = x.subspan(half);
    auto b = x.subspan(0, half);
    auto c = y.subspan(half);
    auto d = y.subspan(0, half);

    {
      auto apb = dyn_buffer<unsigned long long, Allocator>{half + 1, allocator};
      ripple_adder(a, b, apb);

      auto cpd = dyn_buffer<unsigned long long, Allocator>{half + 1, allocator};
      ripple_adder(c, d, cpd);

      // into z += (a+b)(c+d)
      // into z += ac+ad+bc+bd
      mult_and_add(apb, cpd, into.subspan(half), allocator);
    }

    {
      auto ac = dyn_buffer<unsigned long long, Allocator>{half * 2, allocator};
      mult_and_add(a, c, ac, allocator);

      // into z^2 += ac
      ripple_adder(into.subspan(half * 2), ac, into.subspan(half * 2));

      // into z -= ac
      ripple_subber(into.subspan(half), ac, into.subspan(half));
    }

    {
      auto bd = dyn_buffer<unsigned long long, Allocator>{half * 2, allocator};
      mult_and_add(b, d, bd, allocator);

      // into 1 += bd
      ripple_adder(into.subspan(0), bd, into.subspan(0));

      // into z -= bd
      ripple_subber(into.subspan(half), bd, into.subspan(half));
    }
  }
}

/*
template <typename Allocator>
void divide(dyn_int_const_view x, dyn_int_view y, dyn_int& quotient,
            dyn_int_view remainder, Allocator allocator) {
    //std::cerr << "0\n";
  // D1
  y.trim_leading_zeros();
  auto d = __builtin_clzll(y.msq());
  left_shift(y, d, y);
  auto x2 = dyn_buffer<unsigned long long, Allocator>{x.size() + 1, allocator};
  if (auto carry = left_shift(x, d, x2)) {
    x2[x.size()] = *carry;
  }

  // Loop D2-D7
  for (auto j = x2.size() - 1; j >= y.size(); j -= 1) {
    //std::cerr << "1\n";
    auto &q_j = quotient[j - y.size()];

    //std::cerr << "2\n";
    // D3
    q_j = x2[j] == y.msq() ? ~0ull
                           : static_cast<unsigned long long>(
                                 concat(x2[j - 1], x2[j]) / y.msq());

    //std::cerr << "3\n";
    auto d3 = [&] {
      //std::cerr << "3b " << j << " : " << x2.size() << "\n";
      auto z = concat(x2[j - 1], x2[j]) - mult128(y.msq(), q_j);
      if (z != static_cast<unsigned long long>(z)) {
        return false;
      }
      //std::cerr << "3c " << y.size() << "\n";
      if (y.size() < 2) {
        return true; // TODO
      } else {
        return mult128(y[y.size() - 2], q_j) >
               concat(static_cast<unsigned long long>(z), x2[j - 2]);
      }
    };
    while (d3()) {
      q_j -= 1;
    }

    //std::cerr << "4\n";
    // D4
    auto multiplied = dyn_buffer<unsigned long long, Allocator>{
        y.size() + 4, allocator}; // TODO +4?
    mult_and_add(q_j, y, multiplied, allocator);
    auto borrow = ripple_subber(
        dyn_int_view{x2}.subspan(j - y.size() + 1, y.size()), multiplied,
        dyn_int_view{x2}.subspan(j - y.size() + 1, y.size()));

    //std::cerr << "5\n";
    // D5
    if (borrow) {
      // D6
      q_j -= 1;
      ripple_adder(y, dyn_int_view{x2}.subspan(j - y.size(), y.size() + 1),
                   dyn_int_view{x2}.subspan(j - y.size(), y.size() + 1));
    }
  }

  // D8
  right_arithmetic_shift(x2, d, remainder);
}
*/

class dyn_int : public dyn_int_crtp<dyn_int> {
private:
  static constexpr auto small_size = 4ul;
  // static constexpr auto small_size = 2ul;

  struct small_t : std::array<unsigned long long, small_size> {};

  struct large_t {
    dyn_buffer<unsigned long long> _data;
    unsigned long long _size;
    unsigned long long padding = 0b10ull
                                 << (sizeof(large.padding) * CHAR_BIT - 2);

    large_t(std::nullptr_t) : _data{nullptr}, _size{0} {}

    large_t(unsigned long long size) : _data{size}, _size{size} {}

    large_t(unsigned long long size, unsigned long long capacity)
        : _data{capacity}, _size{size} {}

    large_t(dyn_buffer<unsigned long long> data, unsigned long long size)
        : _data{std::move(data)}, _size{size} {}

    auto data() { return _data.data(); }
    auto data() const { return _data.data(); }
    auto size() const { return _size; }
    auto capacity() const { return _data.size(); }

    auto begin() { return _data.begin(); }
    auto begin() const { return _data.begin(); }
    auto end() { return _data.begin() + _size; }
    auto end() const { return _data.begin() + _size; }

    void swap(large_t &lhs, large_t &rhs) {
      using std::swap;
      swap(lhs._data, rhs._data);
      swap(lhs._size, rhs._size);
      swap(lhs.padding, rhs.padding);
    }

    large_t(large_t const &that) : _data{that.size()}, _size{that.size()} {
      std::copy(that.begin(), that.end(), this->begin());
    }

    large_t(large_t &&that) = default;

    // There's no nice way to implement this, since if we're not careful we
    // could actually be small
    auto operator=(large_t that) -> large_t & = delete;
  };

  //  static_assert(sizeof(small_t) == sizeof(large_t),
  //              "Small and large representations have different sizes");

  //  static_assert(sizeof(unsigned long long) * (small_size - 1) ==
  //                  offsetof(large_t, padding),
  //            "Large representation padding lines up with the MSB of the "
  //          "small representation buffer");

  union {
    small_t small;
    large_t large;
  };

  auto is_large() const -> bool {
    return large.padding >> (sizeof(large.padding) * CHAR_BIT - 2) == 0b10;
  }

  // Given that this definitely used to be small, check if the invariant is
  // broken and if so, fix it
  auto was_small_fix_invariant() {
    if (is_large()) {
      // Broken invariant so be very careful
      auto new_data = dyn_buffer<unsigned long long>{small_size + 1};
      std::copy(small.begin(), small.end(), new_data.begin());
      new (&large) large_t{std::move(new_data), small_size};
    }
  }

  auto data() -> unsigned long long * {
    return is_large() ? large.data() : small.data();
  }

  auto data() const -> unsigned long long const * {
    return is_large() ? large.data() : small.data();
  }

  auto size() const -> size_t {
    return is_large() ? large.size() : small.size();
  }

  auto capacity() const -> size_t {
    return is_large() ? large.capacity() : small.size();
  }

  auto begin() -> unsigned long long * {
    return is_large() ? large.begin() : small.begin();
  }

  auto begin() const -> unsigned long long const * {
    return is_large() ? large.begin() : small.begin();
  }

  auto end() -> unsigned long long * {
    return is_large() ? large.end() : small.end();
  }

  auto end() const -> unsigned long long const * {
    return is_large() ? large.end() : small.end();
  }

  auto rbegin() { return std::reverse_iterator{end()}; }
  auto rbegin() const { return std::reverse_iterator{end()}; }

  auto rend() { return std::reverse_iterator{begin()}; }
  auto rend() const { return std::reverse_iterator{begin()}; }

  auto cbegin() const { return begin(); }
  auto cend() const { return end(); }
  auto crbegin() const { return rbegin(); }
  auto crend() const { return rend(); }

  explicit dyn_int(unsigned long long x, unsigned long long sign) {
    small[0] = x;
    std::fill(small.begin() + 1, small.end(), sign);
  }

  dyn_int(std::array<unsigned long long, small_size> data) : small{data} {
    was_small_fix_invariant();
  }

  dyn_int(dyn_buffer<unsigned long long> data, unsigned long long size)
      : large{std::move(data), size} {
    if (size > 1000) {
      std::cerr << "Too big\n";
      std::terminate();
    }
  }

public:
  operator dyn_int_view() { return {{begin(), end()}}; }
  operator dyn_int_const_view() const { return {{begin(), end()}}; }

  auto view() -> dyn_int_view { return *this; }
  auto view() const -> dyn_int_const_view { return *this; }

  dyn_int() : small{} {}

  dyn_int(integral auto x) requires(sizeof(x) <= sizeof(unsigned long long))
      : dyn_int{
            static_cast<unsigned long long>(x),
            static_cast<unsigned long long>(static_cast<signed long long>(x) >>
                                            (sizeof(x) * CHAR_BIT - 1))} {}

  dyn_int(std::string_view str, unsigned int base = 10) : small{} {
    auto bits_per_digit = std::log2(base);
    auto str_small_capacity = bits_per_digit * sizeof(unsigned long long) * 2;

    if (str.size() >= str_small_capacity - 0.5) { // TODO can expand slightly
      // TODO check this
      new (&large) large_t{static_cast<std::size_t>(std::ceil(
                               str.size() * bits_per_digit /
                               (sizeof(unsigned long long) * CHAR_BIT))) +
                           1};
    }

    constexpr auto take_suffix = [](std::string_view &str, std::size_t n) {
      if (str.size() < n) {
        return steal(str);
      } else {
        auto suffix = str.substr(str.size() - n);
        str.remove_suffix(n);
        return suffix;
      }
    };

    constexpr auto parse_hex_nybble = [](char ch) -> unsigned int {
      auto x = static_cast<unsigned char>(ch) & 0b1111u;
      switch (ch >> 4) {
      case 0b0011: // 0..9
        return x;
      case 0b0100: // A..F
        return x + 9;
      case 0b0110: // a..f
        return x + 9;
      default:
        return 0u;
      }
    };

    /*
        auto it = begin();
        while (!str.empty()) {
          auto suffix = take_suffix(str, sizeof(unsigned long long) * 2);
          auto x = 0ull;
          for (auto ch : suffix) {
            x *= base;
            x += parse_hex_nybble(ch);
          }
          *it = x;
          ++it;
        }
    */

    // TODO
    auto str2 = std::string{str};
    std::reverse(str2.begin(), str2.end());
    while (!str2.empty()) {
      *this *= base;
      *this += parse_hex_nybble(str2.back());
      str2.pop_back();
    }
  }

  friend void swap(dyn_int &lhs, dyn_int &rhs) {
    std::swap(lhs.small, rhs.small);
  }

  dyn_int(dyn_int const &that) {
    if (that.is_large()) {
      new (&large) large_t{that.large};
    } else {
      new (&small) small_t{that.small};
    }
  }

  dyn_int &operator=(dyn_int const &that) {
    if (that.size() <= this->capacity()) {
      std::copy(that.data(), that.data() + that.size(), this->data());
    } else {
      auto tmp = that;
      swap(*this, tmp);
    }
    return *this;
  }

  dyn_int(dyn_int &&that) noexcept : small{steal(that.small)} {}

  dyn_int &operator=(dyn_int &&that) noexcept {
    auto tmp = std::move(that);
    swap(*this, tmp);
    return *this;
  }

  // TODO
  dyn_int(dyn_int_const_view that)
      : dyn_int{[&] {
          if (that.size() > small_size) {
            auto res_data = dyn_buffer<unsigned long long>{that.size()};
            std::copy(that.begin(), that.end(), res_data.begin());
            return dyn_int{std::move(res_data), that.size()};
          } else {
            auto res_data = std::array<unsigned long long, small_size>{};
            std::copy(that.begin(), that.end(), res_data.begin());
            return dyn_int{std::move(res_data)};
          }
        }()} {}

  ~dyn_int() {
    if (is_large()) {
      // Large
      large.~large_t();
    }
  }

  static auto copy_with_capacity(dyn_int_const_view view,
                                 unsigned long long capacity) -> dyn_int {
    auto new_data = dyn_buffer<unsigned long long>{capacity};
    std::copy(view.begin(), view.end(), new_data.begin());
    return dyn_int{std::move(new_data), view.size()};
  }

  void push_msq(unsigned long long x) {
    if (size() >= capacity()) {
      *this = copy_with_capacity(view(), capacity() + 1);
    }
    large._data[large._size] = x;
    large._size += 1;
  }

  auto one_extend(std::size_t size, std::size_t amount) const {
    auto quad_bits = sizeof(unsigned long long) * CHAR_BIT;
    auto x = copy_with_capacity(view(), ((size + amount - 1) / quad_bits) + 1);
    if (amount < quad_bits - size % quad_bits) {
      x.view()[size / quad_bits] |=
          (~0 << (size % quad_bits)) & ~(~0 << (amount + size % quad_bits));
    } else {
      auto i = size / quad_bits;
      x.view()[i] |= ~0 << (size % quad_bits);
      amount -= quad_bits - size % quad_bits;
      i += 1;
      while (amount >= quad_bits) {
        x.view()[i] = ~0;
        amount -= quad_bits;
        i += 1;
      }
      x.view()[i] |= ~(~0 << amount);
    }
    return x;
  }

  template <integral T> auto get() const -> std::optional<T> {
    if (!is_large()) {
      if constexpr (std::is_signed_v<T>) {
        if (std::all_of(small.begin() + 1, small.end(), equals{0ull}) &&
            small.front() <= std::numeric_limits<T>::max()) {
          return static_cast<T>(small.front());
        } else if (std::all_of(small.begin() + 1, small.end(), equals{~0ull}) &&
                   ~small.front() <= std::numeric_limits<T>::max()) {
          return static_cast<T>(small.front());
        }
      } else {
        if (std::all_of(small.begin() + 1, small.end(), equals{0ull}) &&
            small.front() <= std::numeric_limits<T>::max()) {
          return static_cast<T>(small.front());
        }
      }
    }
    return std::nullopt;
  }

  auto operator~() const & {
    auto x = *this;
    x.bitwise_not();
    return x;
  }

  auto operator~() && {
    auto x = std::move(*this);
    x.bitwise_not();
    return x;
  }

  friend auto operator&(dyn_int const &lhs, dyn_int const &rhs) {
    if (lhs.is_large() || rhs.is_large()) {
      auto max_size = std::max(lhs.size(), rhs.size());
      auto res_data = dyn_buffer<unsigned long long>{max_size};
      bitwise_and(lhs, rhs, res_data);
      return dyn_int{std::move(res_data), max_size};
    } else {
      auto res_data = std::array<unsigned long long, small_size>{};
      bitwise_and(lhs.small, rhs.small, res_data);
      return dyn_int{std::move(res_data)};
    }
  }

  friend auto operator&(dyn_int &&lhs, dyn_int const &rhs) {
    if (lhs.size() >= rhs.size()) {
      bitwise_and(lhs, rhs, lhs);
      return steal(lhs);
    } else {
      auto res_data = dyn_buffer<unsigned long long>{rhs.size()};
      bitwise_and(lhs, rhs, res_data);
      return dyn_int{std::move(res_data), rhs.size()};
    }
  }

  friend auto operator&(dyn_int const &lhs, dyn_int &&rhs) {
    if (rhs.size() >= lhs.size()) {
      bitwise_and(lhs, rhs, rhs);
      return steal(rhs);
    } else {
      auto res_data = dyn_buffer<unsigned long long>{lhs.size()};
      bitwise_and(lhs, rhs, res_data);
      return dyn_int{std::move(res_data), lhs.size()};
    }
  }

  friend auto operator&(dyn_int &&lhs, dyn_int &&rhs) {
    if (lhs.size() >= rhs.size()) {
      bitwise_and(lhs, rhs, lhs);
      return steal(lhs);
    } else {
      bitwise_and(lhs, rhs, rhs);
      return steal(rhs);
    }
  }

  friend auto operator|(dyn_int const &lhs, dyn_int const &rhs) {
    if (lhs.is_large() || rhs.is_large()) {
      auto max_size = std::max(lhs.size(), rhs.size());
      auto res_data = dyn_buffer<unsigned long long>{max_size};
      bitwise_or(lhs, rhs, res_data);
      return dyn_int{std::move(res_data), max_size};
    } else {
      auto res_data = std::array<unsigned long long, small_size>{};
      bitwise_or(lhs.small, rhs.small, res_data);
      return dyn_int{std::move(res_data)};
    }
  }

  friend auto operator|(dyn_int &&lhs, dyn_int const &rhs) {
    if (lhs.size() >= rhs.size()) {
      bitwise_or(lhs, rhs, lhs);
      return steal(lhs);
    } else {
      auto res_data = dyn_buffer<unsigned long long>{rhs.size()};
      bitwise_or(lhs, rhs, res_data);
      return dyn_int{std::move(res_data), rhs.size()};
    }
  }

  friend auto operator|(dyn_int const &lhs, dyn_int &&rhs) {
    if (rhs.size() >= lhs.size()) {
      bitwise_or(lhs, rhs, rhs);
      return steal(rhs);
    } else {
      auto res_data = dyn_buffer<unsigned long long>{lhs.size()};
      bitwise_or(lhs, rhs, res_data);
      return dyn_int{std::move(res_data), lhs.size()};
    }
  }

  friend auto operator|(dyn_int &&lhs, dyn_int &&rhs) {
    if (lhs.size() >= rhs.size()) {
      bitwise_or(lhs, rhs, lhs);
      return steal(lhs);
    } else {
      bitwise_or(lhs, rhs, rhs);
      return steal(rhs);
    }
  }

  friend auto operator^(dyn_int const &lhs, dyn_int const &rhs) {
    if (lhs.is_large() || rhs.is_large()) {
      auto max_size = std::max(lhs.size(), rhs.size());
      auto res_data = dyn_buffer<unsigned long long>{max_size};
      bitwise_xor(lhs, rhs, res_data);
      return dyn_int{std::move(res_data), max_size};
    } else {
      auto res_data = std::array<unsigned long long, small_size>{};
      bitwise_xor(lhs.small, rhs.small, res_data);
      return dyn_int{std::move(res_data)};
    }
  }

  friend auto operator^(dyn_int &&lhs, dyn_int const &rhs) {
    if (lhs.size() >= rhs.size()) {
      bitwise_xor(lhs, rhs, lhs);
      return steal(lhs);
    } else {
      auto res_data = dyn_buffer<unsigned long long>{rhs.size()};
      bitwise_xor(lhs, rhs, res_data);
      return dyn_int{std::move(res_data), rhs.size()};
    }
  }

  friend auto operator^(dyn_int const &lhs, dyn_int &&rhs) {
    if (rhs.size() >= lhs.size()) {
      bitwise_xor(lhs, rhs, rhs);
      return steal(rhs);
    } else {
      auto res_data = dyn_buffer<unsigned long long>{lhs.size()};
      bitwise_xor(lhs, rhs, res_data);
      return dyn_int{std::move(res_data), lhs.size()};
    }
  }

  friend auto operator^(dyn_int &&lhs, dyn_int &&rhs) {
    if (lhs.size() >= rhs.size()) {
      bitwise_xor(lhs, rhs, lhs);
      return steal(lhs);
    } else {
      bitwise_xor(lhs, rhs, rhs);
      return steal(rhs);
    }
  }

  friend auto operator<<(dyn_int const &lhs, int rhs) {
    // TODO: carry
    if (lhs.is_large()) {
      auto res_data = dyn_buffer<unsigned long long>{lhs.size()};
      left_shift(lhs, rhs, res_data);
      return dyn_int{std::move(res_data), lhs.size()};
    } else {
      auto res_data = std::array<unsigned long long, small_size>{};
      left_shift(lhs.small, rhs, res_data);
      return dyn_int{std::move(res_data)};
    }
  }

  friend auto operator<<(dyn_int &&lhs, int rhs) {
    // TODO: carry
    left_shift(lhs, rhs, lhs);
    return steal(lhs);
  }

  friend auto operator>>(dyn_int const &lhs, int rhs) {
    // TODO: carry
    if (lhs.is_large()) {
      auto res_data = dyn_buffer<unsigned long long>{lhs.size()};
      right_arithmetic_shift(lhs, rhs, res_data);
      return dyn_int{std::move(res_data), lhs.size()};
    } else {
      auto res_data = std::array<unsigned long long, small_size>{};
      right_arithmetic_shift(lhs.small, rhs, res_data);
      return dyn_int{std::move(res_data)};
    }
  }

  friend auto operator>>(dyn_int &&lhs, int rhs) {
    // TODO: carry
    right_arithmetic_shift(lhs, rhs, lhs);
    return steal(lhs);
  }

  auto modPow2(int exp) const & -> dyn_int {
    auto new_size = (exp - 1) / (sizeof(unsigned long long) * CHAR_BIT) + 1;
    if (new_size > size()) {
      return *this;
    } else {
      if (exp > small_size * sizeof(unsigned long long) * CHAR_BIT - 1) {
        auto res_data = dyn_buffer<unsigned long long>{new_size};
        std::memcpy(res_data.data(), data(), new_size);
        res_data[new_size - 1] &=
            ~0ull >> (sizeof(unsigned long long) * CHAR_BIT -
                      exp % (sizeof(unsigned long long) * CHAR_BIT));
        return dyn_int{std::move(res_data), new_size};
      } else {
        auto res_data = std::array<unsigned long long, small_size>{};
        std::memcpy(res_data.data(), data(), new_size);
        res_data[new_size - 1] &=
            ~0ull >> (sizeof(unsigned long long) * CHAR_BIT -
                      exp % (sizeof(unsigned long long) * CHAR_BIT));
        return dyn_int{std::move(res_data)};
      }
    }
  }

  auto modPow2(int exp) && -> dyn_int {
    auto new_size = (exp - 1) / (sizeof(unsigned long long) * CHAR_BIT) + 1;
    if (new_size > size()) {
      return steal(*this);
    } else {
      if (exp > small_size * sizeof(unsigned long long) * CHAR_BIT - 1) {
        large._size = new_size;
        large._data[new_size - 1] &=
            ~0ull >> (sizeof(unsigned long long) * CHAR_BIT -
                      exp % (sizeof(unsigned long long) * CHAR_BIT));
        return steal(*this);
      } else {
        auto res_data = std::array<unsigned long long, small_size>{};
        std::memcpy(res_data.data(), data(), new_size);
        res_data[new_size - 1] &=
            ~0ull >> (sizeof(unsigned long long) * CHAR_BIT -
                      exp % (sizeof(unsigned long long) * CHAR_BIT));
        return dyn_int{std::move(res_data)};
      }
    }
  }

  void negate() {
    bitwise_not();
    *this += 1;
  }

  auto operator-() const & {
    auto x = *this;
    x.negate();
    return x;
  }

  auto operator-() && {
    auto x = std::move(*this);
    x.negate();
    return x;
  }

  auto abs() const & -> dyn_int {
    if (*this < 0) {
      return -*this;
    } else {
      return *this;
    }
  };

  auto abs() && -> dyn_int {
    if (*this < 0) {
      return -std::move(*this);
    } else {
      return std::move(*this);
    }
  };

  void operator+=(dyn_int const &rhs) {
    // TODO support x += x
    auto &lhs = *this;

    auto lhs_data = lhs.data(); // Save this in case the invariant breaks

    auto max_size = std::max(lhs.size(), rhs.size());

    if (lhs.capacity() < rhs.size()) {
      lhs = copy_with_capacity(lhs, rhs.size() + 1);
    }

    if (auto carry = ripple_adder(*this, rhs, *this)) {
      if (lhs.capacity() <= max_size) {
        lhs = copy_with_capacity(lhs, max_size + 1);
      }
      lhs.data()[max_size] = *carry; // TODO sign extension
      if (lhs.is_large()) {
        lhs.large._size = max_size + 1;
      }
    } else {
      if (lhs_data == small.data()) {
        was_small_fix_invariant();
      }
      if (lhs.is_large()) {
        lhs.large._size = max_size;
      }
    }
  }

  friend auto operator+(dyn_int const &lhs, dyn_int const &rhs) {
    // TODO fix always produced large result
    if (lhs.is_large() || rhs.is_large()) {
      auto max_size = std::max(lhs.size(), rhs.size());
      auto res_data = dyn_buffer<unsigned long long>{max_size + 1};
      auto carry = ripple_adder(lhs, rhs, res_data);
      auto res = dyn_int{std::move(res_data), max_size};

      if (carry) {
        res.push_msq(*carry); // TODO sign extension
      }
      return res;
    } else {
      auto res_data = std::array<unsigned long long, small_size>{};
      auto carry = ripple_adder(lhs.small, rhs.small, res_data);
      auto res = dyn_int{std::move(res_data)};
      if (carry) {
        res.push_msq(*carry); // TODO sign extension
      }
      return res;
    }
  }

  friend auto operator+(dyn_int &&lhs, dyn_int const &rhs) {
    auto tmp = std::move(lhs);
    tmp += rhs;
    return tmp;
  }

  friend auto operator+(dyn_int const &lhs, dyn_int &&rhs) {
    return std::move(rhs) + lhs;
  }

  friend auto operator+(dyn_int &&lhs, dyn_int &&rhs) {
    if (lhs.capacity() > rhs.capacity()) {
      return std::move(lhs) + rhs;
    } else {
      return lhs + std::move(rhs);
    }
  }

  void operator-=(dyn_int const &rhs) {
    // TODO support x -= x
    auto &lhs = *this;

    auto lhs_data = lhs.data(); // Save this in case the invariant breaks

    auto max_size = std::max(lhs.size(), rhs.size());

    if (lhs.capacity() < rhs.size()) {
      lhs = copy_with_capacity(lhs, rhs.size() + 1);
    }

    if (auto carry = ripple_subber(*this, rhs, *this)) {
      if (lhs.capacity() <= max_size) {
        lhs = copy_with_capacity(lhs, max_size + 1);
      }
      lhs.data()[max_size] = *carry; // TODO sign extension
      if (lhs.is_large()) {
        lhs.large._size = max_size + 1;
      }
    } else {
      if (lhs_data == small.data()) {
        was_small_fix_invariant();
      }
      if (lhs.is_large()) {
        lhs.large._size = max_size;
      }
    }
  }

  friend auto operator-(dyn_int const &lhs, dyn_int const &rhs) {
    // TODO fix always produced large result
    if (lhs.is_large() || rhs.is_large()) {
      auto max_size = std::max(lhs.size(), rhs.size());
      auto res_data = dyn_buffer<unsigned long long>{max_size + 1};
      auto carry = ripple_subber(lhs, rhs, res_data);
      auto res = dyn_int{std::move(res_data), max_size};

      if (carry) {
        res.push_msq(*carry); // TODO sign extension
      }
      return res;
    } else {
      auto res_data = std::array<unsigned long long, small_size>{};
      auto carry = ripple_subber(lhs.small, rhs.small, res_data);
      auto res = dyn_int{std::move(res_data)};
      if (carry) {
        res.push_msq(*carry); // TODO sign extension
      }
      return res;
    }
  }

  friend auto operator-(dyn_int &&lhs, dyn_int const &rhs) {
    auto tmp = std::move(lhs);
    tmp -= rhs;
    return tmp;
  }

  friend auto operator-(dyn_int const &lhs, dyn_int &&rhs) {
    auto tmp = std::move(lhs);
    tmp -= rhs;
    tmp.negate();
    return tmp;
  }

  friend auto operator-(dyn_int &&lhs, dyn_int &&rhs) {
    if (lhs.capacity() > rhs.capacity()) {
      return std::move(lhs) - rhs;
    } else {
      return lhs - std::move(rhs);
    }
  }

  friend auto operator*(dyn_int const &lhs, dyn_int const &rhs) -> dyn_int {
    if (lhs.is_large() || rhs.is_large()) {
      std::cerr << "operator* large\n";
      auto new_size = lhs.size() + rhs.size();
      std::cerr << "new size: " << new_size << '\n';
      auto res = dyn_buffer<unsigned long long>{new_size};
      std::fill(res.begin(), res.end(), 0);
      mult_and_add<stack_allocator<dyn_buffer<unsigned long long>>>(
          lhs, rhs, res, {2 * new_size});
      std::cerr << "added\n";
      return dyn_int{std::move(res), new_size};
    } else {
      std::cerr << "operator* small\n";
      auto res = std::array<unsigned long long, 2 * small_size>{};
      std::fill(res.begin(), res.end(), 0);
      mult_and_add<
          stack_allocator<std::array<unsigned long long, 3 * small_size>>>(
          lhs, rhs, res);
      std::cerr << "added\n";
      // TODO negative numbers can be small too
      if (std::find_if_not(res.begin() + small_size, res.end(), equals{0ull}) !=
          res.end()) {
        auto dy_res = dyn_buffer<unsigned long long>(2 * small_size);
        std::copy(res.begin(), res.end(), dy_res.begin());
        return dyn_int{std::move(dy_res), 2 * small_size};
      } else {
        auto small_res = std::array<unsigned long long, small_size>{};
        std::copy(res.begin(), res.begin() + small_size, small_res.begin());
        return dyn_int{small_res};
      }
    }
  }

  auto operator*=(dyn_int const &rhs) -> dyn_int & {
    *this = std::move(*this) * rhs;
    return *this;
  }

  friend auto div_mod(dyn_int lhs, dyn_int const &rhs)
      -> std::pair<dyn_int, dyn_int> {
    if (lhs.is_large() || rhs.is_large()) {
      auto size = lhs.size();
      auto quotient = dyn_int{dyn_buffer<unsigned long long>(size), size};
      divide<stack_allocator<dyn_buffer<unsigned long long>>>(
          lhs, rhs, quotient, {rhs.size() + 1});
      return {std::move(quotient), std::move(lhs)};
    } else {
      auto quotient = dyn_int{};
      divide<stack_allocator<std::array<unsigned long long, small_size + 1>>>(
          lhs, rhs, quotient);
      return {std::move(quotient), std::move(lhs)};
    }
  }

  friend auto operator/(dyn_int lhs, dyn_int const &rhs) -> dyn_int {
    return div_mod(std::move(lhs), rhs).first;
  }

  auto operator/=(dyn_int const &rhs) & -> dyn_int & {
    *this = *this / rhs;
    return *this;
  }

  friend auto operator%(dyn_int lhs, dyn_int const &rhs) -> dyn_int {
    return div_mod(std::move(lhs), rhs).second;
  }

  auto operator%=(dyn_int const &rhs) & -> dyn_int & {
    *this = *this % rhs;
    return *this;
  }

  friend auto extended_gcd(dyn_int x, dyn_int y) {
    struct extended_gcd_results {
      std::pair<dyn_int, dyn_int> bezout_coefficients;
      dyn_int gcd;
      std::pair<dyn_int, dyn_int> quotients;
    };

    auto old_s = dyn_int{1};
    auto s = dyn_int{0};
    auto old_t = dyn_int{0};
    auto t = dyn_int{1};

    while (y != 0) {
      auto quotient = x / y;

      swap(x, y);
      y -= quotient * x;

      swap(old_s, s);
      s -= quotient * old_s;

      swap(old_t, t);
      t -= quotient * old_t;
    }

    return extended_gcd_results{{std::move(old_s), std::move(old_t)},
                                std::move(x),
                                {std::move(t), std::move(s)}};
  }

  friend auto gcd(dyn_int x, dyn_int y) -> dyn_int {
    while (true) {
      if (x < y) {
        swap(x, y);
      }
      x %= y;
      if (x == 0) {
        return y;
      }
    }
  }

  friend auto lcm(dyn_int const &x, dyn_int const &y) -> dyn_int {
    auto prod = (x * y).abs();
    return std::move(prod) / gcd(x, y);
  }

  friend auto lcm(dyn_int &&x, dyn_int const &y) -> dyn_int {
    auto prod = (x * y).abs();
    return std::move(prod) / gcd(std::move(x), y);
  }

  friend auto lcm(dyn_int const &x, dyn_int &&y) -> dyn_int {
    auto prod = (x * y).abs();
    return std::move(prod) / gcd(x, std::move(y));
  }

  friend auto lcm(dyn_int &&x, dyn_int &&y) -> dyn_int {
    auto prod = (x * y).abs();
    return std::move(prod) / gcd(std::move(x), std::move(y));
  }

  template <typename> friend struct dyn_int_const_crtp;

  template <typename Allocator>
  friend void divide(dyn_int &x, dyn_int_const_view divisor, dyn_int &quotient,
                     Allocator allocator);
};

inline auto operator""_di(unsigned long long x) { return dyn_int{x}; }
inline auto operator""_di(char const *str) {
  return dyn_int{std::string_view{str}};
}
inline auto operator""_di(char const *str, std::size_t size) {
  return dyn_int{std::string_view{str, size}};
}

template <typename Allocator>
void divide(dyn_int &x, dyn_int_const_view divisor, dyn_int &quotient,
            Allocator allocator) {
  // TODO use custom allocator
  if (divisor == 0) {
#ifdef SIGFPE
    std::raise(SIGFPE);
#else

    volatile auto urgh = 0;
    urgh = 1 / urgh;
    // I hate this
    // TODO: Please fix
#endif
    std::abort();
  }

  if (divisor < 0) {
    std::cerr << "Negative divisor\n";
    // TODO
    auto divisor2 = -dyn_int{divisor};
    x.negate();
    return divide(x, divisor2, quotient, allocator);
  }

  auto const negate_result = x < 0;
  if (negate_result) {
    x.negate();
  }

  // std::cerr << "1 " << std::boolalpha << negate_result << "\n";

  divisor.trim_leading_zeros();

  for (auto quotient_index = x.size() - divisor.size();
       to_signed(quotient_index) >= 0; quotient_index -= 1) {
    // std::cerr << "2\n";
    auto &quotient_quad = quotient.view()[quotient_index];
    auto remainder_span = dyn_int_view{
        x.view().subspan(quotient_index, std::min(divisor.size() + 1,
                                                  x.size() - quotient_index))};

    // std::cerr << "3\n";
    quotient_quad =
        (remainder_span.size() < divisor.size() + 1
             ? static_cast<__uint128_t>(remainder_span[divisor.size() - 1])
             : concat(remainder_span[divisor.size() - 1],
                      remainder_span[divisor.size()])) /
        divisor.msq();
    // std::cerr << "3b " << remainder_span.size() << " "
    //           << remainder_span[remainder_span.size() - 1] << "\n";
    // std::cerr << "4 " << quotient_quad << "\n";
    auto quotient_quad_mult_divisor = dyn_buffer<unsigned long long, Allocator>{
        divisor.size() + 1, allocator};
    quotient_quad += 1; // Offset the first decrement
    do {
      // std::cerr << "5\n";
      quotient_quad -= 1;
      std::fill(quotient_quad_mult_divisor.begin(),
                quotient_quad_mult_divisor.end(), 0);
      mult_and_add(quotient_quad, divisor,
                   quotient_quad_mult_divisor); // TODO allocator
      // std::cerr << "6\n";
    } while (quotient_quad_mult_divisor > remainder_span);
    // std::cerr << "6b\n";

    if (quotient_quad != 0) {
      auto quotient_quad_mult_divisor_view =
          dyn_int_const_view{quotient_quad_mult_divisor};
      quotient_quad_mult_divisor_view.trim_leading_zeros();

      // std::cerr << "7\n";
      ripple_subber(remainder_span, quotient_quad_mult_divisor_view,
                    remainder_span);
    }
    // std::cerr << "8\n";
  }

  if (negate_result) {
    x.negate();
    quotient.negate();
  }
}

template <typename CRTP>
auto dyn_int_const_crtp<CRTP>::get_bit_range(std::size_t count,
                                             std::size_t pos) const {
  auto word_size = sizeof(unsigned long long) * CHAR_BIT;
  auto src =
      dyn_int_const_view{{view().data() + pos / word_size,
                          std::min(count / word_size + 1, view().size())}};
  if (count / word_size + 1 > dyn_int::small_size) {
    auto res_data = dyn_buffer<unsigned long long>{count / word_size + 1};
    right_arithmetic_shift(src, pos % word_size, res_data);
    // TODO zero out right hand end
    return dyn_int{std::move(res_data), count / word_size + 1};
  } else {
    auto res_data = std::array<unsigned long long, dyn_int::small_size>{};
    right_arithmetic_shift(src, pos % word_size, res_data);
    // TODO zero out right hand end
    return dyn_int{std::move(res_data)};
  }
}

template <typename CRTP> auto dyn_int_const_crtp<CRTP>::to_string(int base) {
  auto tmp = dyn_int{view()};
  auto ss = std::stringstream{};
  ss << std::hex;
  while (tmp != 0) {
    ss << (tmp % base).lsq();
    tmp /= base;
  }
  auto res = std::move(ss).str();
  std::reverse(res.begin(), res.end());
  return res;
}

#endif // MPL_HPP
