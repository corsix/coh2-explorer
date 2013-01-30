#pragma once
#include <stdint.h>
#include <string>

namespace Essence
{
  uint32_t Hash(const uint8_t* data, uint32_t data_len, uint32_t initial_value = 0);
  uint32_t Hash(const char* data, uint32_t data_len, uint32_t initial_value = 0);
  uint32_t Hash(const std::string& str, uint32_t initial_value = 0);
}
