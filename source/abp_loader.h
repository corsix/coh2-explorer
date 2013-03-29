#pragma once
#include <nice/d3.h>
#include <memory>
#include <string>

namespace Essence
{
  class FileSource;
}

namespace Essence { namespace Graphics
{
  class Model;
  class ShaderDatabase;

  std::unique_ptr<Model> LoadModel(FileSource* mod_fs, ShaderDatabase* shaders, const std::string& path, C6::D3::Device1& d3);
}}
