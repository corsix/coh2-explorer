#pragma once
#include "window.h"
#include "../arena.h"

namespace C6 { namespace UI
{
  class TabControl : public Window
  {
  public:
    TabControl();
    void appendTab(Arena& arena, const wchar_t* title, Window* contents);
    void removeAllTabs();

    Window* getActiveContents();
    void setActiveContents(Window* contents);

    void resized() override;
  protected:
    using Window::appendChild;
    void render(DC& dc) override;
  };

}}
