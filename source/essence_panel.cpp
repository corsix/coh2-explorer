#include "essence_panel.h"
#include "shader_db.h"

namespace
{
  struct Identity43
  {
    Identity43()
    {
      _[0][0] = 1.f; _[1][0] = 0.f; _[2][0] = 0.f; // _[3][0] = 0.f;
      _[0][1] = 0.f; _[1][1] = 1.f; _[2][1] = 0.f; // _[3][1] = 0.f;
      _[0][2] = 0.f; _[1][2] = 0.f; _[2][2] = 1.f; // _[3][2] = 0.f;
      _[0][3] = 0.f; _[1][3] = 0.f; _[2][3] = 0.f; // _[3][3] = 1.f;
    }
    float _[3][4];

  };

  std::tuple<C6::D3::ShaderResourceView, C6::D3::ShaderResourceView> CreateConstantTexture(C6::D3::Device& d3, float val)
  {
    D3D10_TEXTURE2D_DESC desc;
    desc.Width = 1;
    desc.Height = 1;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D10_USAGE_IMMUTABLE;
    desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    float pixels[] = {val, val, val, 1.f};
    D3D10_SUBRESOURCE_DATA contents = {static_cast<void*>(pixels), 16};
    auto tex = d3.createShaderResourceView(d3.createTexture2D(desc, contents));

    desc.ArraySize = 6;
    desc.MiscFlags = D3D10_RESOURCE_MISC_TEXTURECUBE;
    D3D10_SUBRESOURCE_DATA faces[6] = {contents, contents, contents, contents, contents, contents};
    auto cube = d3.createShaderResourceView(d3.createTexture2D(desc, faces));
  
    return std::make_tuple(std::move(tex), std::move(cube));
  }

  namespace TextureSlot
  {
    enum E : unsigned int
    {
      EnvMapDiffuse = 50,
      EnvMapSpecular = 51,
      FOWTexture = 57,
      lightscatter_Texture = 58,
      g_texLocalAO = 59,
      TerrainHeightTexture = 64,
      snowDiffuseTexture = 68,
      snowNormalTexture = 69,
      snowEdgeNoiseTexture = 70,
      compositorDiffuseTexture = 72,
      compositorSpecularTexture = 73
    };
  }
}

namespace Essence { namespace Graphics
{

  Panel::Panel(wxWindow *parent, wxWindowID winid, FileSource* mod_fs)
    : Direct3DPanel(parent, winid)
  {
    auto device = getDevice();
    setDepthFormat(DXGI_FORMAT_D24_UNORM_S8_UINT);

    initWorldMatrixBuffer();
    initInstanceInfoBuffer();

    std::tie(m_black, m_black_cube) = CreateConstantTexture(device, 0.0f);
    std::tie(m_grey , m_grey_cube ) = CreateConstantTexture(device, 0.5f);
    std::tie(m_white, m_white_cube) = CreateConstantTexture(device, 1.0f);

    m_shaders = m_arena.alloc<ShaderDatabase>(&m_arena, mod_fs, device);
    initShaderVariables();

    Bind(wxEVT_SIZE, [=](wxSizeEvent& e) {
      auto size = e.GetSize();
      ***m_proj = static_cast<float>(size.GetHeight()) / static_cast<float>(size.GetWidth());
      m_shaders->variablesUpdated();
      e.Skip();
    });
  }

  void Panel::initWorldMatrixBuffer()
  {
    Identity43 world_matrices[2];

    D3D10_BUFFER_DESC desc;
    desc.ByteWidth = sizeof(world_matrices);
    desc.Usage = D3D10_USAGE_IMMUTABLE;
    desc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    D3D10_SUBRESOURCE_DATA contents = {static_cast<void*>(&world_matrices)};
    m_world_matrix_buffer = getDevice().createBuffer(desc, contents);

    SetDebugObjectName(m_world_matrix_buffer, "World matrix buffer");
  }

  void Panel::initInstanceInfoBuffer()
  {
    uint32_t info[4] = {0};

    D3D10_BUFFER_DESC desc;
    desc.ByteWidth = sizeof(info);
    desc.Usage = D3D10_USAGE_IMMUTABLE;
    desc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    D3D10_SUBRESOURCE_DATA contents = {static_cast<void*>(&info)};
    m_instance_info_buffer = getDevice().createBuffer(desc, contents);

    SetDebugObjectName(m_instance_info_buffer, "Instance info buffer");
  }

  static void Normalise(vector3_t& v)
  {
    float len = sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    v.x /= len, v.y /= len, v.z /= len;
  }

  static void BuildViewColumn(const vector3_t& v, float (&dest)[4], const vector3_t& eye)
  {
    *reinterpret_cast<vector3_t*>(dest) = v;
    dest[3] = -v.x*eye.x -v.y*eye.y -v.z*eye.z;
  }

  void Panel::lookAtFrom(vector3_t at, vector3_t eye)
  {
    vector3_t z_axis = {at.x - eye.x, at.y - eye.y, at.z - eye.z};
    Normalise(z_axis);
    vector3_t x_axis = {z_axis.z, 0.f, -z_axis.x};
    Normalise(x_axis);
    vector3_t y_axis = {z_axis.y*x_axis.z, z_axis.z*x_axis.x - z_axis.x*x_axis.z, -z_axis.y*x_axis.x};

    auto& view = *m_view;
    BuildViewColumn(x_axis, view[0], eye);
    BuildViewColumn(y_axis, view[1], eye);
    BuildViewColumn(z_axis, view[2], eye);
    *m_eye_position = eye;

    m_shaders->variablesUpdated();
  }

  void Panel::startRendering()
  {
    Direct3DPanel::startRendering();
    auto device = getDevice();

    device.VSSetConstantBuffers(2, m_world_matrix_buffer);
    device.VSSetConstantBuffers(3, m_instance_info_buffer);
    device.PSSetShaderResources(TextureSlot::EnvMapDiffuse, m_white_cube);
    device.PSSetShaderResources(TextureSlot::EnvMapSpecular, m_black_cube);
    device.PSSetShaderResources(TextureSlot::FOWTexture, m_black);
    device.PSSetShaderResources(TextureSlot::lightscatter_Texture, m_black);
    device.PSSetShaderResources(TextureSlot::g_texLocalAO, m_white);
    device.PSSetShaderResources(TextureSlot::TerrainHeightTexture, m_black);
    device.PSSetShaderResources(TextureSlot::snowDiffuseTexture, m_black);
    device.PSSetShaderResources(TextureSlot::snowNormalTexture, m_black);
    device.PSSetShaderResources(TextureSlot::snowEdgeNoiseTexture, m_black);
    device.PSSetShaderResources(TextureSlot::compositorDiffuseTexture, m_black);
    device.PSSetShaderResources(TextureSlot::compositorSpecularTexture, m_grey);
  }

  template <size_t N>
  inline void copy(float* dest, const float (&src)[N])
  {
    copy(dest, reinterpret_cast<const float(&)[N - 1]>(src));
    dest[N - 1] = src[N - 1];
  }

  template <>
  inline void copy(float* dest, const float (&src)[1])
  {
    dest[0] = src[0];
  }

  void Panel::initShaderVariables()
  {
#define set(name, ...) {const float values[] = {__VA_ARGS__}; copy(m_shaders->getVariable(name, sizeof(values)), values); }
#define var(T, name) *reinterpret_cast<T*>(m_shaders->getVariable(name, sizeof(T)))
    m_view = &var(matrix44_t, "view");
    (*m_view)[3][3] = 1.f;
    m_proj = &var(matrix44_t, "projection");
    {
      auto& _ = *m_proj;
      _[0][0] = 1.f;
                     _[1][1] = 1.f;
                                    _[2][2] =  1.f; _[3][2] = 1.f;
                                    _[2][3] = -1.f;
    }
    m_shaders->getVariable("invviewproj", 64);
    m_eye_position = &var(vector3_t, "eyeposition");
    set("dirlight0_dir", -0.577f, -0.577f, 0.577f);
    set("dirlight0_diffuse", 1.f, 1.f, 1.f, 1.f);
    set("pointlight0_position_attenstart", 0.f, 0.f, 0.f, 10000.f);
    set("pointlight0_position_atteninvrange", 1.f, 1.f, 1.f, 1.f);
    set("pointlight1_position_attenstart", 0.f, 0.f, 0.f, 10000.f);
    set("pointlight1_position_atteninvrange", 1.f, 1.f, 1.f, 1.f);
    set("pointlight2_position_attenstart", 0.f, 0.f, 0.f, 10000.f);
    set("pointlight2_position_atteninvrange", 1.f, 1.f, 1.f, 1.f);
    set("pointlight3_position_attenstart", 0.f, 0.f, 0.f, 10000.f);
    set("pointlight3_position_atteninvrange", 1.f, 1.f, 1.f, 1.f);
    m_shaders->getVariable("shlight", 112);
    set("ambientscale", 0.7f);
    set("ambientrotation", 1.f, 0.f, 0.f, 1.f);
    m_shaders->getVariable("unadjustedshadowviewmatrix", 64);
    m_shaders->getVariable("shadowvpmatrix", 64);
    m_shaders->getVariable("shadowvpmatrixnear", 64);
    m_shaders->getVariable("shadowvpmatrixfar", 64);
    m_shaders->getVariable("shadownearshift", 16);
    m_shaders->getVariable("shadownearscale", 16);
    m_shaders->getVariable("shadownearfarcutoff", 12);
    m_shaders->getVariable("shadowtextureresolution", 4);
    set("shadowbias", 1.f, 0.f);
    m_shaders->getVariable("shadowsampleoffsetpcf0", 8);
    m_shaders->getVariable("shadowsampleoffsetpcf1", 8);
    m_shaders->getVariable("shadowsampleoffsetpcf2", 8);
    m_shaders->getVariable("shadowsampleoffsetpcf3", 8);
    set("shadowcubedepthbias", 1.f);
    set("shadowcubenormalbias", 0.1f);
    m_shaders->getVariable("shadowcubecoords", 60);
    m_shaders->getVariable("shadowcubesizes", 52);
    var(int32_t, "currentshadowcube") = -1;
    m_shaders->getVariable("sun_cloud_matrix", 48);
    m_shaders->getVariable("sun_cloud_intensity", 4);
    set("fogconstant", 0.004f, -12500.f, 20.f, 1.5f);
    set("fog_light_direction", 1.f, 0.f, 0.f);
    m_shaders->getVariable("g_lao_parameters", 8);
    set("terrainsizeinv", 1.f, 1.f, 1.f, 1.f);
    set("playablesizeinv", 1.f, 1.f);
    set("chunkSize", 1.f, 1.f);
    m_shaders->getVariable("compositorviewprojection", 64);
    set("highlightcolour_diffuse", 1.f, 1.f, 1.f, 1.f);
    set("highlightcolour_specular", 1.f, 1.f, 1.f, 1.f);
    m_shaders->getVariable("simpledepthmatviewproj", 64);
    set("snowparameters", 0.f, 1.f, 0.f, 1.f);
    set("snowedgefadeparameters", 1.f, 1.f, 16.f, 4.f);
    set("snowdepthparameters", 0.4f, 1.f, 1.f, 1.f);
    set("snowsparkleparameters", 1.8f, 1.f);
    m_shaders->getVariable("g_clipping_parameters", 16);
    set("exposure_control", 1.f, 1.f, 1.f, 0.f);
    set("g_lighting_parameters", 1.f, 1.f, 0.f, 0.f);
    set("g_desaturation", 0.f, 0.f, 0.f, 1.f);
    set("g_soldier_light_desaturation", 0.f, 0.f, 0.f, 1.f);
    set("g_vehicle_light_desaturation", 0.f, 0.f, 0.f, 1.f);
    set("atmosphericskyblend", 0.0395f, -0.0416f);
    set("g_post_process_parameters", 0.18f, 5.f, 10.f, 0.9f);
    m_shaders->getVariable("postprocess_sepia", 4);
    set("g_solder_light_colour", 1.f, 1.f, 1.f, 1.f);
    set("g_solder_light_direction", 0.f, -1.f, 0.f, 0.f);
    set("g_vehicle_light_colour", 1.f, 1.f, 1.f, 1.f);
    set("g_vehicle_light_direction", 0.f, -1.f, 0.f, 0.f);
    set("fxlight_sundirection", 0.f, 0.f, 1.f);
    set("fxlight_suncolour", 0.9f, 1.f, 1.f);
    set("fxlight_ambcolour", 0.5f, 0.55f, 0.6f);
    set("fxlight_modifiers", 1.f, 0.1f);
    set("fx_texelsizes", 0.000488281f, 0.999512f);
    m_shaders->getVariable("watertint", 16);
    set("wateraspectratio", 1.f, 1.f);
    m_shaders->getVariable("watercolourwindscale", 8);
    m_shaders->getVariable("waterreflection_surfaceoffset", 16);
    m_shaders->getVariable("waterreflection_winddirection", 8);
    m_shaders->getVariable("waterreflection_surfacebumpiness", 4);
    m_shaders->getVariable("waterreflection_screenscaleoffset", 16);
    set("icehealththresholds", 0.33f, 0.66f, 1.f, 1.f);
    m_shaders->getVariable("iceslushcolour_tiling", 16);
    m_shaders->getVariable("icestressedcolour_tiling", 16);
    m_shaders->getVariable("icesolidcolour_tiling", 16);
    m_shaders->getVariable("icesectortiling_invwidth", 8);
    m_shaders->getVariable("iceslushreveal", 8);
    m_shaders->getVariable("viewoverride", 384);
    m_shaders->getVariable("projectionoverride", 384);
    set("fowtextureworldscaleoffset", 1.f, 1.f, 0.f, 0.f);
    m_shaders->getVariable("fow_visited", 16);
    m_shaders->getVariable("fow_hidden", 16);
    m_shaders->getVariable("fow_blend", 16);
    m_shaders->getVariable("g_topdownrange", 16);
    m_shaders->getVariable("topdownvpmatrix", 64);
    set("g_topdowntexuresize", 0.f, 0.f, 1.f, 1.f);
    set("muckparameters", 1.f, 1.f, 1.f, 1.f);
    m_shaders->getVariable("g_windvectorandtime", 16);
    m_shaders->getVariable("g_trackmapsize", 16);

    set("teamcolour", 0.f, 0.f, 0.f, .4f);
#undef set
#undef var
  }
}}