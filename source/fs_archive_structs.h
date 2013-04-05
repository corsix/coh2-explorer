#pragma pack(push)
#pragma pack(1)

namespace internal
{
  struct file_header_t
  {
    char signature[8];
    union
    {
      uint32_t version;
      struct
      {
        uint16_t version_major;
        uint16_t version_minor;
      };
    };
    int32_t contents_md5[4];
    wchar_t archive_name[64];
    int32_t header_md5[4];
    uint32_t data_header_size;
    uint32_t data_offset;

    inline uint32_t getPlatform() const {return 1;}
    inline uint32_t getDataHeaderOffset() const {return sizeof(file_header_t);}
  };

  template <typename count>
  struct data_header_t
  {
    typedef uint32_t offset;

    offset entry_point_offset;
    count  entry_point_count;
    offset directory_offset;
    count  directory_count;
    offset file_offset;
    count  file_count;
    offset strings_offset;
    count  strings_count;
  };

  template <typename index>
  struct entry_point_t
  {
    char directory_name[64];
    char alias[64];
    index first_directory;
    index last_directory;
    index first_file;
    index last_file;
  };

  template <typename index>
  struct directory_t
  {
    uint32_t name_offset;
    index first_directory;
    index last_directory;
    index first_file;
    index last_file;
  };
}

template <int version>
struct file_header_t;

template <int version>
struct data_header_t;

template <int version>
struct entry_point_t;

template <int version>
struct file_t;

template <int version>
struct directory_t;

/***** Version 2.0 *****/

template <>
struct file_header_t<2> : internal::file_header_t
{
};

template <>
struct data_header_t<2> : internal::data_header_t<uint16_t>
{
};

template <>
struct entry_point_t<2> : internal::entry_point_t<uint16_t>
{
  uint32_t folder_offset;
};

template <>
struct directory_t<2> : internal::directory_t<uint16_t>
{
};

template <>
struct file_t<2>
{
  uint32_t name_offset;
  uint32_t flags;
  uint32_t data_offset;
  uint32_t data_length_compressed;
  uint32_t data_length;

  static bool hasTimestamp() {return false;}
  uint32_t getTimestamp() const {return 0;}
};

/***** Version 4.0 *****/

template <>
struct file_header_t<4> : internal::file_header_t
{
  uint32_t platform;

  inline uint32_t getPlatform() const {return platform;}
  inline uint32_t getDataHeaderOffset() const {return sizeof(file_header_t);}
};

template <>
struct data_header_t<4> : internal::data_header_t<uint16_t>
{
};

template <>
struct entry_point_t<4> : entry_point_t<2>
{
};

template <>
struct directory_t<4> : internal::directory_t<uint16_t>
{
};

template <>
struct file_t<4>
{
  uint32_t name_offset;
  uint32_t data_offset;
  uint32_t data_length_compressed;
  uint32_t data_length;
  uint32_t modification_time;
  uint16_t flags;

  static bool hasTimestamp() {return true;}
  uint32_t getTimestamp() const {return modification_time;}
};

/***** Version 5.0 as used by CoH2 alpha *****/

template <>
struct file_header_t<45> : file_header_t<4>
{
};

template <>
struct data_header_t<45> : internal::data_header_t<uint32_t>
{
};

template <>
struct entry_point_t<45> : entry_point_t<4>
{
  uint32_t folder_offset;
};

template <>
struct directory_t<45> : internal::directory_t<uint32_t>
{
};

template <>
struct file_t<45> : file_t<4>
{
};

/***** Version 5.0 as used by DoW2 *****/

template <>
struct file_header_t<5> : file_header_t<2>
{
  uint32_t data_header_offset;
  uint32_t platform;

  inline uint32_t getPlatform() const {return platform;}
  inline uint32_t getDataHeaderOffset() const {return data_header_offset;}
};

template <>
struct data_header_t<5> : internal::data_header_t<uint16_t>
{
};

template <>
struct entry_point_t<5> : internal::entry_point_t<uint16_t>
{
  uint16_t folder_offset;
};

template <>
struct directory_t<5> : internal::directory_t<uint16_t>
{
};

template <>
struct file_t<5> : file_t<4>
{
};

/***** Version 6.0 (CoH2 beta) *****/

template <>
struct file_header_t<6>
{
  char signature[8];
  uint32_t version;
  wchar_t archive_name[64];
  uint32_t data_header_size;
  uint32_t data_offset;
  uint32_t platform;

  inline uint32_t getPlatform() const {return platform;}
  inline uint32_t getDataHeaderOffset() const {return sizeof(file_header_t);}
};

template <>
struct data_header_t<6> : data_header_t<45>
{
};

template <>
struct entry_point_t<6> : internal::entry_point_t<uint32_t>
{
  uint32_t folder_offset;
};

template <>
struct directory_t<6> : directory_t<45>
{
};

template <>
struct file_t<6> : file_t<4>
{
  uint32_t hash;
};

#pragma pack(pop)
