#include "png.h"

C6::D3::Texture2D LoadPng(C6::D3::Device1& d3, C6::WIC::ImagingFactory& wic, std::unique_ptr<MappableFile> file)
{
  auto file_contents = file->mapAll();
  auto stream = wic.createStream();
  HRESULT hr = stream.getRawInterface()->InitializeFromMemory(const_cast<WICInProcPointer>(file_contents.data()), file_contents.size());
  if(FAILED(hr))
    throw C6::COMException(hr, "IWICStream::InitializeFromMemory");
  auto frame = wic.createDecoderFromStream(stream, WICDecodeMetadataCacheOnDemand).getFrame(0);
  auto converter = wic.createFormatConverter();
  converter.initialize(frame, GUID_WICPixelFormat32bppPRGBA, WICBitmapDitherTypeNone, 0.f, WICBitmapPaletteTypeCustom);

  D3D10_TEXTURE2D_DESC desc;
  std::tie(desc.Width, desc.Height) = frame.getSize();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D10_USAGE_DYNAMIC;
  desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
  desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
  desc.MiscFlags = 0;

  auto tex = d3.createTexture2D(desc);
  auto mapped = tex.map(0, D3D10_MAP_WRITE_DISCARD, 0);
  hr = converter.getRawInterface()->CopyPixels(nullptr, mapped.RowPitch, mapped.RowPitch * desc.Height, static_cast<BYTE*>(mapped.pData));
  if(FAILED(hr))
    throw C6::COMException(hr, "IWICFormatConverter::CopyPixels");
  tex.unmap(0);

  return tex;
}
