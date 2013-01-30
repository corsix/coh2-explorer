#include "mappable.h"
#include <Windows.h>
#include <nice/com.h>
#include <new>

static void NullaryDeleter(MappedMemory&)
{
}

MappedMemory::MappedMemory()
  : begin(nullptr)
  , end(nullptr)
  , m_deleter(&NullaryDeleter)
{
}

MappedMemory::~MappedMemory()
{
  m_deleter(*this);
}

MappedMemory::MappedMemory(const uint8_t* begin_, const uint8_t* end_)
  : begin(begin_)
  , end(end_)
  , m_deleter(&NullaryDeleter)
{
}

MappedMemory::MappedMemory(const uint8_t* begin_, const uint8_t* end_, void (*deleter)(MappedMemory&))
  : begin(begin_)
  , end(end_)
  , m_deleter(deleter)
{
}

MappedMemory::MappedMemory(MappedMemory&& move_from)
  : begin(move_from.begin)
  , end(move_from.end)
  , m_deleter(move_from.m_deleter)
{
  new (&move_from) MappedMemory;
}

MappedMemory& MappedMemory::operator= (nullptr_t)
{
  this->~MappedMemory();
  new (this) MappedMemory;
  return *this;
}

MappedMemory& MappedMemory::operator= (MappedMemory&& move_from)
{
  if(&move_from != this)
  {
    this->~MappedMemory();
    new (this) MappedMemory (std::move(move_from));
  }
  return *this;
}

MappableFile::~MappableFile()
{
}

void MappableFile::expand(uint64_t& offset_begin, uint64_t& offset_end)
{
}

MappedMemory MappableFile::mapAll()
{
  return map(0, getSize());
}

namespace
{
  class MappedPhysicalFile : public MappableFile
  {
  public:
    MappedPhysicalFile()
      : m_file(INVALID_HANDLE_VALUE)
      , m_map(nullptr)
    {
    }

    void initialise(const char* path)
    {
      m_file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
      if(m_file == INVALID_HANDLE_VALUE)
        throw C6::CreateFileException(path);
      initialiseWithFile();
    }

    void initialise(const wchar_t* path)
    {
      m_file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
      if(m_file == INVALID_HANDLE_VALUE)
        throw C6::CreateFileException(path);
      initialiseWithFile();
    }

    ~MappedPhysicalFile()
    {
      if(m_map != nullptr)
        CloseHandle(m_map);
      if(m_file != INVALID_HANDLE_VALUE)
        CloseHandle(m_file);
    }

    void expand(uint64_t& offset_begin, uint64_t& offset_end) override
    {
      offset_begin &=~ granularity_mask;
      if(offset_end < m_size)
      {
        offset_end = (offset_end + granularity_mask) &~ granularity_mask;
        if(offset_end > m_size)
          offset_end = m_size;
      }
    }

    MappedMemory map(uint64_t offset_begin, uint64_t offset_end) override
    {
      if(offset_end > m_size) throw std::out_of_range("Cannot map beyond end of file.");
      if(offset_end == offset_begin) return MappedMemory();
      if(offset_end < offset_begin) throw std::runtime_error("Cannot map backwards range.");

      auto orig_size = offset_end - offset_begin;
      auto padding = offset_begin & granularity_mask;
      expand(offset_begin, offset_end);
      auto size = offset_end - offset_begin;
      if(static_cast<SIZE_T>(size) != size)
        throw std::runtime_error("Range is too large to map as a single block.");

      if(auto mapped_ = MapViewOfFile(m_map, FILE_MAP_READ, static_cast<uint32_t>(offset_begin >> 32), static_cast<uint32_t>(offset_begin), static_cast<SIZE_T>(size)))
      {
        auto mapped = reinterpret_cast<const uint8_t*>(mapped_) + padding;
        return MappedMemory(mapped, mapped + orig_size, &UnmapPhysical);
      }
      else
        throw C6::COMException(HRESULT_FROM_WIN32(GetLastError()), "MapViewOfFile");
    }

    static void UnmapPhysical(MappedMemory& mm)
    {
      UnmapViewOfFile((LPCVOID)((uintptr_t)(mm.begin) &~ static_cast<uintptr_t>(granularity_mask)));
    }

  private:
    void initialiseWithFile()
    {
      LARGE_INTEGER size;
      if(!GetFileSizeEx(m_file, &size))
        throw C6::COMException(HRESULT_FROM_WIN32(GetLastError()), "GetFileSizeEx");
      m_size = size.QuadPart;

      if(m_size != 0)
      {
        m_map = CreateFileMapping(m_file, nullptr, PAGE_READONLY, size.HighPart, size.LowPart, nullptr);
        if(m_map == nullptr)
          throw C6::COMException(HRESULT_FROM_WIN32(GetLastError()), "CreateFileMapping");
      }
    }

    HANDLE m_file;
    HANDLE m_map;
    static const uint64_t granularity_mask = 0xffff;
  };

  class SmallPhysicalFile : public MappableFile
  {
  public:
    SmallPhysicalFile(std::unique_ptr<MappableFile> file)
      : m_file(move(file))
      , m_contents(m_file->mapAll())
    {
      m_size = m_file->getSize();
    }

    void expand(uint64_t& offset_begin, uint64_t& offset_end) override
    {
      offset_begin = 0;
      if(offset_end < m_size)
        offset_end = m_size;
    }

    MappedMemory map(uint64_t offset_begin, uint64_t offset_end) override
    {
      if(offset_begin < offset_end && offset_end <= m_size)
        return MappedMemory(m_contents.begin + offset_begin, m_contents.begin + offset_end);
      else
        return m_file->map(offset_begin, offset_end);
    }

  private:
    std::unique_ptr<MappableFile> m_file;
    MappedMemory m_contents;
  };

  template <typename T>
  std::unique_ptr<MappableFile> MapPhysicalFile(T* path)
  {
    std::unique_ptr<MappedPhysicalFile> mf(new MappedPhysicalFile);
    mf->initialise(path);
    if(mf->getSize() <= (1024 * 1024))
      return std::unique_ptr<MappableFile>(new SmallPhysicalFile(move(mf)));
    return move(mf);
  }
}

std::unique_ptr<MappableFile> MapPhysicalFileA(const char* path)
{
  return MapPhysicalFile(path);
}

std::unique_ptr<MappableFile> MapPhysicalFileW(const wchar_t* path)
{
  return MapPhysicalFile(path);
}
