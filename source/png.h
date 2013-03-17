#pragma once
#include <nice/d3.h>
#include <nice/wic.h>
#include "mappable.h"

C6::D3::Texture2D LoadPng(C6::D3::Device1& d3, C6::WIC::ImagingFactory& wic, std::unique_ptr<MappableFile> file);
