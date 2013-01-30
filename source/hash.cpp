// The algorithm in this file is public domain.
// See http://burtleburtle.net/bob/c/lookup2.c for more details.
#include "hash.h"

namespace Essence
{
  static inline void mix(uint32_t& a, uint32_t& b, uint32_t& c)
  {
    a -= b; a -= c; a ^= (c>>13);
    b -= c; b -= a; b ^= (a<<8);
    c -= a; c -= b; c ^= (b>>13);
    a -= b; a -= c; a ^= (c>>12);
    b -= c; b -= a; b ^= (a<<16);
    c -= a; c -= b; c ^= (b>>5);
    a -= b; a -= c; a ^= (c>>3);
    b -= c; b -= a; b ^= (a<<10);
    c -= a; c -= b; c ^= (b>>15);
  }

  uint32_t Hash(const uint8_t* k, uint32_t data_len, uint32_t c)
  {
     uint32_t a,b,len;

     /* Set up the internal state */
     len = data_len;
     a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */

     /*---------------------------------------- handle most of the key */
     while (len >= 12)
     {
        a += reinterpret_cast<const uint32_t*>(k)[0];
        b += reinterpret_cast<const uint32_t*>(k)[1];
        c += reinterpret_cast<const uint32_t*>(k)[2];
        mix(a,b,c);
        k += 12; len -= 12;
     }

     /*------------------------------------- handle the last 11 bytes */
     c += data_len;
     switch(len)              /* all the case statements fall through */
     {
     case 11: c+=((uint32_t)k[10]<<24);
     case 10: c+=((uint32_t)k[9]<<16);
     case 9 : c+=((uint32_t)k[8]<<8);
        /* the first byte of c is reserved for the length */
     case 8 : b+=((uint32_t)k[7]<<24);
     case 7 : b+=((uint32_t)k[6]<<16);
     case 6 : b+=((uint32_t)k[5]<<8);
     case 5 : b+=k[4];
     case 4 : a+=((uint32_t)k[3]<<24);
     case 3 : a+=((uint32_t)k[2]<<16);
     case 2 : a+=((uint32_t)k[1]<<8);
     case 1 : a+=k[0];
       /* case 0: nothing left to add */
     }
     mix(a,b,c);
     /*-------------------------------------------- report the result */
     return c;
  }

  uint32_t Hash(const char* data, uint32_t data_len, uint32_t initial_value)
  {
    return Hash(reinterpret_cast<const uint8_t*>(data), data_len, initial_value);
  }

  uint32_t Hash(const std::string& str, uint32_t initial_value)
  {
    return Hash(str.c_str(), static_cast<uint32_t>(str.size()), initial_value);
  }
}
