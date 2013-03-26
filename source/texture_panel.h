#pragma once
#include "c6ui/scrolling_container.h"

class TexturePanel : public C6::UI::ScrollableWindow
{
public:
  TexturePanel(C6::D3::ShaderResourceView texture, const uint32_t& exposure);

protected:
  void render(C6::UI::DC& dc) override;

private:
  C6::D3::ShaderResourceView m_texture;
  const uint32_t& m_exposure;
  float m_width;
  float m_height;
};
