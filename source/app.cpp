#include "wx.h"
#include "main_window.h"
namespace
{

class coh2explorer : public wxApp
{
public:
  bool OnInit() override
  {
    auto window = new MainWindow();
    return window->Show();
  }
};

wxIMPLEMENT_APP(coh2explorer);

} // anonymous namespace
