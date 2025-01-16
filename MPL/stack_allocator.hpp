#ifndef STACK_ALLOCATOR_HPP
#define STACK_ALLOCATOR_HPP

#include <cassert>
#include <memory>
#include <utility>

namespace mpl {
template <typename T, std::size_t size> class stack_allocator_ref;

template <std::size_t size> class stack_allocator {
private:
  struct header_t {
    header_t *prev;
    header_t *next;
  };

  alignas(std::max_align_t) std::array<std::byte, size> buffer;
  std::size_t head = 0;
  header_t *last_header = nullptr;

  auto currect_header() -> header_t * {
    return reinterpret_cast<header_t *>(&buffer[head]);
  }

public:
  stack_allocator() = default;

  ~stack_allocator() = default;

  stack_allocator(stack_allocator const &) = delete;
  stack_allocator &operator=(stack_allocator const &) = delete;
  stack_allocator(stack_allocator &&) = delete;
  stack_allocator &operator=(stack_allocator &&) = delete;

  template <typename T> auto ref() -> stack_allocator_ref<T, size> {
    return *this;
  }

  auto allocate(std::size_t n) -> void * {
    auto header = currect_header();
    head += sizeof(header_t);

    auto p = &buffer[head];
    head += n;

    head += alignof(std::max_align_t) -
            ((head - 1) % alignof(std::max_align_t) + 1);

    assert(head <= size);
    assert(reinterpret_cast<std::uintptr_t>(p) % alignof(std::max_align_t) ==
           0);

    header->prev = last_header;
    header->next = currect_header();
    last_header = header;

    return p;
  }

  void deallocate(void *p, std::size_t n) {
    if (p) {
      auto header = reinterpret_cast<header_t *>(static_cast<std::byte *>(p) -
                                                 sizeof(header_t));

      if (header == last_header) {
        head = reinterpret_cast<std::byte *>(header) - buffer.data();
        last_header = header->prev;
      } else {
        if (header->prev) {
          header->prev->next = header->next;
        }
        assert(header->next != currect_header());
        header->next->prev = header->prev;
      }
    }
  }
};

template <typename T, std::size_t size> class stack_allocator_ref {
private:
  stack_allocator<size> &buffer;

  stack_allocator_ref(stack_allocator<size> &buffer) : buffer{buffer} {}

public:
  using pointer = T *;
  using const_pointer = T const *;
  using void_pointer = void *;
  using const_void_pointer = void const *;
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  template <typename U> struct rebind {
    using other = stack_allocator_ref<U, size>;
  };

  auto allocate(std::size_t n) -> T * {
    return static_cast<T *>(buffer.allocate(n * sizeof(T)));
  }

  void deallocate(T *p, std::size_t n) {
    return buffer.deallocate(p, n * sizeof(T));
  }

  template <std::size_t> friend class stack_allocator;
};
} // namespace mpl

#endif // STACK_ALLOCATOR_HPP
