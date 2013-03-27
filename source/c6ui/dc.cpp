#include "stdafx.h"
#include "dc.h"
#include "app.h"
#include "../texture_loader.h"
#include "../mappable.h"
#include "../win32.h"

static C6::D3::DepthStencilView CreateDepthStencilView(C6::D3::Device& d3, D3D10_TEXTURE2D_DESC& desc)
{
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D10_USAGE_DEFAULT;
  desc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = 0;
  return d3.createDepthStencilView(d3.createTexture2D(desc));
}

namespace
{
  MappedMemory resource(const char* name)
  {
    std::string problem;

    if(auto hrsc = FindResourceA(HINST_THISCOMPONENT, name, "c6ui"))
    {
      if(auto hglob = LoadResource(HINST_THISCOMPONENT, hrsc))
      {
        if(auto begin = static_cast<const uint8_t*>(LockResource(hglob)))
        {
          auto end = begin + SizeofResource(HINST_THISCOMPONENT, hrsc);
          return MappedMemory(begin, end);
        }
        else
          problem = "lock";
      }
      else
        problem = "load";
    }
    else
      problem = "find";
     throw std::logic_error("Cannot " + problem + " resource `" + name + "'");
  }

  class ResourceFile : public MappableFile
  {
  public:
    ResourceFile(const char* name)
      : m_resource(resource(name))
    {
      m_size = m_resource.size();
    }

    MappedMemory map(uint64_t offset_begin, uint64_t offset_end) override
    {
      return MappedMemory(m_resource.begin + offset_begin, m_resource.begin + offset_end);
    }

  private:
    MappedMemory m_resource;
  };

  std::unique_ptr<ResourceFile> resourceFile(const char* name)
  {
    return std::unique_ptr<ResourceFile>(new ResourceFile(name));
  }
}

#define COMMAND_BUFFER_SIZE 65536

namespace { namespace CMD
{
  // A = num aux commands, A and K = kind, V = variant
  //                                 0xAKKKV;
  static const uint32_t Rectangle  = 0x00000;
  static const uint32_t RectangleV = 0x00001;
  static const uint32_t Sprite     = 0x00010;
  static const uint32_t Text       = 0x10000;
  static const uint32_t Texture    = 0x10010;
}}

namespace C6 { namespace UI
{
  float DC::getScaleFactor()
  {
    return m_scale_factor;
  }

  C6::DW::TextFormat DC::createFont(DWRITE_FONT_WEIGHT weight, DWRITE_FONT_STRETCH stretch)
  {
    auto tfmt = m_dw.createTextFormat(L"Segoe UI", weight, DWRITE_FONT_STYLE_NORMAL, stretch, 12.f * m_scale_factor, L"en-us");
    tfmt.setTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    tfmt.setParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    return tfmt;
  }

  DC::DC(Factories& factories)
    : m_dw(factories.dw)
    , m_d3(factories.d3)
    , m_command_buffer(static_cast<Command*>(VirtualAlloc(nullptr, COMMAND_BUFFER_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)))
    , m_command_buffer_size(0)
    , m_num_aux_commands(0)
    , m_dx(0.f)
    , m_dy(0.f)
    , m_dz(0.f)
  {
#ifdef C6UI_NO_TEXT
    m_scale_factor = 1.f;
#else
    auto dpi = factories.d2.getDesktopDpi();
    m_scale_factor = sqrt(std::get<0>(dpi) * std::get<1>(dpi)) / 96.f;

    m_tfmt[Fonts::Headings] = createFont(DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_SEMI_EXPANDED);
    m_tfmt[Fonts::Body]     = createFont(DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL);
#endif

    D3D10_BLEND_DESC blend_state = {0};
    blend_state.BlendEnable[0] = TRUE;
    blend_state.SrcBlend = D3D10_BLEND_ONE;
    blend_state.DestBlend = D3D10_BLEND_INV_SRC_ALPHA;
    blend_state.BlendOp = D3D10_BLEND_OP_ADD;
    blend_state.SrcBlendAlpha = D3D10_BLEND_ONE;
    blend_state.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
    blend_state.BlendOpAlpha = D3D10_BLEND_OP_ADD;
    blend_state.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;
    m_blend_state = m_d3.createBlendState(blend_state);

    D3D10_RASTERIZER_DESC rasterizer_state = {};
    rasterizer_state.FillMode = D3D10_FILL_SOLID;
    rasterizer_state.CullMode = D3D10_CULL_NONE;
    rasterizer_state.DepthBias = 0;
    rasterizer_state.DepthBiasClamp = 0.f;
    rasterizer_state.SlopeScaledDepthBias = 0.f;
    rasterizer_state.DepthClipEnable = FALSE;
    rasterizer_state.ScissorEnable = TRUE;
    rasterizer_state.MultisampleEnable = FALSE;
    rasterizer_state.AntialiasedLineEnable = FALSE;
    m_rasterizer_state = m_d3.createRasterizerState(rasterizer_state);

    D3D10_DEPTH_STENCIL_DESC ds_state = {};
    ds_state.DepthEnable = FALSE;
    ds_state.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ZERO;
    ds_state.StencilEnable = FALSE;
    m_ds_state = m_d3.createDepthStencilState(ds_state);

    D3D10_BUFFER_DESC cbuf;
    cbuf.ByteWidth = 16;
    cbuf.Usage = D3D10_USAGE_DYNAMIC;
    cbuf.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
    cbuf.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    cbuf.MiscFlags = 0;
    m_cbuf = m_d3.createBuffer(cbuf);

    D3D10_BUFFER_DESC vbuf;
    vbuf.ByteWidth = COMMAND_BUFFER_SIZE;
    vbuf.Usage = D3D10_USAGE_DYNAMIC;
    vbuf.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    vbuf.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    vbuf.MiscFlags = 0;
    m_vbuf = m_d3.createBuffer(vbuf);

    m_viewport.TopLeftX = 0;
    m_viewport.TopLeftY = 0;
    m_viewport.MinDepth = 0.f;
    m_viewport.MaxDepth = 1.f;

    D3D10_INPUT_ELEMENT_DESC rectangle_il[] = {
      {"K", 0, DXGI_FORMAT_R32_UINT,           0,  4, D3D10_INPUT_PER_VERTEX_DATA, 0},
      {"C", 0, DXGI_FORMAT_B8G8R8A8_UNORM,     0,  8, D3D10_INPUT_PER_VERTEX_DATA, 0},
      {"S", 0, DXGI_FORMAT_B8G8R8A8_UNORM,     0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0},
      {"P", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D10_INPUT_PER_VERTEX_DATA, 0}
    };
    m_rectangle_il = m_d3.createInputLayout(rectangle_il, resource("rectangle_vs.bin"));
    m_rectangle_vs = m_d3.createVertexShader(resource("rectangle_vs.bin"));
    m_rectangle_gs = m_d3.createGeometryShader(resource("rectangle_gs.bin"));
    m_rectangle_ps = m_d3.createPixelShader(resource("rectangle_ps.bin"));
    m_sprite_gs = m_d3.createGeometryShader(resource("sprite_gs.bin"));
    m_sprite_ps = m_d3.createPixelShader(resource("sprite_ps.bin"));
    m_texture_vs = m_d3.createVertexShader(resource("texture_vs.bin"));

    m_noise_srv = m_d3.createShaderResourceView(Essence::Graphics::LoadTexture(m_d3, resourceFile("noise.rgt")));

    D3D10_SAMPLER_DESC noise_sampler = {};
    noise_sampler.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
    noise_sampler.AddressU = D3D10_TEXTURE_ADDRESS_WRAP;
    noise_sampler.AddressV = D3D10_TEXTURE_ADDRESS_WRAP;
    noise_sampler.AddressW = D3D10_TEXTURE_ADDRESS_WRAP;
    noise_sampler.MaxAnisotropy = 1;
    noise_sampler.ComparisonFunc = D3D10_COMPARISON_NEVER;
    m_noise_sampler = m_d3.createSamplerState(noise_sampler);

    m_sprite_srv = m_d3.createShaderResourceView(Essence::Graphics::LoadTexture(m_d3, resourceFile("sprites.rgt")));

    D3D10_SAMPLER_DESC sprite_sampler = {};
    sprite_sampler.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
    sprite_sampler.AddressU = D3D10_TEXTURE_ADDRESS_WRAP;
    sprite_sampler.AddressV = D3D10_TEXTURE_ADDRESS_WRAP;
    sprite_sampler.AddressW = D3D10_TEXTURE_ADDRESS_WRAP;
    sprite_sampler.MaxAnisotropy = 1;
    sprite_sampler.ComparisonFunc = D3D10_COMPARISON_NEVER;
    m_sprite_sampler = m_d3.createSamplerState(sprite_sampler);
  }

  DC::~DC()
  {
    VirtualFree(m_command_buffer, 0, MEM_RELEASE);
  }

  bool DC::canDraw()
  {
#ifdef C6UI_NO_TEXT
    return m_dsv;
#else
    return m_rt;
#endif
  }

  void DC::acquireSwapChain(C6::D2::Factory& d2, C6::DXGI::SwapChain swapchain)
  {
    auto tex = swapchain.getBuffer<D3::Texture2D>(0);
    auto desc = tex.getDesc();
    m_viewport.Width = desc.Width;
    m_viewport.Height = desc.Height;
    m_rtv = m_d3.createRenderTargetView(tex);
    m_dsv = CreateDepthStencilView(m_d3, desc);
    tex = nullptr;

    auto cbuf = static_cast<float*>(m_cbuf.map(D3D10_MAP_WRITE_DISCARD, 0));
    cbuf[0] = 2.f / static_cast<float>(desc.Width);
    cbuf[1] = -2.f / static_cast<float>(desc.Height);
    cbuf[2] = -1.f;
    cbuf[3] = 1.f;
    m_cbuf.unmap();

#ifndef C6UI_NO_TEXT
    m_rt = d2.createDxgiSurfaceRenderTarget(swapchain.getBuffer<DXGI::Surface>(0),
      D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED)));
    m_rt.setAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    if(!m_brush)
      m_brush = m_rt.createSolidColorBrush(D2D1::ColorF(0));
#endif
  }

  void DC::unlockSwapChain()
  {
    m_rt = nullptr;
    m_rtv = nullptr;
    m_dsv = nullptr;
  }

  void DC::setTextRenderingParams(C6::DW::RenderingParams params)
  {
    if(m_rt)
    {
      m_rt.setTextRenderingParams(params);
      m_rt.setTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    }
  }

  void DC::transform(float dx, float dy, float dz)
  {
    m_dx += dx;
    m_dy += dy;
    m_dz += dz;
  }

  D2D_RECT_F DC::getClipRectangle()
  {
    return getClipRectangle(m_dx, m_dy);
  }

  D2D_RECT_F DC::getClipRectangle(float dx, float dy)
  {
    UINT num_rects = 1;
    RECT scissor;
    m_d3.getRawInterface()->RSGetScissorRects(&num_rects, &scissor);

    D2D_RECT_F result;
    result.left   = static_cast<float>(scissor.left  ) - dx;
    result.right  = static_cast<float>(scissor.right ) - dx;
    result.top    = static_cast<float>(scissor.top   ) - dy;
    result.bottom = static_cast<float>(scissor.bottom) - dy;
    return result;
  }

  D2D_RECT_F DC::pushClipRect(D2D_RECT_F rect)
  {
    auto current = getClipRectangle();
    rect.left   = (std::max)(current.left  , rect.left  );
    rect.top    = (std::max)(current.top   , rect.top   );
    rect.right  = (std::min)(current.right , rect.right );
    rect.bottom = (std::min)(current.bottom, rect.bottom);
    restoreClipRect(rect);

    return current;
  }

  void DC::restoreClipRect(const D2D_RECT_F& rect)
  {
    flushCommands();

    RECT scissor;
    scissor.left   = static_cast<LONG>(rect.left   + m_dx);
    scissor.right  = static_cast<LONG>(rect.right  + m_dx);
    scissor.top    = static_cast<LONG>(rect.top    + m_dy);
    scissor.bottom = static_cast<LONG>(rect.bottom + m_dy);
    m_d3.RSSetScissorRects(scissor);
  }

  namespace
  {
    struct TextCommandAux
    {
      const wchar_t* text;
    };

    static_assert(sizeof(TextCommandAux) <= sizeof(DC::Command), "TextCommandAux too big.");

    struct TextureCommandAux
    {
      ID3D10ShaderResourceView* texture;
    };

    static_assert(sizeof(TextureCommandAux) <= sizeof(DC::Command), "TextureCommandAux too big.");
  }

  void DC::text(float z, const D2D_RECT_F& rect, uint32_t colour, Fonts::E font, const wchar_t* text)
  {
#ifndef C6UI_NO_TEXT
    auto clip = getClipRectangle();
    if(rect.right <= clip.left || rect.left >= clip.right || rect.bottom <= clip.top || rect.top >= clip.bottom)
      return;

    auto cmd = allocateCommand(z, CMD::Text + static_cast<uint32_t>(font), rect);
    cmd->colour = colour;
    auto aux = reinterpret_cast<TextCommandAux*>(m_command_buffer + cmd->aux_command_index);
    aux->text = text;
#endif
  }

  void DC::texture(float z, const D2D_RECT_F& rect, ID3D10ShaderResourceView* texture, uint32_t colour)
  {
    auto cmd = allocateCommand(z, CMD::Texture, rect);
    cmd->colour = colour;
    auto aux = reinterpret_cast<TextureCommandAux*>(m_command_buffer + cmd->aux_command_index);
    aux->texture = texture;
  }

  void DC::alphaPattern(float z, const D2D_RECT_F& rect)
  {
    rectangle(z, rect, 0xFF000000);
    z += .125f;
    float yd = 5.f;
    for(float x = rect.left; x < rect.right; x += 5.f, yd = 5.f - yd)
    {
      for(float y = rect.top + yd; y < rect.bottom; y += 10.f)
      {
        rectangle(z, D2D1::RectF(x, y, (std::min)(x + 5.f, rect.right), (std::min)(y + 5.f, rect.bottom)), 0xFFFFFFFF);
      }
    }
  }

  void DC::rectangle(float z, const D2D_RECT_F& rect, uint32_t colour)
  {
    rectangleH(z, rect, colour, colour);
  }

  void DC::rectangleH(float z, const D2D_RECT_F& rect, uint32_t colour_left, uint32_t colour_right)
  {
    auto cmd = allocateCommand(z, CMD::Rectangle, rect);
    cmd->colour = colour_left;
    cmd->colour_secondary = colour_right;
  }

  void DC::rectangleV(float z, const D2D_RECT_F& rect, uint32_t colour_top, uint32_t colour_bottom)
  {
    auto cmd = allocateCommand(z, CMD::RectangleV, rect);
    cmd->colour = colour_top;
    cmd->colour_secondary = colour_bottom;
  }

  D2D_RECT_F DC::rectangleOutline(float z, const D2D_RECT_F& rect, uint32_t colour)
  {
    D2D_RECT_F result = rect;
    result.left++;
    result.right--;
    result.top++;
    result.bottom--;

    {
      D2D_RECT_F left = rect;
      left.right = result.left;
      rectangle(z, left, colour);
    }
    {
      D2D_RECT_F right = rect;
      right.left = result.right;
      rectangle(z, right, colour);
    }
    {
      D2D_RECT_F top = rect;
      top.bottom = result.top;
      rectangle(z, top, colour);
    }
    {
      D2D_RECT_F bottom = rect;
      bottom.top = result.bottom;
      rectangle(z, bottom, colour);
    }

    return result;
  }

  void DC::sprite(float z, const D2D_RECT_F& rect, Sprites::E which, uint32_t colour)
  {
    auto cmd = allocateCommand(z, CMD::Sprite, rect);
    cmd->colour = colour;
    cmd->sprite = static_cast<uint32_t>(which);
  }

  DC::Command* DC::allocateCommand(float z, uint32_t kind, const D2D_RECT_F& rect)
  {
    uint32_t num_aux = kind >> 16;
    if(m_command_buffer_size + m_num_aux_commands + static_cast<int>(num_aux) >= COMMAND_BUFFER_SIZE / sizeof(Command))
      flushCommands();

    auto cmd = m_command_buffer + m_command_buffer_size++;
    cmd->z = m_dz + z;
    cmd->kind = kind;
    cmd->rect = rect;
    cmd->rect.left += m_dx;
    cmd->rect.right += m_dx;
    cmd->rect.top += m_dy;
    cmd->rect.bottom += m_dy;
    if(num_aux)
    {
      m_num_aux_commands += static_cast<int>(num_aux);
      cmd->aux_command_index = (COMMAND_BUFFER_SIZE / sizeof(Command)) - m_num_aux_commands;
    }
    return cmd;
  }

  struct NullTerminated
  {
    NullTerminated(const wchar_t* text)
      : m_data(text)
      , m_size(wcslen(text))
    {
    }

    const wchar_t* data() { return m_data; }
    size_t size() { return m_size; }

  private:
    const wchar_t* m_data;
    size_t m_size;
  };

  D2D_SIZE_F DC::measureText(Fonts::E font, const wchar_t* text)
  {
#ifdef C6UI_NO_TEXT
    return D2D1::SizeF(wcslen(text) * 4.f, 12.f);
#else
    auto metrics = m_dw.createTextLayout(NullTerminated(text), m_tfmt[font]).getMetrics();
    return D2D1::SizeF(metrics.width, metrics.height);
#endif
  }

  static const float blend_factor[4] = {0};

  void DC::beginDraw()
  {
    restoreViewport(m_viewport);
    m_d3.OMSetRenderTargets(m_rtv, m_dsv);
  }

  D3D10_VIEWPORT DC::pushCustomViewport(D2D_RECT_F rect)
  {
    flushCommands();

    D3D10_VIEWPORT vp;
    vp.TopLeftX = (int)floor(rect.left + m_dx);
    vp.TopLeftY = (int)floor(rect.top + m_dy);
    vp.Width = (int)ceil(rect.right - rect.left);
    vp.Height = (int)ceil(rect.bottom - rect.top);
    vp.MinDepth = m_viewport.MinDepth;
    vp.MaxDepth = m_viewport.MaxDepth;
    m_d3.RSSetViewports(vp);

    return m_viewport;
  }

  void DC::restoreViewport(const D3D10_VIEWPORT& vp)
  {
    m_d3.GSSetConstantBuffers(0, m_cbuf);
    m_d3.RSSetViewports(vp);
    m_d3.RSSetState(m_rasterizer_state);
    m_d3.PSSetSamplers(0, m_sprite_sampler);
    m_d3.PSSetShaderResources(0, m_sprite_srv);
    m_d3.PSSetSamplers(1, m_noise_sampler);
    m_d3.PSSetShaderResources(1, m_noise_srv);
    m_d3.OMSetBlendState(m_blend_state, blend_factor, 0xffffffff);
    m_d3.OMSetDepthStencilState(m_ds_state, 0);
  }

  static void memcpy_sse(__m128* dst, const __m128* src, int n)
  {
    while(n--)
      _mm_stream_ps(reinterpret_cast<float*>(dst++), _mm_load_ps(reinterpret_cast<const float*>(src++)));
  }

  void DC::flushCommands()
  {
    if(m_command_buffer_size == 0)
      return;

    std::sort(m_command_buffer, m_command_buffer + m_command_buffer_size, [](Command& lhs, Command& rhs) -> bool
    {
      if(lhs.z < rhs.z)
        return true;
      if(lhs.z > rhs.z)
        return false;
      return lhs.kind < rhs.kind;
    });
    const auto size = m_command_buffer_size;
    memcpy_sse(static_cast<__m128*>(m_vbuf.map(D3D10_MAP_WRITE_DISCARD, 0)), reinterpret_cast<const __m128*>(m_command_buffer), size * (sizeof(Command) / sizeof(__m128))), m_vbuf.unmap();
    unsigned int stride = sizeof(Command);
    unsigned int offset = 0;
    m_d3.IASetVertexBuffers(0, m_vbuf, stride, offset);
    for(int first = 0; first < size; )
    {
      auto mask = ~static_cast<uint32_t>(15);
      auto kind = m_command_buffer[first].kind & mask;
      int last = first + 1;
      while(last < size && (m_command_buffer[last].kind & mask) == kind)
        ++last;
      auto begin = m_command_buffer + first;
      auto end = m_command_buffer + last;
      switch(kind)
      {
      case CMD::Rectangle: evaluateRectangles(begin, end); break;
      case CMD::Text: evaluateTexts(begin, end); break;
      case CMD::Texture: evaluateTextures(begin, end); break;
      case CMD::Sprite: evaluateSprites(begin, end); break;
      }
      first = last;
    }

    discardDraw();
  }

  void DC::discardDraw()
  {
    m_command_buffer_size = 0;
    m_num_aux_commands = 0;
  }

  void DC::evaluateRectangles(Command* begin, Command* end)
  {
    m_d3.IASetInputLayout(m_rectangle_il);
    m_d3.IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
    m_d3.VSSetShader(m_rectangle_vs);
    m_d3.GSSetShader(m_rectangle_gs);
    m_d3.PSSetShader(m_rectangle_ps);
    m_d3.draw(static_cast<unsigned int>(end - begin), static_cast<unsigned int>(begin - m_command_buffer));
  }

  void DC::evaluateSprites(Command* begin, Command* end)
  {
    m_d3.IASetInputLayout(m_rectangle_il);
    m_d3.IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
    m_d3.VSSetShader(m_rectangle_vs);
    m_d3.GSSetShader(m_sprite_gs);
    m_d3.PSSetShader(m_sprite_ps);
    m_d3.draw(static_cast<unsigned int>(end - begin), static_cast<unsigned int>(begin - m_command_buffer));
  }

  void DC::evaluateTextures(Command* begin, Command* end)
  {
    m_d3.IASetInputLayout(m_rectangle_il);
    m_d3.IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
    m_d3.VSSetShader(m_texture_vs);
    m_d3.GSSetShader(m_sprite_gs);
    m_d3.PSSetShader(m_sprite_ps);
    for(auto cmd = begin; cmd != end; ++cmd)
    {
      auto aux = reinterpret_cast<TextureCommandAux*>(m_command_buffer + cmd->aux_command_index);
      m_d3.getRawInterface()->PSSetShaderResources(0, 1, &aux->texture);
      m_d3.draw(1, static_cast<unsigned int>(cmd - m_command_buffer));
    }
    m_d3.PSSetShaderResources(0, m_sprite_srv);
  }

  void DC::evaluateTexts(Command* begin, Command* end)
  {
    if(!m_rt)
      return;

    m_rt.getRawInterface()->BeginDraw();
    m_rt.pushAxisAlignedClip(getClipRectangle(0.f, 0.f), D2D1_ANTIALIAS_MODE_ALIASED);
    for(auto cmd = begin; cmd != end; ++cmd)
    {
      m_brush.setColor(D2D1::ColorF(cmd->colour));
      auto aux = reinterpret_cast<TextCommandAux*>(m_command_buffer + cmd->aux_command_index);
      m_rt.getRawInterface()->DrawText(aux->text, wcslen(aux->text), m_tfmt[cmd->kind & 0xF].getRawInterface(), cmd->rect, m_brush.getRawInterface(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    m_rt.popAxisAlignedClip();
    auto hr = m_rt.getRawInterface()->EndDraw();
    if(hr == D2DERR_RECREATE_TARGET)
      m_rt = nullptr;
    else if(!SUCCEEDED(hr))
      throw COMException(hr, "ID2D1RenderTarget::EndDraw");
  }
}}
