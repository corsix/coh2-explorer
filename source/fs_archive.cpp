#include "fs.h"
#include "mappable.h"
#include "arena.h"
#include "hash.h"
#include <zlib.h>
#include <Windows.h>
using namespace std;

namespace
{
  struct StringHashTable
  {
  public:
    static StringHashTable* create(Arena* arena, size_t num_entries)
    {
      num_entries = num_entries + (num_entries / 3);
      size_t mask = 1;
      while(mask < num_entries)
        mask = mask | (mask << 1);

      auto result = static_cast<StringHashTable*>(arena->malloc(sizeof(StringHashTable) + mask * sizeof(Entry)));
      result->m_mask = mask;
      return result;
    }

    void insert(const char* str, uint32_t len, const void* data)
    {
      Entry e = {
        Essence::Hash(str, len),
#ifndef ASSUME_PERFECT_HASHING
        str, len,
#endif
        data};
      auto index = e.hash;
      do
      {
        index = (index + 1) & m_mask;
      } while(m_table[index].data);
      m_table[index] = e;
    }

    const void* lookup(const char* str, uint32_t len)
    {
      auto hash = Essence::Hash(str, len);
      auto index = hash;
      for(;;)
      {
        index = (index + 1) & m_mask;
        auto& e = m_table[index];
        if(!e.data)
          return nullptr;
        if(hash == e.hash) {
#ifndef ASSUME_PERFECT_HASHING
          if(len == e.len && !memcmp(str, e.str, len))
#endif
            return e.data;
        }
      }
    }

  private:
    uint32_t m_mask;

    struct Entry {
      uint32_t hash;
#ifndef ASSUME_PERFECT_HASHING
      const char* str;
      uint32_t len;
#endif
      const void* data;
    } m_table[1];
  };
}

namespace
{
#include "fs_archive_structs.h"

  template <typename T>
  void addNames(const char* strings, T* entities, uint32_t first, uint32_t last, std::vector<std::string>& names)
  {
    auto itr = entities + first;
    auto end = entities + last;
    for(; itr != end; ++itr)
      names.push_back(strings + itr->name_offset);
  }

  class StoredFile : public MappableFile
  {
  public:
    StoredFile(MappableFile* file, uint64_t begin, uint64_t size)
      : m_file(file)
      , m_begin(begin)
    {
      m_size = size;
    }

    uint64_t getSize() const {return m_size;}

    void expand(uint64_t& offset_begin, uint64_t& offset_end) override
    {
      offset_begin += m_begin;
      offset_end += m_begin;
      m_file->expand(offset_begin, offset_end);
      offset_begin -= m_begin;
      offset_end -= m_begin;
      if(offset_end > m_size)
        offset_end = m_size;
    }

    MappedMemory map(uint64_t offset_begin, uint64_t offset_end) override
    {
      return m_file->map(m_begin + offset_begin, m_begin + offset_end);
    }

  private:
    MappableFile* m_file;
    uint64_t m_begin;
  };

  class UncompressedFile : public MappableFile
  {
  public:
    UncompressedFile(unique_ptr<uint8_t[]> memory, uint64_t size)
      : m_memory(move(memory))
    {
      m_size = size;
    }

    void expand(uint64_t& offset_begin, uint64_t& offset_end) override
    {
      offset_begin = 0;
      if(offset_end < m_size)
        offset_end = m_size;
    }

    MappedMemory map(uint64_t offset_begin, uint64_t offset_end) override
    {
      if(offset_begin <= offset_end && offset_end <= m_size)
        return MappedMemory(m_memory.get() + offset_begin, m_memory.get() + offset_end);
      else
        throw std::runtime_error("Invalid range for mapping.");
    }

  private:
    unique_ptr<uint8_t[]> m_memory;
  };

  unique_ptr<MappableFile> ReadCompressedFile(MappableFile* archive, uint32_t data_offset, uint32_t data_length_compressed, uint32_t data_length)
  {
    if(data_length_compressed == data_length)
      return unique_ptr<MappableFile>(new StoredFile(archive, data_offset, data_length));

    auto mapped = archive->map(data_offset, data_offset + data_length_compressed);
    unique_ptr<uint8_t[]> uncompressed(new uint8_t[data_length]);
    uLong destLen = data_length;
    if(uncompress(uncompressed.get(), &destLen, mapped.begin, data_length_compressed) != Z_OK)
      throw std::runtime_error("Could not inflate compressed file.");
    return unique_ptr<MappableFile>(new UncompressedFile(move(uncompressed), destLen));
  }

  template <int version>
  class Archive : public Essence::FileSource
  {
  public:
    typedef const file_header_t<version>* file_header_ptr;
    typedef const data_header_t<version>* data_header_ptr;
    typedef const directory_t<version>* directory_ptr;
    typedef const file_t<version>* file_ptr;

    static FileSource* create(Arena* arena, unique_ptr<MappableFile> archive, MappedMemory& file_header_mem)
    {
      auto file_header = reinterpret_cast<file_header_ptr>(file_header_mem.begin);
      runtime_assert(file_header->getPlatform() == 1, "Unsupported archive platform.");

      auto self = arena->alloc<Archive>();
      uint64_t data_header_begin = file_header->getDataHeaderOffset();
      self->m_data_header_mem = archive->map(data_header_begin, data_header_begin + file_header->data_header_size);

      auto data_header = reinterpret_cast<data_header_ptr>(self->m_data_header_mem.begin);
      self->m_dirs_lut = StringHashTable::create(arena, data_header->directory_count);
      self->m_data_offset = file_header->data_offset;
      self->m_directories = reinterpret_cast<directory_ptr>(self->m_data_header_mem.begin + data_header->directory_offset);
      self->m_files = reinterpret_cast<file_ptr>(self->m_data_header_mem.begin + data_header->file_offset);
      self->m_strings = reinterpret_cast<const char*>(self->m_data_header_mem.begin + data_header->strings_offset);
      self->m_archive_file = move(archive);
      for(uint32_t i = 0; i < data_header->directory_count; ++i)
      {
        auto dir = self->m_directories + i;
        auto name = self->m_strings + dir->name_offset;
        self->m_dirs_lut->insert(name, static_cast<uint32_t>(strlen(name)), static_cast<const void*>(dir));
      }

      return self;
    }

    void getFiles(const std::string& path, std::vector<std::string>& files) override
    {
      if(auto dir = getDirectory(path))
        addNames(m_strings, m_files, dir->first_file, dir->last_file, files);
    }

    void getDirs(const std::string& path, std::vector<std::string>& dirs) override
    {
      if(auto dir = getDirectory(path))
        addNames(m_strings + path.size() + !path.empty(), m_directories, dir->first_directory, dir->last_directory, dirs);
    }

    unique_ptr<MappableFile> readFile(const string& path) override
    {
      auto file_sep_pos = path.find_last_of('\\');
      uint32_t dir_part_length = 0;
      auto file_part = path.c_str();
      if(file_sep_pos != string::npos)
      {
        dir_part_length = static_cast<uint32_t>(file_sep_pos);
        file_part += file_sep_pos + 1;
      }
      if(auto dir = static_cast<directory_ptr>(m_dirs_lut->lookup(path.c_str(), dir_part_length)))
      {
        auto itr = m_files + dir->first_file;
        auto end = m_files + dir->last_file;
        for(; itr != end; ++itr)
        {
          if(strcmp(m_strings + itr->name_offset, file_part) == 0)
            return ReadCompressedFile(&*m_archive_file, m_data_offset + itr->data_offset, itr->data_length_compressed, itr->data_length);
        }
      }
      return nullptr;
    }

  private:
    directory_ptr getDirectory(const std::string& path)
    {
      // NB: Assuming that path is already normalised.
      return static_cast<directory_ptr>(m_dirs_lut->lookup(path.c_str(), static_cast<uint32_t>(path.size())));
    }

    StringHashTable* m_dirs_lut;
    uint32_t m_data_offset;
    directory_ptr m_directories;
    file_ptr m_files;
    const char* m_strings;
    std::unique_ptr<MappableFile> m_archive_file;
    MappedMemory m_data_header_mem;
  };
}

namespace Essence
{
  FileSource* Essence::CreateArchiveFileSource(Arena* arena, std::unique_ptr<MappableFile> archive)
  {
    if(archive->getSize() < sizeof(file_header_t<4>))
      throw std::runtime_error("File too small to be an archive.");

    auto file_header_mem = archive->map(0, sizeof(file_header_t<5>));
    auto file_header = reinterpret_cast<const file_header_t<5>*>(file_header_mem.begin);

    if(memcmp(file_header->signature, "_ARCHIVE", 8))
      throw std::runtime_error("File is not an archive.");
    switch(file_header->version)
    {
    case 2:
      return Archive<2>::create(arena, move(archive), file_header_mem);
    case 4:
      return Archive<4>::create(arena, move(archive), file_header_mem);
    case 5:
      if(file_header->data_header_offset < 8)
        return Archive<45>::create(arena, move(archive), file_header_mem);
      else
        return Archive<5>::create(arena, move(archive), file_header_mem);
    default:
      throw std::runtime_error("Unsupported archive version.");
    }
  }
}
