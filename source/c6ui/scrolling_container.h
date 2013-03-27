#pragma once
#include "window.h"

class Arena;

namespace C6 { namespace UI
{
  class ScrollingContainer;

  class ScrollBar : public Window
  {
  public:
    ScrollBar(D2D_POINT_2F axis, float& scroll_offset, float& viewport_length, float& content_length);

    std::unique_ptr<DragHandler> onMouseDown(D2D_POINT_2F position, MouseButton::E button) override;
    Cursor::E onMouseEnter() override;

    float getScroll() const { return m_scroll_offset; }
    void scrollBy(float delta);
    void scrollTo(float pos);

  private:
    D2D_POINT_2F m_axis;
    float& m_scroll_offset;
    float& m_viewport_length;
    float& m_content_length;
    float m_initial_margin;
    float m_shaft_length;
    float m_final_margin;
    float m_thumb_length;
    float m_thumb_center;

    void render(DC& dc) override;

    void clampScrollOffset();
    void recalcuatePosition(float shaft_size, float final_margin);
    friend class ScrollingContainer;
  };

  class ScrollableWindow : public Window
  {
  public:
    ScrollableWindow();
    ScrollingContainer* wrapInScrollingContainer(Arena& arena);
    ScrollingContainer* getContainer() { return m_container; }

    std::unique_ptr<DragHandler> createDragViewport();
    std::unique_ptr<DragHandler> onMouseDown(D2D_POINT_2F position, MouseButton::E button) override;
  protected:
    ScrollingContainer* m_container;

  private:
    friend class ScrollingContainer;
  };

  class ScrollingContainer : public Window
  {
  public:
    ScrollingContainer(Arena& arena, ScrollableWindow* content);
    void setContentSize(D2D_SIZE_F content_size);
    auto getContentSize() -> const D2D_SIZE_F& {return m_content_size;}
    void setAlignment(float alignment);
    auto getAlignment() const -> float { return m_alignment; }
    bool areScrollBarsInUse() const { return m_bars_visible != 0; }
    auto getScroll() const -> D2D_POINT_2F;
    void scrollTo(D2D_POINT_2F point);

    void resized() override;
    void onMouseWheel(D2D_POINT_2F delta) override;
    void onKeyDown(int vk) override;

  protected:
    void render(DC& dc) override;

  private:
    ScrollableWindow* m_content;
    D2D_SIZE_F m_content_size;
    D2D_SIZE_F m_viewport_size;
    ScrollBar* m_bottom;
    ScrollBar* m_right;
    int m_bars_visible;
    float m_alignment;
  };
}}
