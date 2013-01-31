#include "model.h"
#include "shader_db.h"
#include "texture_loader.h"
#include <algorithm>
#include <map>
using namespace std;
using namespace C6::D3;

namespace Essence { namespace Graphics
{
  struct ModelLoadContext
  {
    Arena& arena;
    Device1& d3;
    ShaderDatabase& shaders;
    TextureCache& textures;
    map<string, Material*> materials;
  };

  MaterialVariable::MaterialVariable(const Chunk* datavar)
  {
    ChunkReader r(datavar);
    m_name = r.read<string>();
    m_data_type = r.read<uint32_t>();
    m_value = r.tell();
  }

  void MaterialVariable::apply(C6::D3::Device1& d3, uint32_t pass)
  {
  }

  MaterialVariable::~MaterialVariable()
  {
  }

  namespace
  {
    class TextureVariable : public MaterialVariable
    {
    public:
      TextureVariable(const Chunk* datavar, ModelLoadContext& ctx, Effect* fx)
        : MaterialVariable(datavar)
      {
        auto slot_info = fx->getPrimaryTechnique().getPass(0)->findTexture(getName());
        if(slot_info == nullptr)
          throw runtime_error("Missing input slot binding for texture " + getName() + ".");
        m_slot = slot_info->slot;
        auto len = *reinterpret_cast<const uint32_t*>(m_value);
        auto str = reinterpret_cast<const char*>(m_value + 4);
        auto path = string(str, str + len - 1) + ".rgt";
        m_srv = ctx.textures.load(path);
      }

      void apply(C6::D3::Device1& d3, uint32_t pass) override
      {
        d3.PSSetShaderResources(m_slot, m_srv);
      }

    private:
      ShaderResourceView m_srv;
      unsigned int m_slot;
    };
  }

  Material::Material(const Chunk* foldmtrl, ModelLoadContext& ctx)
  {
    {
      auto datainfo = foldmtrl->findFirst("DATAINFO v1");
      if(!datainfo)
        throw runtime_error("Material missing shader name");
      ChunkReader r(datainfo);
      m_effect = &ctx.shaders.load(r.read<string>());
    }
    {
      auto vars = foldmtrl->findAll("DATAVAR v1");
      m_material_variables.reserve(vars.size());
      transform(vars.begin(), vars.end(), back_inserter(m_material_variables), [&](const Chunk* chunk) -> MaterialVariable*
      {
        ChunkReader r(chunk);
        r.seek(r.read<uint32_t>());
        auto data_type = r.read<uint32_t>();
        switch(data_type)
        {
        case 9: return ctx.arena.alloc<TextureVariable>(chunk, ctx, m_effect);
        default: return ctx.arena.alloc<MaterialVariable>(chunk);
        }
      });
    }
  }

  Technique& Material::apply(C6::D3::Device1& d3)
  {
    m_effect->apply(d3);
    auto& tech = m_effect->getPrimaryTechnique();
    return tech;
  }

  void Material::applyVariablesForPass(C6::D3::Device1& d3, uint32_t pass)
  {
    for_each(m_material_variables.begin(), m_material_variables.end(), [&](MaterialVariable* variable)
    {
      variable->apply(d3, pass);
    });
  }

  Object::Object(ChunkReader& r, ModelLoadContext& ctx)
  {
    m_index_count = r.read<uint32_t>();
    
    D3D10_BUFFER_DESC ib;
    ib.ByteWidth = m_index_count * 2;
    ib.Usage = D3D10_USAGE_IMMUTABLE;
    ib.BindFlags = D3D10_BIND_INDEX_BUFFER;
    ib.CPUAccessFlags = 0;
    ib.MiscFlags = 0;
    D3D10_SUBRESOURCE_DATA ib_data = {r.tell()};
    r.seek(ib.ByteWidth);
    m_indicies = ctx.d3.createBuffer(ib, ib_data);

    r.seek(sizeof(float) * 3 + 1);
    m_name = r.read<string>();
    SetDebugObjectName(m_indicies, m_name);
  }

  static D3D10_INPUT_ELEMENT_DESC ReadInputLayoutElement(ChunkReader& r, UINT& offset)
  {
    D3D10_INPUT_ELEMENT_DESC result;
    result.SemanticIndex = 0;
    auto semantic_code = r.read<uint32_t>();
    switch(semantic_code)
    {
    case 0: result.SemanticName = "POSITION"; break;
    case 1: result.SemanticName = "BLENDINDICES"; break;
    case 2: result.SemanticName = "BLENDWEIGHT"; break;
    case 3: result.SemanticName = "NORMAL"; break;
    case 4: result.SemanticName = "BINORMAL"; break;
    case 5: result.SemanticName = "TANGENT"; break;
    case 6: result.SemanticName = "COLOR"; break;
    case 8: result.SemanticName = "TEXCOORD"; break;
    case 9: result.SemanticName = "TEXCOORD"; result.SemanticIndex = 1; break;
    case 14: result.SemanticName = "TEXCOORD"; result.SemanticIndex = 9; break;
    default: throw runtime_error("Unrecognised input layout element semantic.");
    }

    r.seek(4);

    result.AlignedByteOffset = offset;
    switch(r.read<uint32_t>())
    {
    case 2: result.Format = DXGI_FORMAT_B8G8R8A8_UNORM; offset += 4; break;
    case 3: result.Format = DXGI_FORMAT_R32G32_FLOAT; offset += 8; break;
    case 4: result.Format = DXGI_FORMAT_R32G32B32_FLOAT; offset += 12; break;
    case 13: result.Format = DXGI_FORMAT_R8G8B8A8_UINT; offset += 4; break;
    default: throw runtime_error("Unrecognised input layout element data type.");
    }

    result.InputSlot = 0;
    result.InputSlotClass = D3D10_INPUT_PER_VERTEX_DATA;
    result.InstanceDataStepRate = 0;

    return result;
  }

  static const bounding_volume_t NullBoundingVolume = {};

  Mesh::Mesh(const Chunk* foldmesh, ModelLoadContext& ctx)
    : m_bvol(&NullBoundingVolume)
  {
    auto foldmrgm = foldmesh->findFirst("FOLDMRGM");
    auto datadata = foldmrgm->findFirst("DATADATA v8");
    if(!datadata)
      throw runtime_error("Mesh missing data");

    {
      ChunkReader r(datadata);
      r.seek(1);
      auto num_objects = r.read<uint32_t>();
      m_objects.reserve(num_objects);
      for(uint32_t i = 0; i < num_objects; ++i)
        m_objects.push_back(ctx.arena.alloc<Object>(r, ctx));

      auto num_input_layout_elements = r.read<uint32_t>();
      vector<D3D10_INPUT_ELEMENT_DESC> input_layout;
      input_layout.reserve(num_input_layout_elements);
      UINT element_offset = 0;
      for(uint32_t i = 0; i < num_input_layout_elements; ++i)
        input_layout.push_back(ReadInputLayoutElement(r, element_offset));

      auto num_verticies = r.read<uint32_t>();
      m_vertex_stride = r.read<uint32_t>();
      if(element_offset != m_vertex_stride)
        throw runtime_error("Computed vertex stride differs from actual vertex stride");
      D3D10_BUFFER_DESC vb;
      vb.ByteWidth = num_verticies * m_vertex_stride;
      vb.Usage = D3D10_USAGE_IMMUTABLE;
      vb.BindFlags = D3D10_BIND_VERTEX_BUFFER;
      vb.CPUAccessFlags = 0;
      vb.MiscFlags = 0;
      D3D10_SUBRESOURCE_DATA vb_data;
      vb_data.pSysMem = r.tell();
      r.seek(vb.ByteWidth);
      m_verticies = ctx.d3.createBuffer(vb, vb_data);
      SetDebugObjectName(m_verticies, foldmesh->getName() + " verticies");

      r.seek(4);

      {
        auto material_name = r.read<string>();
        auto material = ctx.materials.find(material_name);
        if(material == ctx.materials.end())
          throw runtime_error("Missing materal: " + material_name);
        m_material = material->second;
        auto pass0 = m_material->getEffect().getPrimaryTechnique().getPass(0);
        m_input_layout = ctx.d3.createInputLayout(input_layout, pass0->getInputSignature());
      }
    }

    auto databvol = foldmrgm->findFirst("DATABVOL v2");
    if(databvol && databvol->getSize() >= 61)
      m_bvol = reinterpret_cast<const bounding_volume_t*>(databvol->getContents() + 1);
  }

  const bounding_volume_t& Mesh::getBoundingVolume() const
  {
    return *m_bvol;
  }

  Model::Model(FileSource* mod_fs, ShaderDatabase* shaders, unique_ptr<MappableFile> rgm_file, Device1& d3)
    : m_shaders(shaders)
  {
    m_file = ChunkyFile::Open(move(rgm_file));
    if(!m_file)
      throw runtime_error("Expected a chunky file.");
    auto modl = m_file->findFirst("FOLDMODL");
    auto mtrls = modl->findAll("FOLDMTRL v1");
    auto mgrp = modl->findFirst("FOLDMESH")->findFirst("FOLDMGRP");
    if(!mgrp)
      throw runtime_error("Chunky file doesn't contain a model.");
    auto meshes = mgrp->findAll("FOLDMESH v3");

    TextureCache textures(mod_fs, d3);
    ModelLoadContext ctx = {m_arena, d3, *m_shaders, textures};

    for_each(mtrls.begin(), mtrls.end(), [&](const Chunk* foldmtrl)
    {
      auto name = foldmtrl->getName();
      name.resize(name.size() - 1);
      ctx.materials[name] = m_arena.alloc<Material>(foldmtrl, ctx);
    });

    for_each(meshes.begin(), meshes.end(), [&](const Chunk* foldmesh)
    {
      if(strstr(foldmesh->getName().c_str(), "wreck") == nullptr)
        m_meshes.push_back(m_arena.alloc<Mesh>(foldmesh, ctx));
    });
  }

  void Model::render(Device1& d3)
  {
    for_each(m_meshes.begin(), m_meshes.end(), [&](Mesh* mesh)
    {
      mesh->render(d3);
    });
  }

  void Mesh::render(Device1& d3)
  {
    const unsigned int vertex_offset = 0;
    d3.IASetVertexBuffers(0, m_verticies, m_vertex_stride, vertex_offset);
    d3.IASetInputLayout(m_input_layout);
    d3.IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    auto tech = m_material->apply(d3);
    const auto num_passes = tech.getNumPasses();
    for(unsigned int i = 0; i < num_passes; ++i)
    {
      tech.getPass(i)->apply(d3);
      m_material->applyVariablesForPass(d3, i);
      for_each(m_objects.begin(), m_objects.end(), [&](Object* object)
      {
        object->render(d3);
      });
    }
  }

  void Object::render(Device1& d3)
  {
    d3.IASetIndexBuffer(m_indicies, DXGI_FORMAT_R16_UINT, 0);
    d3.drawIndexed(m_index_count, 0, 0);
  }

} }
