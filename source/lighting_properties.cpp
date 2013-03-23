#include "lighting_properties.h"
#include "shader_db.h"
#include "essence_panel.h"

namespace
{
  class Saturation : public C6::UI::PropertyListener
  {
  public:
    Saturation(Essence::Graphics::ShaderDatabase* shader_db)
      : m_desaturation(shader_db->getVariable("g_desaturation", 16))
    {
      m_value = m_desaturation[3];
    }

    void onPropertyChanged() override
    {
      m_desaturation[0] = .299f * (1.f - m_value);
      m_desaturation[1] = .587f * (1.f - m_value);
      m_desaturation[2] = .114f * (1.f - m_value);
      m_desaturation[3] = m_value;
    }

    float m_value;

  private:
    float* m_desaturation;
  };
}

namespace Essence { namespace Graphics
{
  UIColour::UIColour(float* value)
    : m_value(value)
  {
    onPropertyChanged();
  }

  static uint32_t pack(float x, int shift)
  {
    return static_cast<uint32_t>(floor(x * 255.f + .5f)) << shift;
  }

  void UIColour::onPropertyChanged()
  {
    m_packed = pack(m_value[2], 0) | pack(m_value[1], 8) | pack(m_value[0], 16) | pack(m_value[3], 24);
  }

  const uint32_t& LightingProperties::getExposure()
  {
    return m_exposure.getPacked();
  }

  LightingProperties::LightingProperties(Arena& a, C6::UI::DC& dc, Panel& panel)
    : m_active_window(&panel)
    , m_shader_db(panel.getShaders())
    , m_bg_colour(m_shader_db->getVariable("(c6ui_background)", 16))
    , m_exposure(m_shader_db->getVariable("exposure_control", 16))
  {
    auto shaders = m_shader_db;
    addListener(this);

    {
      auto bg = a.alloc<C6::UI::ColourProperty>(a, dc, L"Background");
      bg->addListener(&m_bg_colour);
      bg->appendChannel(m_bg_colour.m_value[0], 0xFF000000, 0xFFFF0000);
      bg->appendChannel(m_bg_colour.m_value[1], 0xFF000000, 0xFF00FF00);
      bg->appendChannel(m_bg_colour.m_value[2], 0xFF000000, 0xFF0000FF);
      bg->firePropertyChanged();
      appendGroup(bg);
    }

    {
      auto tc = a.alloc<C6::UI::ColourProperty>(a, dc, L"Team Colour");
      auto var = shaders->getVariable("teamcolour", 16);
      tc->appendChannel(var[0], 0xFF000000, 0xFFFF0000);
      tc->appendChannel(var[1], 0xFF000000, 0xFF00FF00);
      tc->appendChannel(var[2], 0xFF000000, 0xFF0000FF);
      tc->appendChannel(var[3], 0x00000000, 0xFF000000);
      tc->firePropertyChanged();
      appendGroup(tc);
    }

    {
      auto saturation = a.alloc<C6::UI::ColourProperty>(a, dc, L"Saturation");
      auto listener = a.allocTrivial<Saturation>(shaders);
      saturation->addListener(listener);
      saturation->appendChannel(listener->m_value, 0xFF808080, 0x00000000);
      appendGroup(saturation);
    }

    {
      auto exposure = a.alloc<C6::UI::ColourProperty>(a, dc, L"Exposure");
      exposure->addListener(&m_exposure);
      exposure->appendChannel(m_exposure.m_value[0], 0xFF000000, 0xFFFF0000);
      exposure->appendChannel(m_exposure.m_value[1], 0xFF000000, 0xFF00FF00);
      exposure->appendChannel(m_exposure.m_value[2], 0xFF000000, 0xFF0000FF);
      exposure->appendChannel(m_exposure.m_value[3], 0x00000000, 0xFFFFFFFF);
      exposure->firePropertyChanged();
      appendGroup(exposure);
    }

    onPropertyChanged();
  }

  void LightingProperties::onPropertyChanged()
  {
    m_shader_db->variablesUpdated();
    ex(m_active_window)->m_background_colour = m_bg_colour.getPacked();
    m_active_window->refresh();
  }

  void LightingProperties::setActiveWindow(Window* active_window)
  {
    m_active_window = active_window;
    onPropertyChanged();
  }

}}
