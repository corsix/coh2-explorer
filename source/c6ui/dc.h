#pragma once
#include "app.h"

namespace C6 { namespace UI
{
  namespace Sprites
  {
    enum E : uint32_t
    {
      SliderArrow        = 0x00003732UL,
      TreeArrowCollapsed = 0x0E001830UL,
      TreeArrowExpanded  = 0x14001830UL,
    };
  }

  namespace Fonts
  {
    enum E
    {
      Headings,
      Body,
      __count
    };
  }

  class DC
  {
  public:
    DC(Factories& factories);
    ~DC();

    D2D_SIZE_F measureText(Fonts::E font, const wchar_t* text);
    D2D_RECT_F getClipRectangle();
    D2D_RECT_F pushClipRect(D2D_RECT_F rect);
    void restoreClipRect(const D2D_RECT_F& rect);
    float getScaleFactor();

    void text(float z, const D2D_RECT_F& rect, uint32_t colour, Fonts::E font, const wchar_t* text);
    void rectangle(float z, const D2D_RECT_F& rect, uint32_t colour);
    void alphaPattern(float z, const D2D_RECT_F& rect);
    void rectangleH(float z, const D2D_RECT_F& rect, uint32_t colour_left, uint32_t colour_right);
    void rectangleV(float z, const D2D_RECT_F& rect, uint32_t colour_top, uint32_t colour_bottom);
    auto rectangleOutline(float z, const D2D_RECT_F& rect, uint32_t colour) -> D2D_RECT_F;
    void sprite(float z, const D2D_RECT_F& rect, Sprites::E which, uint32_t colour = 0xFFFFFFFFUL);
    void texture(float z, const D2D_RECT_F& rect, ID3D10ShaderResourceView* texture, uint32_t colour = 0xFFFFFFFFUL);
    void transform(float dx, float dy, float dz);

    D3D10_VIEWPORT pushCustomViewport(D2D_RECT_F rect);
    void restoreViewport(const D3D10_VIEWPORT& vp);

#pragma pack(push)
#pragma pack(1)
    struct Command
    {
      float z;
      uint32_t kind;
      uint32_t colour;
      union
      {
        uint32_t colour_secondary;
        uint32_t aux_command_index;
        uint32_t sprite;
      };
      D2D_RECT_F rect;
    };
    static_assert(sizeof(Command) == 32, "Command packing has gone wrong.");
#pragma pack(pop)

  private:
    friend class Frame;

    C6::DW::TextFormat createFont(DWRITE_FONT_WEIGHT weight, DWRITE_FONT_STRETCH stretch);

    Command* allocateCommand(float z, uint32_t kind, const D2D_RECT_F& rect);
    void flushCommands();
    void evaluateRectangles(Command* begin, Command* end);
    void evaluateTexts(Command* begin, Command* end);
    void evaluateTextures(Command* begin, Command* end);
    void evaluateSprites(Command* begin, Command* end);

    bool canDraw();
    void acquireSwapChain(C6::D2::Factory& d2, C6::DXGI::SwapChain swapchain);
    void unlockSwapChain();
    void setTextRenderingParams(C6::DW::RenderingParams params);

    D2D_RECT_F getClipRectangle(float dx, float dy);
    void beginDraw();
    void discardDraw();
    void endDraw() { flushCommands(); }

    Command* m_command_buffer;
    int m_command_buffer_size;
    int m_num_aux_commands;
    float m_dx, m_dy, m_dz;

    C6::D2::RenderTarget m_rt;
    C6::D2::SolidColorBrush m_brush;
    C6::DW::Factory m_dw;
    C6::DW::TextFormat m_tfmt[Fonts::__count];
    C6::D3::RenderTargetView m_rtv;
    C6::D3::DepthStencilView m_dsv;
    C6::D3::Device1 m_d3;
    C6::D3::Buffer m_cbuf;
    C6::D3::Buffer m_vbuf;
    C6::D3::BlendState m_blend_state;
    C6::D3::RasterizerState m_rasterizer_state;
    C6::D3::DepthStencilState m_ds_state;
    C6::D3::InputLayout m_rectangle_il;
    C6::D3::VertexShader m_rectangle_vs;
    C6::D3::GeometryShader m_rectangle_gs;
    C6::D3::PixelShader m_rectangle_ps;
    C6::D3::ShaderResourceView m_noise_srv;
    C6::D3::SamplerState m_noise_sampler;
    C6::D3::GeometryShader m_sprite_gs;
    C6::D3::PixelShader m_sprite_ps;
    C6::D3::ShaderResourceView m_sprite_srv;
    C6::D3::SamplerState m_sprite_sampler;
    C6::D3::VertexShader m_texture_vs;
    D3D10_VIEWPORT m_viewport;
    float m_scale_factor;
  };

}}