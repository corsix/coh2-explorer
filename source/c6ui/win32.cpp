#include "win32.h"
#include <nice/com.h>

void ThrowLastError(const char* function_name)
{
  throw C6::COMException(HRESULT_FROM_WIN32(GetLastError()), function_name);
}
