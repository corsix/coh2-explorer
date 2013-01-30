#include "wx.h"
#include "main_window.h"
#include <wx/cmdline.h>
namespace
{

class coh2explorer : public wxApp
{
public:
  bool OnInit() override
  {
    if(!wxApp::OnInit())
      return false;

    auto window = new MainWindow(m_module_path.c_str(), m_rgm_path.c_str());
    return window->Show();
  }

  void OnInitCmdLine(wxCmdLineParser& parser) override
  {
    parser.AddParam("<.module file path>");
    parser.AddParam("<.rgm file path>", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
  }

  bool OnCmdLineParsed(wxCmdLineParser& parser) override
  {
    m_module_path = parser.GetParam(0);
    if(parser.GetParamCount() > 1)
      m_rgm_path = parser.GetParam(1);
    return true;
  }

private:
  wxString m_module_path;
  wxString m_rgm_path;
};

wxIMPLEMENT_APP(coh2explorer);

} // anonymous namespace
