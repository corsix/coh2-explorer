#include "texture_panel.h"

TexturePanel::TexturePanel(C6::D3::ShaderResourceView texture)
  : m_texture(texture)
{
  auto desc = texture.getResource().queryInterface<C6::D3::Texture2D>().getDesc();
  m_width = static_cast<float>(desc.Width);
  m_height = static_cast<float>(desc.Height);
  m_position = D2D1::RectF(0.f, 0.f, m_width, m_height);
}

void TexturePanel::render(C6::UI::DC& dc)
{
  auto rect = D2D1::RectF(m_position.left, m_position.top, m_position.left + m_width, m_position.top + m_height);
  dc.rectangle(0.f, rect, 0xFF000000UL); // Black background for transparent bits
  std::swap(rect.bottom, rect.top);
  dc.texture(1000.f, rect, m_texture.getRawInterface());
}
