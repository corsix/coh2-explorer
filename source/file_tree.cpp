#include "file_tree.h"
#include "c6ui/mouse.h"
#include <algorithm>
using namespace std;
using namespace C6::UI;

namespace
{
  class FileTreeItem : public TreeItem
  {
  public:
    FileTreeItem(const wchar_t* label, const char* path)
      : TreeItem(label)
      , m_path(path)
    {
    }

    std::unique_ptr<DragHandler> onMouseDown(D2D_POINT_2F position, MouseButton::E button)
    {
      if(button == MouseButton::Left)
      {
        static_cast<FileTree*>(m_parent)->fireActivation(m_path);
        return nullptr;
      }
      else
        return __super::onMouseDown(position, button);
    }

  private:
    const char* m_path;
  };

  class DirectoryTreeItem : public TreeItem
  {
  public:
    DirectoryTreeItem(Arena& arena, const wchar_t* label, const char* path)
      : TreeItem(label)
      , m_arena(arena)
      , m_path(path)
      , m_path_len(strlen(path))
    {
      m_has_children = true;
    }

    void addChildrenAsRootsOf(TreeControl& tc)
    {
      m_parent = &tc;
      populateChildren();
      for(auto child : m_children)
        tc.appendRoot(*child);
    }

  protected:
    void populateChildren()
    {
      auto fs = static_cast<FileTree*>(m_parent)->getFileSource();
      vector<string> dirs, files;
      fs->getDirs(m_path, dirs);
      fs->getFiles(m_path, files);
      auto count = dirs.size() + files.size();
      if(count == 0)
      {
        m_has_children = false;
        return;
      }

      m_children.recreate(&m_arena, count);
      count = 0;
      for(auto& dir : dirs)
        m_children[count++] = m_arena.allocTrivial<DirectoryTreeItem>(m_arena, labelFor(dir), pathFor(dir, '/'));
      for(auto& file : files)
        m_children[count++] = m_arena.allocTrivial<FileTreeItem>(labelFor(file), pathFor(file, 0));
    }

    const char* pathFor(const string& s, char extra)
    {
      auto path = m_arena.mallocArray<char>(m_path_len + s.size() + 2);
      memcpy(path, m_path, m_path_len);
      memcpy(path + m_path_len, s.data(), s.size());
      path[m_path_len + s.size()] = extra;
      return path;
    }

    const wchar_t* labelFor(const string& s)
    {
      auto label = m_arena.mallocArray<wchar_t>(s.size() + 1);
      transform(s.begin(), s.end(), label, [](char c) -> wchar_t { return c; });
      return label;
    }

  private:
    Arena& m_arena;
    const char* m_path;
    size_t m_path_len;
  };
}

FileTree::FileTree(Arena& arena, DC& dc, Essence::FileSource* mod_fs)
  : TreeControl(dc)
  , m_mod_fs(mod_fs)
  , m_listener(nullptr)
{
  DirectoryTreeItem root(arena, L"", "");
  root.addChildrenAsRootsOf(*this);
}

void FileTree::addListener(FileTreeListener* listener)
{
  if(m_listener != nullptr)
    throw std::logic_error("YAGNI: More than one FileTree listener");
  m_listener = listener;
}

void FileTree::fireActivation(string path)
{
  if(m_listener)
    m_listener->onFileTreeActivation(move(path));
}
