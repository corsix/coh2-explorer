#pragma once
#include "scrolling_container.h"

namespace C6 { namespace UI
{
  class TreeItem : public Window
  {
  public:
    TreeItem(const wchar_t* text);
    bool isExpanded() const { return m_is_expanded; }

    std::unique_ptr<DragHandler> onMouseDown(D2D_POINT_2F position, MouseButton::E button) override;
    Cursor::E onMouseEnter() override;
    void onMouseLeave() override;

  protected:
    void render(DC& dc) override;
    virtual void populateChildren();

    bool m_has_children;
    ArenaArray<TreeItem*> m_children;
  private:
    TreeItem* m_next_logical_sibling;
    const wchar_t* m_text;
    float m_left_margin;
    float m_text_right_edge;
    bool m_is_expanded;
    bool m_has_ever_expanded;
    bool m_is_hovered;

    friend class TreeControl;
  };

  class TreeControl : public ScrollableWindow
  {
  public:
    TreeControl(DC& dc);

    void appendRoot(TreeItem& root);
    void expand(TreeItem& item);
    void collapse(TreeItem& item);

    auto onMouseDown(D2D_POINT_2F position, MouseButton::E button) -> std::unique_ptr<DragHandler> override;

  protected:
    using Window::appendChild;
    
  private:
    void recalculateWidth(float left_margin, TreeItem* item);
    void recalculateY(TreeItem* item);

    DC& m_dc;
    float m_item_height;
    float m_indent_width;
  };

}}