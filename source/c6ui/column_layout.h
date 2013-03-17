#pragma once
#include "window.h"

namespace C6 { namespace UI
{
  class ColumnLayout : public Window
  {
  public:
    std::unique_ptr<DragHandler> onMouseDown(D2D_POINT_2F position, MouseButton::E button) override;
    Cursor::E onMouseEnter() override;

    void resized() override;
  };
}}
