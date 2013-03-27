#pragma once
#include "directx.h"
#include "arena.h"
#include "hash.h"
#include <memory>

namespace Essence
{
  class FileSource;
  class ChunkyFile;
  class ChunkReader;
  struct ChunkyString;
}

namespace Essence { namespace Graphics
{
  typedef std::pair<const ChunkyString*, C6::D3::DeviceChild*> LazyShader;

  class TechniquePass
  {
  public:
    void apply(C6::D3::Device1& d3);
    const ChunkyString& getInputSignature();

#pragma pack(push)
#pragma pack(1)
    struct TextureInfo
    {
      public : uint32_t hash;
      public : uint32_t flags;
      private: uint8_t unused1[8];
      public : uint32_t slot;
      private: uint8_t unused2[13];
    };

    struct Constants
    {
      uint32_t stencil_ref_value;
      float blend_factor[4];
      uint32_t om_sample_mask;
    };
#pragma pack(pop)

    const TextureInfo* findTexture(Hashable name);

  private:
    void instantiateShaders(Arena* arena, C6::D3::Device1& d3);
    friend class Effect;

    LazyShader* m_vs;
    LazyShader* m_gs;
    LazyShader* m_ps;
    ID3D10RasterizerState* m_rasterizer_state;
    ID3D10BlendState1* m_blend_state;
    ID3D10DepthStencilState* m_depth_stencil_state;
    const Constants* m_constants;
    ID3D10SamplerState* m_vs_samplers[16];
    ID3D10SamplerState* m_gs_samplers[16];
    ID3D10SamplerState* m_ps_samplers[16];
    const TextureInfo* m_textures;
    uint32_t m_num_textures;
  };

  class Technique
  {
  public:
    Technique(Arena* arena, uint32_t num_passes);

    uint32_t getNumPasses() const { return m_passes.size(); }
    TechniquePass* getPass(uint32_t index);

  private:
    ArenaArray<TechniquePass> m_passes;
  };

  class ShaderDatabaseImpl;
  class ConstantBuffer;

  class Effect
  {
  public:
    Effect(ShaderDatabaseImpl& db, const std::string& name);
    ~Effect();

    void apply(C6::D3::Device1& d3);
    Technique& getPrimaryTechnique();

  private:
    void readFxinfo();
    Technique* readFxinfoTechnique(ChunkReader& r, uint32_t datatech_version);
    void readFxinfoParameters(const uint32_t* num_variables, uint32_t datatech_version);
    void readFxo();

    ShaderDatabaseImpl& m_db;
    std::unique_ptr<const ChunkyFile> m_fxo_file;
    std::unique_ptr<const ChunkyFile> m_fxinfo_file;
    ArenaArray<ArenaArray<LazyShader>> m_shaders;
    uint32_t m_primary_technique_index;
    ArenaArray<Technique*> m_techniques;
    ArenaArray<ID3D10RasterizerState*> m_rasterizer_states;
    ArenaArray<ID3D10BlendState1*> m_blend_states;
    ArenaArray<ID3D10DepthStencilState*> m_depth_stencil_states;
    ArenaArray<ID3D10SamplerState*> m_sampler_states;
    ConstantBuffer* m_variables[2];
  };

  class ShaderDatabase
  {
  public:
    ShaderDatabase(Arena* arena, FileSource* mod_fs, C6::D3::Device1 d3);

    Effect& load(const std::string& name);
    Effect& load(const ChunkyString* name);
    float* getVariable(Hashable name, uint32_t size);
    void variablesUpdated();

  private:
    ShaderDatabaseImpl* m_impl;
  };

}}
