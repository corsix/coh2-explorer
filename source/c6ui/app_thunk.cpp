#include "app.h"
#include "../win32.h"
#include <string>
using namespace C6;
using namespace C6::UI;
using namespace std;
#pragma comment(lib, "d2d1")
#pragma comment(lib, "d3d10_1")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dwrite")

namespace C6 { namespace UI
{
  static void InstantiateFactories(Factories& factories)
  {
    HRESULT hr;
#ifndef C6UI_NO_TEXT
    {
      ID2D1Factory* d2;
      hr = ::D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, &d2);
      if(!SUCCEEDED(hr)) throw COMException(hr, "D2D1CreateFactory");
      factories.d2 = D2::Factory(d2);
    }
    {
      IUnknown* dw = nullptr;
      hr = ::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &dw);
      if(!SUCCEEDED(hr)) throw COMException(hr, "DWriteCreateFactory");
      factories.dw = DW::Factory(static_cast<IDWriteFactory*>(dw));
    }
#endif
    {
      void* wic;
      hr = ::CoCreateInstance(CLSID_WICImagingFactory1, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory), &wic);
      if(!SUCCEEDED(hr)) throw COMException(hr, "CoCreateInstance(CLSID_WICImagingFactory)");
      factories.wic = WIC::ImagingFactory(static_cast<IWICImagingFactory*>(wic));
    }
    {
      void* dxgi;
      hr = ::CreateDXGIFactory1(__uuidof(IDXGIFactory1), &dxgi);
      if(!SUCCEEDED(hr)) throw COMException(hr, "CreateDXGIFactory1");
      factories.dxgi = DXGI::Factory1(static_cast<IDXGIFactory1*>(dxgi));
    }
    {
      ID3D10Device1* d3;
      auto adapter = factories.dxgi.enumAdapters1(0);
      UINT flags = D3D10_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
      flags |= D3D10_CREATE_DEVICE_DEBUG;
#endif
      hr = ::D3D10CreateDevice1(adapter.getRawInterface(), D3D10_DRIVER_TYPE_HARDWARE, nullptr, flags, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, &d3);
      if(!SUCCEEDED(hr)) throw COMException(hr, "D3D10CreateDevice1");
      factories.d3 = D3::Device1(d3);
    }
  }

  void CreateInitialWindow(Factories& factories);

  int main()
  {
    Factories factories;
    InstantiateFactories(factories);

    CreateInitialWindow(factories);
  
    for(MSG msg;;)
    {
      // We'd like to enter an alertable state in order to run any APCs, but
      // GetMessage doesn't do this. Hence MsgWaitForMultipleObjectsEx does
      // the waiting rather than GetMessage.
      if(MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLEVENTS, MWMO_ALERTABLE | MWMO_INPUTAVAILABLE) == WAIT_IO_COMPLETION)
        continue;

      switch(GetMessageA(&msg, nullptr, 0, 0))
      {
      case -1:
        ThrowLastError("GetMessage");
      case 0:
        return 0;
      default:
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
      }
    }
  }

  string GetTitle()
  {
    return "C6UI demo";
  }

}}

int WINAPI WinMain(HINSTANCE /* hInstance */, HINSTANCE /* hPrevInstance */, LPSTR /* lpCmdLine */, int /* nCmdShow */)
{
  HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
  try
  {
    auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if(!SUCCEEDED(hr)) throw COMException(hr, "CoInitializeEx");
    try
    {
      int result = C6::UI::main();
      _CrtDumpMemoryLeaks();
      return result;
    }
    catch(...)
    {
      CoUninitialize();
      throw;
    }
  }
  catch(const exception& e)
  {
    auto title = C6::UI::GetTitle();
    auto msg = title + " has encountered an uncaught top-level exception, and will therefore close.\nThe cause is: ";
    msg += e.what();
    if(*msg.rbegin() != '.')
      msg += '.';
    ::MessageBoxA(nullptr, msg.c_str(), title.c_str(), MB_OK | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
    return 1;
  }
}
