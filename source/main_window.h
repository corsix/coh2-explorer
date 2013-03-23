#pragma once
#include "c6ui/frame.h"
#include "essence_panel.h"
#include "arena.h"
#include "file_tree.h"

namespace Essence { namespace Graphics
{
  class Model;
  class LightingProperties;
}}
namespace C6 { namespace UI { class TabControl; }}

class MainWindow : public C6::UI::Frame, private FileTreeListener
{
public:
  MainWindow(C6::UI::Factories& factories, const char* module_file, const char* rgm_path);
  ~MainWindow();

  void resized() override;

private:
  void onFileTreeActivation(std::string path) override;
  void setContentTexture(C6::D3::Texture2D texture);
  void setContent(C6::UI::Window* content, std::unique_ptr<Arena> arena);
  void createModelPropertiesUI(Arena& arena);

  Arena m_arena;
  Essence::FileSource* m_mod_fs;
  Essence::Graphics::Panel* m_essence;
  Essence::Graphics::LightingProperties* m_essence_lighting_properties;
  C6::UI::Window* m_layout;
  C6::UI::Window* m_active_content;
  C6::UI::TabControl* m_property_tabs;
  C6::WIC::ImagingFactory& m_wic_factory;
  std::unique_ptr<Arena> m_active_content_arena;
};
