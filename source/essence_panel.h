#pragma once
#include "directx.h"
#include "arena.h"
#include "math.h"

namespace Essence { class FileSource; }

namespace Essence { namespace Graphics
{
  class ShaderDatabase;

  class Panel : public Direct3DPanel
  {
  public:
    Panel(wxWindow *parent, wxWindowID winid, FileSource* mod_fs);

    void lookAtFrom(vector3_t at, vector3_t eye);
    auto getShaders() -> ShaderDatabase* { return m_shaders; }
    void startRendering() override;

  private:
    using Direct3DPanel::setDepthFormat;

    Arena m_arena;
    C6::D3::Buffer m_world_matrix_buffer;
    C6::D3::Buffer m_instance_info_buffer;
    C6::D3::ShaderResourceView m_black, m_grey, m_white;
    C6::D3::ShaderResourceView m_black_cube, m_grey_cube, m_white_cube;

    ShaderDatabase* m_shaders;
    matrix44_t* m_view;
    matrix44_t* m_proj;
    vector3_t* m_eye_position;
    
    void initWorldMatrixBuffer();
    void initInstanceInfoBuffer();
    void initShaderVariables();
  };

}}
