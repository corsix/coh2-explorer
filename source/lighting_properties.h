#include "c6ui\properties.h"

namespace Essence { namespace Graphics
{
  class Panel;
  class ShaderDatabase;

  class UIColour : public C6::UI::PropertyListener
  {
  public:
    UIColour(float* value);

    const uint32_t& getPacked() const { return m_packed; }

    float* m_value;
  private:
    uint32_t m_packed;

    void onPropertyChanged() override;
  };

  class LightingProperties : public C6::UI::PropertyGroupList, private C6::UI::PropertyListener
  {
  public:
    LightingProperties(Arena& a, C6::UI::DC& dc, Panel& panel);

    void setActiveWindow(Window* active_window);
    const uint32_t& getExposure();

  private:
    void onPropertyChanged() override;

    Window* m_active_window;
    ShaderDatabase* m_shader_db;
    UIColour m_bg_colour;
    UIColour m_exposure;
  };
}}
