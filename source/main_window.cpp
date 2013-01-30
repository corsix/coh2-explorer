#include "main_window.h"
#include "fs.h"
#include "shader_db.h"
#include "model.h"

MainWindow::MainWindow(const char* module_file, const char* rgm_path)
  : wxFrame(nullptr, wxID_ANY, L"CoH2 Explorer")
  , m_essence(nullptr)
  , m_camera_angle(0)
  , m_camera_height(2.f)
{
  auto fs = Essence::CreateModFileSource(&m_arena, module_file);
  m_essence = new Essence::Graphics::Panel(this, wxID_ANY, fs);
  auto device = m_essence->getDevice();
  auto model = m_arena.alloc<Essence::Graphics::Model>(fs, m_essence->getShaders(), fs->readFile(rgm_path), device);
  updateCamera();

  m_essence->Bind(wxEVT_PAINT, [=](wxPaintEvent& e) mutable {
    m_essence->startRendering();
    model->render(device);
    m_essence->finishRendering();
  });

  m_essence->Bind(wxEVT_KEY_DOWN, [=](wxKeyEvent& e) {
    int dir;
    switch(e.GetKeyCode())
    {
    case WXK_LEFT: dir = 1; break;
    case WXK_RIGHT: dir = -1; break;
    default: e.Skip(); return;
    }
    m_camera_angle += dir;
    updateCamera();
  });

  m_essence->Bind(wxEVT_MOUSEWHEEL, [=](wxMouseEvent& e) {
    m_camera_height += static_cast<float>(e.GetWheelRotation()) / static_cast<float>(e.GetWheelDelta());
    updateCamera();
  });
}

void MainWindow::updateCamera()
{
  float a = (static_cast<float>(m_camera_angle) / 16.f) * M_PI;
  float dist = 8.f / (1.f + abs(m_camera_height - 2.f) * 0.1f);
  Essence::Graphics::vector3_t eye = {cos(a) * dist, m_camera_height, sin(a) * dist};
  Essence::Graphics::vector3_t at = {0.f, (m_camera_height - 2.f) * 0.01f, 0.f};
  m_essence->lookAtFrom(at, eye);
  m_essence->getShaders()->variablesUpdated();
  m_essence->Refresh(false);
}
