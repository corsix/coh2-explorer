#include "main_window.h"
#include "fs.h"
#include "shader_db.h"
#include "model.h"
#include "file_tree.h"
#include "c6ui/column_layout.h"
#include "c6ui/tab_control.h"
#include "texture_loader.h"
#include "texture_panel.h"
#include "png.h"
#include "lighting_properties.h"
using namespace C6::UI;

MainWindow::MainWindow(C6::UI::Factories& factories, const char* module_file, const char* rgm_path)
  : Frame("CoH2 Explorer", factories)
  , m_wic_factory(factories.wic)
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
  m_active_content = m_essence;
  m_layout->appendChild(m_essence);
  auto& device = factories.d3;
  if(*rgm_path)
    onFileTreeActivation(rgm_path);

  auto sidebar_tab = m_arena.allocTrivial<TabControl>();
  m_layout->appendChild(sidebar_tab);
  m_essence_lighting_properties = m_arena.alloc<Essence::Graphics::LightingProperties>(m_arena, getDC(), *m_essence);
  sidebar_tab->appendTab(m_arena, L"Lighting", m_essence_lighting_properties->wrapInScrollingContainer(m_arena));

  ShowWindow(getHwnd(), SW_MAXIMIZE);
  resized();
}

MainWindow::~MainWindow()
{
}

void MainWindow::resized()
{
  m_layout->resized();
}

void MainWindow::setContentTexture(C6::D3::Texture2D texture)
{
  auto srv = m_essence->getDevice().createShaderResourceView(std::move(texture));
  std::unique_ptr<Arena> a(new Arena);
  auto panel = a->alloc<TexturePanel>(std::move(srv), m_essence_lighting_properties->getExposure())->wrapInScrollingContainer(*a);
  panel->setAlignment(.5f);
  setContent(panel, move(a));
}

void MainWindow::onFileTreeActivation(std::string path)
{
  std::string extension;
  {
    auto ext_pos = path.find_last_of('.');
    if(ext_pos != std::string::npos)
    {
      extension = path.substr(ext_pos + 1);
      for(char& c : extension)
        c = tolower(c);
    }
  }

  try
  {
    if(extension == "rgt" || extension == "tga")
    {
      setContentTexture(Essence::Graphics::LoadTexture(m_essence->getDevice(), m_mod_fs->readFile(path)));
    }
    else if(extension == "png")
    {
      setContentTexture(LoadPng(m_essence->getDevice(), m_wic_factory, m_mod_fs->readFile(path)));
    }
    else
    {
      m_essence->setModel(m_mod_fs, path);
      setContent(m_essence, nullptr);
    }
  }
  catch(const std::exception& e)
  {
    auto msg = "Error whilst loading " + path + ":\n" + e.what();
    MessageBoxA(getHwnd(), msg.c_str(), "Exception", MB_ICONEXCLAMATION);
    return;
  }
}

void MainWindow::setContent(C6::UI::Window* content, std::unique_ptr<Arena> arena)
{
  if(m_active_content != content)
  {
    auto old_content = ex(m_active_content);
    auto new_content = ex(content);
    auto parent  = ex(old_content->m_parent);

    new_content->m_parent = parent;
    new_content->m_position = old_content->m_position;
    new_content->m_prev_sibling = old_content->m_prev_sibling;
    new_content->m_next_sibling = old_content->m_next_sibling;
    if(new_content->m_prev_sibling)
      ex(new_content->m_prev_sibling)->m_next_sibling = new_content;
    if(new_content->m_next_sibling)
      ex(new_content->m_next_sibling)->m_prev_sibling = new_content;
    if(parent->m_first_child == old_content)
      parent->m_first_child = new_content;
    if(parent->m_last_child == old_content)
      parent->m_last_child = new_content;

    m_active_content = new_content;
    new_content->resized();
    m_essence_lighting_properties->setActiveWindow(content);
  }
  m_active_content_arena = move(arena);
}
