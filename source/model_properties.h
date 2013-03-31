#include "c6ui\properties.h"

namespace Essence { namespace Graphics
{
  class Panel;

  class ModelProperties : public C6::UI::PropertyGroupList
  {
  public:
    ModelProperties(Arena& a, C6::UI::DC& dc, Panel& panel);

    void setActiveWindow(Window* active_window);

  private:
    Window* m_active_window;
  };
}}
