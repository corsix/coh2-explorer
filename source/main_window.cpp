#include "main_window.h"
#include "fs.h"
#include "shader_db.h"
#include "model.h"
#include "file_tree.h"
#include "c6ui/column_layout.h"
#include "c6ui/tab_control.h"
using namespace C6::UI;

MainWindow::MainWindow(C6::UI::Factories& factories, const char* module_file, const char* rgm_path)
  : Frame("CoH2 Explorer", factories)
  , m_essence(nullptr)
{
  m_background_colour = 0xFF282828;

  m_layout = m_arena.allocTrivial<ColumnLayout>();
  appendChild(m_layout);

  m_mod_fs = Essence::CreateModFileSource(&m_arena, module_file);
  auto tree = m_arena.allocTrivial<FileTree>(m_arena, getDC(), m_mod_fs);
  tree->addListener(this);
  auto tree_tab = m_arena.allocTrivial<TabControl>();
  m_layout->appendChild(tree_tab);
  tree_tab->appendTab(m_arena, L"Files", tree->wrapInScrollingContainer(m_arena));
  m_essence = m_arena.alloc<Essence::Graphics::Panel>(factories, m_mod_fs);
  m_layout->appendChild(m_essence);
  auto& device = factories.d3;
  if(*rgm_path)
    onFileTreeActivation(rgm_path);

  resized();
}

MainWindow::~MainWindow()
{
}

void MainWindow::resized()
{
  m_layout->resized();
}

void MainWindow::onFileTreeActivation(std::string path)
{
  try
  {
    m_essence->setModel(m_mod_fs, path); 
  }
  catch(const std::exception& e)
  {
    auto msg = "Error whilst loading " + path + ":\n" + e.what();
    MessageBoxA(getHwnd(), msg.c_str(), "Exception", MB_ICONEXCLAMATION);
    return;
  }
}
