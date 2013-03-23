#include "properties.h"
#include "mouse.h"

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
      ColourChannel(DC& dc, float& storage, uint32_t min_colour, uint32_t max_colour)
        : m_min_colour(min_colour)
        , m_max_colour(max_colour)
        , m_reference_min_colour(min_colour)
        , m_reference_max_colour(max_colour)
        , m_value(storage)
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
          m_value = m_our_value = (m_our_x - left) / (right - left);
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
        auto bar_inner = dc.rectangleOutline(0.f, bar, 0xFF909090);
        if(((m_min_colour & m_max_colour) >> 24) != 255)
          dc.alphaPattern(0.f, bar_inner);
        dc.rectangleH(0.25f, bar_inner, m_min_colour, m_max_colour);

        if(abs(m_value - m_our_value) > 1.f/2048.f)
        {
          m_our_value = m_value;
          m_our_x = floor(bar.left + (bar.right - bar.left) * m_value + 0.5f);
        }

        auto slider = D2D1::RectF(m_our_x - semi_slider_width - 1.f, m_position.bottom - m_slider_height, m_our_x + semi_slider_width, m_position.bottom);
        dc.sprite(50.f, slider, Sprites::SliderArrow, m_being_dragged ? 0xFFD0D0D0UL : 0xFFFFFFFFUL);
      }

    private:
      friend class ColourProperty;

      uint32_t m_min_colour;
      uint32_t m_max_colour;
      uint32_t m_reference_min_colour;
      uint32_t m_reference_max_colour;
      float& m_value;

      float m_our_value;
      float m_our_x;
      bool m_being_dragged;

      float m_bar_height;
      float m_slider_width;
      float m_slider_height;
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
