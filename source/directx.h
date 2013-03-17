#pragma once
#include <nice/d3.h>
#include <nice/dxgi.h>
#include <D3DCommon.h>

#if defined(_DEBUG) || defined(PROFILE)
#pragma comment(lib, "dxguid")

template<UINT TNameLength>
inline void SetDebugObjectName(C6::D3::DeviceChild resource, const char (&name)[TNameLength])
{
  resource.getRawInterface()->SetPrivateData(WKPDID_D3DDebugObjectName, TNameLength - 1, name);
}

template <typename T>
typename std::enable_if<sizeof(typename T::value_type)==1>::type
/* void */ SetDebugObjectName(C6::D3::DeviceChild resource, const T& name)
{
  resource.getRawInterface()->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data());
}

#else
#define SetDebugObjectName(resource, name)
#endif
