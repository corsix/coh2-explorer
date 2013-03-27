#pragma once
#include "window.h"
#include "dc.h"

namespace C6 { namespace UI
{
  class Frame : public Window
  {
  public:
    Frame(const char* title, Factories& factories);
    virtual ~Frame();

    HWND getHwnd() { return m_hwnd; }
    DC& getDC() { return m_dc; }

  private:
    void createWindow(const char* windowName, DWORD style, int x, int y, int width, int height);
           LRESULT          wndProc(           UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void onWindowPosChanged(WINDOWPOS& pos);
    LRESULT onMouseButtonDown(MouseButton::E button, WPARAM win_button, int x, int y);
    LRESULT onMouseMove(WPARAM win_buttons, int x, int y);
    void resizeSwapchain();
    void checkForMonitorChange();
    void createSwapchain();
    void onPaint();
    void setTextRenderingParams();
    void updateCursor(int x, int y);

    C6::DXGI::SwapChain m_swapchain;
    HMONITOR m_hmonitor;
    HWND m_hwnd;
    DC m_dc;

    Window* m_current_hover;
    std::unique_ptr<DragHandler> m_drag_handler;
    WPARAM m_drag_button;

    Factories& m_factories;
  };
}}
