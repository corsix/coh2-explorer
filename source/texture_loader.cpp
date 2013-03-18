#include "texture_loader.h"
#include "fs.h"
#include "directx.h"
#include "chunky.h"
#include "arena.h"
#include "presized_arena.h"
#include <unordered_map>
#include <zlib.h>
using namespace std;
using namespace C6::D3;
using namespace Essence;

namespace
{
#pragma pack(push)
#pragma pack(1)
  struct tga_header_t
  {
    uint8_t id_length;
    uint8_t colour_map_type;
    uint8_t image_type;
    uint16_t palette_origin;
    uint16_t palette_size;
    uint8_t palette_bpp;
    uint16_t x_origin;
    uint16_t y_origin;
    uint16_t width;
    uint16_t height;
    uint8_t depth;
    uint8_t flags;
  };

  struct dxtc_tfmt_t
  {
    uint32_t width;
    uint32_t height;
    uint32_t unknown[2];
    uint32_t compression;
  };

  struct dxtc_tman_t
  {
    uint32_t mip_count;
    struct
    {
      uint32_t data_length;
      uint32_t data_length_compressed;
    } mips[1];
  };

  struct dxtc_tdat_entry_header_t
  {
    uint32_t mip_level;
    uint32_t width;
    uint32_t height;
    uint32_t num_physical_texels;
  };
#pragma pack(pop)

  struct DefaultTextureDesc : D3D10_TEXTURE2D_DESC
  {
    DefaultTextureDesc()
    {
      MipLevels = 1;
      ArraySize = 1;
      SampleDesc.Count = 1;
      SampleDesc.Quality = 0;
      Usage = D3D10_USAGE_IMMUTABLE;
      BindFlags = D3D10_BIND_SHADER_RESOURCE;
      CPUAccessFlags = 0;
      MiscFlags = 0;
    }
  };

  Texture2D CreateConstantTexture(Device1& d3, float r, float g, float b, float a)
  {
    DefaultTextureDesc desc;
    desc.Width = 1;
    desc.Height = 1;
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    float texel[] = {r, g, b, a};
    D3D10_SUBRESOURCE_DATA contents;
    contents.pSysMem = static_cast<void*>(texel);
    contents.SysMemPitch = sizeof(texel);
    contents.SysMemSlicePitch = 0;

    return d3.createTexture2D(desc, contents);
  }

  class Inflater
  {
  public:
    Inflater()
    {
      m_z.zalloc = nullptr;
      m_z.zfree = nullptr;
      m_z.opaque = nullptr;
      z_assert(inflateInit(&m_z), Z_OK);
    }

    void uncompress(uint8_t* dest, uint32_t dest_len, const uint8_t* src, uint32_t src_len)
    {
      m_z.next_out = dest;
      m_z.avail_out = dest_len;
      m_z.next_in = const_cast<Bytef*>(src);
      m_z.avail_in = src_len;
      z_assert(inflate(&m_z, Z_FINISH), Z_STREAM_END);
      runtime_assert(m_z.avail_out == 0 && m_z.avail_in == 0, "Compressed data size mismatch.");
      z_assert(inflateReset(&m_z), Z_OK);
    }

    ~Inflater()
    {
      inflateEnd(&m_z);
    }

  private:
    void z_assert(int result, int expected)
    {
      if(result != expected)
        throw std::runtime_error(m_z.msg);
    }

    z_stream m_z;
  };

  Texture2D LoadChunkyDXTC(Device1& d3, const Chunk* dxtc)
  {
    auto tfmt_chunk = dxtc->findFirst("DATATFMT");
    runtime_assert(tfmt_chunk != nullptr, "FOLDDXTC missing DATATFMT.");
    runtime_assert(tfmt_chunk->getSize() >= sizeof(dxtc_tfmt_t), "DATATFMT is too small.");
    auto tfmt = reinterpret_cast<const dxtc_tfmt_t*>(tfmt_chunk->getContents());

    DXGI_FORMAT format;
    uint32_t ratio = 4;
    switch(tfmt->compression)
    {
    case 13: format = DXGI_FORMAT_BC1_UNORM_SRGB; break;
    case 14: format = DXGI_FORMAT_BC2_UNORM_SRGB; break;
    case 15: format = DXGI_FORMAT_BC3_UNORM_SRGB; break;
    case 22: format = DXGI_FORMAT_BC1_UNORM     ; break;
    case 23: format = DXGI_FORMAT_BC2_UNORM     ; break;
    case 24: format = DXGI_FORMAT_BC3_UNORM     ; break;
    default:
      if(tfmt->compression >> 16 == 0xC600)
      {
        // CoH2 doesn't accept these values, but it is useful for us to accept them.
        format = static_cast<DXGI_FORMAT>(tfmt->compression & 0xFF);
        ratio = (tfmt->compression >> 8) & 0xFF;
      }
      else
        throw runtime_error("FOLDDXTC uses unknown texture format.");
    }
    /*
      The following is CoH2's texture type enumeration, which bears an
      uncanny resemblance to the values in the above switch statement:
       0 - RG
       1 - RGB
       2 - RGBA
       3 - RGBAmask
       4 - L
       5 - LA
       6 - A
       7 - UV
       8 - UVWQ
       9 - Rf
      10 - RGf
      11 - RGBAf
      12 - depth
      13 - DXT1 (BC1)
      14 - DXT3 (BC2)
      15 - DXT5 (BC3)
      16 - DXT7
      17 - SHADOWMAP
      18 - NULL
      19 - DepthStencil
      20 - RGB_sRGB
      21 - RGBA_sRGB
      22 - DXT1_sRGB (BC1)
      23 - DXT3_sRGB (BC2)
      24 - DXT5_sRGB (BC3)
      25 - DXT7_sRGB
      26 - Invalid
    */

    auto tman_chunk = dxtc->findFirst("DATATMAN");
    runtime_assert(tman_chunk != nullptr, "FOLDDXTC missing DATATMAN.");
    runtime_assert(tman_chunk->getSize() >= 4, "DATATMAN is too small.");
    auto tman = reinterpret_cast<const dxtc_tman_t*>(tman_chunk->getContents());
    runtime_assert(tman->mip_count != 0, "DATATMAN contains no mip levels.");
    runtime_assert(tman_chunk->getSize() >= 4 + tman->mip_count * 8, "DATATMAN is incomplete.");
    auto tdat_chunk = dxtc->findFirst("DATATDAT");
    runtime_assert(tdat_chunk != nullptr, "FOLDDXTC missing DATATDAT.");
    ChunkReader tdat_reader(tdat_chunk);

    size_t img_data_capacity = sizeof(D3D10_SUBRESOURCE_DATA) * tman->mip_count;
    for(uint32_t level = 0; level < tman->mip_count; ++level)
      img_data_capacity += tman->mips[level].data_length;
    PresizedArena img_data(img_data_capacity);

    uint32_t level = 0;
    auto resources = img_data.mallocArray<D3D10_SUBRESOURCE_DATA>(tman->mip_count);
    for(Inflater inflater; level < tman->mip_count; ++level)
    {
      auto& mip = tman->mips[level];
      auto src = tdat_reader.reinterpret<uint8_t>(mip.data_length_compressed);
      if(mip.data_length != mip.data_length_compressed)
      {
        auto uncompressed = img_data.mallocArray<uint8_t>(mip.data_length);
        inflater.uncompress(uncompressed, mip.data_length, src, mip.data_length_compressed);
        src = uncompressed;
      }

      auto tdat_header = reinterpret_cast<const dxtc_tdat_entry_header_t*>(src);
      runtime_assert(tdat_header->mip_level < tman->mip_count, "Unexpected mip level.");
      runtime_assert(tdat_header->width  == (std::max)(tfmt->width  >> tdat_header->mip_level, 1U), "Invalid mip level width.");
      runtime_assert(tdat_header->height == (std::max)(tfmt->height >> tdat_header->mip_level, 1U), "Invalid mip level height.");

      auto& resource = resources[tdat_header->mip_level];
      resource.pSysMem = tdat_header + 1;
      resource.SysMemPitch = tdat_header->num_physical_texels / tdat_header->height * ratio;
    }

    DefaultTextureDesc desc;
    desc.Width = tfmt->width;
    desc.Height = tfmt->height;
    desc.MipLevels = level;
    desc.Format = format;

    return d3.createTexture2D(desc, resources);
  }

  Texture2D LoadChunky(Device1& d3, unique_ptr<MappableFile> file)
  {
    auto chunky = ChunkyFile::Open(move(file));
    auto txtr = chunky->findFirst("FOLDTSET")->findFirst("FOLDTXTR");
    runtime_assert(txtr != nullptr, "Could not find FOLDTXTR in chunky file.");

    if(auto dxtc = txtr->findFirst("FOLDDXTCv3")) return LoadChunkyDXTC(d3, dxtc);
    throw runtime_error("Unknown texture type in FOLDTXTR.");
  }

  Texture2D LoadDDS(Device1& d3, unique_ptr<MappableFile> file)
  {
    throw runtime_error("TODO: Implement DDS loading.");
  }

  class CpuId
  {
  public:
    CpuId()
    {
      int info[4] = {0};
      __cpuid(info, 1);
      has_ssse3 = (info[2] & 0x100) != 0;
    }

    bool has_ssse3;
  } const cpuid;

  void Convert24To32_SSSE3(__m128i* dst, const __m128i* src, size_t num_iterations)
  {
    const auto alpha = _mm_set1_epi32(0xFF000000UL);
    const auto shuffle = _mm_set_epi8(-1, 15, 14, 13, -1, 12, 11, 10, -1, 9, 8, 7, -1, 6, 5, 4);

    do
    {
      const auto src0 = _mm_load_si128(src);
      const auto src1 = _mm_load_si128(src + 1);
      const auto src2 = _mm_load_si128(src + 2);
      src += 3;

      _mm_store_si128(dst, _mm_or_si128(_mm_shuffle_epi8(_mm_slli_si128(src0, 4), shuffle), alpha));
      _mm_store_si128(dst + 1, _mm_or_si128(_mm_shuffle_epi8(_mm_alignr_epi8(src1, src0, 8), shuffle), alpha));
      _mm_store_si128(dst + 2, _mm_or_si128(_mm_shuffle_epi8(_mm_alignr_epi8(src2, src1, 4), shuffle), alpha));
      _mm_store_si128(dst + 3, _mm_or_si128(_mm_shuffle_epi8(src2, shuffle), alpha));
      dst += 4;
    } while(--num_iterations);
  }

  template <typename T>
  bool is_aligned(T* ptr)
  {
    return (reinterpret_cast<uintptr_t>(ptr) & 0xF) == 0;
  }

  uint8_t* Convert24To32(uint8_t* dst, const uint8_t* src, size_t num_bytes)
  {
    // Increment dst by at most 15, so that dst's and src's 16-byte-alignments will be in phase.
    auto phase_delta = ((reinterpret_cast<uintptr_t>(src) & 3) * 4) & 15;
    dst = reinterpret_cast<uint8_t*>(((reinterpret_cast<uintptr_t>(dst) + phase_delta + 15) & ~static_cast<uintptr_t>(15)) - phase_delta);

    auto dst_base = dst;
    while(num_bytes >= 4)
    {
      *reinterpret_cast<uint32_t*>(dst) = 0xFF000000UL | *reinterpret_cast<const uint32_t*>(src);
      src += 3;
      dst += 4;
      num_bytes -= 3;

      if(cpuid.has_ssse3 && is_aligned(src) && num_bytes >= 48)
      {
        auto num_iterations = num_bytes / 48;
        Convert24To32_SSSE3(reinterpret_cast<__m128i*>(dst), reinterpret_cast<const __m128i*>(src), num_iterations);
        src += num_iterations * 48;
        dst += num_iterations * 64;
        num_bytes -= num_iterations * 48;
      }
    }
    if(num_bytes == 3)
    {
      dst[0] = src[0];
      dst[1] = src[1];
      dst[2] = src[2];
      dst[3] = 0xFF;
    }

    return dst_base;
  }

  void DecompressRLE(uint8_t* dst, const uint8_t* src, size_t pixel_size, size_t num_pixels)
  {
    while(num_pixels)
    {
      auto head = *src++;
      auto count = static_cast<size_t>(1 + (head & 0x7F));
      if(count > num_pixels)
        throw runtime_error("Invalid RLE data.");
      num_pixels -= count;

      if(static_cast<int8_t>(head) < 0)
      {
        if(pixel_size == 4)
        {
          __stosd(reinterpret_cast<unsigned long*>(dst), *reinterpret_cast<const unsigned long*>(src), count);
          dst += 4 * count;
        }
        else
        {
          do
          {
            __movsb(dst, src, pixel_size);
            dst += pixel_size;
          } while(--count);
        }
        src += pixel_size;
      }
      else
      {
        count *= pixel_size;
        memcpy(dst, src, count);
        src += count;
        dst += count;
      }
    }
  }

  Texture2D LoadTGA(Device1& d3, unique_ptr<MappableFile> file)
  {
    runtime_assert(file->getSize() >= sizeof(tga_header_t), "TGA file is too small.");
    auto mapped = file->mapAll();
    auto& header = *reinterpret_cast<const tga_header_t*>(mapped.begin);
    if((header.image_type & 0xF7) != 2)
      throw runtime_error("Only truecolour TGA files are supported.");

    size_t predata_size = sizeof(tga_header_t) + header.id_length;
    if(header.colour_map_type)
      predata_size += header.palette_size * (header.palette_bpp / 8);

    DefaultTextureDesc desc;
    desc.Width = header.width;
    desc.Height = header.height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

    D3D10_SUBRESOURCE_DATA contents;
    contents.pSysMem = mapped.begin + predata_size;
    contents.SysMemPitch = header.width * 4;

    size_t data_size = header.width * header.height * (header.depth / 8);
    unique_ptr<uint8_t[]> rle_buf;
    if(header.image_type & 8)
    {
      rle_buf.reset(new uint8_t[data_size]);
      DecompressRLE(rle_buf.get(), static_cast<const uint8_t*>(contents.pSysMem), header.depth / 8, header.width * header.height);
      contents.pSysMem = rle_buf.get();
    }
    else
    {
      runtime_assert(file->getSize() >= predata_size + data_size, "TGA file is missing image data.");
    }

    unique_ptr<uint8_t[]> buf;
    switch(header.depth)
    {
    case 24:
      buf.reset(new uint8_t[contents.SysMemPitch * header.height + 15]);
      contents.pSysMem = Convert24To32(buf.get(), static_cast<const uint8_t*>(contents.pSysMem), data_size);
      break;
    case 32:
      break;
    default:
      throw runtime_error("Only 24bpp and 32bpp TGA files are supported.");
    }

    return d3.createTexture2D(desc, contents);
  }

  decltype(&LoadChunky) IdentifyTexture(MappableFile* file)
  {
    runtime_assert(file->getSize() >= 32, "File is too small to be a texture.");

    const char rgt_head[] = "Relic Chunky\x0D\x0A\x1A";
    auto sig = file->map(0, (std::max)(sizeof(rgt_head), sizeof(tga_header_t)));
    if(memcmp(sig.begin, rgt_head, sizeof(rgt_head)) == 0)
      return LoadChunky;

    if(memcmp(sig.begin, "DDS ", 4) == 0)
      return LoadDDS;

    bool tga_is_possible = false;
    auto tga_header = reinterpret_cast<const tga_header_t*>(sig.begin);
    if(tga_header->colour_map_type <= 1 && tga_header->image_type <= 11 && (tga_header->depth & 7) == 0)
      tga_is_possible = true;

    auto size = file->getSize();
    const char tga_tail[] = "TRUEVISION-XFILE.";
    sig = file->map(size - sizeof(tga_tail), size);
    if(memcmp(sig.begin, tga_tail, sizeof(tga_tail)) == 0)
      return LoadTGA;

    if(tga_is_possible)
      return LoadTGA;

    throw runtime_error("Unrecognised texture file format.");
  }
}

namespace Essence { namespace Graphics
{
  Texture2D LoadTexture(Device1& d3, unique_ptr<MappableFile> file)
  {
    auto loader = IdentifyTexture(&*file);
    return loader(d3, move(file));
  }

  class TextureCacheImpl
  {
  public:
    TextureCacheImpl(FileSource* mod_fs, C6::D3::Device1 d3)
      : m_mod_fs(mod_fs)
      , m_d3(d3)
    {
      auto white = CreateConstantTexture(d3, 1.f, 1.f, 1.f, 1.f);
      string white_name("shaders\\texture_white.rgt");
      SetDebugObjectName(white, white_name);
      m_textures[white_name] = d3.createShaderResourceView(white);
    }

    C6::D3::ShaderResourceView load(const std::string& path)
    {
      auto& srv = m_textures[path];
      if(!srv)
      {
        auto tex = LoadTexture(m_d3, m_mod_fs->readFile(path));
        SetDebugObjectName(tex, path);
        srv = m_d3.createShaderResourceView(tex);
      }
      return srv;
    }

  private:
    std::unordered_map<std::string, C6::D3::ShaderResourceView> m_textures;
    FileSource* m_mod_fs;
    C6::D3::Device1 m_d3;
  };

  TextureCache::TextureCache(FileSource* mod_fs, C6::D3::Device1 d3)
    : m_impl(new TextureCacheImpl(mod_fs, d3))
  {
  }

  TextureCache::~TextureCache()
  {
  }

  C6::D3::ShaderResourceView TextureCache::load(const std::string& path)
  {
    return m_impl->load(path);
  }
}}
