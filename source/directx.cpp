#include "directx.h"
#pragma comment(lib, "d3d10_1")
#if defined(_DEBUG) || defined(PROFILE)
#pragma comment(lib, "dxguid")
#endif
using namespace std;
using namespace C6;

static tuple<D3::Device1, DXGI::SwapChain> InitDirect3D(HWND hWnd)
{
  DXGI_SWAP_CHAIN_DESC desc = {0};
  desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = 1;
  desc.OutputWindow = hWnd;
  desc.Windowed = TRUE;

  IDXGISwapChain* swapChain = nullptr;
  ID3D10Device1* device = nullptr;

  HRESULT hr = D3D10CreateDeviceAndSwapChain1(nullptr, D3D10_DRIVER_TYPE_HARDWARE, nullptr,
    D3D10_CREATE_DEVICE_BGRA_SUPPORT, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, &desc,
    &swapChain, &device);

  if(FAILED(hr))
    throw COMException(hr, "D3D10CreateDeviceAndSwapChain1");

  return make_tuple(D3::Device1(device), DXGI::SwapChain(swapChain));
}

Direct3DPanel::Direct3DPanel(wxWindow *parent, wxWindowID winid)
  : wxPanel(parent, winid)
  , m_depth_format(DXGI_FORMAT_UNKNOWN)
{
  tie(m_device, m_swap_chain) = InitDirect3D(GetHWND());
  Bind(wxEVT_SIZE, [&](wxSizeEvent& e) {
    m_rtv = nullptr;
    m_dsv = nullptr;
    m_swap_chain.resizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
    e.Skip();
  });
}

void Direct3DPanel::setDepthFormat(DXGI_FORMAT format)
{
  m_depth_format = format;
  m_depth_buffer = nullptr;
  m_dsv = nullptr;
}

void Direct3DPanel::createDepthStencilView()
{
  auto desc = m_swap_chain.getBuffer<D3::Texture2D>(0).getDesc();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = m_depth_format;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D10_USAGE_DEFAULT;
  desc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = 0;
  m_depth_buffer = m_device.createTexture2D(desc);
  m_dsv = m_device.createDepthStencilView(m_depth_buffer);
}

void Direct3DPanel::startRendering()
{
  if(!m_rtv)
    m_rtv = m_device.createRenderTargetView(m_swap_chain.getBuffer<D3::Texture2D>(0));
  if(m_depth_format != DXGI_FORMAT_UNKNOWN)
  {
    if(!m_dsv)
      createDepthStencilView();
    m_device.clearDepthStencilView(m_dsv, D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, 1.f, 0);
  }

  const float bg[] = {0.f, 0.f, 0.f, 1.f};
  m_device.clearRenderTargetView(m_rtv, bg);
  m_device.OMSetRenderTargets(m_rtv, m_dsv);

  auto size = GetClientSize();
  D3D10_VIEWPORT vp;
  vp.Width = size.GetWidth();
  vp.Height = size.GetHeight();
  vp.MinDepth = 0.0f;
  vp.MaxDepth = 1.0f;
  vp.TopLeftX = 0;
  vp.TopLeftY = 0;
  m_device.RSSetViewports(vp);
}

void Direct3DPanel::finishRendering()
{
  m_swap_chain.present(0, 0);
  m_device.clearState();
}

#if defined(_DEBUG) || defined(PROFILE)
void SetDebugObjectName(C6::D3::DeviceChild resource, std::string name)
{
  resource.getRawInterface()->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.c_str());
}
#endif
