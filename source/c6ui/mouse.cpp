#include "stdafx.h"
#include "mouse.h"

namespace C6 { namespace UI
{
  DragHandler::DragHandler(Window& window, D2D_POINT_2F reference_value)
    : m_window(window)
    , m_reference_value(reference_value)
  {
  }

  DragHandler::~DragHandler()
  {
  }
  
  void DragHandler::finishDrag()
  {
  }

  std::unique_ptr<DragHandler> NullDragHandler(Window& window)
  {
    return SimpleDragHandler(window, D2D1::Point2F(), [](const D2D_POINT_2F&){});
  }
}}
