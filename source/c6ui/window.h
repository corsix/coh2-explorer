#pragma once
#include "dc.h"
#include <memory>

namespace C6 { namespace UI
{
  class DragHandler;

  namespace MouseButton
  {
    enum E
    {
      Left,
      LeftDouble,
      Middle,
      MiddleDouble,
      Right,
      RightDouble
    };
  };

  namespace Cursor
  {
    enum E
    {
      Normal,
      Hand,
      ArrowEastWest,
    };
  }

  class Window
  {
  public:
    Window();

    void appendChild(Window* child);
    auto findWindowAt(D2D_POINT_2F position) -> Window*;
    auto getDC() -> DC&;
    void refresh();

    virtual void resized();
    virtual auto onMouseEnter() -> Cursor::E;
    virtual auto onMouseDown(D2D_POINT_2F position, MouseButton::E button) -> std::unique_ptr<DragHandler>;
    virtual void onMouseWheel(D2D_POINT_2F delta);
    virtual void onMouseLeave();
    virtual void onKeyDown(int vk);
    virtual void onKeyUp(int vk);

  protected:
    virtual void render(DC& dc);
    void renderChildren(DC& dc, float dz = 0.f);
    static class WindowEx* ex(Window* wnd);

    Window* m_parent;
    Window* m_first_child;
    Window* m_last_child;
    Window* m_prev_sibling;
    Window* m_next_sibling;
    D2D_RECT_F m_position;
    uint32_t m_background_colour;
  };

  class WindowEx : public Window
  {
  public:
    using Window::m_parent;
    using Window::m_first_child;
    using Window::m_last_child;
    using Window::m_prev_sibling;
    using Window::m_next_sibling;
    using Window::m_position;
    using Window::m_background_colour;

    class iterator
    {
    public:
      inline iterator(WindowEx* ptr) : m_ptr(ptr) {}
      inline WindowEx* operator*() { return m_ptr; }
      inline iterator& operator++() { m_ptr = static_cast<WindowEx*>(m_ptr->m_next_sibling); return *this; }
      inline bool operator!=(const iterator& other) const { return m_ptr != other.m_ptr; }

    private:
      WindowEx* m_ptr;
    };

    inline iterator begin() { return iterator(ex(m_first_child)); }
    inline iterator end() { return iterator(nullptr); }
  };

}}
