#pragma once
#include "chunky.h"
#include "directx.h"
#include "fs.h"

namespace Essence { namespace Graphics
{
  class ShaderDatabase;
  class Effect;
  class Technique;
  struct ModelLoadContext;

  class MaterialVariable
  {
  public:
    MaterialVariable(const Chunk* datavar);
    virtual ~MaterialVariable();
    virtual void apply(C6::D3::Device1& d3, uint32_t pass);

    inline const std::string& getName() const { return m_name; }

  private:
    std::string m_name;
    uint32_t m_data_type;
  protected:
    const uint8_t* m_value;
  };

  class Material
  {
  public:
    Material(const Chunk* foldmtrl, ModelLoadContext& ctx);

    Technique& apply(C6::D3::Device1& d3);
    void applyVariablesForPass(C6::D3::Device1& d3, uint32_t pass);
    inline Effect& getEffect() {return *m_effect;}

  private:
    Effect* m_effect;
    std::vector<MaterialVariable*> m_material_variables;
  };

  class Object
  {
  public:
    Object(ChunkReader& r, ModelLoadContext& ctx);

    void render(C6::D3::Device1& d3);

  private:
    std::string m_name;
    unsigned int m_index_count;
    C6::D3::Buffer m_indicies;
  };

  class Mesh
  {
  public:
    Mesh(const Chunk* foldmesh, ModelLoadContext& ctx);

    void render(C6::D3::Device1& d3);

  private:
    Material* m_material;
    C6::D3::InputLayout m_input_layout;
    C6::D3::Buffer m_verticies;
    unsigned int m_vertex_stride;
    std::vector<Object*> m_objects;
  };

  class Model
  {
  public:
    Model(FileSource* mod_fs, ShaderDatabase* shaders, std::unique_ptr<MappableFile> rgm_file, C6::D3::Device1& d3);

    void render(C6::D3::Device1& d3);

  private:
    Arena m_arena;
    std::vector<Mesh*> m_meshes;
    std::unique_ptr<const ChunkyFile> m_file;
    ShaderDatabase* m_shaders;
  };

} }