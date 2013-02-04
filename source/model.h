#pragma once
#include "chunky.h"
#include "directx.h"
#include "fs.h"
#include "math.h"

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

    inline const ChunkyString& getName() const { return *m_name; }

  private:
    const ChunkyString* m_name;
    uint32_t m_data_type;
  protected:
    const ChunkyString* m_value;
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
    ArenaArray<MaterialVariable*> m_material_variables;
  };

  class Object
  {
  public:
    Object(ChunkReader& r, ModelLoadContext& ctx, unsigned int& first_index);
    static void readIndices(ChunkReader& r, uint16_t*& destination);

    void render(C6::D3::Device1& d3);

  private:
    const ChunkyString* m_name;
    unsigned int m_index_count;
    unsigned int m_first_index;
  };

  class Mesh
  {
  public:
    Mesh(const Chunk* foldmesh, ModelLoadContext& ctx);

    void render(C6::D3::Device1& d3);
    const bounding_volume_t& getBoundingVolume() const;

  private:
    Material* m_material;
    const bounding_volume_t* m_bvol;
    C6::D3::InputLayout m_input_layout;
    C6::D3::Buffer m_indices;
    C6::D3::Buffer m_verticies;
    unsigned int m_vertex_stride;
    ArenaArray<Object*> m_objects;
  };

  class Model
  {
  public:
    Model(FileSource* mod_fs, ShaderDatabase* shaders, std::unique_ptr<MappableFile> rgm_file, C6::D3::Device1& d3);

    void render(C6::D3::Device1& d3);
    auto getMeshes() -> const ArenaArray<Mesh*>& { return m_meshes; }

  private:
    void collectMeshes(const Chunk* foldmesh, std::vector<const Chunk*>& meshes);

    Arena m_arena;
    ArenaArray<Mesh*> m_meshes;
    std::unique_ptr<const ChunkyFile> m_file;
    ShaderDatabase* m_shaders;
  };

} }
