#pragma once
#include "window.h"

namespace C6 { namespace UI
{
  class DragHandler
  {
  public:
    DragHandler(Window& window, D2D_POINT_2F reference_value);

    virtual ~DragHandler();

    virtual void updateDrag(D2D_POINT_2F position) = 0;
    virtual void finishDrag();

  private:
    friend class Frame;

    Window& m_window;
    D2D_POINT_2F m_reference_value;
  };

  template <typename Functor>
  std::unique_ptr<DragHandler> SimpleDragHandler(Window& window, D2D_POINT_2F reference_value, Functor&& updateDrag)
  {
    class FunctorDragHandler : public DragHandler
    {
    public:
      FunctorDragHandler(Window& window, D2D_POINT_2F reference_value, Functor&& updateDrag)
        : DragHandler(window, reference_value)
        , m_updateDrag(std::move(updateDrag))
      {
      }

      void updateDrag(D2D_POINT_2F position) override
      {
        return m_updateDrag(position);
      }

    private:
      Functor m_updateDrag;
    };

    return std::unique_ptr<DragHandler>(new FunctorDragHandler(window, reference_value, std::move(updateDrag)));
  }

  template <typename FunctorDrag, typename FunctorFinish>
  std::unique_ptr<DragHandler> SimpleDragHandler(Window& window, D2D_POINT_2F reference_value, FunctorDrag&& updateDrag, FunctorFinish&& finish)
  {
    class FunctorDragHandler : public DragHandler
    {
    public:
      FunctorDragHandler(Window& window, D2D_POINT_2F reference_value, FunctorDrag&& updateDrag, FunctorFinish&& finish)
        : DragHandler(window, reference_value)
        , m_updateDrag(std::move(updateDrag))
        , m_finish(std::move(finish))
      {
      }

      void updateDrag(D2D_POINT_2F position) override
      {
        return m_updateDrag(position);
      }

      void finishDrag() override
      {
        m_finish();
      }

    private:
      FunctorDrag m_updateDrag;
      FunctorFinish m_finish;
    };

    return std::unique_ptr<DragHandler>(new FunctorDragHandler(window, reference_value, std::move(updateDrag), std::move(finish)));
  }

  std::unique_ptr<DragHandler> NullDragHandler(Window& window);
}}
