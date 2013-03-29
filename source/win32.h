#pragma once
#include <Windows.h>

#ifndef HINST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

void ThrowLastError(const char* function_name);

template <typename T, typename Functor>
auto WithResource(const char* type, const char* name, Functor&& f) -> decltype(f(nullptr, 0))
{
  std::string problem;
  if(auto hrsc = FindResourceA(HINST_THISCOMPONENT, name, type))
  {
    if(auto hglob = LoadResource(HINST_THISCOMPONENT, hrsc))
    {
      if(auto begin = static_cast<const T*>(LockResource(hglob)))
      {
        return f(begin, SizeofResource(HINST_THISCOMPONENT, hrsc));
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
