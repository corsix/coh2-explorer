#pragma once
#include "wx.h"
#include "essence_panel.h"
#include "arena.h"

class MainWindow : public wxFrame
{
public:
  MainWindow(const char* module_file, const char* rgm_path);

private:
  void updateCamera();

  Arena m_arena;
  Essence::Graphics::Panel* m_essence;
  int m_camera_angle;
  float m_camera_height;
};
