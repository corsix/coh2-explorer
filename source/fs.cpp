#include "stdafx.h"
#include "fs.h"
#include "mappable.h"
using namespace std;

namespace
{
  class Win32FileSource : public Essence::FileSource
  {
  public:
    unique_ptr<MappableFile> readFile(const string& path) override
    {
      if(GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        return nullptr;

      return MapPhysicalFileA(path.c_str());
    }

    void getDirs(const string& path, vector<std::string>& dirs) override
    {
      getFilesOrDirs(path, dirs, FILE_ATTRIBUTE_DIRECTORY);
    }

    void getFiles(const string& path, vector<string>& files) override
    {
      getFilesOrDirs(path, files, 0);
    }

  private:
    void getFilesOrDirs(const string& path, vector<string>& names, DWORD dirFlag)
    {
      if(!path.empty())
      {
        string search(path);
        switch(*search.rbegin())
        {
        default:
          search += '\\';
        case '\\':
        case '/':
          break;
        }
        search += '*';

        WIN32_FIND_DATAA fd;
        HANDLE hfind = FindFirstFileA(search.c_str(), &fd);
        if(hfind != INVALID_HANDLE_VALUE)
        {
          do
          {
            auto head = *reinterpret_cast<const uint32_t*>(fd.cFileName);
            if((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != dirFlag) continue;
            if(static_cast<uint16_t>(head) == '.') continue; // fd.cFileName == "."
            if(((head << 8) >> 8) == '..') continue;         // fd.cFileName == ".."

            // To be consistent with .sga archives, names are always presented in lowercase.
            transform(begin(fd.cFileName), end(fd.cFileName), fd.cFileName, [](char c) { return static_cast<char>(tolower(c)); });
            names.push_back(fd.cFileName);
          } while(FindNextFileA(hfind, &fd));
          FindClose(hfind);
        }
      }
    }
  } g_Win32FileSource;
}

namespace Essence
{
  FileSource* CreatePhysicalFileSource(Arena* arena)
  {
    return &g_Win32FileSource;
  }
}