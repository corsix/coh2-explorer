#include "stdafx.h"
#include "presized_arena.h"
#include "arena.h"

PresizedArena::PresizedArena(size_t allocation_limit)
  : m_base(static_cast<char*>(VirtualAlloc(nullptr, allocation_limit, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)))
  , m_bump(m_base)
#ifndef ASSUME_ASSERTIONS_ARE_TRUE
  , m_end(m_base + allocation_limit)
#endif
{
}

PresizedArena::~PresizedArena()
{
  VirtualFree(m_base, 0, MEM_RELEASE);
}

void* PresizedArena::malloc(size_t size)
{
#ifndef ASSUME_ASSERTIONS_ARE_TRUE
  runtime_assert(static_cast<size_t>(m_end - m_bump) >= size, "Pre-sized arena is too small.");
#endif

  auto result = static_cast<void*>(m_bump);
  m_bump += size;
  return result;
}
