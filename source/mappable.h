#pragma once
#include <stdint.h>
#include <memory>

class MappedMemory
{
public:
  MappedMemory();
  MappedMemory(const uint8_t* begin, const uint8_t* end);
  MappedMemory(const uint8_t* begin, const uint8_t* end, void (*deleter)(MappedMemory&));
  MappedMemory(MappedMemory&& move_from);
  MappedMemory& operator= (MappedMemory&& move_from);
  MappedMemory& operator= (nullptr_t);
  ~MappedMemory();

  size_t size() const { return static_cast<size_t>(end - begin); }
  const uint8_t* data() const { return begin; }

  const uint8_t* const begin;
  const uint8_t* const end;

private:
  MappedMemory(const MappedMemory& cannot_copy);
  MappedMemory& operator= (const MappedMemory& cannot_copy);

  void (*m_deleter)(MappedMemory& mm);
};

//! Interface for reading from files in a memory-mapped fashion.
class MappableFile
{
public:
  virtual ~MappableFile();

  uint64_t getSize() const {return m_size;}
  virtual void expand(uint64_t& offset_begin, uint64_t& offset_end);
  virtual MappedMemory map(uint64_t offset_begin, uint64_t offset_end) = 0;
  MappedMemory mapAll();

protected:
  uint64_t m_size;
};

std::unique_ptr<MappableFile> MapPhysicalFileA(const char* path);
std::unique_ptr<MappableFile> MapPhysicalFileW(const wchar_t* path);
