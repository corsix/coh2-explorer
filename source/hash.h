#pragma once
#include <stdint.h>

namespace Essence
{
  uint32_t Hash(const uint8_t* data, uint32_t data_len, uint32_t initial_value = 0);
  uint32_t Hash(const char* data, uint32_t data_len, uint32_t initial_value = 0);

  struct Hashable
  {
  public:
    inline Hashable(uint32_t hash)
      : m_hash(hash)
    {
    }

    template <uint32_t N>
    inline Hashable(const char (&str)[N])
      : m_hash(Hash(str, N - 1))
    {
    }

    template <typename T>
    inline Hashable(const T& container)
      : m_hash(Hash(reinterpret_cast<const uint8_t*>(container.data()), static_cast<uint32_t>(container.size())))
    {
      static_assert(sizeof(*container.data()) == 1, "Implicit hashing should only be done for strings.");
    }

    uint32_t getHash() const { return m_hash; }

  private:
    const uint32_t m_hash;
  };
}
