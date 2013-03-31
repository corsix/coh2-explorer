#include "stdafx.h"
#include "properties.h"
#include "mouse.h"
#include "dc.h"

namespace C6 { namespace UI
{
  PropertyGroup::PropertyGroup(DC& dc, const wchar_t* title)
    : m_dc(dc)
    , m_title(title)
  {
    auto size = dc.measureText(Fonts::Headings, title);
    size.height = ceil(size.height * 1.25f);
    size.width += 16.f;
    m_title_size = size;
  }

  void PropertyGroup::addListener(PropertyListener* listener)
  {
    m_listeners.push_back(listener);
  }

  void PropertyGroup::render(DC& dc)
  {
    auto width = m_position.right - m_position.left;
    auto height = m_position.bottom - m_position.top;

    dc.text(100.f, D2D1::RectF(m_position.left, m_position.top, m_position.left + m_title_size.width, m_position.top + m_title_size.height), 0xFFFFFFFF, Fonts::Headings, m_title);
    __super::render(dc);
    auto r = m_position;
    r.top = r.bottom - 2.f;
    --r.bottom;
    dc.rectangle(0.f, r, 0xFF383838);
    r.bottom++, r.top++;
    dc.rectangle(0.f, r, 0xFF707070);
  }

  void PropertyGroup::firePropertyChanged()
  {
    for(auto listener : m_listeners)
      listener->onPropertyChanged();
  }

  void PropertyGroup::resized()
  {
    auto right = m_position.right - m_position.left - 8.f;
    float x = 8.f;
    float y = m_title_size.height;
    for(auto child : *ex(this))
    {
      auto& pos = child->m_position;
      auto height = pos.bottom - pos.top;
      pos.left = x;
      pos.right = right;
      pos.top = y + 5.f;
      pos.bottom = pos.top + height;
      child->resized();

      y = pos.bottom + 5.f;
    }
    m_position.bottom = m_position.top + y;
  }

  namespace
  {
#define INVALID_VALUE -123.45f

    class ColourChannel : public Window
    {
    public:
      ColourChannel(DC& dc, float& storage, uint32_t min_colour, uint32_t max_colour, float min_value = 0.f, float max_value = 1.f)
        : m_min_colour(min_colour)
        , m_max_colour(max_colour)
        , m_reference_min_colour(min_colour)
        , m_reference_max_colour(max_colour)
        , m_value(storage)
        , m_min_value(min_value)
        , m_max_value(max_value)
        , m_our_value(INVALID_VALUE)
        , m_being_dragged(false)
      {
        m_bar_height = ceil(12.f * dc.getScaleFactor());
        m_slider_width = floor(m_bar_height * .7f) * 2.f + 1.f;
        m_slider_height = m_slider_width - floor(m_slider_width * .1f + .5f);

        m_position.top = 0.f;
        m_position.bottom = floor((m_bar_height + m_slider_height) * 0.875f);
      }

      std::unique_ptr<DragHandler> onMouseDown(D2D_POINT_2F position, MouseButton::E button) override
      {
        if(button != MouseButton::Left)
          return __super::onMouseDown(position, button);

        m_being_dragged = true;
        return SimpleDragHandler(*this, position, [this](D2D_POINT_2F position){
          auto semi_slider_width = (m_slider_width - 1.f) * 0.5f;
          auto left = m_position.left + semi_slider_width + 1.f;
          auto right = m_position.right - semi_slider_width;
          m_our_x = (std::min)((std::max)(left, position.x), right);
          m_value = m_our_value = m_min_value + (m_our_x - left) / (right - left) * (m_max_value - m_min_value);
          static_cast<ColourProperty*>(m_parent)->firePropertyChanged();
          refresh();
        }, [this]{
          m_being_dragged = false;
          refresh();
        });
      }

      Cursor::E onMouseEnter() override
      {
        return Cursor::Hand;
      }

      void resized() override
      {
        m_our_value = INVALID_VALUE;
      }

    protected:
      void render(DC& dc)
      {
        auto semi_slider_width = (m_slider_width - 1.f) * 0.5f;
        auto bar = D2D1::RectF(m_position.left + semi_slider_width, m_position.top, m_position.right - semi_slider_width, m_position.top + m_bar_height);

        if(abs(m_value - m_our_value) > 1.f/2048.f)
        {
          m_our_value = m_value;
          m_our_x = floor(bar.left + (bar.right - bar.left) * (m_value - m_min_value) / (m_max_value - m_min_value) + 0.5f);
        }

        renderInner(dc, dc.rectangleOutline(0.f, bar, 0xFF909090));

        auto slider = D2D1::RectF(m_our_x - semi_slider_width - 1.f, m_position.bottom - m_slider_height, m_our_x + semi_slider_width, m_position.bottom);
        dc.sprite(50.f, slider, Sprites::SliderArrow, m_being_dragged ? 0xFFD0D0D0UL : 0xFFFFFFFFUL);
      }

      virtual void renderInner(DC& dc, D2D1_RECT_F bar_inner)
      {
        if(((m_min_colour & m_max_colour) >> 24) != 255)
          dc.alphaPattern(0.f, bar_inner);
        dc.rectangleH(0.25f, bar_inner, m_min_colour, m_max_colour);
      }

    protected:
      friend class ColourProperty;

      uint32_t m_min_colour;
      uint32_t m_max_colour;
      uint32_t m_reference_min_colour;
      uint32_t m_reference_max_colour;
      float& m_value;
      float m_min_value;
      float m_max_value;

      float m_our_value;
      float m_our_x;
      bool m_being_dragged;

      float m_bar_height;
      float m_slider_width;
      float m_slider_height;
    };

    class FloatChannel : public ColourChannel
    {
    public:
      FloatChannel(DC& dc, float& storage, float min_value, float max_value)
        : ColourChannel(dc, storage, 0, 0, min_value, max_value)
      {
        auto bar_height = ceil(dc.measureText(Fonts::Body, L"0123456789").height * 1.1f);
        m_position.bottom = m_position.bottom - m_bar_height + bar_height;
        m_bar_height = bar_height;
        m_min_text.format(dc, min_value);
        m_max_text.format(dc, max_value);
      }

      void renderInner(DC& dc, D2D1_RECT_F bar_inner) override
      {
        m_value_text.format(dc, m_value);

        auto min_rect = D2D1::RectF(bar_inner.left, bar_inner.top, bar_inner.left + m_min_text.width, bar_inner.bottom);
        auto max_rect = D2D1::RectF(bar_inner.right - m_max_text.width, bar_inner.top, bar_inner.right, bar_inner.bottom);
        auto val_rect = D2D1::RectF(m_our_x - m_value_text.width * .5f, bar_inner.top, m_our_x + m_value_text.width * .5f, bar_inner.bottom);

        if(min_rect.right <= val_rect.left)
        {
          dc.text(100.f, min_rect, 0xFF909090, Fonts::Body, m_min_text.buffer);
        }
        else
        {
          auto adj = (std::max)(0.f, min_rect.left - val_rect.left);
          val_rect.left += adj;
          val_rect.right += adj;
        }

        if(val_rect.right <= max_rect.left)
        {
          dc.text(100.f, max_rect, 0xFF909090, Fonts::Body, m_max_text.buffer);
        }
        else
        {
          auto adj = (std::min)(0.f, max_rect.right - val_rect.right);
          val_rect.left += adj;
          val_rect.right += adj;
        }

        dc.text(100.f, val_rect, 0xFFFFFFFF, Fonts::Body, m_value_text.buffer);
      }

    private:
      struct text_t
      {
        void format(DC& dc, float value)
        {
          swprintf_s(buffer, L"%.2f", value);
          width = ceil(dc.measureText(Fonts::Body, buffer).width * .5f) * 2.f;
        }

        wchar_t buffer[32];
        float width;
      } m_min_text, m_value_text, m_max_text;
    };
  }

  ColourProperty::ColourProperty(Arena& arena, DC& dc, const wchar_t* title)
    : PropertyGroup(dc, title)
    , m_arena(arena)
  {
  }

  static uint32_t lerp(uint32_t x, uint32_t y, float s)
  {
    uint32_t result = 0;
    for(int channel = 0; channel < 32; channel += 8)
    {
      auto x1 = (x >> channel) & 0xFF;
      auto y1 = (y >> channel) & 0xFF;
      auto r1 = x1 + static_cast<uint32_t>(floor(s * (static_cast<float>(y1) - static_cast<float>(x1))));
      result |= (r1 << channel);
    }
    return result;
  }

  template <typename Out, typename In>
  static Out implicit_cast(In&& in)
  {
    return in;
  }

  static uint32_t premult(uint32_t colour, uint32_t mask)
  {
    if(mask << 8)
      return colour | 0xFF000000;
    else
      return lerp(colour & 0xFF000000, colour, static_cast<float>(colour >> 24) / 255.f);
  }

  void ColourProperty::firePropertyChanged()
  {
    uint32_t combined = 0;
    uint32_t mask = 0x00FF0000;
    for(auto child : *ex(this))
    {
      auto colour = static_cast<ColourChannel*>(implicit_cast<Window*>(child));
      combined |= lerp(colour->m_reference_min_colour, colour->m_reference_max_colour, colour->m_value) & mask;
      mask = _rotr(mask, 8);
    }
    mask = 0x00FF0000;
    for(auto child : *ex(this))
    {
      auto colour = static_cast<ColourChannel*>(implicit_cast<Window*>(child));
      if((colour->m_reference_min_colour ^ colour->m_reference_max_colour) == mask)
      {
        colour->m_min_colour = premult((colour->m_reference_min_colour & mask) | (combined &~ mask), mask);
        colour->m_max_colour = premult((colour->m_reference_max_colour & mask) | (combined &~ mask), mask);
      }
      mask = _rotr(mask, 8);
    }
    refresh();
    __super::firePropertyChanged();
  }

  void ColourProperty::appendChannel(float& storage, uint32_t min_colour, uint32_t max_colour)
  {
    appendChild(m_arena.allocTrivial<ColourChannel>(m_dc, storage, min_colour, max_colour));
    resized();
  }

  void ColourProperty::appendScalar(float& storage, float min_value, float max_value)
  {
    appendChild(m_arena.allocTrivial<FloatChannel>(m_dc, storage, min_value, max_value));
    resized();
  }

  EnumProperty::EnumProperty(Arena& arena, DC& dc, const wchar_t* title, void*& storage)
    : PropertyGroup(dc, title)
    , m_arena(arena)
    , m_storage(storage)
  {
  }

  void EnumProperty::firePropertyChanged()
  {
    refresh();
    __super::firePropertyChanged();
  }

  namespace
  {
    class EnumCase : public Window
    {
    public:
      EnumCase(DC& dc, void*& storage, void* value, const wchar_t* title)
        : m_storage(storage)
        , m_value(value)
        , m_title(title)
      {
        auto size = dc.measureText(Fonts::Body, title);
        size.width = ceil(size.width * 1.3f);
        size.height = ceil(size.height * 1.3f);
        m_position = D2D1::RectF(0.f, 0.f, size.width, size.height);
      }

      std::unique_ptr<DragHandler> onMouseDown(D2D_POINT_2F position, MouseButton::E button) override
      {
        if(button != MouseButton::Left)
          return __super::onMouseDown(position, button);

        bool was_inside = false;
        auto old_value = m_storage;
        return SimpleDragHandler(*this, position, [=](D2D_POINT_2F position) mutable {
          bool inside = m_position.left <= position.x && position.x < m_position.right && m_position.top <= position.y && position.y < m_position.bottom;
          if(inside == was_inside)
            return;
          
          was_inside = inside;
          m_storage = inside ? m_value : old_value;
          static_cast<EnumProperty*>(m_parent)->firePropertyChanged();
        });
      }

      Cursor::E onMouseEnter() override
      {
        return Cursor::Hand;
      }

    protected:
      void render(DC& dc) override
      {
        bool is_active = m_storage == m_value;
        auto inner = dc.rectangleOutline(0.f, m_position, is_active ? 0xFF2B2B2B : 0xFF3E3E3E);
        dc.rectangle(0.f, D2D1::RectF(inner.left, inner.top, inner.right, inner.top + 1.f), is_active ? 0xFF323232 : 0xFF727272);
        inner.top += 1.f;
        if(is_active)
          dc.rectangleV(0.f, inner, 0xFF404040, 0xFF373737);
        else
          dc.rectangleV(0.f, inner, 0xFF636363, 0xFF5B5B5B);
        dc.text(100.f, m_position, 0xFFFFFFFF, Fonts::Body, m_title);
      }

    private:
      void*& m_storage;
      void* m_value;
      const wchar_t* m_title;
    };
  }

  void EnumProperty::appendCase(void* value, const wchar_t* title)
  {
    appendChild(m_arena.allocTrivial<EnumCase>(m_dc, m_storage, value, title));
    resized();
  }

  void PropertyGroupList::addListener(PropertyListener* listener)
  {
    m_listeners.push_back(listener);

    for(auto child : *ex(this))
      static_cast<PropertyGroup*>(implicit_cast<Window*>(child))->addListener(listener);
  }

  void PropertyGroupList::appendGroup(PropertyGroup* group)
  {
    for(auto listener : m_listeners)
      group->addListener(listener);
    appendChild(group);
  }

  void PropertyGroupList::resized()
  {
    auto right = m_position.right - m_position.left;
    float y = 0.f;
    for(auto child : *ex(this))
    {
      auto& pos = child->m_position;
      auto height = pos.bottom - pos.top;
      pos.left = 0.f;
      pos.right = right;
      pos.top = y;
      pos.bottom = pos.top + height;
      child->resized();

      y = pos.bottom;
    }
    m_position.bottom = m_position.top + y;
  }

  std::unique_ptr<DragHandler> PropertyGroupList::onMouseDown(D2D_POINT_2F position, MouseButton::E button)
  {
    auto handler = Window::onMouseDown(position, button);
    if(!handler)
      handler = createDragViewport();
    return move(handler);
  }
}}
