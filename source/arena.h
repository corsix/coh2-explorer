#pragma once
#include <new>
#include <stdexcept>
#include <stdint.h>

//! An arena-based memory allocator.
/*!
  All the memory allocated from an arena will be freed when said arena is destroyed.
  This results in much faster allocation than operator new, and can also be a nice
  solution to object lifetime issues. The potential downside is that some memory
  may be wasted.
*/
class Arena
{
public:
  Arena();
  ~Arena();

#ifdef ARENA_WITH_VARIADIC_TEMPLATES
  //! Allocate a single object in the arena which will be destructed with the arena.
  /*!
    \param T The type of object to allocate.
    \param args Arguments to the object's constructor.
    \throws std::bad_alloc
  */
  template <typename T, typename... Args>
  T* alloc(Args&&... args) {
    return new (static_cast<T*>(malloc(sizeof(T), &deleter<T>))) T(std::forward<Args>(args)...);
  }
#else
  template <typename T>
  T* alloc() {
    return new (static_cast<T*>(malloc(sizeof(T), &deleter<T>))) T();
  }

  template <typename T, typename A1>
  T* alloc(A1&& a1) {
    return new (static_cast<T*>(malloc(sizeof(T), &deleter<T>))) T(std::forward<A1>(a1));
  }

  template <typename T, typename A1, typename A2>
  T* alloc(A1&& a1, A2&& a2) {
    return new (static_cast<T*>(malloc(sizeof(T), &deleter<T>))) T(std::forward<A1>(a1), std::forward<A2>(a2));
  }

  template <typename T, typename A1, typename A2, typename A3>
  T* alloc(A1&& a1, A2&& a2, A3&& a3) {
    return new (static_cast<T*>(malloc(sizeof(T), &deleter<T>))) T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3));
  }

  template <typename T, typename A1, typename A2, typename A3, typename A4>
  T* alloc(A1&& a1, A2&& a2, A3&& a3, A4&& a4) {
    return new (static_cast<T*>(malloc(sizeof(T), &deleter<T>))) T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3), std::forward<A4>(a4));
  }
#endif

#ifdef ARENA_WITH_VARIADIC_TEMPLATES
  //! Allocate a single object in the arena.
  /*!
    Note that objects allocated with this method will not have their destructor called when the arena
    is destroyed. As such, it should only be used when the destructor wouldn't have any effect.

    \param T The type of object to allocate.
    \param args Arguments to the object's constructor.
    \throws std::bad_alloc
  */
  template <typename T, typename... Args>
  T* allocTrivial(Args&&... args) {
    return new (static_cast<T*>(malloc(sizeof(T)))) T(std::forward<Args>(args)...);
  }
#else
  template <typename T>
  T* allocTrivial() {
    return new (static_cast<T*>(malloc(sizeof(T)))) T;
  }

  template <typename T, typename A1>
  T* allocTrivial(A1&& a1) {
    return new (static_cast<T*>(malloc(sizeof(T)))) T(std::forward<A1>(a1));
  }

  template <typename T, typename A1, typename A2>
  T* allocTrivial(A1&& a1, A2&& a2) {
    return new (static_cast<T*>(malloc(sizeof(T)))) T(std::forward<A1>(a1), std::forward<A2>(a2));
  }

  template <typename T, typename A1, typename A2, typename A3>
  T* allocTrivial(A1&& a1, A2&& a2, A3&& a3) {
    return new (static_cast<T*>(malloc(sizeof(T)))) T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3));
  }

  template <typename T, typename A1, typename A2, typename A3, typename A4>
  T* allocTrivial(A1&& a1, A2&& a2, A3&& a3, A4&& a4) {
    return new (static_cast<T*>(malloc(sizeof(T)))) T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3), std::forward<A4>(a4));
  }
#endif

  //! Allocate a block of memory in the arena.
  /*!
    \param size The number of bytes to allocate.
    \throws std::bad_alloc
  */
  void* malloc(size_t size);

  template <typename T>
  T* mallocArray(size_t extent) {
    return static_cast<T*>(malloc(sizeof(T) * extent));
  }

private:
  static void align(size_t& size);
  void* malloc(size_t size, bool(*deleter)(char*&));
  void ensureSpace(size_t full_size);

  template <typename T>
  static bool deleter(char*& mem)
  {
    reinterpret_cast<T*>(mem)->~T();
    mem += sizeof(T);
    return true;
  }

  struct Block
  {
    Block* prev;
    Block* dummy_for_alignment;
  } *m_cur_block;
  char *m_bump, *m_end;

#ifdef _DEBUG
  size_t m_amount_from_os;
  size_t m_amount_trivial;
  size_t m_amount_nontrivial;
  size_t m_num_trivial;
  size_t m_num_nontrivial;
#endif
};

inline void runtime_assert(bool condition, const char* msg)
{
#ifndef ASSUME_ASSERTIONS_ARE_TRUE
  if(!condition)
    throw std::runtime_error(msg);
#endif
}

//! A lightweight substitute to std::vector which uses an Arena for allocation.
template <typename T>
class ArenaArray
{
public:
  ArenaArray(Arena* arena, uint32_t size)
    : m_size(size)
    , m_elements(arena->mallocArray<T>(size))
  {
  }

  ArenaArray& recreate(Arena* arena, uint32_t new_size)
  {
    m_elements = arena->mallocArray<T>(new_size);
    m_size = new_size;
    return *this;
  }

  uint32_t size()  const { return m_size; }
        T* data()        { return m_elements; }
  const T* data()  const { return m_elements; }
        T* begin()       { return m_elements; }
  const T* begin() const { return m_elements; }
        T* end()         { return m_elements + m_size; }
  const T* end()   const { return m_elements + m_size; }

  T& operator[] (uint32_t index) { return m_elements[index]; }

  T& at(uint32_t index)
  {
    runtime_assert(index < m_size, "Array index out of bounds.");
    return m_elements[index];
  }

  template <typename F>
  void for_each(F&& functor)
  {
    for(auto itr = begin(), e = end(); itr != e; ++itr)
      functor(*itr);
  }

  template <typename F>
  void for_each(F&& functor) const
  {
    for(auto itr = begin(), e = end(); itr != e; ++itr)
      functor(*itr);
  }

private:
  uint32_t m_size;
  T* m_elements;
};
