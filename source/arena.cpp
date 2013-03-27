#include "stdafx.h"
#include "arena.h"

Arena::Arena()
  : m_cur_block(nullptr)
  , m_bump(nullptr)
  , m_end(nullptr)
#ifdef _DEBUG
  , m_amount_from_os(0)
  , m_amount_trivial(0)
  , m_amount_nontrivial(0)
  , m_num_trivial(0)
  , m_num_nontrivial(0)
#endif
{
}

Arena::~Arena()
{
  auto block = m_cur_block;
  while(block)
  {
    auto mem = reinterpret_cast<char*>(block + 1);
    for(;;)
    {
      auto deleter = reinterpret_cast<bool(**)(char*&)>(mem);
      mem = reinterpret_cast<char*>(deleter + 1);
      if(!(*deleter)(mem))
        break;
    }
    auto prev = block->prev;
    VirtualFree(block, 0, MEM_RELEASE);
    block = prev;
  }
}

static bool end_of_objects(char*&)
{
  return false;
}

void Arena::ensureSpace(size_t full_size)
{
  if(m_bump + full_size > m_end)
  {
    full_size += sizeof(Block);
    size_t block_size = 0x8000;
    while(block_size < full_size)
      block_size <<= 1;
    block_size <<= 1;

    auto new_block = static_cast<Block*>(VirtualAlloc(nullptr, block_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if(!new_block)
      throw std::bad_alloc();
#ifdef _DEBUG
    m_amount_from_os += block_size;
#endif
    new_block->prev = m_cur_block;
    m_cur_block = new_block;
    m_bump = reinterpret_cast<char*>(new_block + 1);
    m_end = reinterpret_cast<char*>(new_block) + block_size;
  }
}

void Arena::align(size_t& size)
{
  size = (size + MEMORY_ALLOCATION_ALIGNMENT - 1) &~ static_cast<size_t>(MEMORY_ALLOCATION_ALIGNMENT - 1);
}

void* Arena::malloc(size_t size)
{
  align(size);
  ensureSpace(size + sizeof(bool(*)(char*&)));

#ifdef _DEBUG
  m_num_trivial += 1;
  m_amount_trivial += size;
#endif

  m_end -= size;
  *reinterpret_cast<bool(**)(char*&)>(m_bump) = end_of_objects;
  return static_cast<void*>(m_end);
}

void* Arena::malloc(size_t size, bool(*deleter)(char*&))
{
  ensureSpace(size + sizeof(deleter) * 2);

#ifdef _DEBUG
  m_num_nontrivial += 1;
  m_amount_nontrivial += size;
#endif

  *reinterpret_cast<bool(**)(char*&)>(m_bump) = deleter;
  m_bump += sizeof(deleter);
  auto result = m_bump;
  m_bump += size;
  *reinterpret_cast<bool(**)(char*&)>(m_bump) = end_of_objects;
  return static_cast<void*>(result);
}
