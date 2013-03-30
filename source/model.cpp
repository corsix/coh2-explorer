#include "stdafx.h"
#include "model.h"
#include "shader_db.h"
#include "texture_loader.h"
#include "chunky.h"
#include "hash.h"
#include "containers.h"
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

  struct ConditionLoadContext
  {
    Arena& arena;
    map<string, Condition::Clause**> dependent_clause_head;
    multimap<string, bool*> objects;
  };

  namespace
  {
    bool operator ==(const ChunkyString& lhs, const ChunkyString& rhs)
    {
      return lhs.size() == rhs.size() && !memcmp(lhs.data(), rhs.data(), rhs.size());
    }

    struct EqualityClause : Condition::Clause
    {
      EqualityClause(const ChunkyString* to_match)
      {
        evaluate = &EqualityClause::_evaluate;
        this->to_match = to_match;
      }

      static bool _evaluate(const Clause* self, const ChunkyString* property_value)
      {
        return *static_cast<const EqualityClause*>(self)->to_match == *property_value;
      }

      const ChunkyString* to_match;
    };

    struct RangeClause : Condition::Clause
    {
      RangeClause(const float* bounds)
        : lower_bound(bounds[0])
        , upper_bound(bounds[1])
      {
        evaluate = &RangeClause::_evaluate;
      }

      static bool _evaluate(const Clause* self_, const ChunkyString* property_value_)
      {
        runtime_assert(property_value_->size() == sizeof(float), "RangeClause applied to non-float property.");
        auto property_value = *reinterpret_cast<const float*>(property_value_->data());
        auto self = static_cast<const RangeClause*>(self_);
        return self->lower_bound <= property_value && property_value < self->upper_bound;
      }

      float lower_bound;
      float upper_bound;
    };

    template <typename Functor>
    Condition::Clause* MakeFloatTest(Arena& arena, Functor&& f)
    {
      struct Test : Condition::Clause
      {
        Test(Functor&& f)
          : m_f(std::move(f))
        {
          evaluate = &Test::_evaluate;
        }

        static bool _evaluate(const Clause* self_, const ChunkyString* property_value_)
        {
          runtime_assert(property_value_->size() == sizeof(float), "FloatTest applied to non-float property.");
          auto property_value = *reinterpret_cast<const float*>(property_value_->data());
          auto self = static_cast<const Test*>(self_);
          return self->m_f(property_value);
        }

      private:
        Functor m_f;
      };

      return arena.allocTrivial<Test>(std::move(f));
    }
  }

  namespace
  {
    const uint8_t BooleanPropertyValues[] = {5, 0, 0, 0, 'f', 'a', 'l', 's', 'e', 4, 0, 0, 0, 't', 'r', 'u', 'e'};
    const ChunkyString* BoxFloat(Arena& arena, float value)
    {
      auto result = reinterpret_cast<ChunkyString*>(arena.mallocArray<char>(sizeof(ChunkyString) + sizeof(float)));
      result->size() = sizeof(float);
      *reinterpret_cast<float*>(result->data()) = value;
      return result;
    }
  }

  Condition::Condition(ConditionLoadContext& ctx, const Chunk* datacnbp)
    : m_clauses(&ctx.arena, 0)
    , m_truth_counter(0)
    , m_listener(nullptr)
  {
    ChunkReader r(datacnbp);
    auto num_clauses = r.read<uint32_t>();
    if(num_clauses == 0)
      return;

    for(auto& clause : m_clauses.recreate(&ctx.arena, num_clauses))
    {
      auto test_type = r.read<uint32_t>();
      auto is_negated = r.read<uint8_t>();
      auto property_name = r.readString()->as<string>();
      auto list = ctx.dependent_clause_head[property_name];
      if(list == nullptr)
        throw runtime_error("DATACNBP refers to unbound variable `" + property_name + "'.");
      switch(test_type)
      {
      case 1: clause = ctx.arena.allocTrivial<EqualityClause>(reinterpret_cast<const ChunkyString*>(BooleanPropertyValues + 9)); break;
      case 2: clause = ctx.arena.allocTrivial<EqualityClause>(r.readString()); break;
      case 3: {
        auto reference_value = r.read<float>();
        auto comparison = r.read<uint32_t>();
        float equality_range[2] = {reference_value - .001f, reference_value + .001f};
        switch(comparison)
        {
        case 0: clause = ctx.arena.allocTrivial<RangeClause>(equality_range); break;
        case 1: clause = ctx.arena.allocTrivial<RangeClause>(equality_range); is_negated ^= 1; break;
        case 2: clause = MakeFloatTest(ctx.arena, [=](float x) { return x <  reference_value; }); break;
        case 3: clause = MakeFloatTest(ctx.arena, [=](float x) { return x >  reference_value; }); break;
        case 4: clause = MakeFloatTest(ctx.arena, [=](float x) { return x <= reference_value; }); break;
        case 5: clause = MakeFloatTest(ctx.arena, [=](float x) { return x >= reference_value; }); break;
        default: throw runtime_error("Unknown condition clause float comparison function.");
        }
        break; }
      case 4: clause = ctx.arena.allocTrivial<RangeClause>(r.reinterpret<float>(2)); break;
      default:
        throw runtime_error("Unknown condition clause type.");
      }
      clause->next_dependent_clause = *list;
      *list = clause;
      clause->parent = this;
      clause->satisfied = 0;
      clause->is_negated = is_negated;
      clause->weight = 1;
    }

    uint32_t num_ands = 0;
    uint32_t num_ors = 0;
    auto unknowns = r.reinterpret<uint32_t>(num_clauses);
    for(uint32_t i = 0; i < num_clauses; ++i)
    {
      switch(unknowns[i])
      {
      case  0: ++num_ands; break;
      case  1: ++num_ors;  break;
      default: throw runtime_error("Unsupported value in condition clause unknown array.");
      }
    }
    uint16_t and_weight = static_cast<uint16_t>(num_ors + 1);
    m_truth_counter = -(static_cast<int32_t>(and_weight) * static_cast<int32_t>(num_ands));
    if(num_ors != 0)
      --m_truth_counter;
    for(uint32_t i = 0; i < num_clauses; ++i)
    {
      auto clause = m_clauses[i];
      if(unknowns[i] == 0)
        clause->weight = and_weight;
      if(clause->is_negated)
        m_truth_counter += clause->weight;
    }
  }

  void Condition::addListener(ConditionListener* listener)
  {
    if(m_listener != nullptr)
      throw logic_error("YAGNI: More than one listener on a condition");
    m_listener = listener;
  }

  void Condition::fireStateChanged()
  {
    if(m_listener)
      m_listener->onConditionStateChanged();
  }

  Property::Property(const Chunk* datavar)
  {
    ChunkReader r(datavar);
    m_name = r.readString();
    m_data_type = r.read<PropertyDataType::E>();
    m_value = r.readString();
  }

  Property::Property(const ChunkyString* name, PropertyDataType::E data_type)
    : m_name(name)
    , m_data_type(data_type)
  {
  }

  ModelVariable::ModelVariable(Arena& arena, ChunkReader& datadtbp, PropertyDataType::E data_type)
    : Property(datadtbp.readString(), data_type == PropertyDataType::boolean ? PropertyDataType::string : data_type)
    , m_values(&arena, 0)
    , m_first_dependent_clause(nullptr)
  {
    switch(data_type)
    {
    case PropertyDataType::boolean:
      m_values.recreate(&arena, 2);
      m_values[0] = reinterpret_cast<const ChunkyString*>(BooleanPropertyValues + 0);
      m_values[1] = reinterpret_cast<const ChunkyString*>(BooleanPropertyValues + 9);
      m_value = m_default_value = m_values[0];
      break;

    case PropertyDataType::float1: {
      auto def = datadtbp.read<float>();
      m_value = BoxFloat(arena, def);
      m_default_value = BoxFloat(arena, def);
      m_values.recreate(&arena, 2);
      m_values[0] = BoxFloat(arena, datadtbp.read<float>());
      m_values[1] = BoxFloat(arena, datadtbp.read<float>());
      break; }

    case PropertyDataType::string:
      for(auto& value : m_values.recreate(&arena, datadtbp.read<uint32_t>()))
      {
        value = datadtbp.readString();
      }
      m_value = datadtbp.readString();
      for(auto value : m_values)
      {
        if(*value == *m_value)
        {
          m_value = value;
          break;
        }
      }
      m_default_value = m_value;
      break;

    default:
      throw logic_error("Unsupported property blueprint data type");
    }
  }

  bool ModelVariable::affectsAnything() const
  {
    return m_first_dependent_clause != nullptr;
  }

  void ModelVariable::setValue(const ChunkyString* new_value)
  {
    m_value = new_value;
    for(auto clause = m_first_dependent_clause; clause; clause = clause->next_dependent_clause)
    {
      uint32_t inverted = clause->satisfied ^ 1;
      if(static_cast<uint32_t>(clause->evaluate(clause, new_value)) == inverted)
      {
        auto condition = clause->parent;
        bool was_true = condition->isTrue();
        clause->satisfied = static_cast<uint8_t>(inverted);
        condition->m_truth_counter += ((inverted ^ clause->is_negated) * 2 - 1) * static_cast<int32_t>(clause->weight);
        if(was_true != condition->isTrue())
          condition->fireStateChanged();
      }
    }
  }

  void ModelVariable::setValue(float new_value)
  {
    runtime_assert(m_value->size() == sizeof(float), "setValue(float) can only be called on float variables.");
    *reinterpret_cast<float*>(const_cast<char*>(m_value->data())) = new_value;
    setValue(m_value);
  }

  MaterialVariable::MaterialVariable(const Chunk* datavar)
    : Property(datavar)
  {
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

  void Model::defineVariables(const Chunk* datadtbp)
  {
    ChunkReader r(datadtbp);
    static const PropertyDataType::E data_types[] = {PropertyDataType::boolean, PropertyDataType::string, PropertyDataType::float1};
    for(auto data_type : data_types)
    {
      auto num = r.read<uint32_t>();
      while(num --> 0)
      {
        auto variable = m_arena.allocTrivial<ModelVariable>(m_arena, r, data_type);
        auto& existing = m_variables[variable->getName().as<string>()];
        if(existing != nullptr && variable->getType() != existing->getType())
          throw runtime_error("Model variable `" + variable->getName().as<string>() + "' redefined with different type.");
        existing = variable;
      }
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

      if(auto foldmesh = modl->findFirst("FOLDMESH"))
      {
        for(auto foldmtrl : modl->findAll("FOLDMTRL v1"))
        {
          auto name = foldmtrl->getName();
          name.resize(name.size() - 1);
          ctx.materials[name] = m_arena.allocTrivial<Material>(foldmtrl, ctx);
        }
        loadMeshes(foldmesh, ctx);
        ctx.materials.clear();
      }
      if(auto datadtbp = modl->findFirst("DATADTBP v3"))
      {
        defineVariables(datadtbp);
      }
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

  namespace
  {
    class ObjectVisibilityBinding : public ConditionListener
    {
    public:
      ObjectVisibilityBinding(ConditionLoadContext& ctx, const Chunk* datamsd, Condition* condition, ConditionListener* downstream)
        : m_condition(condition)
        , m_objects(&ctx.arena, 0)
        , m_downstream(downstream)
      {
        uint32_t num_states = 0;
        {
          ChunkReader r(datamsd);
          r.readString();
        
          auto num_objects = r.read<uint32_t>();
          while(num_objects --> 0)
          {
            auto name = r.readString();
            auto count = ctx.objects.count(name->as<string>());
            if(count == 0)
              throw runtime_error("DATAMSD references non-existent object `" + name->as<string>() + "'");
            num_states += static_cast<uint32_t>(count);
          }
        }
        {
          ChunkReader r(datamsd);
          r.readString();
          r.read<uint32_t>();

          bool state = m_condition->isTrue();
          for(auto ptr = m_objects.recreate(&ctx.arena, num_states).begin(), end = m_objects.end(); ptr != end; )
          {
            auto name = r.readString();
            for(auto& object : ctx.objects.equal_range(name->as<string>()))
            {
              *object.second = state;
              *ptr++ = object.second;
            }
          }
        }
        condition->addListener(this);
      }

      void onConditionStateChanged() override
      {
        bool state = m_condition->isTrue();
        for(auto ptr : m_objects)
          *ptr = state;
        if(m_downstream)
          m_downstream->onConditionStateChanged();
      }

    private:
      Condition* m_condition;
      ArenaArray<bool*> m_objects;
      ConditionListener* m_downstream;
    };
  }

  void Model::bindVariablesToObjectVisibility(bool* object_visibility, ConditionListener* listener)
  {
    ConditionLoadContext ctx = {m_arena};
    for(auto variable : m_variables)
      ctx.dependent_clause_head[variable.first] = &variable.second->m_first_dependent_clause;

    for(auto object : getObjects())
    {
      string name(object->getName()->begin(), find(object->getName()->begin(), object->getName()->end(), ':'));
      for(auto& c : name)
        c = static_cast<char>(tolower(c));
      
      ctx.objects.insert(make_pair(move(name), object_visibility++));
    }
    
    for(auto& file : m_files)
    {
      if(auto foldmsbp = file->findFirst("FOLDMODL")->findFirst("FOLDMSBP v1"))
      {
        auto msds = foldmsbp->findAll("DATAMSD? v2");
        auto cnbps = foldmsbp->findAll("DATACNBP");
        auto count = (min)(msds.size(), cnbps.size());
        runtime_assert(cnbps.size() == msds.size(), "Mismatch between number of DATAMSDs and number of DATACNBPs");
        for(size_t i = 0; i < count; ++i)
        {
          auto condition = m_arena.allocTrivial<Condition>(ctx, cnbps[i]);
          m_arena.allocTrivial<ObjectVisibilityBinding>(ctx, msds[i], condition, listener);
        }
      }
    }

    for(auto variable : m_variables)
      variable.second->setValue(variable.second->getValue());
  }

} }
