#include "stdafx.h"
#include "chunky.h"
#include "arena.h"

namespace Essence
{
  const uint8_t* Chunk::_getEndOfHeader() const
  {
    if(m_kind == 'TOOR')
    {
      return m_root_data;
    }
    else
    {
      return reinterpret_cast<const uint8_t*>(this + 1);
    }
  }

  std::string Chunk::getName() const
  {
    auto begin = reinterpret_cast<const char*>(_getEndOfHeader());
    return std::string(begin, begin + m_name_size);
  }
    
  const uint8_t* Chunk::getContents() const
  {
    return _getEndOfHeader() + m_name_size;
  }

  bool Chunk::_isChildOf(const Chunk* parent) const
  {
    auto begin = parent->getContents();
    auto self = reinterpret_cast<const uint8_t*>(this);
    auto end = begin + parent->getSize();
    return begin <= self && (self + sizeof(*this)) <= end && (getContents() + getSize()) <= end;
  }

  namespace
  {
    class ChunkPredicate
    {
    public:
      ChunkPredicate(const char* query)
        : kind_mask(0)
        , kind(0)
        , type_mask(0)
        , type(0)
        , version_mask(0)
        , version(0)
      {
        if(query == nullptr || *query == 0)
          return;

        auto end = query + strlen(query);
        auto kindtype_end = std::find(query, end, ' ');
        if((kindtype_end - query) > 4)
        {
          kind_mask = ~kind_mask;
          memcpy(&kind, query, 4);
          query += 4;
        }
        
        auto type_len = static_cast<size_t>(kindtype_end - query);
        if(type_len > 4)
          type_len = 4;
        memcpy(reinterpret_cast<uint8_t*>(&type_mask) + 4 - type_len, "\xFF\xFF\xFF\xFF", type_len);
        memcpy(reinterpret_cast<uint8_t*>(&type) + 4 - type_len, query, type_len);
        query += type_len;

        while(*query == ' ')
          ++query;

        if(*query == 'v')
        {
          version_mask = ~version_mask;
          version = static_cast<uint32_t>(atoi(query + 1));
        }
      }

      bool operator()(const Chunk* chunk)
      {
        return 0 ==
        ( ((chunk->getKind()    & kind_mask   ) ^ kind   )
        | ((chunk->getType()    & type_mask   ) ^ type   )
        | ((chunk->getVersion() & version_mask) ^ version)
        );
      }

    private:
      uint32_t kind_mask;
      uint32_t kind;
      uint32_t type_mask;
      uint32_t type;
      uint32_t version_mask;
      uint32_t version;
    };
  }

#define FOREACH_CHILD(child, query) \
  auto predicate = ChunkPredicate(query); \
  if(this != nullptr && m_kind != 'ATAD') \
    for(auto child = reinterpret_cast<const Chunk*>(getContents()); child->_isChildOf(this); child = reinterpret_cast<const Chunk*>(child->getContents() + child->getSize())) \
      if(predicate(child))

  const Chunk* Chunk::findFirst(const char* query) const
  {
    FOREACH_CHILD(child, query)
    {
      return child;
    }
    return nullptr;
  }

  std::vector<const Chunk*> Chunk::findAll(const char* query) const
  {
    std::vector<const Chunk*> results;
    FOREACH_CHILD(child, query)
    {
      results.push_back(child);
    }
    return results;
  }

  namespace
  {
    struct ChunkyFileHeader
    {
      char signature[16];
      uint32_t version;
      uint32_t unknown;
      uint32_t data_offset;
    };
  }

  ChunkyFile::ChunkyFile()
  {
  }

  std::unique_ptr<const ChunkyFile> ChunkyFile::Open(std::unique_ptr<MappableFile> file)
  {
    const uint32_t header_size = sizeof(ChunkyFileHeader);
    if(file->getSize() < header_size)
      return nullptr;

    auto header_mem = file->map(0, header_size);
    auto header = reinterpret_cast<const ChunkyFileHeader*>(header_mem.begin);
    bool has_signature = memcmp(header->signature, "Relic Chunky\x0D\x0A\x1A", 16) == 0;
    auto version = header->version;
    auto data_offset = header->data_offset;
    header_mem = nullptr;
    if(!has_signature || data_offset > file->getSize())
      return nullptr;

    std::unique_ptr<ChunkyFile> c(new ChunkyFile);
    c->m_type = c->m_kind = 'TOOR';
    c->m_version = version;
    c->m_data_size = static_cast<uint32_t>(file->getSize() - data_offset);
    c->m_name_size = 0;
    c->m_contents = file->map(data_offset, file->getSize());
    c->m_file = move(file);
    c->m_root_data = c->m_contents.begin;
    return move(c);
  }

  ChunkReader::ChunkReader(const Chunk* chunk)
    : m_ptr(chunk->getContents())
    , m_end(m_ptr + chunk->getSize())
  {
  }

  void ChunkReader::seek(size_t amount)
  {
    runtime_assert(static_cast<size_t>(m_end - m_ptr) >= amount, "Unexpected end of chunk");
    m_ptr += amount;
  }

  const ChunkyString* ChunkReader::readString()
  {
    auto str = reinterpret<ChunkyString>();
    seek(str->size());
    return str;
  }
}
