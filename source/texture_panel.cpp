#include "texture_panel.h"

TexturePanel::TexturePanel(C6::D3::ShaderResourceView texture, const uint32_t& exposure)
  : m_texture(texture)
  , m_exposure(exposure)
{
  auto desc = texture.getResource().queryInterface<C6::D3::Texture2D>().getDesc();
  m_width = static_cast<float>(desc.Width);
  m_height = static_cast<float>(desc.Height);
  m_position = D2D1::RectF(0.f, 0.f, m_width, m_height);
}

void TexturePanel::render(C6::UI::DC& dc)
{
  auto rect = D2D1::RectF(m_position.left, m_position.top, m_position.left + m_width, m_position.top + m_height);
  auto whiteness = m_exposure & 0xFF000000;
  whiteness |= (whiteness >> 8);
  whiteness |= (whiteness >> 16);
  dc.rectangle(2000.f, rect, whiteness);
  std::swap(rect.bottom, rect.top);
  dc.texture(1000.f, rect, m_texture.getRawInterface(), m_exposure | 0xFF000000);
}
