#include "chunky_writer.h"
#include <nice/com.h>
#include <Windows.h>

namespace Essence
{
  ChunkyWriter::ChunkyWriter(const wchar_t* filename)
  {
    m_file = CreateFileW(filename, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if(m_file == INVALID_HANDLE_VALUE)
      throw C6::CreateFileException(filename);
    m_innermost_chunk_length_field_offset = 0;

    const char header[] = "Relic Chunky\x0D\x0A\x1A\x00\x03\x00\x00\x00\x01\x00\x00\x00\x24\x00\x00\x00\x1C\x00\x00\x00\x01\x00\x00";
    DWORD num_written;
    if(!WriteFile(m_file, header, sizeof(header), &num_written, nullptr) || num_written != sizeof(header))
    {
      CloseHandle(m_file);
      throw C6::COMException(HRESULT_FROM_WIN32(GetLastError()), "WriteFile");
    }
  }

  ChunkyWriter::~ChunkyWriter()
  {
    while(m_innermost_chunk_length_field_offset)
      endChunk();

    CloseHandle(m_file);
  }

  void ChunkyWriter::beginChunk(const char (&kind_type)[9], uint32_t version, const char* name)
  {
    payload(reinterpret_cast<const uint8_t*>(kind_type), 8);
    uint32_t name_len = name ? static_cast<uint32_t>(strlen(name) + 1) : 0;
    const uint32_t fields[] = {version, m_innermost_chunk_length_field_offset, name_len, ~static_cast<uint32_t>(0), 0};
    m_innermost_chunk_length_field_offset = tell() + 4;
    payload(reinterpret_cast<const uint8_t*>(fields), sizeof(fields));
  }

  void ChunkyWriter::payload(const uint8_t* data, uint32_t length)
  {
    DWORD num_written;
    if(!WriteFile(m_file, data, length, &num_written, nullptr) || length != num_written)
      throw C6::COMException(HRESULT_FROM_WIN32(GetLastError()), "WriteFile");
  }

  uint32_t ChunkyWriter::tell()
  {
    return SetFilePointer(m_file, 0, nullptr, FILE_CURRENT);
  }

  void ChunkyWriter::seekTo(uint32_t position)
  {
    SetFilePointer(m_file, position, nullptr, FILE_BEGIN);
  }

  void ChunkyWriter::seekToEnd()
  {
    SetFilePointer(m_file, 0, nullptr, FILE_END);
  }

  void ChunkyWriter::endChunk()
  {
    DWORD end_of_data = tell();

    uint32_t fields[2];
    DWORD num_read;
    seekTo(m_innermost_chunk_length_field_offset);
    if(!ReadFile(m_file, fields, sizeof(fields), &num_read, nullptr) || sizeof(fields) != num_read)
      throw C6::COMException(HRESULT_FROM_WIN32(GetLastError()), "ReadFile");

    uint32_t start_of_data = m_innermost_chunk_length_field_offset + fields[1] + 16;
    uint32_t length = end_of_data - start_of_data;
    seekTo(m_innermost_chunk_length_field_offset);
    payload(reinterpret_cast<const uint8_t*>(&length), 4);

    m_innermost_chunk_length_field_offset = fields[0];
    seekToEnd();
  }
}
