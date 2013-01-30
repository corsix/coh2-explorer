#pragma once
#include "shader_db.h"
#include "chunky.h"
#include "fs.h"
#include "hash.h"
#include <algorithm>
#include <map>
using namespace std;

namespace
{
#pragma pack(push)
#pragma pack(1)
  struct datatech_common_t
  {
    uint32_t num_passes;
    uint32_t prop_count;
  };

  struct datatech7_t
  {
    uint32_t hash[2];
    uint32_t list_len[3];
    uint32_t unknown[4];
    uint32_t list_len3;
    uint32_t unknown4;
    datatech_common_t common;
  };

  struct datatech8_t
  {
    uint32_t hash[2];
    uint32_t unknown[2];
    uint32_t uses[3];
    uint32_t unknown2;
    datatech_common_t common;
    uint32_t variants[8];
  };

  struct parameter_desc7_t
  {
    uint32_t hash;
    uint32_t data_type;
    uint32_t array_size;
    uint32_t unknown[2];

    uint32_t getByteOffset(uint32_t previous_offset, uint32_t size) const
    {
      uint32_t align = (data_type < 4 && array_size == 1) ? (size - 1) : 15;
      return (previous_offset + align) &~ align;
    }
  };

  struct parameter_desc8_t
  {
    uint32_t hash;
    uint32_t data_type;
    uint32_t array_size;
    uint32_t byte_offset;
    uint32_t unknown[2];

    uint32_t getByteOffset(uint32_t previous_offset, uint32_t) const
    {
      runtime_assert(previous_offset <= byte_offset, "Unexpected non-monotonic constant buffer parameter offset.");
      return byte_offset;
    }
  };
#pragma pack(pop)

  class ShaderVariableStore
  {
  public:
    ShaderVariableStore()
      : m_next_code(static_cast<uint8_t*>(VirtualAlloc(nullptr, 64 * 1024, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)))
      , m_end_of_code(m_next_code + 52 * 1024)
      , m_next_var(m_end_of_code)
    {
      clearPrev();
    }

    ~ShaderVariableStore()
    {
      VirtualFree(static_cast<void*>(m_end_of_code - 52 * 1024), 0, MEM_RELEASE);
    }

    void(__fastcall* beginBlock())(void*)
    {
      m_padding = 0;
      m_last_rax = nullptr;
      return (void(__fastcall*)(void*))m_next_code;
    }

  private:
    template<typename T>
    static bool isAligned(T t)
    {
      return (((uintptr_t)t) & 0xf) == 0;
    }

    void clearPrev()
    {
      m_prev_dst_offset = ~static_cast<uint32_t>(0);
      m_last_rax = nullptr;
    }

  public:
    void addToBlock(uint32_t dst_offset, float* src, int32_t size)
    {
      for(; size >= 16; src += 4, dst_offset += 16, size -= 16)
      {
copy16:
        intptr_t delta = static_cast<intptr_t>((src - m_last_rax) * 4);
        if(-128 <= delta && delta < 128)
        {
          // xmm0 = xmmword [rax or eax + delta]
          *reinterpret_cast<uint32_t*>(m_next_code) = (static_cast<uint32_t>(delta) << 24) | 0x40280F; m_next_code += 4;
        }
        else
        {
          // rax or eax = src
#ifdef _WIN64
          *reinterpret_cast<uint32_t*>(m_next_code) = 0xB848; m_next_code += 2;
#else
          *m_next_code++ = 0xB8;
#endif
          *reinterpret_cast<float**>(m_next_code) = src; m_next_code += sizeof(float*);
          m_last_rax = src;
          // xmm0 = xmmword [rax or eax]
          *reinterpret_cast<uint32_t*>(m_next_code) = 0x00280F; m_next_code += 3;
        }
        // xmmword [rcx or ecx+dst_offset] = xmm0 (with non-temporal hint)
        *reinterpret_cast<uint32_t*>(m_next_code) = 0x812B0F; m_next_code += 3;
        *reinterpret_cast<uint32_t*>(m_next_code) = dst_offset; m_next_code += 4;
      }
      if(size > 0)
      {
        if(m_prev_dst_offset <= dst_offset && dst_offset + size <= m_prev_dst_offset + 16 && (src - m_prev_src) * 4 == dst_offset - m_prev_dst_offset)
        {
          // already done
          return;
        }
        if(isAligned(dst_offset) && isAligned(src))
        {
          m_prev_dst_offset = dst_offset;
          m_prev_src = src;
          goto copy16;
        }
        clearPrev();
      }
      for(; size >= 4; src += 1, dst_offset += 4, size -= 4)
      {
        // eax = dword [src]
        *m_next_code++ = 0xA1;
        *reinterpret_cast<float**>(m_next_code) = src; m_next_code += sizeof(float*);
        // dword [rcx or ecx+dst_offset] = eax
        *reinterpret_cast<uint32_t*>(m_next_code) = 0x8189; m_next_code += 2;
        *reinterpret_cast<uint32_t*>(m_next_code) = dst_offset; m_next_code += 4;
      }
    }

    void* allocateVar(uint32_t size)
    {
      if(size <= m_padding)
      {
        auto result = m_next_var - m_padding;
        m_padding -= size;
        return static_cast<void*>(result);
      }
      else
      {
        auto result = m_next_var;
        auto padded = (size + 15) &~ static_cast<uint32_t>(15);
        m_padding = padded - size;
        m_next_var += padded;
        runtime_assert(m_next_var < (m_end_of_code + 12 * 1024), "Too many distinct shader variables.");
        return static_cast<void*>(result);
      }
    }

    void endBlock()
    {
      // ret
      *m_next_code++ = 0xC3;
      runtime_assert(m_next_code <= m_end_of_code, "Too many distinct effects.");
    }

  private:
    uint8_t* m_next_code;
    uint8_t* const m_end_of_code;
    uint8_t* m_next_var;
    uint32_t m_padding;

    uint32_t m_prev_dst_offset;
    float* m_prev_src;
    float* m_last_rax;
  };

  class DescObjectCache
  {
  public:
    DescObjectCache(Arena* arena, uint32_t initial_capacity = 16)
      : m_arena(arena)
      , m_hashes(arena->mallocArray<uint32_t>(initial_capacity))
      , m_objects(arena->mallocArray<void*>(initial_capacity))
      , m_size(0)
      , m_capacity(initial_capacity)
    {
    }

#define FETCH(param, init_hash, factory_arg) \
    template <typename T, typename F> \
    T* fetch(param, uint32_t data_len, F&& factory) \
    { \
      grow(); \
      \
      init_hash; \
      auto mask = m_capacity - 1; \
      auto index = hash; \
      for(;;) \
      { \
        index = (index + 1) & mask; \
        if(m_hashes[index] == hash) \
        { \
          --m_size; \
          return static_cast<T*>(m_objects[index]); \
        } \
        if(m_objects[index] == nullptr) \
        { \
          auto obj = factory(factory_arg, data_len); \
          m_hashes[index] = hash; \
          m_objects[index] = obj; \
          return obj; \
        } \
      } \
    }
    FETCH(const uint8_t* data, auto hash = Essence::Hash(data, data_len), data)
    FETCH(const uint32_t hash, /*                                     */, hash)
#undef FETCH

  private:
    void grow()
    {
      if(m_size * 4 == m_capacity * 3)
      {
        DescObjectCache larger(m_arena, m_capacity * 2);
        for(uint32_t i = 0; i < m_capacity; ++i)
        {
          if(m_objects[i])
            larger.raw_insert(m_hashes[i], m_objects[i]);
        }
        m_hashes = larger.m_hashes;
        m_objects = larger.m_objects;
        m_capacity = larger.m_capacity;
      }
      ++m_size;
    }

    void raw_insert(uint32_t hash, void* obj)
    {
      auto mask = m_capacity - 1;
      auto index = hash;
      do
      {
        index = (index + 1) & mask;
      } while(m_objects[index]);
      m_hashes[index] = hash;
      m_objects[index] = obj;
    }

    Arena* m_arena;
    uint32_t* m_hashes;
    void** m_objects;
    uint32_t m_size;
    uint32_t m_capacity;
  };
}

namespace Essence { namespace Graphics
{
  class ShaderDatabaseImpl
  {
  public:
    ShaderDatabaseImpl(Arena* arena, FileSource* mod_fs, C6::D3::Device1 d3);

    ID3D10RasterizerState*   getRasterizerState  (const uint8_t* desc_data, uint32_t desc_len);
    ID3D10BlendState1*       getBlendState       (const uint8_t* desc_data, uint32_t desc_len);
    ID3D10DepthStencilState* getDepthStencilState(const uint8_t* desc_data, uint32_t desc_len);
    ID3D10SamplerState*      getSamplerState     (const uint8_t* desc_data, uint32_t desc_len);

    ConstantBuffer* getConstantBuffer(const uint32_t* parameter_descs, uint32_t num_parameters, uint32_t version, bool srv);

    ShaderVariableStore& getVariableStore() { return m_variables; }
    float* getVariable(uint32_t hash, uint32_t size);
    void variablesUpdated() { ++m_variables_serial; }
    uint64_t getVariablesSerial() { return m_variables_serial; }

    FileSource* getModFs() { return m_mod_fs; }
    C6::D3::Device1& getD3() { return m_d3; }
    Arena* getArena() { return m_arena; }

    Effect& load(const std::string& name);

  private:
    DescObjectCache m_rasterizer_descs;
    DescObjectCache m_blend_descs;
    DescObjectCache m_depth_stencil_descs;
    DescObjectCache m_sampler_descs;
    DescObjectCache m_ConstantBuffers;
    FileSource* m_mod_fs;
    C6::D3::Device1 m_d3;
    map<string, Effect*> m_effects;
    Arena* m_arena;
    ShaderVariableStore m_variables;
    DescObjectCache m_variable_lut;
    uint64_t m_variables_serial;
  };

  template <typename T>
  static auto InArena(Arena* arena, T obj) -> decltype(obj.getRawInterface())
  {
    return arena->alloc<T>(move(obj))->getRawInterface();
  }

  ID3D10RasterizerState* ShaderDatabaseImpl::getRasterizerState(const uint8_t* desc_data, uint32_t desc_len)
  {
    runtime_assert(desc_len == sizeof(D3D10_RASTERIZER_DESC), "Invalid rasterizer desc size.");
    return m_rasterizer_descs.fetch<ID3D10RasterizerState>(desc_data, desc_len, [this](const uint8_t* desc_data, uint32_t desc_len)
    {
      return InArena(m_arena, m_d3.createRasterizerState(*reinterpret_cast<const D3D10_RASTERIZER_DESC*>(desc_data)));
    });
  }

  ID3D10BlendState1* ShaderDatabaseImpl::getBlendState(const uint8_t* desc_data, uint32_t desc_len)
  {
    runtime_assert(desc_len == sizeof(D3D10_BLEND_DESC1), "Invalid blend desc size.");
    return m_blend_descs.fetch<ID3D10BlendState1>(desc_data, desc_len, [this](const uint8_t* desc_data, uint32_t desc_len)
    {
      return InArena(m_arena, m_d3.createBlendState1(*reinterpret_cast<const D3D10_BLEND_DESC1*>(desc_data)));
    });
  }

  ID3D10DepthStencilState* ShaderDatabaseImpl::getDepthStencilState(const uint8_t* desc_data, uint32_t desc_len)
  {
    runtime_assert(desc_len == 50, "Invalid depth-stencil desc size.");
    return m_depth_stencil_descs.fetch<ID3D10DepthStencilState>(desc_data, desc_len, [this](const uint8_t* desc_data, uint32_t desc_len) -> ID3D10DepthStencilState*
    {
      D3D10_DEPTH_STENCIL_DESC desc;
      memcpy(&desc, desc_data, 18);
      memcpy(reinterpret_cast<uint8_t*>(&desc) + 20, desc_data + 18, 32);
      return InArena(m_arena, m_d3.createDepthStencilState(desc));
    });
  }

  ID3D10SamplerState* ShaderDatabaseImpl::getSamplerState(const uint8_t* desc_data, uint32_t desc_len)
  {
    runtime_assert(desc_len == sizeof(D3D10_SAMPLER_DESC), "Invalid sampler desc size.");
    return m_sampler_descs.fetch<ID3D10SamplerState>(desc_data, desc_len, [this](const uint8_t* desc_data, uint32_t desc_len)
    {
      return InArena(m_arena, m_d3.createSamplerState(*reinterpret_cast<const D3D10_SAMPLER_DESC*>(desc_data)));
    });
  }

  class ConstantBuffer
  {
  public:
    C6::D3::ShaderResourceView srv;
    C6::D3::Buffer buffer;
    void (__fastcall* update)(void*);
    uint64_t serial;
  };

  template <typename T>
  static std::tuple<void (__fastcall*)(void*), uint32_t> ReadParameters(T* descs, ShaderDatabaseImpl& db, uint32_t num)
  {
    ShaderVariableStore& store = db.getVariableStore();
    const auto result = store.beginBlock();
    uint32_t offset = 0;
    for(const auto end = descs + num; descs != end; ++descs)
    {
      uint32_t size, size_in_array;
      switch(descs->data_type)
      {
      case 0: case 1:         size = 4; size_in_array = 16; break;
      case 3: case 4: case 5: size = (descs->data_type - 1) * 4; size_in_array = 16; break;
      case 7: case 8:         size = size_in_array = (descs->data_type - 4) * 16; break;
      case 9:                 continue;
      default:                throw runtime_error("Unknown constant buffer parameter type.");
      }
      runtime_assert(descs->array_size != 0, "Unexpected constant buffer parameter array dimension.");
      offset = descs->getByteOffset(offset, size);
      size = (descs->array_size - 1) * size_in_array + size;
      store.addToBlock(offset, db.getVariable(descs->hash, size), size);
      offset += size;
    }
    store.endBlock();
    return std::make_tuple(result, (offset + 15) &~ static_cast<uint32_t>(15));
  }

  ConstantBuffer* ShaderDatabaseImpl::getConstantBuffer(const uint32_t* parameter_descs, uint32_t num_parameters, uint32_t version, bool srv)
  {
    const uint32_t fields_per_record = 5 + version / 8;
    const uint32_t data_len = num_parameters * sizeof(uint32_t) * fields_per_record;

    return m_ConstantBuffers.fetch<ConstantBuffer>(reinterpret_cast<const uint8_t*>(parameter_descs), data_len, [=](const uint8_t* desc_data, uint32_t) -> ConstantBuffer*
    {
      auto cb = m_arena->alloc<ConstantBuffer>();

      D3D10_BUFFER_DESC desc;
      switch(version) {
      case 7: tie(cb->update, desc.ByteWidth) = ReadParameters(reinterpret_cast<const parameter_desc7_t*>(desc_data), *this, num_parameters); break;
      case 8: tie(cb->update, desc.ByteWidth) = ReadParameters(reinterpret_cast<const parameter_desc8_t*>(desc_data), *this, num_parameters); break;
      default: throw runtime_error("Unsupported constant buffer parameter description version.");
      }

      if(desc.ByteWidth == 0)
        return cb;

      desc.Usage = D3D10_USAGE_DYNAMIC;
      desc.BindFlags = srv ? D3D10_BIND_SHADER_RESOURCE : D3D10_BIND_CONSTANT_BUFFER;
      desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
      desc.MiscFlags = 0;
      cb->buffer = m_d3.createBuffer(desc);

      if(srv)
      {
        D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc;
        srv_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        srv_desc.ViewDimension = D3D10_SRV_DIMENSION_BUFFER;
        srv_desc.Buffer.FirstElement = 0;
        srv_desc.Buffer.NumElements = desc.ByteWidth / 16;
        cb->srv = m_d3.createShaderResourceView(cb->buffer, srv_desc);
      }

      return cb;
    });
  }

  float* ShaderDatabaseImpl::getVariable(uint32_t hash, uint32_t size)
  {
    return m_variable_lut.fetch<float>(hash, size, [&](uint32_t, uint32_t size)
    {
      return static_cast<float*>(m_variables.allocateVar(size));
    });
  }

  void TechniquePass::instantiateShaders(Arena* arena, C6::D3::Device1& d3)
  {
#define INSTANTIATE(field, stage) \
    if(!field->second) field->second = arena->alloc<C6::D3::stage##Shader> \
    (field->first.size() ? d3.create##stage##Shader(field->first) : nullptr)

    INSTANTIATE(m_vs, Vertex);
    INSTANTIATE(m_gs, Geometry);
    INSTANTIATE(m_ps, Pixel);
#undef INSTANTIATE
  }

  LengthPrefixBlock TechniquePass::getInputSignature()
  {
    return m_vs->first;
  }

  void TechniquePass::apply(C6::D3::Device1& d3)
  {
    d3.getRawInterface()->RSSetState(m_rasterizer_state);
    d3.getRawInterface()->OMSetBlendState(m_blend_state, m_constants->blend_factor, m_constants->om_sample_mask);
    d3.getRawInterface()->OMSetDepthStencilState(m_depth_stencil_state, m_constants->stencil_ref_value);
    d3.VSSetShader(static_cast<C6::D3::VertexShader&>(*m_vs->second));
    d3.GSSetShader(static_cast<C6::D3::GeometryShader&>(*m_gs->second));
    d3.PSSetShader(static_cast<C6::D3::PixelShader&>(*m_ps->second));
    d3.VSSetSamplers(0, m_vs_samplers);
    d3.GSSetSamplers(0, m_gs_samplers);
    d3.PSSetSamplers(0, m_ps_samplers);
  }

  const TechniquePass::TextureInfo* TechniquePass::findTexture(const std::string& name)
  {
    auto hash = Hash(name);
    for(auto ti = m_textures, end = m_textures + m_num_textures; ti != end; ++ti)
    {
      if(ti->hash == hash)
        return ti;
    }
    return nullptr;
  }

  Technique::Technique(Arena* arena, uint32_t num_passes)
    : m_passes(arena, num_passes)
  {
  }

  TechniquePass* Technique::getPass(uint32_t index)
  {
    return &m_passes[index];
  }

#define DB_ROOT "shaderdatabase/d3d11/ps40/"

  Effect::Effect(ShaderDatabaseImpl& db, const string& name)
    : m_db(db)
    , m_techniques(db.getArena(), 0)
    , m_shaders(db.getArena(), 0)
    , m_rasterizer_states(db.getArena(), 0)
    , m_blend_states(db.getArena(), 0)
    , m_depth_stencil_states(db.getArena(), 0)
    , m_sampler_states(db.getArena(), 0)
  {
    auto mod_fs = db.getModFs();

    m_fxinfo_file = ChunkyFile::Open(mod_fs->readFile(DB_ROOT + name + ".fxinfo"));
    readFxinfo();

    m_fxo_file = ChunkyFile::Open(mod_fs->readFile(DB_ROOT + name + ".fxo"));
    readFxo();
  }

  template <typename T, T* (ShaderDatabaseImpl:: *F)(const uint8_t*, uint32_t)>
  static void ReadSpusDescs(ArenaArray<T*>& arr, ShaderDatabaseImpl& db, ChunkReader& r, uint32_t desc_size)
  {
    arr.recreate(db.getArena(), r.read<uint32_t>()).for_each([&, desc_size](T*& dest){
      auto data = r.tell();
      r.seek(desc_size);
      dest = (db.*F)(data, desc_size);
    });
  }

  void Effect::readFxinfo()
  {
    const auto arena = m_db.getArena();
    const uint32_t* num_variables;

    {
      auto datadesc = m_fxinfo_file->findFirst("FOLDSPFI")->findFirst("DATADESC");
      runtime_assert(datadesc != nullptr, "No DATADESC found in fxinfo file.");

      ChunkReader r(datadesc);
      m_primary_technique_index = r.read<uint32_t>();
      num_variables = reinterpret_cast<const uint32_t*>(r.tell());
      r.seek(12);
      m_techniques.recreate(arena, r.read<uint32_t>());
      runtime_assert(m_primary_technique_index < m_techniques.size(), "Invalid primary technique index in fxinfo file.");
    }

    {
      auto dataspus = m_fxinfo_file->findFirst("FOLDSPFI")->findFirst("DATASPUS");
      runtime_assert(dataspus != nullptr, "No DATASPUS found in fxinfo file.");
      runtime_assert(dataspus->getVersion() == 1, "Unsupported DATASPUS version in fxinfo file.");

      ChunkReader r(dataspus);
      ReadSpusDescs<ID3D10RasterizerState  , &ShaderDatabaseImpl::getRasterizerState  >(m_rasterizer_states   , m_db, r, sizeof(D3D10_RASTERIZER_DESC));
      ReadSpusDescs<ID3D10BlendState1      , &ShaderDatabaseImpl::getBlendState       >(m_blend_states        , m_db, r, sizeof(D3D10_BLEND_DESC1));
      ReadSpusDescs<ID3D10DepthStencilState, &ShaderDatabaseImpl::getDepthStencilState>(m_depth_stencil_states, m_db, r, 50);
      ReadSpusDescs<ID3D10SamplerState     , &ShaderDatabaseImpl::getSamplerState     >(m_sampler_states      , m_db, r, sizeof(D3D10_SAMPLER_DESC));
    }

    {
      auto datatech = m_fxinfo_file->findFirst("FOLDSPFI")->findFirst("DATATECH");
      runtime_assert(datatech != nullptr, "No DATATECH found in fxinfo file.");
      const auto version = datatech->getVersion();
      runtime_assert(version == 7 || version == 8, "Unsupported DATATECH version in fxinfo file.");

      num_variables += version / 8;
      readFxinfoParameters(num_variables, version);

      ChunkReader r(datatech);
      for_each(m_techniques.begin(), m_techniques.end(), [&](Technique*& technique)
      {
        technique = readFxinfoTechnique(r, datatech->getVersion());
      });
    }
  }

  void Effect::readFxinfoParameters(const uint32_t* num_variables, const uint32_t datatech_version)
  {
    auto dataparm = m_fxinfo_file->findFirst("FOLDSPFI")->findFirst("DATAPARM");
    runtime_assert(dataparm != nullptr, "No DATAPARM found in fxinfo file.");
    runtime_assert(dataparm->getVersion() == 1, "Unsupported DATAPARM version in fxinfo file.");

    const uint32_t fields_per_record = 5 + datatech_version / 8;
    auto records = reinterpret_cast<const uint32_t*>(dataparm->getContents());
    runtime_assert(fields_per_record * sizeof(uint32_t) * (num_variables[0] + num_variables[1]) <= dataparm->getSize(), "Incomplete DATAPARM in fxinfo file.");

    for(int buffer = 0; buffer < 2; ++buffer)
    {
      m_variables[buffer] = m_db.getConstantBuffer(records, *num_variables, datatech_version, buffer == 0);
      records += *num_variables * fields_per_record;
      ++num_variables;
    }
  }

  Technique* Effect::readFxinfoTechnique(ChunkReader& r, uint32_t datatech_version)
  {
    const auto arena = m_db.getArena();
    const datatech_common_t* common;
    if(datatech_version == 7)
    {
      auto t = r.reinterpret<datatech7_t>();
      common = &t->common;
      r.seek(4 * (t->list_len[0] + t->list_len[1] + t->list_len[2] + t->list_len3));
    }
    else
    {
      common = &r.reinterpret<datatech8_t>()->common;
    }

    auto technique = arena->allocTrivial<Technique>(arena, common->num_passes);
    for(uint32_t p = 0; p < common->num_passes; ++p)
    {
      auto pass = technique->getPass(p);
      auto unknown_count = r.read<uint32_t>();
      auto texture_count = r.read<uint32_t>();
      r.seek(unknown_count);
      pass->m_rasterizer_state = m_rasterizer_states.at(r.read<uint32_t>());
      pass->m_blend_state = m_blend_states.at(r.read<uint32_t>());
      pass->m_depth_stencil_state = m_depth_stencil_states.at(r.read<uint32_t>());
      pass->m_constants = reinterpret_cast<const TechniquePass::Constants*>(r.tell()); r.seek(sizeof(TechniquePass::Constants));
      for(auto itr = pass->m_vs_samplers; itr != end(pass->m_ps_samplers); ++itr)
        *itr = m_sampler_states.at(r.read<uint32_t>());
      r.seek(16 * 4 * 3);
      pass->m_num_textures = texture_count;
      pass->m_textures = r.reinterpret<TechniquePass::TextureInfo>(texture_count);
    }
    for(uint32_t p = 0; p < common->prop_count; ++p)
    {
      r.seek(8);
      r.seek(r.read<uint32_t>());
    }
    return technique;
  }

  void Effect::readFxo()
  {
    const auto arena = m_db.getArena();

    auto dataspff = m_fxo_file->findFirst("DATASPFF");
    if(!dataspff)
      throw runtime_error("No DATASPFF found in fxo file.");

    uint32_t num_categories;
    switch(dataspff->getVersion())
    {
    case 7: num_categories = 6; break;
    case 8: num_categories = 1; break;
    default: throw runtime_error("Unsupported DATASPFF version in fxo file.");
    }
    ChunkReader ptr(dataspff);

    m_shaders.recreate(arena, num_categories);
    for(uint32_t category = 0; category < num_categories; ++category)
    {
      m_shaders[category].recreate(arena, ptr.read<uint32_t>()).for_each([&ptr](LazyShader& dest){
        ptr.seek(8);
        auto size_ptr = reinterpret_cast<const uint32_t*>(ptr.tell());
        dest = make_pair(size_ptr, nullptr);
        ptr.seek(4 + *size_ptr);
      });
    }

    auto num_techniques = ptr.read<uint32_t>();
    runtime_assert(num_techniques == m_techniques.size(), "Technique count in fxo file differs from count in fxinfo file.");
    m_techniques.for_each([&](Technique* technique)
    {
      auto num_passes = ptr.read<uint32_t>();
      runtime_assert(num_passes == technique->getNumPasses(), "Technique pass count in fxo file differs from count in fxinfo file.");
      for(uint32_t p = 0; p < num_passes; ++p)
      {
        auto pass = technique->getPass(p);
        LazyShader* shaders[6];
        for(int i = 0; i < 6; ++i)
        {
          auto& category = m_shaders[i % num_categories];
          auto index = ptr.read<uint32_t>();
          runtime_assert(index < category.size(), "Invalid shader index in fxinfo file.");
          shaders[i] = &category[index];
        }
        pass->m_vs = shaders[0];
        pass->m_gs = shaders[1];
        pass->m_ps = shaders[2];
      }
    });
  }

  Technique& Effect::getPrimaryTechnique()
  {
    auto t = m_techniques[m_primary_technique_index];
    if(t->getPass(0)->m_vs->second == nullptr)
    {
      for(uint32_t i = 0; i < t->getNumPasses(); ++i)
        t->getPass(i)->instantiateShaders(m_db.getArena(), m_db.getD3());
    }
    return *t;
  }

  void Effect::apply(C6::D3::Device1& d3)
  {
    const auto serial = m_db.getVariablesSerial();
    for(auto itr = begin(m_variables); itr != end(m_variables); ++itr)
    {
      auto& buf = **itr;
      if(buf.serial == serial)
        continue;

      if(buf.buffer)
      {
        auto mapped = buf.buffer.map(D3D10_MAP_WRITE_DISCARD, 0);
        buf.update(mapped);
        buf.buffer.unmap();
      }
      buf.serial = serial;
    }

#define SET_EVERY_STAGE(what, slot, rsrc) d3.VSSet ## what(slot, rsrc), d3.GSSet ## what(slot, rsrc), d3.PSSet ## what(slot, rsrc)
    SET_EVERY_STAGE(ShaderResources, 48, m_variables[0]->srv);
    SET_EVERY_STAGE(ConstantBuffers,  0, m_variables[1]->buffer);
  }

  ShaderDatabaseImpl::ShaderDatabaseImpl(Arena* arena, FileSource* mod_fs, C6::D3::Device1 d3)
    : m_rasterizer_descs(arena)
    , m_blend_descs(arena)
    , m_depth_stencil_descs(arena)
    , m_sampler_descs(arena)
    , m_ConstantBuffers(arena)
    , m_mod_fs(mod_fs)
    , m_d3(move(d3))
    , m_arena(arena)
    , m_variable_lut(arena, 256)
  {
  }

  ShaderDatabase::ShaderDatabase(Arena* arena, FileSource* mod_fs, C6::D3::Device1 d3)
    : m_impl(arena->alloc<ShaderDatabaseImpl>(arena, mod_fs, move(d3)))
  {
  }

  Effect& ShaderDatabaseImpl::load(const string& name)
  {
    Effect*& e = m_effects[name];
    if(!e)
    {
      try
      {
        e = m_arena->alloc<Effect>(*this, name);
      }
      catch(const exception& e)
      {
        throw runtime_error("Error whilst loading effect " + name + ": " + e.what());
      }
    }
    return *e;
  }

  Effect& ShaderDatabase::load(const string& name)
  {
    return m_impl->load(name);
  }

  float* ShaderDatabase::getVariable(const std::string& name, uint32_t size)
  {
    return m_impl->getVariable(Hash(name), size);
  }

  void ShaderDatabase::variablesUpdated()
  {
    m_impl->variablesUpdated();
  }

}}
