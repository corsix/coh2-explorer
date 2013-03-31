#pragma once
#include "window.h"
#include "scrolling_container.h"
#include "../arena.h"

namespace C6 { namespace UI
{
  class PropertyListener
  {
  public:
    virtual void onPropertyChanged() = 0;
  };

  class PropertyGroup : public Window
  {
  public:
    PropertyGroup(DC& dc, const wchar_t* title);
    void addListener(PropertyListener* listener);
    void firePropertyChanged();

    void resized() override;

  protected:
    void render(DC& dc) override;

    DC& m_dc;
  private:
    const wchar_t* m_title;
    std::vector<PropertyListener*> m_listeners;
    D2D_SIZE_F m_title_size;
  };

  class ColourProperty : public PropertyGroup
  {
  public:
    ColourProperty(Arena& arena, DC& dc, const wchar_t* title);
    void firePropertyChanged();

    void appendChannel(float& storage, uint32_t min_colour, uint32_t max_colour);
    void appendScalar(float& storage, float min_value, float max_value);

  private:
    Arena& m_arena; 
  };

  class EnumProperty : public PropertyGroup
  {
  public:
    EnumProperty(Arena& arena, DC& dc, const wchar_t* title, void*& storage);
    void firePropertyChanged();

    void appendCase(void* value, const wchar_t* title);

  private:
    Arena& m_arena;
    void*& m_storage;
  };

  class PropertyGroupList : public ScrollableWindow
  {
  public:
    void appendGroup(PropertyGroup* group);
    void addListener(PropertyListener* listener);

    void resized() override;
    std::unique_ptr<DragHandler> onMouseDown(D2D_POINT_2F position, MouseButton::E button) override;
  private:
    std::vector<PropertyListener*> m_listeners;

    using ScrollableWindow::appendChild;
  };
}}
