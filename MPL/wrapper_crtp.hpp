#ifndef MPL_WRAPPER_CRTP_HPP
#define MPL_WRAPPER_CRTP_HPP

#include "algorithms.hpp"
#include "utility.hpp"

namespace mpl {
template <typename Derived> class wrapper_crtp {
public:
  constexpr auto self() & -> Derived & { return static_cast<Derived &>(*this); }
  constexpr auto self() && -> Derived && {
    return static_cast<Derived &&>(*this);
  }
  constexpr auto self() const & -> Derived const & {
    return static_cast<Derived const &>(*this);
  }
  constexpr auto self() const && -> Derived const && {
    return static_cast<Derived const &&>(*this);
  }

  auto shrunk() const {
    return mpl::algorithms::impl::trim_leading_sign_bits(self().container);
  }

  auto shrunk() {
    return mpl::algorithms::impl::trim_leading_sign_bits(self().container);
  }

  auto get_ll() const -> std::optional<long long> {
    if (self().container.size() == 0) {
      return 0;
    } else {
      auto x = static_cast<long long>(self().container[0]);
      if (std::all_of(std::next(self().container.begin()),
                      self().container.end(), [&](ull y) {
                        return y ==
                               static_cast<ull>(
                                   x >> (sizeof(long long) * CHAR_BIT - 1));
                      })) {
        return x;
      } else {
        return std::nullopt;
      }
    }
  }

  auto get_ull() const -> std::optional<unsigned long long> {
    if (self().container.size() == 0) {
      return 0;
    } else {
      if (std::all_of(std::next(self().container.begin()),
                      self().container.end(), [](ull x) { return x == 0; })) {
        return self().container[0];
      } else {
        return std::nullopt;
      }
    }
  }

  auto to_string(int base = 10) const & -> std::string {
    return auto{self()}.to_string(base);
  }

  auto to_string(int base = 10) && -> std::string {
    return mpl::algorithms::to_string(std::move(self().container), base);
  }

  friend auto operator<<(std::ostream &os, wrapper_crtp const &x)
      -> std::ostream & {
    return os << x.to_string();
  }

  friend auto operator<<(std::ostream &os, wrapper_crtp &&x) -> std::ostream & {
    return os << std::move(x).to_string();
  }

  template <std::integral T> auto get() const -> std::optional<T> {
    if constexpr (std::is_signed_v<T>) {
      if (auto x = get_ll()) {
        if (static_cast<T>(*x) == x) {
          return x;
        }
      }
      return std::nullopt;
    } else {
      if (auto x = get_ull()) {
        if (static_cast<T>(*x) == x) {
          return x;
        }
      }
      return std::nullopt;
    }
  }

  template <std::floating_point T = double> auto to_float() const -> T {
    auto shrunk_ = shrunk();
    switch (shrunk_.size()) {
    case 0:
      return 0;
    case 1:
      return static_cast<long long>(shrunk_[0]);
    default:
      if (static_cast<long long>(shrunk_[shrunk_.size() - 1]) < 0) {
        return -(~shrunk_[shrunk_.size() - 1] *
                     std::exp2((shrunk_.size() - 1) * ull_bits) +
                 ~shrunk_[shrunk_.size() - 2] *
                     std::exp2((shrunk_.size() - 2) * ull_bits));
      } else {
        return shrunk_[shrunk_.size() - 1] *
                   std::exp2((shrunk_.size() - 1) * ull_bits) +
               shrunk_[shrunk_.size() - 2] *
                   std::exp2((shrunk_.size() - 2) * ull_bits);
      }
    }
  }

  auto is_zero() const { return algorithms::is_zero(self().container); }
  auto is_negative() const { return algorithms::is_negative(self().container); }
  auto signum() const { return algorithms::signum(self().container); }

  auto is_pow_2() const -> int {
    // If self() == 2^k-1 return k, else return 0

    auto k = 0;
    for (auto it = self().container.rbegin(); it != self().container.rend();
         ++it) {
      if (*it == 0) {
        if (k != 0) {
          k += ull_bits;
        }
      } else if (__builtin_popcountll(*it) == 1) {
        if (k != 0) {
          return 0;
        } else {
          k = __builtin_ffsll(*it);
        }
      } else {
        return 0;
      }
    }
    return k;
  }

  auto length() const -> std::size_t {
    if (self().signum() < 0) {
      // TODO
      return self().abs().length();
    } else {
      for (auto i = static_cast<int>(self().container.size()) - 1; i >= 0;
           i -= 1) {
        if (self().container[i] != 0) {
          auto clz = __builtin_clzll(self().container[i]);
          if (clz < static_cast<int>(ull_bits)) {
            return (i + 1) * ull_bits - clz;
          }
        }
      }
      return 1;
    }
  }

  auto bit_is_set(uint32_t i) const -> bool {
    if (i < self().container.size() * ull_bits) {
      return (self().container[i / ull_bits] >> (i % ull_bits)) & 1;
    } else {
      return self().container.back() >> (ull_bits - 1);
    }
  }

  auto operator-() && {
    self().negate();
    return std::move(self());
  }

  auto operator-() const & { return -auto{self()}; }

  auto abs() const & {
    if (self().signum() < 0) {
      return -self();
    } else {
      return self();
    }
  }

  auto abs() && {
    if (self().signum() < 0) {
      return -std::move(self());
    } else {
      return std::move(self());
    }
  }

  friend auto operator==(wrapper_crtp const &lhs, wrapper_crtp const &rhs)
      -> bool {
    return algorithms::is_eq(
        algorithms::compare(lhs.self().container, rhs.self().container));
  }

  friend auto operator==(wrapper_crtp const &lhs, long long const rhs) -> bool {
    return algorithms::is_eq(algorithms::compare(
        lhs.self().container, std::array<ull, 1>{static_cast<ull>(rhs)}));
  }

  friend auto operator<=>(wrapper_crtp const &lhs, wrapper_crtp const &rhs) {
    return algorithms::compare(lhs.self().container, rhs.self().container);
  }

  friend auto operator<=>(wrapper_crtp const &lhs, long long const rhs) {
    return algorithms::compare(lhs.self().container,
                               std::array<ull, 1>{static_cast<ull>(rhs)});
  }
};
} // namespace mpl

#endif // MPL_WRAPPER_CRTP_HPP
