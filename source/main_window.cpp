#include "main_window.h"
#include "fs.h"
#include "shader_db.h"
#include "model.h"
#include "file_tree.h"
#include <wx/splitter.h>

MainWindow::MainWindow(const char* module_file, const char* rgm_path)
  : wxFrame(nullptr, wxID_ANY, L"CoH2 Explorer")
  , m_essence(nullptr)
  , m_camera_angle(0)
  , m_camera_height(2.f)
{
  auto splitter = new wxSplitterWindow(this, wxID_ANY);
  auto fs = Essence::CreateModFileSource(&m_arena, module_file);
  auto tree = new FileTree(splitter, wxID_ANY, fs);
  m_essence = new Essence::Graphics::Panel(splitter, wxID_ANY, fs);
  auto device = m_essence->getDevice();
  if(*rgm_path)
    setModel(fs, rgm_path);
  updateCamera();

  tree->Bind(FILE_TREE_FILE_ACTIVATED, [=](FileTreeEvent& e) {
    setModel(e.getFileSource(), e.getPath());
  });

  m_essence->Bind(wxEVT_PAINT, [=](wxPaintEvent& e) mutable {
    m_essence->startRendering();
    if(m_model)
      m_model->render(device);
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

  splitter->SplitVertically(tree, m_essence, 300);
  Maximize();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setModel(Essence::FileSource* mod_fs, const std::string& path)
{
  try
  {
    m_model.reset(new Essence::Graphics::Model(mod_fs, m_essence->getShaders(), mod_fs->readFile(path), m_essence->getDevice()));

    m_model_size = 1.f;
    m_model->getMeshes().for_each([&](Essence::Graphics::Mesh* mesh)
    {
      auto& bvol = mesh->getBoundingVolume();
      const float x = bvol.center.x + bvol.size.x;
      const float y = bvol.center.y + bvol.size.y;
      const float z = bvol.center.z + bvol.size.z;
      const float size = sqrt(x*x + y*y + z*z);
      if(m_model_size < size)
        m_model_size = size;
    });
  }
  catch(const std::exception& e)
  {
    ::wxMessageBox(wxString("Error whilst loading " + path + ":\n" + e.what()), L"Exception", wxICON_EXCLAMATION | wxOK, this);
    return;
  }
  updateCamera();
}

void MainWindow::updateCamera()
{
  float a = (static_cast<float>(m_camera_angle) / 16.f) * M_PI;
  float dist = m_model_size / (1.f + abs(m_camera_height - 2.f) * 0.1f);
  Essence::Graphics::vector3_t eye = {cos(a) * dist, m_camera_height, sin(a) * dist};
  Essence::Graphics::vector3_t at = {0.f, (m_camera_height - 2.f) * 0.01f, 0.f};
  m_essence->lookAtFrom(at, eye);
  m_essence->getShaders()->variablesUpdated();
  m_essence->Refresh(false);
}
