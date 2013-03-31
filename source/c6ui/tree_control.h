#pragma once
#include "scrolling_container.h"
#include "../arena.h"

namespace C6 { namespace UI
{
  namespace internal
  {
    template <size_t N> struct int_t {};
    template <> struct int_t< 8> { typedef  int8_t type; };
    template <> struct int_t<16> { typedef int16_t type; };
    template <> struct int_t<32> { typedef int32_t type; };
  }
  typedef internal::int_t<CHAR_BIT * sizeof(bool)>::type tristate_bool_t;

  namespace TreeItemHitTest
  {
    enum E
    {
      LeftMargin,
      Icon,
      Expander,
      Label,
    };
  }

  class TreeItem : public Window
  {
  public:
    TreeItem(const wchar_t* text, tristate_bool_t* tick_state = nullptr);
    bool isExpanded() const { return m_is_expanded; }
    TreeItemHitTest::E hitTest(D2D_POINT_2F position) const;

    std::unique_ptr<DragHandler> onMouseDown(D2D_POINT_2F position, MouseButton::E button) override;
    Cursor::E onMouseEnter() override;
    void onMouseLeave() override;

  protected:
    void render(DC& dc) override;
    virtual void populateChildren();

    bool m_has_children;
    ArenaArray<TreeItem*> m_children;
    tristate_bool_t* m_tick_state;
    const wchar_t* m_text;
  private:
    TreeItem* m_next_logical_sibling;
    TreeItem* m_logical_parent;
    float m_left_margin;
    float m_text_right_edge;
    bool m_is_expanded;
    bool m_has_ever_expanded;
    bool m_is_hovered;

    friend class TreeControl;
    friend class BooleanTreeControl;
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
    float m_margin_coefficient;
    using Window::appendChild;
    
  private:
    void recalculateWidth(float left_margin, TreeItem* item);
    void recalculateY(TreeItem* item);
    

    DC& m_dc;
    float m_item_height;
    float m_indent_width;
  };

  class PropertyListener;

  class BooleanTreeControl : public TreeControl
  {
  public:
    BooleanTreeControl(DC& dc);

    void addListener(PropertyListener* listener);

    void toggleTickState(TreeItem& item);
    void recomputeStates();
  protected:
    void render(DC& dc) override;
  private:
    tristate_bool_t recomputeStates(TreeItem& item);
    void setTickStateDownward(TreeItem& item, bool state);
    void setTickStateUpward(TreeItem* item);

    std::vector<PropertyListener*> m_listeners;
    bool m_recompute_states_on_render;
  };

}}