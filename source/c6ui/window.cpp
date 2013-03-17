#include "window.h"
#include "frame.h"
#include "mouse.h"

namespace C6 { namespace UI
{
  Window::Window()
    : m_parent(nullptr)
    , m_first_child(nullptr)
    , m_last_child(nullptr)
    , m_prev_sibling(nullptr)
    , m_next_sibling(nullptr)
    , m_background_colour(0)
  {
  }

  WindowEx* Window::ex(Window* wnd)
  {
    return static_cast<WindowEx*>(wnd);
  }

  void Window::appendChild(Window* child)
  {
    child->m_parent = this;
    if(m_last_child)
    {
      m_last_child->m_next_sibling = child;
      child->m_prev_sibling = m_last_child;
    }
    else
    {
      m_first_child = child;
    }
    m_last_child = child;
  }

  void Window::refresh()
  {
    D2D_RECT_F rect = m_position;
    rect.left = (std::max)(0.f, rect.left);
    rect.top = (std::max)(0.f, rect.top);
    auto ancestor = this;
    while(ancestor->m_parent)
    {
      ancestor = ancestor->m_parent;
      rect.left += ancestor->m_position.left;
      rect.right = (std::min)(rect.right + ancestor->m_position.left, ancestor->m_position.right);
      rect.top += ancestor->m_position.top;
      rect.bottom = (std::min)(rect.bottom + ancestor->m_position.top, ancestor->m_position.bottom);
    }

    RECT rc, *prc;
    rc.left = static_cast<LONG>(floor(rect.left));
    rc.top = static_cast<LONG>(floor(rect.top));
    rc.right = static_cast<LONG>(ceil(rect.right));
    rc.bottom = static_cast<LONG>(ceil(rect.bottom));
#ifdef C6UI_NO_TEXT
    prc = NULL;
#else
    prc = &rc;
#endif
    InvalidateRect(static_cast<Frame*>(ancestor)->getHwnd(), prc, FALSE);
  }

  void Window::onKeyDown(int vk)
  {
    if(m_parent)
      m_parent->onKeyDown(vk);
  }

  void Window::onKeyUp(int vk)
  {
    if(m_parent)
      m_parent->onKeyUp(vk);
  }

  DC& Window::getDC()
  {
    auto ancestor = this;
    do { ancestor = ancestor->m_parent; } while(ancestor->m_parent);
    return static_cast<Frame*>(ancestor)->getDC();
  }

  Window* Window::findWindowAt(D2D_POINT_2F position)
  {
    auto self = this;
re_entry:
    for(auto child = self->m_first_child; child; child = child->m_next_sibling)
    {
      auto& rc = child->m_position;
      if(rc.left <= position.x && position.x < rc.right && rc.top <= position.y && position.y < rc.bottom)
      {
        position.x -= rc.left;
        position.y -= rc.top;
        self = child;
        goto re_entry;
      }
    }
    return self;
  }

  std::unique_ptr<DragHandler> Window::onMouseDown(D2D_POINT_2F position, MouseButton::E button)
  {
    position.x -= m_position.left;
    position.y -= m_position.top;
    for(auto child = m_first_child; child; child = child->m_next_sibling)
    {
      auto& rc = child->m_position;
      if(rc.left <= position.x && position.x < rc.right && rc.top <= position.y && position.y < rc.bottom)
      {
        return child->onMouseDown(position, button);
      }
    }
    return NullDragHandler(*this);
  }

  void Window::render(DC& dc)
  {
    if(m_background_colour)
      dc.rectangle(0.f, m_position, m_background_colour);

    renderChildren(dc, .125f);
  }

  void Window::renderChildren(DC& dc, float dz)
  {
    if(!m_last_child)
      return;

    auto dx = m_position.left;
    auto dy = m_position.top;
    dc.transform(dx, dy, dz);
    __try
    {
      auto clip = dc.getClipRectangle();
      for(auto child = m_last_child; child; child = child->m_prev_sibling)
      {
        if(child->m_position.right <= clip.left || clip.right <= child->m_position.left
        || child->m_position.bottom <= clip.top || clip.bottom <= child->m_position.top)
          continue;

        child->render(dc);
      }
    }
    __finally
    {
      dc.transform(-dx, -dy, -dz);
    }
  }

  void Window::resized()
  {
  }

  Cursor::E Window::onMouseEnter()
  {
    return Cursor::Normal;
  }

  void Window::onMouseWheel(D2D_POINT_2F delta)
  {
    if(m_parent)
      m_parent->onMouseWheel(delta);
  }

  void Window::onMouseLeave()
  {
  }
}}
