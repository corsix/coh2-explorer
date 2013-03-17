#include "column_layout.h"
#include "mouse.h"
#include <numeric>
using namespace std;

namespace C6 { namespace UI
{

#define SPACING 5.f

  unique_ptr<DragHandler> ColumnLayout::onMouseDown(D2D_POINT_2F position, MouseButton::E button)
  {
    if(button == MouseButton::Left)
    {
      WindowEx* left = nullptr;
      for(auto right : *ex(this))
      {
        if(left && left->m_position.right <= position.x && position.x < right->m_position.left)
        {
          return SimpleDragHandler(*this, D2D1::Point2F(left->m_position.right, 0.f), [=](D2D_POINT_2F position)
          {
            left->m_position.right = position.x;
            right->m_position.left = position.x + SPACING;
            left->resized();
            left->refresh();
            right->resized();
            right->refresh();
          });
        }
        left = right;
      }
    }
    return __super::onMouseDown(position, button);
  }

  Cursor::E ColumnLayout::onMouseEnter()
  {
    return Cursor::ArrowEastWest;
  }

  void ColumnLayout::resized()
  {
    if(m_prev_sibling == nullptr && m_next_sibling == nullptr)
    {
      auto parent = ex(m_parent)->m_position;
      m_position = D2D1::RectF(0.f, 0.f, parent.right - parent.left, parent.bottom - parent.top);
    }

    auto height = m_position.bottom - m_position.top;
    auto available = m_position.right - m_position.left;
    vector<float> widths;
    for(auto child : *ex(this))
    {
      auto& rect = child->m_position;
      widths.push_back(rect.right - rect.left);
      rect.top = 0.f;
      rect.bottom = height;
    }

    switch(widths.size())
    {
    case 0:
      return;
    case 1:
      widths[0] = available;
      break;
    default:
      available -= SPACING * static_cast<float>(widths.size() - 1);
      available -= accumulate(widths.begin() + 2, widths.end(), widths[0]);
      widths[1] = available;
      break;
    }

    float x = 0.f;
    auto width = widths.begin();
    auto child = ex(m_first_child);
    for(auto child : *ex(this))
    {
      auto& rect = child->m_position;
      rect.left = x;
      x += *width++;
      rect.right = x;
      x += SPACING;
      child->resized();
    }
  }
}}
