#include "file_tree.h"
using namespace std;

wxDEFINE_EVENT(FILE_TREE_FILE_ACTIVATED, FileTreeEvent);

namespace
{
  class FileTreeItem : public wxTreeItemData
  {
  public:
    FileTreeItem(string path)
      : m_path(move(path))
      , m_once(false)
    {
    }

    const string& getPath() const { return m_path; }

    bool once()
    {
      if(m_once)
        return false;
      m_once = true;
      return true;
    }

  private:
    string m_path;
    bool m_once;
  };
}

FileTree::FileTree(wxWindow* parent, wxWindowID winid, Essence::FileSource* mod_fs)
  : wxPanel(parent, winid)
  , m_mod_fs(mod_fs)
{
  auto sizer = new wxBoxSizer(wxVERTICAL);
  m_tree = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_HAS_BUTTONS | wxTR_HIDE_ROOT | wxTR_LINES_AT_ROOT);
  m_tree->Bind(wxEVT_COMMAND_TREE_ITEM_EXPANDING, [this](wxTreeEvent& e)
  {
    if(auto data = static_cast<FileTreeItem*>(e.GetClientObject()))
    {
      if(data->once())
        populateChildren(e.GetItem(), data->getPath());
    }
  });
  m_tree->Bind(wxEVT_COMMAND_TREE_ITEM_ACTIVATED, [this](wxTreeEvent& e)
  {
    auto item = e.GetItem();
    if(m_tree->GetItemData(item))
    {
      e.Skip();
      return;
    }
    auto label = m_tree->GetItemText(item);
    string path;
    if(auto data = static_cast<FileTreeItem*>(m_tree->GetItemData(m_tree->GetItemParent(item))))
    {
      path = data->getPath();
      path += '/';
    }
    path += label;
    FileTreeEvent fte(FILE_TREE_FILE_ACTIVATED, GetId(), move(label), move(path), m_mod_fs);
    fte.SetEventObject(this);
    ProcessWindowEvent(fte);
  });
  sizer->Add(m_tree, 1, wxEXPAND);
  SetSizer(sizer);
  populateChildren(m_tree->AddRoot(L""), "");
}

void FileTree::populateChildren(wxTreeItemId parent, const string& path)
{
  // Note that PrependItem is called instead of AppendItem.
  // See http://blogs.msdn.com/b/oldnewthing/archive/2011/11/25/10241394.aspx for the reason.

  vector<string> names;
  m_mod_fs->getFiles(path, names);
  for(auto itr = names.rbegin(), end = names.rend(); itr != end; ++itr)
  {
    m_tree->PrependItem(parent, wxString(*itr), -1, -1, nullptr);
  }
  bool any = !names.empty();
  names.clear();
  m_mod_fs->getDirs(path, names);
  for(auto itr = names.rbegin(), end = names.rend(); itr != end; ++itr)
  {
    string child_path(path);
    if(!path.empty())
      child_path += '/';
    child_path += *itr;
    auto dir = m_tree->PrependItem(parent, wxString(*itr), -1, -1, new FileTreeItem(move(child_path)));
    m_tree->SetItemHasChildren(dir);
  }
  if(names.empty() && !any)
    m_tree->SetItemHasChildren(parent, false);
}

FileTreeEvent::FileTreeEvent(wxEventType eventType, int winid, wxString filename, std::string path, Essence::FileSource* mod_fs)
  : wxCommandEvent(eventType, winid)
  , m_filename(move(filename))
  , m_path(move(path))
  , m_mod_fs(mod_fs)
{
}

wxEvent* FileTreeEvent::Clone() const
{
  return new FileTreeEvent(GetEventType(), GetId(), m_filename, m_path, m_mod_fs);
}

const std::string& FileTreeEvent::getPath() const
{
  return m_path;
}

Essence::FileSource* FileTreeEvent::getFileSource() const
{
  return m_mod_fs;
}

const wxString& FileTreeEvent::getFileName() const
{
  return m_filename;
}
