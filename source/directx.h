#pragma once
#include "wx.h"
#include <nice/d3.h>
#include <nice/dxgi.h>
#include <D3DCommon.h>

class Direct3DPanel : public wxPanel
{
public:
  Direct3DPanel(wxWindow *parent, wxWindowID winid = wxID_ANY);

  inline C6::D3::Device1 getDevice() {return m_device;}

  virtual void startRendering();
  virtual void finishRendering();

private:
  C6::D3::Device1 m_device;
  C6::D3::RenderTargetView m_rtv;
  C6::DXGI::SwapChain m_swap_chain;
};

#if defined(_DEBUG) || defined(PROFILE)

template<UINT TNameLength>
inline void SetDebugObjectName(C6::D3::DeviceChild resource, const char (&name)[TNameLength])
{
  resource.getRawInterface()->SetPrivateData(WKPDID_D3DDebugObjectName, TNameLength - 1, name);
}
void SetDebugObjectName(C6::D3::DeviceChild resource, std::string name);

#else
#define SetDebugObjectName(resource, name)
#endif
