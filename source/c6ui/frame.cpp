#include "frame.h"
#include "win32.h"
#include "mouse.h"
#include "app.h"
#include <WindowsX.h>
using namespace std;

namespace C6 { namespace UI
{
  Frame::Frame(const char* title, Factories& factories)
    : m_hmonitor(nullptr)
    , m_hwnd(nullptr)
    , m_dc(factories)
    , m_current_hover(this)
    , m_factories(factories)
  {
#ifdef C6UI_NO_TEXT
    auto dpi = std::make_tuple(96.f, 96.f);
#else
    auto dpi = m_factories.d2.getDesktopDpi();
#endif
    m_position.left = 0.f;
    m_position.top = 0.f;
    m_position.right = ceil(800.f * std::get<0>(dpi) / 96.f);
    m_position.bottom = ceil(600.f * std::get<1>(dpi) / 96.f);

    createWindow(title, WS_OVERLAPPEDWINDOW | WS_MAXIMIZE, CW_USEDEFAULT, CW_USEDEFAULT,
      static_cast<UINT>(m_position.right),
      static_cast<UINT>(m_position.bottom));

    ShowWindow(m_hwnd, SW_SHOWNORMAL);
    UpdateWindow(m_hwnd);
  }

  Frame::~Frame()
  {
  }

  void Frame::createWindow(const char* windowName, DWORD style, int x, int y, int width, int height)
  {
    WNDCLASSEXA wcex = {0};
    wcex.cbSize = sizeof(wcex);
    wcex.lpfnWndProc = Frame::WndProc;
    wcex.hInstance = HINST_THISCOMPONENT;
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpszClassName = "C6UIFrame";
    RegisterClassExA(&wcex);

    if(width != CW_USEDEFAULT)
    {
      RECT rect = {0, 0, width, height};
      AdjustWindowRect(&rect, style, FALSE);
      width = rect.right - rect.left;
      height = rect.bottom - rect.top;
    }

    m_hwnd = CreateWindowExA(0, wcex.lpszClassName, windowName, style, x, y, width, height, nullptr, nullptr, HINST_THISCOMPONENT, this);
    if(!m_hwnd)
      ThrowLastError("CreateWindowEx");
  }

  LRESULT Frame::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
  {
    if(message == WM_NCCREATE)
    {
      LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
      auto self = static_cast<Frame*>(pcs->lpCreateParams);
      self->m_hwnd = hwnd;
      ::SetWindowLongPtrA(hwnd, GWLP_USERDATA, (ULONG_PTR)self);
    }
    else if(auto self = (Frame*)::GetWindowLongPtrA(hwnd, GWLP_USERDATA))
    {
      try
      {
        LRESULT result = self->wndProc(message, wParam, lParam);
        if(message == WM_NCDESTROY)
        {
          ::SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
          self->m_hwnd = nullptr;
          delete self;
        }
        return result;
      }
      catch(exception& e)
      {
        auto title = GetTitle();
        auto msg = title + " has encountered an exception.\nThe cause is: ";
        msg += e.what();
        if(*msg.rbegin() != '.')
          msg += '.';
        ::MessageBoxA(hwnd, msg.c_str(), title.c_str(), MB_OK | MB_ICONEXCLAMATION);
      }
    }
    return DefWindowProcA(hwnd, message, wParam, lParam);
  }

  LRESULT Frame::wndProc(UINT message, WPARAM wParam, LPARAM lParam)
  {
    switch(message)
    {
    case WM_PAINT:
      onPaint();
      return 0;

    case WM_MOUSEWHEEL:
      m_current_hover->onMouseWheel(D2D1::Point2F(0.f, static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA));
      return 0;

    case WM_KEYDOWN:
      m_current_hover->onKeyDown(static_cast<int>(wParam));
      break;

    case WM_KEYUP:
      m_current_hover->onKeyUp(static_cast<int>(wParam));
      break;

    case WM_LBUTTONDOWN:
      return onMouseButtonDown(MouseButton::Left, MK_LBUTTON, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

    case WM_MBUTTONDOWN:
      return onMouseButtonDown(MouseButton::Middle, MK_MBUTTON, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

    case WM_RBUTTONDOWN:
      return onMouseButtonDown(MouseButton::Right, MK_RBUTTON, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

    case WM_LBUTTONDBLCLK:
      return onMouseButtonDown(MouseButton::LeftDouble, MK_LBUTTON, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

    case WM_MBUTTONDBLCLK:
      return onMouseButtonDown(MouseButton::MiddleDouble, MK_MBUTTON, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

    case WM_RBUTTONDBLCLK:
      return onMouseButtonDown(MouseButton::RightDouble, MK_RBUTTON, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MOUSEMOVE:
      return onMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

    case WM_DISPLAYCHANGE:
      resizeSwapchain();
      return 0;

    case WM_WINDOWPOSCHANGED:
      onWindowPosChanged(*(WINDOWPOS*)lParam);
      return 0;

    case WM_NCDESTROY:
      m_factories.d3.clearState();
#ifdef _DEBUG
      this->~Frame();
#endif
      PostQuitMessage(0);
      break;
    }
    return DefWindowProcA(m_hwnd, message, wParam, lParam);
  }

  LRESULT Frame::onMouseButtonDown(MouseButton::E button, WPARAM win_button, int x, int y)
  {
    if(!m_drag_handler)
    {
      if(m_drag_handler = onMouseDown(D2D1::Point2F(static_cast<float>(x), static_cast<float>(y)), button))
      {
        m_drag_handler->m_reference_value.x -= x;
        m_drag_handler->m_reference_value.y -= y;
      }
      m_drag_button = win_button;
      updateCursor(x, y);
      onMouseMove(win_button, x, y);
    }

    return 0;
  }

  LRESULT Frame::onMouseMove(WPARAM win_buttons, int x, int y)
  {
    if(m_drag_handler && !(win_buttons & m_drag_button))
    {
      decltype(m_drag_handler) drag_handler;
      swap(m_drag_handler, drag_handler);
      drag_handler->finishDrag();
    }

    if(m_drag_handler)
      m_drag_handler->updateDrag(D2D1::Point2F(m_drag_handler->m_reference_value.x + x, m_drag_handler->m_reference_value.y + y));
    else
      updateCursor(x, y);

    return 0;
  }

  void Frame::updateCursor(int x, int y)
  {
    auto new_hover = this->findWindowAt(D2D1::Point2F(static_cast<float>(x), static_cast<float>(y)));
    if(new_hover != m_current_hover)
    {
      m_current_hover->onMouseLeave();
      m_current_hover = new_hover;
      auto cursor = m_current_hover->onMouseEnter();

      LPTSTR win_cursor;
      switch(cursor)
      {
      case Cursor::Normal       : win_cursor = IDC_ARROW ; break;
      case Cursor::Hand         : win_cursor = IDC_HAND  ; break;
      case Cursor::ArrowEastWest: win_cursor = IDC_SIZEWE; break;
      default: __assume(0);
      }
      SetCursor(LoadCursor(nullptr, win_cursor));
    }
  }

  void Frame::onWindowPosChanged(WINDOWPOS& pos)
  {
    if(SWP_NOSIZE & ~pos.flags)
      resizeSwapchain();
    else if(SWP_NOMOVE & ~pos.flags)
      checkForMonitorChange();
  }

  void Frame::resizeSwapchain()
  {
    RECT rect;
    GetClientRect(m_hwnd, &rect);
    m_position.right = static_cast<float>(rect.right);
    m_position.bottom = static_cast<float>(rect.bottom);

    bool had_rt = m_dc.canDraw();
    m_dc.unlockSwapChain();
    m_hmonitor = nullptr;
    if(m_swapchain)
      m_swapchain.resizeBuffers(1, rect.right, rect.bottom, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
    if(had_rt)
      m_dc.acquireSwapChain(m_factories.d2, m_swapchain);
    checkForMonitorChange();

    refresh();
    resized();
  }

  void Frame::createSwapchain()
  {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    DXGI_SWAP_CHAIN_DESC desc = {0};
    desc.BufferDesc.Width = size.width;
    desc.BufferDesc.Height = size.height;
    desc.BufferDesc.RefreshRate.Numerator = 60;
    desc.BufferDesc.RefreshRate.Denominator = 1;
    desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 1;
    desc.OutputWindow = m_hwnd;
    desc.Windowed = true;
    desc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;

    m_swapchain = m_factories.dxgi.createSwapChain(m_factories.d3, &desc);
  }

  void Frame::checkForMonitorChange()
  {
    HMONITOR hmonitor = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
    if(m_hmonitor != hmonitor)
    {
      m_hmonitor = hmonitor;
      setTextRenderingParams();
      refresh();
    }
  }

  void Frame::setTextRenderingParams()
  {
#ifndef C6UI_NO_TEXT
    auto params = m_factories.dw.createMonitorRenderingParams(m_hmonitor);
    params = m_factories.dw.createCustomRenderingParams(params.getGamma(), params.getEnhancedContrast(), 1.f, params.getPixelGeometry(), DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL);
    m_dc.setTextRenderingParams(params);
#endif
  }

  void Frame::onPaint()
  {
    RECT rect;
#ifndef C6UI_NO_TEXT
    if(!m_factories.d2)
      return;
#endif
    if(!GetUpdateRect(m_hwnd, &rect, FALSE))
      return;

    if(!m_swapchain)
      createSwapchain();
    if(!m_dc.canDraw())
    {
      m_dc.acquireSwapChain(m_factories.d2, m_swapchain);
      setTextRenderingParams();
    }

    m_factories.d3.RSSetScissorRects(rect);
    m_dc.beginDraw();
    try
    {
      render(m_dc);
    }
    catch(...)
    {
      m_dc.discardDraw();
      throw;
    }
    m_dc.endDraw();
    m_factories.d3.flush();
    m_swapchain.present(0, 0);
    ValidateRect(m_hwnd, &rect);
  }
}}