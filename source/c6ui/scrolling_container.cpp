#include "scrolling_container.h"
#include "mouse.h"

namespace C6 { namespace UI
{
  namespace
  {
    float operator | (D2D_POINT_2F lhs, D2D_POINT_2F rhs)
    {
      return lhs.x * rhs.x + lhs.y * rhs.y;
    }

    D2D_POINT_2F operator ^ (D2D_POINT_2F lhs, D2D_POINT_2F rhs)
    {
      return D2D1::Point2F(lhs.x * rhs.y, lhs.y * rhs.x);
    }

    D2D_POINT_2F operator ^ (float lhs, D2D_POINT_2F rhs)
    {
      return D2D1::Point2F(lhs * rhs.y, lhs * rhs.x);
    }

    D2D_POINT_2F operator * (D2D_POINT_2F lhs, D2D_POINT_2F rhs)
    {
      return D2D1::Point2F(lhs.x * rhs.x, lhs.y * rhs.y);
    }

    D2D_POINT_2F operator * (float lhs, D2D_POINT_2F rhs)
    {
      return D2D1::Point2F(lhs * rhs.x, lhs * rhs.y);
    }

    D2D_POINT_2F size(const D2D_RECT_F& rc)
    {
      return D2D1::Point2F(rc.right - rc.left, rc.bottom - rc.top);
    }

    D2D_POINT_2F top_left(const D2D_RECT_F& rc)
    {
      return D2D1::Point2F(rc.left, rc.top);
    }

    D2D_POINT_2F bottom_right(const D2D_RECT_F& rc)
    {
      return D2D1::Point2F(rc.right, rc.bottom);
    }

    D2D_RECT_F rect(D2D_POINT_2F top_left, D2D_POINT_2F bottom_right)
    {
      return D2D1::RectF(top_left.x, top_left.y, bottom_right.x, bottom_right.y);
    }
  }

  ScrollableWindow::ScrollableWindow()
    : m_container(nullptr)
  {
  }

  ScrollingContainer* ScrollableWindow::wrapInScrollingContainer(Arena& arena)
  {
    resized();
    return m_container = arena.allocTrivial<ScrollingContainer>(arena, this);
  }

  std::unique_ptr<DragHandler> ScrollableWindow::createDragViewport()
  {
    auto container = m_container;
    if(container && container->areScrollBarsInUse())
    {
      return SimpleDragHandler(*this, container->getScroll(), [=](D2D_POINT_2F new_pos)
      {
        container->scrollTo(new_pos);
      });
    }
    else
      return nullptr;
  }

  std::unique_ptr<DragHandler> ScrollableWindow::onMouseDown(D2D_POINT_2F position, MouseButton::E button)
  {
    if(auto handler = createDragViewport())
      return move(handler);
    else
      return __super::onMouseDown(position, button);
  }

  ScrollingContainer::ScrollingContainer(Arena& arena, ScrollableWindow* content)
    : m_content(content)
    , m_bars_visible(-1)
    , m_alignment(0.f)
  {
    auto content_size = size(content->m_position);
    m_content_size = D2D1::SizeF(content_size.x, content_size.y);

    appendChild(content);
    appendChild(m_bottom = arena.allocTrivial<ScrollBar>(D2D1::Point2F(1.f, 0.f), content->m_position.left, m_viewport_size.width, m_content_size.width));
    appendChild(m_right  = arena.allocTrivial<ScrollBar>(D2D1::Point2F(0.f, 1.f), content->m_position.top, m_viewport_size.height, m_content_size.height));
  }

  void ScrollingContainer::setAlignment(float alignment)
  {
    m_alignment = alignment;
  }

  void ScrollingContainer::resized()
  {
    D2D_SIZE_F avail_size = D2D1::SizeF(m_position.right - m_position.left, m_position.bottom - m_position.top);
    float shaft_size = ceil(getDC().getScaleFactor() * 16.f);
    bool need_bottom = false;
    bool need_right = false;
    int bars_visible = 0;
    for(int iter = 0; iter < 2; ++iter)
    {
      if(!need_right && m_content_size.height > avail_size.height)
      {
        avail_size.width -= shaft_size;
        need_right = true;
        bars_visible |= 1;
      }
      if(!need_bottom && m_content_size.width > avail_size.width)
      {
        avail_size.height -= shaft_size;
        need_bottom = true;
        bars_visible |= 2;
      }
    }
    m_viewport_size = avail_size;
    float right_width = need_right ? shaft_size : 0.f;
    float bottom_height = need_bottom ? shaft_size : 0.f;

    if(bars_visible != m_bars_visible)
    {
      m_bars_visible = bars_visible;
      refresh();
    }

    m_content->m_position.right = avail_size.width;
    m_content->m_position.bottom = avail_size.height;
    m_bottom->recalcuatePosition(bottom_height, right_width);
    m_right->recalcuatePosition(right_width, bottom_height);
    m_content->resized();
  }

  void ScrollingContainer::setContentSize(D2D_SIZE_F content_size)
  {
    m_content_size = content_size;
    resized();
  }

  void ScrollingContainer::render(DC& dc)
  {
    if(m_bars_visible)
    {
      m_bottom->m_prev_sibling = nullptr;
      __try
      {
        __super::render(dc);
      }
      __finally
      {
        m_bottom->m_prev_sibling = m_content;
      }

      auto old_clip = dc.pushClipRect(D2D1::RectF(m_position.left, m_position.top, m_position.left + m_content->m_position.right, m_position.top + m_content->m_position.bottom));
      m_last_child = m_content;
      __try
      {
        renderChildren(dc, .125f);
      }
      __finally
      {
        m_last_child = m_right;
        dc.restoreClipRect(old_clip);
      }
    }
    else
    {
      __super::render(dc);
    }
  }

  void ScrollingContainer::onMouseWheel(D2D_POINT_2F delta)
  {
    delta = (32.f * getDC().getScaleFactor()) * delta;
    m_bottom->scrollBy(delta.x);
    m_right->scrollBy(delta.y);
    refresh();
  }

  D2D_POINT_2F ScrollingContainer::getScroll() const
  {
    return D2D1::Point2F(m_bottom->getScroll(), m_right->getScroll());
  }

  void ScrollingContainer::scrollTo(D2D_POINT_2F point)
  {
    m_bottom->scrollTo(point.x);
    m_right->scrollTo(point.y);
    refresh();
  }

  void ScrollingContainer::onKeyDown(int vk)
  {
    auto delta = D2D1::Point2F();
    switch(vk)
    {
    case VK_LEFT : delta.x =  1; break;
    case VK_RIGHT: delta.x = -1; break;
    case VK_UP   : delta.y =  1; break;
    case VK_DOWN : delta.y = -1; break;
    default: __super::onKeyDown(vk); return;
    }
    onMouseWheel(.25f * delta);
  }

  ScrollBar::ScrollBar(D2D_POINT_2F axis, float& scroll_offset, float& viewport_length, float& content_length)
    : m_axis(axis)
    , m_scroll_offset(scroll_offset)
    , m_viewport_length(viewport_length)
    , m_content_length(content_length)
  {
  }

  void ScrollBar::recalcuatePosition(float shaft_size, float final_margin)
  {
    auto far_corner = size(ex(m_parent)->m_position);
    auto near_corner = (far_corner ^ m_axis) - (shaft_size ^ m_axis);
    m_position = rect(near_corner, far_corner);
    m_final_margin = final_margin;

    auto shaft_length = (far_corner | m_axis) - final_margin;
    m_thumb_length = ceil((std::max)((shaft_length * shaft_length) / m_content_length, 16.f * getDC().getScaleFactor()) * .5f) * 2.f;
    m_initial_margin = m_thumb_length * .5f;
    m_final_margin += m_initial_margin;
    shaft_length -= m_thumb_length;
    m_shaft_length = shaft_length;
    
    clampScrollOffset();
  }

  void ScrollBar::clampScrollOffset()
  {
    if(m_viewport_length > m_content_length)
      m_scroll_offset = (m_viewport_length - m_content_length) * static_cast<ScrollingContainer*>(m_parent)->getAlignment();
    else
      m_scroll_offset = (std::min)(0.f, (std::max)(m_scroll_offset, m_viewport_length - m_content_length));
    m_thumb_center = m_initial_margin + floor(m_shaft_length * -m_scroll_offset / (m_content_length - m_viewport_length) + .5f);
  }

  void ScrollBar::render(DC& dc)
  {
    auto start = top_left(m_position);
    auto end = bottom_right(m_position);
    if(start.x == end.x || start.y == end.y)
      return;
    auto mid = (.25f * start + .75f * end) ^ m_axis;

    uint32_t edge = 0x66000000UL;
    uint32_t midd = 0x32000000UL;
    auto rectangle = m_axis.x > m_axis.y ? &DC::rectangleV : &DC::rectangleH;
    (dc.*rectangle)(0.f, rect(start, mid + (end * m_axis)), edge, midd);
    (dc.*rectangle)(0.f, rect(       mid ,  end          ), midd, edge);

    auto thumb_start = start + (m_thumb_center - m_thumb_length * .5f) * m_axis + (1.f ^ m_axis);
    auto thumb_end = thumb_start + (m_thumb_length * m_axis) + (size(m_position) ^ m_axis) - (1.f ^ m_axis);
    (dc.*rectangle)(0.125f, dc.rectangleOutline(0.125f, rect(thumb_start, thumb_end), midd), 0x99999999UL, 0x997A7A7AUL);
  }

  std::unique_ptr<DragHandler> ScrollBar::onMouseDown(D2D_POINT_2F position, MouseButton::E button)
  {
    if(button == MouseButton::Left)
    {
      auto pos = (position | m_axis);
      if(0.f <= pos && pos < m_shaft_length + m_thumb_length)
      {
        if(abs(pos - m_thumb_center) < m_thumb_length * .5f)
          pos = m_thumb_center;

        return SimpleDragHandler(*this, D2D1::Point2F(pos, pos), [this](D2D_POINT_2F position)
        {
          m_thumb_center = (std::min)((std::max)(m_initial_margin, position | m_axis), m_initial_margin + m_shaft_length);
          m_scroll_offset = (m_thumb_center - m_initial_margin) / -m_shaft_length * (m_content_length - m_viewport_length);
          m_parent->refresh();
        });
      }
    }
    return __super::onMouseDown(position, button);
  }

  void ScrollBar::scrollBy(float delta)
  {
    m_scroll_offset += floor(delta + .5f);
    clampScrollOffset();
  }

  void ScrollBar::scrollTo(float pos)
  {
    m_scroll_offset = floor(pos + .5f);
    clampScrollOffset();
  }

  Cursor::E ScrollBar::onMouseEnter()
  {
    return Cursor::Hand;
  }
}}
