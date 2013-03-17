#include "tab_control.h"
#include "mouse.h"

namespace C6 { namespace UI
{
  namespace
  {
    class Tab : public Window
    {
    public:
      Tab(DC& dc, const wchar_t* title, Window* contents)
        : m_title(title)
        , m_contents(contents)
      {
        auto size = dc.measureText(Fonts::Headings, title);
        size.height *= 1.3f;

        m_position.left = 0.f;
        m_position.right = ceil(size.width + size.height);
        m_position.top = 1.f;
        m_position.bottom = 1.f + ceil(size.height);
      }

      std::unique_ptr<DragHandler> onMouseDown(D2D_POINT_2F position, MouseButton::E button)
      {
        if(button == MouseButton::Left)
          static_cast<TabControl*>(m_parent)->setActiveContents(m_contents);
        
        return NullDragHandler(*this);
      }

      bool isActive()
      {
        return static_cast<TabControl*>(m_parent)->getActiveContents() == m_contents;
      }

    protected:
      void render(DC& dc) override
      {
        bool is_active = isActive();
        D2D_RECT_F main_bg = m_position;
        main_bg.top++;
        main_bg.right--;
        if(is_active)
        {
          D2D_RECT_F top_edge = main_bg;
          top_edge.top = m_position.top;
          top_edge.bottom = main_bg.top;
          dc.rectangle(0.f, top_edge, 0xFF6A6A6A);

          main_bg.bottom += 2.f;
          dc.rectangle(0.125f, main_bg, 0xFF535353);
          main_bg.bottom -= 2.f;
        }

        D2D_RECT_F right_edge = m_position;
        right_edge.left = main_bg.right;
        dc.rectangle(0.125f, right_edge, 0xFF282828);

        dc.text(100.f, main_bg, is_active ? 0xFFE7E7E7 : 0xA6A6A6, Fonts::Headings, m_title);
      }

    private:
      const wchar_t* m_title;
      Window* m_contents;
    };
  }

  TabControl::TabControl()
  {
    m_position.right = 300.f;
    m_position.left = 0.f;
  }

  Window* TabControl::getActiveContents()
  {
    return m_last_child;
  }

  void TabControl::setActiveContents(Window* contents)
  {
    if(contents == getActiveContents())
      return;

    auto last_tab = ex(m_last_child)->m_prev_sibling;

    ex(last_tab)->m_next_sibling = contents;
    ex(contents)->m_prev_sibling = last_tab;
    m_last_child = contents;

    resized();
    refresh();
  }

  void TabControl::appendTab(Arena& arena, const wchar_t* title, Window* contents)
  {
    auto tab = ex(arena.allocTrivial<Tab>(getDC(), title, contents));
    appendChild(tab);

    float left = 0.f;
    if(m_first_child != m_last_child)
    {
      ex(contents)->m_parent = this;

      auto last_tab = ex(m_last_child);
      auto contents = ex(last_tab->m_prev_sibling);
      auto penultimate_tab = ex(contents->m_prev_sibling);
      penultimate_tab->m_next_sibling = last_tab;
      last_tab->m_prev_sibling = penultimate_tab;
      last_tab->m_next_sibling = contents;
      contents->m_prev_sibling = last_tab;
      contents->m_next_sibling = nullptr;
      m_last_child = contents;

      left = penultimate_tab->m_position.right;
    }
    else
    {
      appendChild(contents);

      left = 1.f;
    }
    tab->m_position.left += left;
    tab->m_position.right += left;
    ex(contents)->m_background_colour = 0xFF535353;
    ex(contents)->m_position.left = 1.f;
    ex(contents)->m_position.top = tab->m_position.bottom + 2.f;

    resized();
    refresh();
  }

  void TabControl::render(DC& dc)
  {
    D2D_RECT_F r = dc.rectangleOutline(0.f, m_position, 0xFF282828);
    if(m_last_child)
    {
      r.bottom = r.top + 1.f;
      dc.rectangle(0.f, r, 0xFF4A4A4A);
      
      r.top = r.bottom;
      const auto& last_tab_position = ex(ex(m_last_child)->m_prev_sibling)->m_position;
      r.bottom = m_position.top + last_tab_position.bottom;
      dc.rectangleV(0.f, r, 0xFF3A3A3A, 0xFF333333);

      r.top = r.bottom;
      r.bottom = r.top + 1.f;
      dc.rectangle(0.f, r, 0xFF282828);

      r.top = r.bottom;
      r.bottom = r.top + 1.f;
      dc.rectangle(0.f, r, 0xFF6A6A6A);

      __super::render(dc);
    }
  }

  void TabControl::resized()
  {
    auto contents = ex(getActiveContents());
    contents->m_position.right = m_position.right - m_position.left - 1.f;
    contents->m_position.bottom = m_position.bottom - m_position.top - 1.f;
    contents->resized();
  }
}}
