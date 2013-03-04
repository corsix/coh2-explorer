#pragma once
#include <stdint.h>

namespace Essence
{
  class ChunkyWriter
  {
  public:
    ChunkyWriter(const wchar_t* filename);
    ~ChunkyWriter();

    void beginChunk(const char (&kind_type)[9], uint32_t version, const char* name = nullptr);
    void payload(const uint8_t* data, uint32_t length);
    uint32_t tell();
    void seekTo(uint32_t position);
    void seekToEnd();
    void endChunk();

    template <typename T, size_t N>
    void payload(const T (&data)[N])
    {
      payload(reinterpret_cast<const uint8_t*>(data), static_cast<uint32_t>(sizeof(T) * N));
    }

    template <uint32_t N>
    void string(const char (&literal)[N])
    {
      auto len = N - 1;
      payload(reinterpret_cast<const uint8_t*>(&len), sizeof(len));
      payload(reinterpret_cast<const uint8_t*>(literal), len);
    }

  private:
    void* m_file;
    uint32_t m_innermost_chunk_length_field_offset;
  };
}
