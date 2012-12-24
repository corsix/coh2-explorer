#pragma once
#include "wx.h"
#include <nice/d3.h>
#include <nice/dxgi.h>

class Direct3DPanel : public wxPanel
{
public:
  Direct3DPanel(wxWindow *parent, wxWindowID winid = wxID_ANY);

  inline C6::D3::Device1 getDevice() {return m_device;}

  void startRendering();
  void finishRendering();

private:
  C6::D3::Device1 m_device;
  C6::D3::RenderTargetView m_rtv;
  C6::DXGI::SwapChain m_swap_chain;
};
