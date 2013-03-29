#include "stdafx.h"
#include "model.h"
#include "shader_db.h"
#include "texture_loader.h"
#include "chunky.h"
#include "hash.h"
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
    m_name = r.readString();
    m_data_type = r.read<uint32_t>();
    m_value = r.readString();
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
          throw runtime_error("Missing input slot binding for texture " + getName().as<string>() + ".");
        m_slot = slot_info->slot;
        auto path = string(m_value->begin(), m_value->end() - 1) + ".rgt";
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

  template <typename TA, typename TV, typename F>
  static void transform(Arena& arena, ArenaArray<TA>& arr_out, const vector<TV>& vec_in, F&& functor)
  {
    const auto size = vec_in.size();
    arr_out.recreate(&arena, static_cast<uint32_t>(size));
    for(size_t i = 0; i < size; ++i)
      arr_out[i] = functor(vec_in[i]);
  }

  Material::Material(const Chunk* foldmtrl, ModelLoadContext& ctx)
    : m_material_variables(&ctx.arena, 0)
  {
    {
      auto datainfo = foldmtrl->findFirst("DATAINFO v1");
      if(!datainfo)
        throw runtime_error("Material missing shader name");
      ChunkReader r(datainfo);
      m_effect = &ctx.shaders.load(r.readString());
    }
    {
      auto vars = foldmtrl->findAll("DATAVAR v1");
      transform(ctx.arena, m_material_variables, vars, [&](const Chunk* chunk) -> MaterialVariable*
      {
        ChunkReader r(chunk);
        r.readString();
        auto data_type = r.read<uint32_t>();
        switch(data_type)
        {
        case 9: return ctx.arena.alloc<TextureVariable>(chunk, ctx, m_effect);
        default: return ctx.arena.allocTrivial<MaterialVariable>(chunk);
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

  Object::Object(ChunkReader& r, ModelLoadContext& ctx, unsigned int& first_index)
  {
    m_index_count = r.read<uint32_t>();

    m_first_index = first_index;
    first_index += m_index_count;
    r.seek(2 * m_index_count);

    r.seek(sizeof(float) * 3 + 1);
    m_name = r.readString();
  }

  Object::Object(const ChunkyString* name, unsigned int index_count)
    : m_name(name)
    , m_index_count(index_count)
    , m_first_index(0)
  {
  }

  void Object::readIndices(ChunkReader& r, uint16_t*& destination)
  {
    auto index_count = r.read<uint32_t>();
    auto source = r.tell();
    auto size = 2 * index_count;
    r.seek(size);
    r.seek(sizeof(float) * 3 + 1);
    r.readString();
    memcpy(destination, source, size);
    destination += index_count;
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
    case 10: result.SemanticName = "TEXCOORD"; result.SemanticIndex = 2; break;
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
    case 5: result.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; offset += 16; break;
    case 13: result.Format = DXGI_FORMAT_R8G8B8A8_UINT; offset += 4; break;
    default: throw runtime_error("Unrecognised input layout element data type.");
    }

    result.InputSlot = 0;
    result.InputSlotClass = D3D10_INPUT_PER_VERTEX_DATA;
    result.InstanceDataStepRate = 0;

    return result;
  }

  void Mesh::loadDataData5(const Chunk* datadata, ModelLoadContext& ctx)
  {
    ChunkReader r(datadata);

    auto input_layout = loadVertexData(r, ctx.d3);
    r.seek(12);
    loadIndicesAsObject(r, ctx);
    loadMaterial(r, ctx, move(input_layout));
    r.seek(12);
  }

  void Mesh::loadIndicesAsObject(ChunkReader& r, ModelLoadContext& ctx)
  {
    auto index_count = r.read<uint32_t>();

    D3D10_BUFFER_DESC ib;
    ib.ByteWidth = index_count * 2;
    ib.Usage = D3D10_USAGE_IMMUTABLE;
    ib.BindFlags = D3D10_BIND_INDEX_BUFFER;
    ib.CPUAccessFlags = 0;
    ib.MiscFlags = 0;
    D3D10_SUBRESOURCE_DATA contents = {r.tell()};
    r.seek(ib.ByteWidth);
    m_indices = ctx.d3.createBuffer(ib, contents);

    auto name = reinterpret_cast<ChunkyString*>(ctx.arena.mallocArray<char>(sizeof(ChunkyString) + m_name.size() + 1));
    copy(m_name.begin(), m_name.end(), name->begin());

    m_objects.recreate(&ctx.arena, 1)[0] = ctx.arena.allocTrivial<Object>(name, index_count);
  }

  void Mesh::loadDataData8(const Chunk* datadata, ModelLoadContext& ctx)
  {
    ChunkReader r(datadata);
    r.seek(1);
    loadObjects(r, ctx);
    auto input_layout = loadVertexData(r, ctx.d3);
    r.seek(4);
    loadMaterial(r, ctx, move(input_layout));
    // TODO: loadBones
    r.seek(5);
  }

  void Mesh::loadObjects(ChunkReader& r, ModelLoadContext& ctx)
  {
    unsigned int index_count = 0;
    auto num_objects = r.read<uint32_t>();
    m_objects.recreate(&ctx.arena, num_objects);

    auto start = r.tell();
    for(uint32_t i = 0; i < num_objects; ++i)
      m_objects[i] = ctx.arena.allocTrivial<Object>(r, ctx, index_count);

    D3D10_BUFFER_DESC ib;
    ib.ByteWidth = index_count * 2;
    ib.Usage = D3D10_USAGE_DYNAMIC;
    ib.BindFlags = D3D10_BIND_INDEX_BUFFER;
    ib.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    ib.MiscFlags = 0;
    m_indices = ctx.d3.createBuffer(ib);

    auto mapped = reinterpret_cast<uint16_t*>(m_indices.map(D3D10_MAP_WRITE_DISCARD, 0));
    r.seek(start);
    for(uint32_t i = 0; i < num_objects; ++i)
      Object::readIndices(r, mapped);
    m_indices.unmap();
  }

  auto Mesh::loadVertexData(ChunkReader& r, C6::D3::Device1& d3) -> std::vector<D3D10_INPUT_ELEMENT_DESC>
  {
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
    m_verticies = d3.createBuffer(vb, vb_data);
    SetDebugObjectName(m_verticies, m_name + " verticies");

    return input_layout;
  }

  void Mesh::loadMaterial(ChunkReader& r, ModelLoadContext& ctx, std::vector<D3D10_INPUT_ELEMENT_DESC> input_layout)
  {
    auto material_name = r.readString()->as<string>();
    auto material = ctx.materials.find(material_name);
    if(material == ctx.materials.end())
      throw runtime_error("Missing materal: " + material_name);
    m_material = material->second;
    auto pass0 = m_material->getEffect().getPrimaryTechnique().getPass(0);
    m_input_layout = ctx.d3.createInputLayout(input_layout, pass0->getInputSignature());
  }

  static const bounding_volume_t NullBoundingVolume = {};

  Mesh::Mesh(const Chunk* foldmrgm, ModelLoadContext& ctx)
    : m_bvol(&NullBoundingVolume)
    , m_objects(&ctx.arena, 0)
  {
    m_name = foldmrgm->getName();
    auto datadata = foldmrgm->findFirst("DATADATA");
    if(!datadata)
      throw runtime_error("Mesh missing data");

    switch(datadata->getVersion())
    {
    case 5: loadDataData5(datadata, ctx); break;
    case 8: loadDataData8(datadata, ctx); break;
    default: throw runtime_error("Unsupported DATADATA version in model file.");
    }
    SetDebugObjectName(m_indices, m_name + " indices");

    auto databvol = foldmrgm->findFirst("DATABVOL v2");
    if(databvol && databvol->getSize() >= 61)
      m_bvol = reinterpret_cast<const bounding_volume_t*>(databvol->getContents() + 1);
  }

  const bounding_volume_t& Mesh::getBoundingVolume() const
  {
    return *m_bvol;
  }

  void Model::loadMeshes(const Chunk* foldmesh, ModelLoadContext& ctx)
  {
    if(auto mgrp = foldmesh->findFirst("FOLDMGRP"))
    {
      for(auto child : mgrp->findAll("FOLDMESH"))
        loadMeshes(child, ctx);
    }
    else if(auto mrgm = foldmesh->findFirst("FOLDMRGM"))
    {
      m_meshes.push_back(m_arena.alloc<Mesh>(mrgm, ctx));
    }
    else if(auto trim = foldmesh->findFirst("FOLDTRIM"))
    {
      m_meshes.push_back(m_arena.alloc<Mesh>(trim, ctx));
    }
    else
    {
      throw runtime_error("Unknown FOLDMESH variant.");
    }
  }

  Model::Model(FileSource* mod_fs, ShaderDatabase* shaders, std::vector<std::unique_ptr<const ChunkyFile>> files, Device1& d3)
    : m_shaders(shaders)
    , m_files(move(files))
  {
    TextureCache textures(mod_fs, d3);
    ModelLoadContext ctx = {m_arena, d3, *m_shaders, textures};

    for(auto& file : m_files)
    {
      auto modl = file->findFirst("FOLDMODL");

      auto root = modl->findFirst("FOLDMESH");
      if(!root)
        continue;

      for(auto foldmtrl : modl->findAll("FOLDMTRL v1"))
      {
        auto name = foldmtrl->getName();
        name.resize(name.size() - 1);
        ctx.materials[name] = m_arena.allocTrivial<Material>(foldmtrl, ctx);
      }

      loadMeshes(root, ctx);
      ctx.materials.clear();
    }

    if(m_meshes.empty())
      throw runtime_error(m_files.size() == 1 ? "Chunky file doesn't contain a model." : "Chunky files don't contain a model.");
  }

  Model::~Model()
  {
  }

  void Model::render(Device1& d3, const bool* object_visibility)
  {
    for(auto mesh : m_meshes)
    {
      object_visibility = mesh->render(d3, object_visibility);
    }
  }

  const bool* Mesh::render(Device1& d3, const bool* object_visibility)
  {
    const unsigned int vertex_offset = 0;
    d3.IASetIndexBuffer(m_indices, DXGI_FORMAT_R16_UINT, 0);
    d3.IASetVertexBuffers(0, m_verticies, m_vertex_stride, vertex_offset);
    d3.IASetInputLayout(m_input_layout);
    d3.IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    auto tech = m_material->apply(d3);
    const auto num_passes = tech.getNumPasses();
    for(unsigned int pass = 0; pass < num_passes; ++pass)
    {
      tech.getPass(pass)->apply(d3);
      m_material->applyVariablesForPass(d3, pass);
      int index = 0;
      for(auto object : m_objects)
      {
        if(object_visibility && !object_visibility[index++])
          continue;

        object->render(d3);
      }
    }
    return object_visibility ? (object_visibility + m_objects.size()) : nullptr;
  }

  void Object::render(Device1& d3)
  {
    d3.drawIndexed(m_index_count, m_first_index, 0);
  }

  vector<Object*> Model::getObjects()
  {
    vector<Object*> result;
    for(auto mesh : m_meshes)
    {
      for(auto object : mesh->getObjects())
        result.push_back(object);
    }
    return result;
  }

} }
