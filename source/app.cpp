#include "wx.h"

namespace
{
class coh2explorer : public wxApp
{
public:
    bool OnInit() override;
};
}

wxIMPLEMENT_APP(coh2explorer);

bool coh2explorer::OnInit()
{
  return true;
}