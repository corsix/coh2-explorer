#pragma once
#include "mappable.h"
#include "arena.h"
#include <vector>
#include <string>

namespace Essence
{
  //! Common interface for accessing SGA archives, directories, and unions thereof.
  class FileSource
  {
  public:
    virtual ~FileSource() {}

    //! Open a single file for reading.
    virtual std::unique_ptr<MappableFile> readFile(const std::string& path) = 0;

    //! Get a list of all files within a particular directory.
    virtual void getFiles(const std::string& path, std::vector<std::string>& files) = 0;

    //! Get a list of of all subdirectories within a particular directory.
    virtual void getDirs(const std::string& path, std::vector<std::string>& dirs) = 0;
  };

  //! View the computer's file system (C:\, etc.) as a FileSource.
  FileSource* CreatePhysicalFileSource(Arena* arena);

  //! View an SGA archive as a FileSource.
  FileSource* CreateArchiveFileSource(Arena* arena, std::unique_ptr<MappableFile> archive);

  //! View a .module file as a FileSource.
  FileSource* CreateModFileSource(Arena* arena, const std::string& module_file_path);
}
