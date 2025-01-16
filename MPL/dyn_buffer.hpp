#ifndef MPL_DYN_BUFFER_HPP
#define MPL_DYN_BUFFER_HPP

#include <memory>
#include <span>

// It is important that dyn_buffer be relocatable

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

  auto cbegin() const { return begin(); }
  auto rbegin() { return std::reverse_iterator{end()}; }
  auto rbegin() const { return std::reverse_iterator{end()}; }
  auto crbegin() const { return rbegin(); }

  auto cend() const { return end(); }
  auto rend() { return std::reverse_iterator{begin()}; }
  auto rend() const { return std::reverse_iterator{begin()}; }
  auto crend() const { return rend(); }

  auto operator[](std::size_t i) -> T & { return _data[i]; }
  auto operator[](std::size_t i) const -> T const & { return _data[i]; }

  auto front() { return (*this)[0]; }
  auto front() const { return (*this)[0]; }
  auto back() { return (*this)[_size - 1]; }
  auto back() const { return (*this)[_size - 1]; }

  operator std::span<T>() { return {data(), size()}; }
  operator std::span<T const>() const { return {data(), size()}; }

  explicit dyn_buffer(std::nullptr_t, Allocator allocator = {})
      : Allocator{std::move(allocator)}, _data{nullptr}, _size{0} {}

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

  dyn_buffer(dyn_buffer const &that)
      : dyn_buffer{that.size(), static_cast<Allocator const &>(that)} {
    std::copy(that.begin(), that.end(), this->begin());
  }

  auto operator=(dyn_buffer const &that) -> dyn_buffer & {
    if (this->size() >= that.size()) {
      if (this != &that) {
        std::copy(that.begin(), that.end(), this->begin());
      }
    } else {
      auto tmp = that;
      swap(*this, tmp);
    }
    return *this;
  }

  dyn_buffer(dyn_buffer &&that)
      : Allocator{static_cast<Allocator &&>(that)},
        _data{std::exchange(that._data, {})},
        _size{std::exchange(that._size, {})} {}

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

  friend auto operator==(dyn_buffer const &lhs, std::nullptr_t) -> bool {
    return lhs._data == nullptr;
  }
};

#endif // MPL_DYN_BUFFER_HPP
