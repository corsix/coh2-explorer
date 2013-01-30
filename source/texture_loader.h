#pragma once
#include <memory>
#include <string>

class MappableFile;
namespace Essence { class FileSource; }
namespace C6 { namespace D3 {
  class Device1;
  class Texture2D;
  class ShaderResourceView;
}}

namespace Essence { namespace Graphics
{
  C6::D3::Texture2D LoadTexture(C6::D3::Device1& d3, std::unique_ptr<MappableFile> file);

  class TextureCacheImpl;
  class TextureCache
  {
  public:
    TextureCache(FileSource* mod_fs, C6::D3::Device1 d3);
    ~TextureCache();

    C6::D3::ShaderResourceView load(const std::string& path);

  private:
    std::unique_ptr<TextureCacheImpl> m_impl;
  };
}}
