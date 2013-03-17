#pragma once
#include "c6ui/window.h"
#include "directx.h"
#include "arena.h"
#include "math.h"

namespace Essence { class FileSource; }

namespace Essence { namespace Graphics
{
  class ShaderDatabase;
  class Model;

  class Panel : public C6::UI::Window
  {
  public:
    Panel(C6::UI::Factories& factories, FileSource* mod_fs);
    ~Panel();

    void setModel(FileSource* mod_fs, const std::string& path);
    void setModel(std::unique_ptr<Model> model);
    void lookAtFrom(vector3_t at, vector3_t eye);
    auto getShaders() -> ShaderDatabase* { return m_shaders; }
    auto getDevice() -> C6::D3::Device1& { return m_device; }

    void onKeyDown(int vk) override;
    void onMouseWheel(D2D_POINT_2F delta) override;
    void resized() override;
  protected:
    void render(C6::UI::DC& dc) override;

  private:
    Arena m_arena;
    C6::D3::Device1& m_device;
    C6::D3::Buffer m_world_matrix_buffer;
    C6::D3::Buffer m_instance_info_buffer;
    C6::D3::ShaderResourceView m_black, m_grey, m_white;
    C6::D3::ShaderResourceView m_black_cube, m_grey_cube, m_white_cube;

    int m_camera_angle;
    float m_camera_height;
    float m_model_size;
    std::unique_ptr<Model> m_model;
    ShaderDatabase* m_shaders;
    matrix44_t* m_view;
    matrix44_t* m_proj;
    vector3_t* m_eye_position;
    
    void initWorldMatrixBuffer();
    void initInstanceInfoBuffer();
    void initShaderVariables();
    void updateCamera();
  };

}}
