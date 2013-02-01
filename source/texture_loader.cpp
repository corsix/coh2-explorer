#include "texture_loader.h"
#include "fs.h"
#include "directx.h"
#include "chunky.h"
#include "arena.h"
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
    uint32_t unknown1;
    uint32_t width;
    uint32_t height;
    uint32_t unknown2;
  };
#pragma pack(pop)

  unsigned int DXTC_Stride(DWORD width, DWORD coeff)
  {
    return (max)(static_cast<DWORD>(1), (width + 3) / 4) * coeff;
  }

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

  Texture2D LoadChunkyDXTC(Device1& d3, const Chunk* dxtc)
  {
    auto tfmt_chunk = dxtc->findFirst("DATATFMT");
    runtime_assert(tfmt_chunk != nullptr, "FOLDDXTC missing DATATFMT.");
    runtime_assert(tfmt_chunk->getSize() >= sizeof(dxtc_tfmt_t), "DATATFMT is too small.");
    auto tfmt = reinterpret_cast<const dxtc_tfmt_t*>(tfmt_chunk->getContents());

    DXGI_FORMAT format;
    unsigned int stride;
    auto c = tfmt->compression;
    switch(tfmt->compression)
    {
    // TODO: Establish the difference between these pairs. Possibly one is meant to be SRGB and one isn't?
    case 13: case 22: format = DXGI_FORMAT_BC1_UNORM; stride = DXTC_Stride(tfmt->width,  8); break;
    case 14: case 23: format = DXGI_FORMAT_BC2_UNORM; stride = DXTC_Stride(tfmt->width, 16); break;
    case 15: case 24: format = DXGI_FORMAT_BC3_UNORM; stride = DXTC_Stride(tfmt->width, 16); break;
    default: throw runtime_error("FOLDDXTC uses unknown compression method.");
    }
    if(tfmt->compression < 16)
      format = static_cast<DXGI_FORMAT>(static_cast<int>(format) + 1);

    auto tman_chunk = dxtc->findFirst("DATATMAN");
    runtime_assert(tman_chunk != nullptr, "FOLDDXTC missing DATATMAN.");
    runtime_assert(tman_chunk->getSize() >= 4, "DATATMAN is too small.");
    auto tman = reinterpret_cast<const dxtc_tman_t*>(tman_chunk->getContents());
    runtime_assert(tman->mip_count != 0, "DATATMAN contains no mip levels.");
    runtime_assert(tman_chunk->getSize() >= 4 + tman->mip_count * 8, "DATATMAN is incomplete.");

    auto mip = tman->mips;
    uint32_t tdat_offset = 0;
    for(uint32_t i = 1; i < tman->mip_count; ++i, ++mip)
    {
      tdat_offset += mip->data_length_compressed;
    }
    runtime_assert(mip->data_length == tfmt->height * (stride / 4) + sizeof(dxtc_tdat_entry_header_t), "FOLDDXTC mip level data does not match texture dimensions.");

    auto tdat_chunk = dxtc->findFirst("DATATDAT");
    runtime_assert(tdat_chunk != nullptr, "FOLDDXTC missing DATATDAT.");
    runtime_assert(tdat_chunk->getSize() >= tdat_offset + mip->data_length_compressed, "DATATDAT is too small.");

    unique_ptr<uint8_t[]> uncompressed;
    auto tdat = tdat_chunk->getContents() + tdat_offset;
    if(mip->data_length != mip->data_length_compressed)
    {
      uncompressed.reset(new uint8_t[mip->data_length]);
      uLong length = mip->data_length;
      if(uncompress(uncompressed.get(), &length, tdat, mip->data_length_compressed) != Z_OK)
        throw runtime_error("Could not inflate payload of DATATDAT.");
      runtime_assert(length == mip->data_length, "Inflation of DATATDAT yielded unexpected size.");
      tdat = uncompressed.get();
    }

    auto tdat_header = reinterpret_cast<const dxtc_tdat_entry_header_t*>(tdat);
    runtime_assert(tdat_header->width == tfmt->width && tdat_header->height == tfmt->height, "DATATDAT dimensions do not match DATATFMT dimensions.");

    DefaultTextureDesc desc;
    desc.Width = tfmt->width;
    desc.Height = tfmt->height;
    desc.Format = format;

    D3D10_SUBRESOURCE_DATA contents;
    contents.pSysMem = tdat_header + 1;
    contents.SysMemPitch = stride;
    contents.SysMemSlicePitch = 0;

    return d3.createTexture2D(desc, contents);
  }

  Texture2D LoadChunkyIMAG(Device1& d3, const Chunk* imag)
  {
    throw runtime_error("TODO: Implement FOLDIMAG loading.");
  }

  Texture2D LoadChunky(Device1& d3, unique_ptr<MappableFile> file)
  {
    auto chunky = ChunkyFile::Open(move(file));
    auto txtr = chunky->findFirst("FOLDTSET")->findFirst("FOLDTXTR");
    runtime_assert(txtr != nullptr, "Could not find FOLDTXTR in chunky file.");

    if(auto dxtc = txtr->findFirst("FOLDDXTC")) return LoadChunkyDXTC(d3, dxtc);
    if(auto imag = txtr->findFirst("FOLDIMAG")) return LoadChunkyIMAG(d3, imag);
    throw runtime_error("Unknown texture type in FOLDTXTR.");
  }

  Texture2D LoadDDS(Device1& d3, unique_ptr<MappableFile> file)
  {
    throw runtime_error("TODO: Implement DDS loading.");
  }

  Texture2D LoadTGA(Device1& d3, unique_ptr<MappableFile> file)
  {
    runtime_assert(file->getSize() >= sizeof(tga_header_t), "TGA file is too small.");
    auto mapped = file->mapAll();
    auto& header = *reinterpret_cast<const tga_header_t*>(mapped.begin);
    if(header.image_type != 2)
      throw runtime_error("Only truecolour TGA files are supported.");
    if(header.depth != 32)
      throw runtime_error("Only 32bpp TGA files are supported.");

    size_t predata_size = sizeof(tga_header_t) + header.id_length;
    if(header.colour_map_type)
      predata_size += header.palette_size * (header.palette_bpp / 8);
    size_t data_size = header.width * header.height * (header.depth / 8);
    runtime_assert(file->getSize() >= predata_size + data_size, "TGA file is missing image data.");

    DefaultTextureDesc desc;
    desc.Width = header.width;
    desc.Height = header.height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

    D3D10_SUBRESOURCE_DATA contents;
    contents.pSysMem = mapped.begin + predata_size;
    contents.SysMemPitch = header.width * (header.depth / 8);

    return d3.createTexture2D(desc, contents);
  }

  decltype(&LoadChunky) IdentifyTexture(MappableFile* file)
  {
    runtime_assert(file->getSize() >= 32, "File is too small to be a texture.");

    const char rgt_head[] = "Relic Chunky\x0D\x0A\x1A";
    auto sig = file->map(0, sizeof(rgt_head));
    if(memcmp(sig.begin, rgt_head, sizeof(rgt_head)) == 0)
      return LoadChunky;

    if(memcmp(sig.begin, "DDS ", 4) == 0)
      return LoadDDS;

    auto size = file->getSize();
    const char tga_tail[] = "TRUEVISION-XFILE.";
    sig = file->map(size - sizeof(tga_tail), size);
    if(memcmp(sig.begin, tga_tail, sizeof(tga_tail)) == 0)
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
