#pragma once

namespace Essence { namespace Graphics
{
#pragma pack(push)
#pragma pack(1)

  struct vector3_t
  {
    float x, y, z;
  };

  typedef float matrix33_t[3][3];

  struct bounding_volume_t
  {
    vector3_t center;
    vector3_t size;
    matrix33_t transform;
  };

#pragma pack(pop)
}}
