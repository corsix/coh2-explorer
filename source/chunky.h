#pragma once
#include "mappable.h"
#include <vector>
#include <string>

namespace Essence
{
#pragma pack(push)
#pragma pack(1)
  //! A single chunk in a Relic Chunky file.
  class Chunk
  {
  public:
    //! The chunk kind, which will be one of "DATA", "FOLD", or "ROOT".
    uint32_t getKind() const {return m_kind;}

    //! The chunk type, for example "SPFF" or "MODL".
    uint32_t getType() const {return m_type;}

    //! The version number of the chunk.
    uint32_t getVersion() const {return m_version;}

    //! The chunk's variable-length human-readable descriptive string.
    std::string getName() const;
    
    const uint8_t* getContents() const;

    //! The number of bytes in the contents of the chunk.
    uint32_t getSize() const {return m_data_size;}

    //! Find the first child chunk satisfying some criteria.
    /*!
      \param query A string of the form "(KIND)?TYPE(vVER)?" where "TYPE" is a string of up to
                   four characters, "KIND" (if present) is either "FOLD" or "DATA", and "VER"
                   (if present) is a decimal integer.
      \return The first child chunk matching the given TYPE, and (if present) KIND and VER, or
              nullptr if no such child exists.
      \note This method can be called on a nullptr instance, thereby allowing for chained calls
            without the need for intermediate null checks.
    */
    const Chunk* findFirst(const char* query) const;

    //! Find all child chunks satisfying some criteria.
    std::vector<const Chunk*> findAll(const char* query) const;

  protected:
    bool _isChildOf(const Chunk* parent) const;
    const uint8_t* _getEndOfHeader() const;

    uint32_t m_kind;
    uint32_t m_type;
    uint32_t m_version;
    uint32_t m_data_size;
    uint32_t m_name_size;
    union
    {
      int32_t m_unknown[2];

      //! For ChunkyFile to store a pointer to the first byte after the file header.
      const uint8_t* m_root_data;
      static_assert(sizeof(const uint8_t*) <= sizeof(int32_t[2]), "Pointers are too large to hide inside a Chunk.");
    };
  };
#pragma pack(pop)

  //! A Relic Chunky file.
  class ChunkyFile : public Chunk
  {
  public:
    //! View a MappableFile as a ChunkyFile.
    /*!
      \return The chunky file, or nullptr if the given file wasn't a chunky file.
    */
    static std::unique_ptr<const ChunkyFile> Open(std::unique_ptr<MappableFile> file);

  private:
    ChunkyFile();
    std::unique_ptr<MappableFile> m_file;
    MappedMemory m_contents;
  };

  //! Utility class for reading a sequence of values from a Chunk.
  class ChunkReader
  {
  public:
    ChunkReader(const Chunk* chunk);
    void seek(size_t amount);
    const uint8_t* tell() {return m_ptr;}

    template <typename T>
    const T* reinterpret(size_t count = 1)
    {
      auto ptr = m_ptr;
      seek(sizeof(T) * count);
      return reinterpret_cast<const T*>(ptr);
    }

    template <typename T>
    T read()
    {
      return *reinterpret<T>();
    }

    template <>
    std::string read<std::string>()
    {
      auto len = read<uint32_t>();
      auto begin = reinterpret_cast<const char*>(m_ptr);
      seek(len);
      return std::string(begin, begin + len);
    }

  private:
    const uint8_t* m_ptr;
    const uint8_t* m_end;
  };
}
