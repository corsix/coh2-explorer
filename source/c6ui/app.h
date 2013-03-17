#pragma once
#include <nice/d2.h>
#include <nice/d3.h>
#include <string>

namespace C6 { namespace UI
{
  std::string GetTitle();

  struct Factories
  {
    C6::D2::Factory d2;
    C6::D3::Device1 d3;
    C6::DW::Factory dw;
    C6::WIC::ImagingFactory wic;
    C6::DXGI::Factory1 dxgi;
  };
}}
