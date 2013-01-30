#include "fs.h"
#include <algorithm>
#include <map>
#include <nice/com.h>
using namespace std;

namespace
{
  string normalise_path(const string& path)
  {
    string norm_path(path);
    transform(norm_path.cbegin(), norm_path.cend(), norm_path.begin(), [](char c)
    {
      return (c == '/') ? '\\' : static_cast<char>(tolower(c));
    });
    if(!norm_path.empty() && *norm_path.rbegin() == '\\')
      norm_path.pop_back();
    return norm_path;
  }

  void uniqify(vector<string>& names)
  {
    sort(names.begin(), names.end());
    names.erase(unique(names.begin(), names.end()), names.end());
  }

  class AggregateFileSource : public Essence::FileSource
  {
  public:
    unique_ptr<MappableFile> readFile(const string& path) override
    {
      auto norm_path = normalise_path(path);
      for(auto itr = m_sources.cbegin(), end = m_sources.cend(); itr != end; ++itr)
      {
        if(auto file = (**itr).readFile(norm_path))
          return move(file);
      }
      throw C6::CreateFileException(path.c_str());
    }

    void getFiles(const string& path, vector<string>& files) override
    {
      auto norm_path = normalise_path(path);
      for(auto itr = m_sources.cbegin(), end = m_sources.cend(); itr != end; ++itr)
        (**itr).getFiles(norm_path, files);
      uniqify(files);
    }

    void getDirs(const string& path, vector<string>& dirs) override
    {
      auto norm_path = normalise_path(path);
      for(auto itr = m_sources.cbegin(), end = m_sources.cend(); itr != end; ++itr)
        (**itr).getDirs(norm_path, dirs);
      uniqify(dirs);
    }

    void appendSource(FileSource* fs)
    {
      m_sources.push_back(fs);
    }

  private:
    vector<FileSource*> m_sources;
  };

  class ChRootFileSource : public Essence::FileSource
  {
  public:
    ChRootFileSource(FileSource* base, string root)
      : m_base(base)
      , m_root(move(root))
    {
    }

    unique_ptr<MappableFile> readFile(const string& path) override
    {
      return m_base->readFile(m_root + path);
    }

    void getFiles(const string& path, vector<string>& files) override
    {
      return m_base->getFiles(m_root + path, files);
    }

    void getDirs(const string& path, vector<string>& dirs) override
    {
      return m_base->getDirs(m_root + path, dirs);
    }

  private:
    FileSource* m_base;
    string m_root;
  };
}

namespace
{
  struct IniKey
  {
    IniKey(const char* name_, unsigned index_) : name(name_), index(index_) {}

    const char* name;
    unsigned index;
  };

  bool operator== (const IniKey& lhs, const IniKey& rhs)
  {
    return lhs.index == rhs.index && !strcmp(lhs.name, rhs.name);
  }

  bool operator< (const IniKey& lhs, const IniKey& rhs)
  {
    return (lhs.index < rhs.index) || (lhs.index == rhs.index && (strcmp(lhs.name, rhs.name) < 0));
  }

  bool is_newline(char c)
  {
    return c == '\r' || c == '\n';
  }

  bool is_white(char c)
  {
    return is_newline(c) || c == ' ' || c == '\t';
  }

  const char* MakeValue(Arena* arena, const char* begin, const char* end)
  {
    begin = find_if_not(begin, end, is_white);
    while(begin < end && is_white(end[-1]))
      --end;
    auto size = static_cast<size_t>(end - begin);
    return static_cast<char*>(memcpy(arena->malloc(size + 1), begin, size));
  }

  IniKey MakeKey(Arena* arena, const char* begin, const char* end)
  {
    while(begin < end && is_white(end[-1]))
      --end;

    unsigned index = 0, multiplier = 1;
    while(end != begin)
    {
      auto c = static_cast<unsigned>(*--end) - '0';
      if(c > 9)
      {
        end += (c != ('.' - '0') && c != (':' - '0'));
        break;
      }
      index += c * multiplier;
      multiplier *= 10;
    }
    return IniKey(MakeValue(arena, begin, end), index == 0U ? 1U : index);
  }

  template <typename T>
  struct IniMap : map<IniKey, T>
  {
    T* try_find(IniKey k)
    {
      auto itr = find(k);
      return (itr == end()) ? nullptr : &itr->second;
    }
  };

  class IniParser
  {
  public:
    typedef IniMap<const char*> section_t;
    IniMap<section_t> sections;

    void parse(Arena* arena, MappedMemory contents)
    {
      auto line = reinterpret_cast<const char*>(contents.begin);
      const auto end = reinterpret_cast<const char*>(contents.end);
      section_t* section = nullptr;
      while(line != end)
      {
        line = find_if_not(line, end, is_white);
        const auto eol = find_if(line, end, [](char c){return c == ';' || is_newline(c);});
        if(line != eol)
        {
          if(line[0] == '[')
          {
            const auto ket = find(line, eol, ']');
            if(ket != eol)
              section = &sections[MakeKey(arena, line + 1, ket)];
          }
          else if(section)
          {
            const auto equ = find(line, eol, '=');
            if(equ != eol)
              (*section)[MakeKey(arena, line, equ)] = MakeValue(arena, equ + 1, eol);
          }
        }
        line = find_if(eol, end, is_newline);
      }
    }
  };

  void AddFileSources(Arena* arena, const string& base_dir, IniParser& ini, AggregateFileSource* afs)
  {
    const char* sections[] = {"data:english", "data:common", "attrib:common", "data:art_high", "data:sound_high"};
    for(auto itr = begin(sections); itr != end(sections); ++itr)
    {
      for(int section_index = 1; auto section = ini.sections.try_find(IniKey(*itr, section_index)); ++section_index)
      {
        if(auto folder = section->try_find(IniKey("folder", 1)))
        {
          auto path = base_dir + *folder + "\\";
          if(GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES)
            afs->appendSource(arena->alloc<ChRootFileSource>(Essence::CreatePhysicalFileSource(arena), move(path)));
        }

        for(int archive_index = 1; auto archive = section->try_find(IniKey("archive", archive_index)); ++archive_index)
        {
          auto path = base_dir + *archive + ".sga";
          if(GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
            continue;
          afs->appendSource(Essence::CreateArchiveFileSource(arena, MapPhysicalFileA(path.c_str())));
        }
      }
    }
  }
}

namespace Essence
{
  FileSource* CreateModFileSource(Arena* arena, const string& module_file_path)
  {
    string dir;
    {
      auto dirpos = module_file_path.find_last_of("\\/");
      if(dirpos != string::npos)
        dir = module_file_path.substr(0, dirpos + 1);
    }

    IniParser ini;
    {
      auto module_file = MapPhysicalFileA(module_file_path.c_str());
      ini.parse(arena, module_file->mapAll());
    }
    AggregateFileSource* afs = arena->alloc<AggregateFileSource>();
    AddFileSources(arena, dir, ini, afs);
    return afs;
  }
}
