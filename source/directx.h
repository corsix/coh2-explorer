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

  void setDepthFormat(DXGI_FORMAT format);
  virtual void startRendering();
  virtual void finishRendering();

private:
  void createDepthStencilView();

  C6::D3::Device1 m_device;
  C6::D3::RenderTargetView m_rtv;
  C6::D3::DepthStencilView m_dsv;
  C6::DXGI::SwapChain m_swap_chain;
  C6::D3::Texture2D m_depth_buffer;
  DXGI_FORMAT m_depth_format;
};

#if defined(_DEBUG) || defined(PROFILE)

template<UINT TNameLength>
inline void SetDebugObjectName(C6::D3::DeviceChild resource, const char (&name)[TNameLength])
{
  resource.getRawInterface()->SetPrivateData(WKPDID_D3DDebugObjectName, TNameLength - 1, name);
}

template <typename T>
typename std::enable_if<sizeof(typename T::value_type)==1>::type
/* void */ SetDebugObjectName(C6::D3::DeviceChild resource, const T& name)
{
  resource.getRawInterface()->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data());
}

#else
#define SetDebugObjectName(resource, name)
#endif
