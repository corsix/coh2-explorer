#include <dxgiformat.h>
#include <nvtt/nvtt.h>
#include <nice/wic.h>
#include "../../../source/presized_arena.h"
#include "../../../source/zlib.h"
#include "../../common/chunky_writer.h"

using namespace std;

struct Factories
{
  Factories()
  {
    void* wic;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory1, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory), &wic);
    if(!SUCCEEDED(hr)) throw C6::COMException(hr, "CoCreateInstance(CLSID_WICImagingFactory1)");
    this->wic = C6::WIC::ImagingFactory(static_cast<IWICImagingFactory*>(wic));
  }

  C6::WIC::ImagingFactory wic;
  nvtt::Compressor compressor;
};

struct EncodingJob
{
  EncodingJob()
    : input_filename(nullptr)
    , output_filename(nullptr)
    , generate_mips(true)
    , alpha_mode(nvtt::AlphaMode_Transparency)
  {
  }

  const wchar_t* input_filename;
  const wchar_t* output_filename;
  uint32_t output_fourcc;
  bool generate_mips;
  nvtt::AlphaMode alpha_mode;
};

////////// Encoding logic //////////

static void WriteFileBurnInfo(Essence::ChunkyWriter& cw)
{
  cw.beginChunk("DATAFBIF", 2);
  cw.string("rgt_encoder"); // burner_name
  uint32_t burner_version[] = {1};
  cw.payload(burner_version);
  cw.string("n/a"); // username
  cw.string("n/a"); // date
  uint32_t num_files[] = {0};
  cw.payload(num_files);
  cw.endChunk();
}

struct dxtc_tman_entry_t
{
  uint32_t data_length;
  uint32_t data_length_compressed;
};

struct dxtc_tman_t
{
  uint32_t mip_count;
  dxtc_tman_entry_t mips[32];
};

struct dxtc_tdat_entry_header_t
{
    uint32_t mip_level;
    uint32_t width;
    uint32_t height;
    uint32_t num_physical_texels;
};

static uint32_t NumMips(uint32_t width, uint32_t height)
{
  uint32_t num = 1;
  while((width /= 2) | (height /= 2))
    ++num;
  return num;
}

class TDATOutputHandler : public nvtt::OutputHandler
{
public:
  TDATOutputHandler(Essence::ChunkyWriter& cw, dxtc_tman_entry_t* tman_entry)
    : m_cw(cw)
    , m_tman_entry(tman_entry)
  {
    _deflateInit();
  }

  ~TDATOutputHandler()
  {
    deflateEnd(&m_z);
  }

  void beginImage(int size, int width, int height, int depth, int face, int miplevel) override
  {
    dxtc_tdat_entry_header_t header[1];
    header->mip_level = static_cast<uint32_t>(miplevel);
    header->width = static_cast<uint32_t>(width);
    header->height = static_cast<uint32_t>(height);
    header->num_physical_texels = header->width * header->height;

    size += sizeof(header);
    m_remaining = size;
    m_tman_entry->data_length = size;

    writeData(header, sizeof(header));
  }

  bool writeData(const void * data, int size) override
  {
    m_z.next_in = static_cast<const Bytef*>(data);
    m_z.avail_in = static_cast<uInt>(size);
    uint8_t outbuf[1024];
    while(m_z.avail_in)
    {
      m_z.next_out = outbuf;
      m_z.avail_out = sizeof(outbuf);
      if(deflate(&m_z, Z_NO_FLUSH) != Z_OK)
        throw runtime_error(m_z.msg);
      m_cw.payload(outbuf, static_cast<uint32_t>(m_z.next_out - outbuf));
    }

    m_remaining -= size;
    if(m_remaining == 0)
    {
      for(;;)
      {
        m_z.next_out = outbuf;
        m_z.avail_out = sizeof(outbuf);
        int err = deflate(&m_z, Z_FINISH);
        if(err < Z_OK)
          throw runtime_error(m_z.msg);
        m_cw.payload(outbuf, static_cast<uint32_t>(m_z.next_out - outbuf));
        if(err > Z_OK)
          break;
      }

      m_tman_entry->data_length_compressed = static_cast<uint32_t>(m_z.total_out);
      if(m_tman_entry->data_length_compressed >= m_tman_entry->data_length)
      {
        deflateEnd(&m_z);
        _reinflateData();
        _deflateInit();
      }
      else
        deflateReset(&m_z);
      ++m_tman_entry;
    }

    return true;
  }

private:
  void _deflateInit()
  {
    m_z.zalloc = nullptr;
    m_z.zfree = nullptr;
    m_z.opaque = nullptr;
    if(deflateInit(&m_z, Z_DEFAULT_COMPRESSION) != Z_OK)
      throw runtime_error(m_z.msg);
  }

  void _reinflateData()
  {
    throw runtime_error("TODO: reinflate");
  }

  Essence::ChunkyWriter& m_cw;
  dxtc_tman_entry_t* m_tman_entry;
  int m_remaining;
  z_stream m_z;
};

static void Encode(Factories& factories, EncodingJob& job)
{
  nvtt::InputOptions inputOptions;
  unsigned width, height;
  {
    auto frame = factories.wic.createDecoderFromFilename(job.input_filename, GENERIC_READ, WICDecodeMetadataCacheOnDemand).getFrame(0);
    auto converter = factories.wic.createFormatConverter();
    converter.initialize(frame, job.alpha_mode == nvtt::AlphaMode_Premultiplied ? GUID_WICPixelFormat32bppPBGRA : GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, 0.f, WICBitmapPaletteTypeCustom);

    tie(width, height) = converter.getSize();
    auto stride = width * 4;
    auto img_data_size = stride * height;
    PresizedArena img_data_allocator(img_data_size);
    auto img_data = img_data_allocator.mallocArray<uint8_t>(img_data_size);
    HRESULT hr = converter.getRawInterface()->CopyPixels(nullptr, stride, img_data_size, img_data);
    if(FAILED(hr))
      throw C6::COMException(hr, "IWICFormatConverter::CopyPixels");

    inputOptions.setTextureLayout(nvtt::TextureType_2D, width, height);
    if(!inputOptions.setMipmapData(img_data, width, height))
      throw runtime_error("nvtt refused mipmap.");
    inputOptions.setAlphaMode(job.alpha_mode);
    inputOptions.setGamma(1.f, 2.2f);
  }

  Essence::ChunkyWriter cw(job.output_filename);
  WriteFileBurnInfo(cw);
  cw.beginChunk("FOLDTSET", 1);

  cw.beginChunk("DATADATA", 3);
  uint32_t datadata[] = {0, 0};
  cw.payload(datadata);
  cw.endChunk();

  cw.beginChunk("FOLDTXTR", 1);
  cw.beginChunk("FOLDDXTC", 3);

  cw.beginChunk("DATATFMT", 1);
  uint32_t fields[7] = {width, height, 1, 2, job.output_fourcc, 0, 1};
  cw.payload(reinterpret_cast<const uint8_t*>(fields), 25);
  cw.endChunk();

  dxtc_tman_t datatman;
  datatman.mip_count = job.generate_mips ? NumMips(width, height) : 1;
  inputOptions.setMipmapGeneration(job.generate_mips);
  uint32_t datatman_size = 4 + datatman.mip_count * 8;

  cw.beginChunk("DATATMAN", 1);
  auto datatman_payload_offset = cw.tell();
  cw.payload(reinterpret_cast<const uint8_t*>(&datatman), datatman_size);
  cw.endChunk();

  nvtt::CompressionOptions compressionOptions;
  compressionOptions.setFormat(nvtt::Format_RGBA);

  cw.beginChunk("DATATDAT", 1);
  {
    nvtt::OutputOptions outputOptions;
    TDATOutputHandler outputHandler(cw, datatman.mips);
    outputOptions.setOutputHeader(false);
    outputOptions.setOutputHandler(&outputHandler);
    if(!factories.compressor.process(inputOptions, compressionOptions, outputOptions))
      throw std::runtime_error("nvtt failed to process image.");
  }
  cw.endChunk();

  cw.seekTo(datatman_payload_offset);
  cw.payload(reinterpret_cast<const uint8_t*>(&datatman), datatman_size);
  cw.seekToEnd();
}

////////// Command line parsing //////////

template <typename T>
struct cmd_line_enum_entry_t
{
  const wchar_t* name;
  T value;
};

template <typename T, uint32_t N>
static void help_enum(cmd_line_enum_entry_t<T> const (&options)[N], const wchar_t* flag)
{
  wprintf(L"The permissible values for %s are:\n", flag);
  for(uint32_t i = 0; i < N; ++i)
  {
    wprintf(L"\t%s\n", options[i].name);
  }
}

template <typename T, uint32_t N>
static T parse_enum(cmd_line_enum_entry_t<T> const (&options)[N], const wchar_t* flag, const wchar_t* value)
{
  for(uint32_t i = 0; i < N; ++i)
  {
    if(_wcsicmp(value, options[i].name) == 0)
      return options[i].value;
  }

  wprintf(L"Invalid value specified for %s.\n");
  help_enum(options, flag);
  exit(EXIT_FAILURE);
}

#define OUTPUT_OPTION L"-o"
static void output_option_handler(EncodingJob& job, const wchar_t* arg)
{
  job.output_filename = arg;
}

#define ALPHA_MODE_OPTION L"--alpha"
const cmd_line_enum_entry_t<nvtt::AlphaMode> g_alpha_modes[] = {
  {L"premultiplied", nvtt::AlphaMode_Premultiplied},
  {L"straight"     , nvtt::AlphaMode_Transparency },
  {L"none"         , nvtt::AlphaMode_None         },
};
static void alpha_mode_option_handler(EncodingJob& job, const wchar_t* arg)
{
  job.alpha_mode = parse_enum(g_alpha_modes, ALPHA_MODE_OPTION, arg);
}

#define FORMAT_OPTION L"--format"
const cmd_line_enum_entry_t<uint32_t> g_formats[] = {
  {L"rgba"     , 0xC6000400 | DXGI_FORMAT_R8G8B8A8_UNORM     },
  {L"rgba_srgb", 0xC6000400 | DXGI_FORMAT_R8G8B8A8_UNORM_SRGB},
};
static void format_option_handler(EncodingJob& job, const wchar_t* arg)
{
  job.output_fourcc = parse_enum(g_formats, FORMAT_OPTION, arg);
}

#define NO_MIPS_OPTION L"--no_mips"
static void no_mips_handler(EncodingJob& job, const wchar_t*)
{
  job.generate_mips = false;
}

#define HELP_OPTION L"--help"
static void help_handler(EncodingJob&, const wchar_t*);

static void generic_option_handler(EncodingJob& job, const wchar_t* arg)
{
  if(arg[0] == '-')
  {
    wprintf(L"Unrecognised option: %s\n", arg);
    exit(EXIT_FAILURE);
  }
  else
  {
    job.input_filename = arg;
  }
}

struct cmd_line_option_t
{
  const wchar_t* name;
  int flags;
  void (*handler)(EncodingJob& job, const wchar_t* arg);
};

#define CMD_LINE_OPT_HAS_VALUE 1
#define CMD_LINE_OPT_MANDATORY 2

const cmd_line_option_t g_cmd_line_options[] = {
  {OUTPUT_OPTION    , CMD_LINE_OPT_HAS_VALUE                         , output_option_handler},
  {FORMAT_OPTION    , CMD_LINE_OPT_HAS_VALUE | CMD_LINE_OPT_MANDATORY, format_option_handler},
  {ALPHA_MODE_OPTION, CMD_LINE_OPT_HAS_VALUE                         , alpha_mode_option_handler},
  {NO_MIPS_OPTION   , 0                                              , no_mips_handler},
  {HELP_OPTION      , 0                                              , help_handler},
  {nullptr          ,                          CMD_LINE_OPT_MANDATORY, generic_option_handler}
};

#define DEFAULT_OPT_NAME L"input-filename"

static void help_handler(EncodingJob&, const wchar_t*)
{
  wprintf(L"rgt_encoder is a tool made by Corsix as part of coh2explorer\n");
  wprintf(L"It uses the NVIDIA Texture Tools to convert an image file into an RGT file, for use within Company of Heroes 2.\n\n");

  wprintf(L"Usage: rgt_encoder");
  for(auto& opt : g_cmd_line_options)
  {
    bool is_optional = !(opt.flags & CMD_LINE_OPT_MANDATORY);
    wprintf(L" %s%s%s%s", is_optional ? L"[" : L"", opt.name ? opt.name : DEFAULT_OPT_NAME, opt.flags & CMD_LINE_OPT_HAS_VALUE ? L" value" : L"", is_optional ? L"]" : L"");
  }
  wprintf(L"\n\n");

  help_enum(g_formats, FORMAT_OPTION);
  help_enum(g_alpha_modes, ALPHA_MODE_OPTION);

  exit(EXIT_SUCCESS);
}

int wmain2(int argc, wchar_t** argv)
{
  Factories factories;
  EncodingJob job;

  bool cmd_line_valid = true;
  uint32_t seen_options = 0;
  for(int argi = 1; argi < argc; ++argi)
  {
    const wchar_t* arg = argv[argi];

    for(int opti = 0; ; ++opti)
    {
      auto& opt = g_cmd_line_options[opti];
      if(opt.name == nullptr || wcscmp(arg, opt.name) == 0)
      {
        uint32_t mask = 1 << opti;
        if(seen_options & mask)
        {
          wprintf(L"%s cannot be specified more than once\n",  opt.name ? opt.name : DEFAULT_OPT_NAME);
          cmd_line_valid = false;
        }
        seen_options |= mask;

        if(opt.flags & CMD_LINE_OPT_HAS_VALUE)
        {
          if(argi + 1 == argc)
          {
            wprintf(L"Expected value after %s\n", opt.name);
            cmd_line_valid = false;
          }
          else
          {
            arg = argv[++argi];
          }
        }

        opt.handler(job, arg);
        break;
      }
    }
  }
  for(int opti = 0; ; ++opti)
  {
    auto& opt = g_cmd_line_options[opti];
    if(opt.flags & CMD_LINE_OPT_MANDATORY)
    {
      if(!(seen_options & (1 << opti)))
      {
        wprintf(L"Missing %s option\n", opt.name ? opt.name : DEFAULT_OPT_NAME);
        cmd_line_valid = false;
      }
    }
    if(!opt.name)
      break;
  }
  if(!cmd_line_valid)
  {
    wprintf(L"Use %s for usage information\n", HELP_OPTION);
    return EXIT_FAILURE;
  }

  if(job.output_filename == nullptr)
  {
    auto output_filename = new wchar_t[wcslen(job.input_filename) + 5];
    wcscpy(output_filename, job.input_filename);
    if(auto dot = wcsrchr(output_filename, '.'))
      *dot = 0;
    wcscat(output_filename, L".rgt");
    job.output_filename = output_filename;
  }

  Encode(factories, job);

  return EXIT_SUCCESS;
}

////////// Initialisation //////////

static void LoadNVidiaTextureTools()
{
  if(LoadLibraryA("nvtt.dll") == nullptr)
  {
    wchar_t nvtt_path[MAX_PATH];
    DWORD nvtt_path_len = sizeof(nvtt_path);
#ifdef _WIN64
    if(RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{65C967FA-29D8-4A5F-99C5-BC9AF1F8F9D2}", L"InstallLocation", RRF_RT_REG_SZ, nullptr, nvtt_path, &nvtt_path_len) != ERROR_SUCCESS)
      throw runtime_error("Could not find NVIDIA Texture Tools 2.0 (64 bit).");
#else
    if(RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\"          L"Microsoft\\Windows\\CurrentVersion\\Uninstall\\{D8D06241-617C-42AB-B9C7-D9BA5A377D10}", L"InstallLocation", RRF_RT_REG_SZ, nullptr, nvtt_path, &nvtt_path_len) != ERROR_SUCCESS
    && RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{D8D06241-617C-42AB-B9C7-D9BA5A377D10}", L"InstallLocation", RRF_RT_REG_SZ, nullptr, nvtt_path, &nvtt_path_len) != ERROR_SUCCESS)
      throw runtime_error("Could not find NVIDIA Texture Tools 2.0 (32 bit).");
#endif
    wcscat(nvtt_path, L"\\bin");
    AddDllDirectory(nvtt_path);
    if(LoadLibraryExA("nvtt.dll", nullptr, LOAD_LIBRARY_SEARCH_USER_DIRS) == nullptr)
      throw C6::COMException(HRESULT_FROM_WIN32(GetLastError()), "LoadLibrary: nvtt.dll");
  }
}

int wmain(int argc, wchar_t** argv)
{
  HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
  try
  {
    auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if(!SUCCEEDED(hr)) throw C6::COMException(hr, "CoInitializeEx");

    LoadNVidiaTextureTools();

    return wmain2(argc, argv);
  }
  catch(const exception& e)
  {
    printf("Uncaught top-level exception:\n%s\n", e.what());
    return EXIT_FAILURE;
  }
}
