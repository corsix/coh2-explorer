#pragma once
#include "wx.h"
#include "essence_panel.h"
#include "arena.h"

namespace Essence { namespace Graphics { class Model; }}

class MainWindow : public wxFrame
{
public:
  MainWindow(const char* module_file, const char* rgm_path);
  ~MainWindow();

private:
  void setModel(Essence::FileSource* mod_fs, const std::string& path);
  void updateCamera();

  Arena m_arena;
  Essence::Graphics::Panel* m_essence;
  int m_camera_angle;
  float m_camera_height;
  float m_model_size;
  std::unique_ptr<Essence::Graphics::Model> m_model;
};
