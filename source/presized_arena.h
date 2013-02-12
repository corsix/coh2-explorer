#pragma once
#include <new>

//! An arena-based memory allocator with a fixed upper bound on the amount of allocated memory.
/*!
  All the memory allocated from an arena will be freed when said arena is destroyed.
  This results in much faster allocation than operator new, and can also be a nice
  solution to object lifetime issues. The potential downside is that some memory
  may be wasted.
*/
class PresizedArena
{
public:
  PresizedArena(size_t allocation_limit);
  ~PresizedArena();

  //! Allocate a single object in the arena.
  /*!
    Note that objects allocated with this method will not have their destructor called when the arena
    is destroyed. As such, it should only be used when the destructor wouldn't have any effect.

    \param T The type of object to allocate.
    \param args Arguments to the object's constructor.
    \throws std::bad_alloc
  */
#define METHOD allocTrivial
#define MALLOC malloc(sizeof(T))
#include "arena_var_tem.h"

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
  char* m_base;
  char* m_bump;
#ifndef ASSUME_ASSERTIONS_ARE_TRUE
  char* m_end;
#endif
};
