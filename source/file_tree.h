#pragma once
#include "wx.h"
#include "fs.h"
#include <wx/treectrl.h>

class FileTree : public wxPanel
{
public:
  FileTree(wxWindow* parent, wxWindowID winid, Essence::FileSource* mod_fs);

private:
  void populateChildren(wxTreeItemId parent, const std::string& path);

  Essence::FileSource* m_mod_fs;
  wxTreeCtrl* m_tree;
};

class FileTreeEvent : public wxCommandEvent
{
public:
  FileTreeEvent(wxEventType eventType, int winid, wxString filename, std::string path, Essence::FileSource* mod_fs);

  wxEvent* Clone() const override;
  const std::string& getPath() const;
  Essence::FileSource* getFileSource() const;
  const wxString& getFileName() const;

private:
  wxString m_filename;
  std::string m_path;
  Essence::FileSource* m_mod_fs;
};

wxDECLARE_EVENT(FILE_TREE_FILE_ACTIVATED, FileTreeEvent);
