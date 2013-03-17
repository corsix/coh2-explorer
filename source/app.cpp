#include "c6ui/app.h"
#include "main_window.h"
#include "containers.h"
#include <wx/cmdline.h>
#include <vector>
using namespace std;

namespace C6 { namespace UI
{
  vector<string> FetchCommandLine()
  {
    vector<string> result;
    int argc;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    
    vector<char> buf;
    for(auto arg : make_range(argv, argv + argc))
    {
      auto len = static_cast<int>(wcslen(arg));
      for(size_t i = 0; i < 2; ++i)
        buf.resize(WideCharToMultiByte(CP_THREAD_ACP, 0, arg, len, buf.data(), buf.size() * i, nullptr, nullptr));
      result.push_back(string(buf.begin(), buf.end()));
    }

    LocalFree(argv);
    return result;
  }
  
  void CreateInitialWindow(Factories& factories)
  {
    auto args = FetchCommandLine();

    if(args.size() < 2)
      throw std::runtime_error("Usage: " + args.at(0) + " <.module file path> [.rgm file path]");

    auto window = new MainWindow(factories, args[1].c_str(), args.size() >= 2 ? args[2].c_str() : nullptr);
  }

}}
