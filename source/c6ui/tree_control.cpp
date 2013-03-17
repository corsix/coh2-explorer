#include "tree_control.h"
#include "mouse.h"

#define FLT_LARGE_EXACT 1e7f

namespace C6 { namespace UI
{
  TreeItem::TreeItem(const wchar_t* text)
    : m_has_children(false)
    , m_children(nullptr, 0)
    , m_next_logical_sibling(nullptr)
    , m_text(text)
    , m_is_expanded(false)
    , m_has_ever_expanded(false)
    , m_is_hovered(false)
  {
    m_position.right = FLT_LARGE_EXACT;
  }

  void TreeItem::populateChildren()
  {
    m_has_children = false;
  }

  std::unique_ptr<DragHandler> TreeItem::onMouseDown(D2D_POINT_2F position, MouseButton::E button)
  {
    auto active_button = position.x < m_left_margin ? MouseButton::Left : MouseButton::LeftDouble;
    if(button == active_button)
    {
      auto parent = static_cast<TreeControl*>(m_parent);
      if(isExpanded())
        parent->collapse(*this);
      else
        parent->expand(*this);

      return nullptr;
    }
    else
      return __super::onMouseDown(position, button);
  }

  Cursor::E TreeItem::onMouseEnter()
  {
    m_is_hovered = true;
    refresh();
    return __super::onMouseEnter();
  }

  void TreeItem::onMouseLeave()
  {
    m_is_hovered = false;
    refresh();
    __super::onMouseLeave();
  }

  void TreeItem::render(DC& dc)
  {
    uint32_t colour = m_is_hovered ? 0xFFD8D8D8UL : 0xFFFFFFFFUL;

    if(m_has_children)
    {
      auto item_height = m_position.bottom - m_position.top;
      auto height = ceil(item_height * 0.75f * 0.5f) * 2.f;
      auto top = m_position.top;
      auto left = m_position.left + m_left_margin - floor(item_height * 0.5f);
      auto right = left + height * 0.5f;
      auto icon_rect = D2D1::RectF(left, top, right, top + height);
      dc.sprite(50.f, icon_rect, isExpanded() ? Sprites::TreeArrowExpanded : Sprites::TreeArrowCollapsed, colour);
    }

    auto text_rect = D2D1::RectF(m_position.left + m_left_margin, m_position.top, m_text_right_edge, m_position.bottom);
    dc.text(100.f, text_rect, colour, Fonts::Body, m_text);
  }

  TreeControl::TreeControl(DC& dc)
    : m_dc(dc)
  {
    auto size = dc.measureText(Fonts::Body, L"");
    m_item_height = ceil(size.height * 1.1f);
    m_indent_width = m_item_height;
  }

  void TreeControl::appendRoot(TreeItem& root)
  {
    if(m_last_child)
      static_cast<TreeItem*>(m_last_child)->m_next_logical_sibling = &root;
    appendChild(&root);
    recalculateWidth(3.f + m_item_height * 0.5f, &root);
    recalculateY(&root);
  }

  void TreeControl::expand(TreeItem& item)
  {
    if(!item.m_has_children || item.m_is_expanded)
      return;
    if(item.m_children.size() == 0)
    {
      item.populateChildren();
      if(!item.m_has_children)
        return;
    }
    if(!item.m_has_ever_expanded)
    {
      auto left_margin = item.m_left_margin + m_indent_width;
      TreeItem* prev = nullptr;
      item.m_children.for_each([left_margin, this, &prev](TreeItem* child)
      {
        child->m_prev_sibling = prev;
        child->m_parent = this;
        if(prev)
          prev->m_next_sibling = prev->m_next_logical_sibling = child;
        recalculateWidth(left_margin, child);
        prev = child;
      });
      prev->m_next_logical_sibling = item.m_next_logical_sibling;
      item.m_has_ever_expanded = true;
    }
    item.m_is_expanded = true;

    {
      auto first_child = item.m_children[0];
      auto last_child = item.m_children[item.m_children.size() - 1];
      auto after = ex(item.m_next_sibling);
      item.m_next_sibling = first_child;
      first_child->m_prev_sibling = &item;
      last_child->m_next_sibling = after;
      if(after)
        after->m_prev_sibling = last_child;
    }

    recalculateY(item.m_children[0]);
    if(m_container)
      m_container->onMouseWheel(D2D1::Point2F(-1000.f, 0.f));
  }

  void TreeControl::recalculateWidth(float left_margin, TreeItem* item)
  {
    auto size = m_dc.measureText(Fonts::Body, item->m_text);
    item->m_position.left = 0.f;
    item->m_left_margin = left_margin;
    auto right = left_margin + size.width + 2.f;
    item->m_text_right_edge = right;

    if(m_container)
    {
      if(right > m_container->getContentSize().width)
        m_container->setContentSize(D2D1::SizeF(right, m_container->getContentSize().height));
    }
    else
    {
      if(m_position.right < m_position.left + right)
        m_position.right = m_position.left + right;
    }
  }

  void TreeControl::recalculateY(TreeItem* item)
  {
    auto x = 0.f;
    auto y = item->m_prev_sibling ? ex(item->m_prev_sibling)->m_position.bottom : 2.f;
    for(;;)
    {
      m_last_child = item;
      auto bottom = y + m_item_height;
      item->m_position.top = y;
      item->m_position.bottom = bottom;
      x = (std::max)(x, item->m_text_right_edge);
      y = bottom;

      auto next = static_cast<TreeItem*>(item->m_next_sibling);
      if(!next)
      {
        m_last_child = item;
        break;
      }
      item = next;
    }

    if(m_container)
    {
      m_container->setContentSize(D2D1::SizeF(x, y));
    }
    else
    {
      if(m_position.bottom < m_position.top + y)
        m_position.bottom = m_position.top + y;
    }
    refresh();
  }

  void TreeControl::collapse(TreeItem& item)
  {
    if(!item.m_has_children || !item.m_is_expanded)
      return;

    item.m_is_expanded = false;
    auto next = item.m_next_logical_sibling;
    item.m_next_sibling = next;
    if(next)
      next->m_prev_sibling = &item;
    recalculateY(m_container ? static_cast<TreeItem*>(m_first_child) : &item);
  }

  std::unique_ptr<DragHandler> TreeControl::onMouseDown(D2D_POINT_2F position, MouseButton::E button)
  {
    position.y -= m_position.top;
    for(auto child : *ex(this))
    {
      auto& rc = child->m_position;
      if(position.y < rc.bottom)
      {
        position.x -= m_position.left;
        return child->onMouseDown(position, button);
      }
    }
    return NullDragHandler(*this);
  }

}}