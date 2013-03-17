#pragma once
#include "fs.h"
#include "c6ui/tree_control.h"

class FileTreeListener
{
public:
  virtual void onFileTreeActivation(std::string path) = 0;
};

class FileTree : public C6::UI::TreeControl
{
public:
  FileTree(Arena& arena, C6::UI::DC& dc, Essence::FileSource* mod_fs);

  void addListener(FileTreeListener* listener);
  void fireActivation(std::string path);

  Essence::FileSource* getFileSource() { return m_mod_fs; }

private:
  Essence::FileSource* m_mod_fs;
  FileTreeListener* m_listener;
};
