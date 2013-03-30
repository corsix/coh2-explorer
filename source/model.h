#pragma once
#include "directx.h"
#include "arena.h"
#include "math.h"

namespace Essence
{
  class FileSource;
  class Chunk;
  class ChunkyFile;
  class ChunkReader;
  struct ChunkyString;
}

namespace Essence { namespace Graphics
{
  class ShaderDatabase;
  class Effect;
  class Technique;
  struct ModelLoadContext;

  namespace PropertyDataType
  {
    enum E : uint32_t
    {
      int1,
      float1,
      _unknown_2, // possibly int2
      float2,
      float3,
      float4,
      _unknown_6, // probably some float matrix
      float4x3,
      float4x4,
      texture,
      boolean,
      string
    };
  }

  class Property
  {
  public:
    Property(const Chunk* datavar);

    inline const ChunkyString& getName() const { return *m_name; }
    inline PropertyDataType::E getType() const { return m_data_type; }

  private:
    const ChunkyString* m_name;
    PropertyDataType::E m_data_type;
  protected:
    const ChunkyString* m_value;
  };

  class MaterialVariable : public Property
  {
  public:
    MaterialVariable(const Chunk* datavar);
    virtual ~MaterialVariable();
    virtual void apply(C6::D3::Device1& d3, uint32_t pass);
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
    Object(const ChunkyString* name, unsigned int index_count);
    static void readIndices(ChunkReader& r, uint16_t*& destination);

    auto getName() const -> const ChunkyString* { return m_name; }
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

    auto render(C6::D3::Device1& d3, const bool* object_visibility = nullptr) -> const bool*;
    auto getBoundingVolume() const -> const bounding_volume_t&;
    auto getObjects() -> const ArenaArray<Object*>& { return m_objects; }
    auto getName() -> const std::string& { return m_name; }

  private:
    void loadDataData5(const Chunk* datadata, ModelLoadContext& ctx);
    void loadDataData8(const Chunk* datadata, ModelLoadContext& ctx);

    void loadObjects(ChunkReader& r, ModelLoadContext& ctx);
    void loadIndicesAsObject(ChunkReader& r, ModelLoadContext& ctx);
    auto loadVertexData(ChunkReader& r, C6::D3::Device1& d3) -> std::vector<D3D10_INPUT_ELEMENT_DESC>;
    void loadMaterial(ChunkReader& r, ModelLoadContext& ctx, std::vector<D3D10_INPUT_ELEMENT_DESC> input_layout);

    Material* m_material;
    const bounding_volume_t* m_bvol;
    C6::D3::InputLayout m_input_layout;
    C6::D3::Buffer m_indices;
    C6::D3::Buffer m_verticies;
    unsigned int m_vertex_stride;
    ArenaArray<Object*> m_objects;
    std::string m_name;
  };

  class Model
  {
  public:
    Model(FileSource* mod_fs, ShaderDatabase* shaders, std::vector<std::unique_ptr<const ChunkyFile>> files, C6::D3::Device1& d3);
    ~Model();

    void render(C6::D3::Device1& d3, const bool* object_visibility = nullptr);
    auto getMeshes() -> const std::vector<Mesh*>& { return m_meshes; }
    auto getObjects() -> std::vector<Object*>;

  private:
    void loadMeshes(const Chunk* foldmesh, ModelLoadContext& ctx);

    Arena m_arena;
    std::vector<Mesh*> m_meshes;
    std::vector<std::unique_ptr<const ChunkyFile>> m_files;
    ShaderDatabase* m_shaders;
  };

} }
