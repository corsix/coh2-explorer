#include "stdafx.h"
#include "abp_loader.h"
#include "fs.h"
#include "model.h"
#include "lua.h"
#include "mappable.h"
#include "chunky.h"
#include "win32.h"
using namespace Essence;
using namespace std;

namespace
{
  string to_std_string(lua_State* L, int index)
  {
    size_t size;
    auto data = lua_tolstring(L, index, &size);
    return string(data, data + size);
  }

  void report_lua_error(lua_State* L)
  {
    auto err = to_std_string(L, -1);
    lua_pop(L, 1);
    throw runtime_error(err);
  }

  int loadfile(lua_State* L)
  {
    auto mod_fs = static_cast<FileSource*>(lua_touserdata(L, lua_upvalueindex(1)));
    size_t path_len;
    auto path = luaL_checklstring(L, 1, &path_len);
    auto file = mod_fs->readFile(std::string(path, path + path_len));
    auto contents = file->mapAll();
    bool has_env = lua_gettop(L) >= 3;
    int err_code = luaL_loadbufferx(L, reinterpret_cast<const char*>(contents.begin), contents.size(), path, lua_tostring(L, 2));
    if(err_code != LUA_OK)
      report_lua_error(L);
    if(has_env)
    {
      lua_pushvalue(L, 3);
      if(!lua_setupvalue(L, -2, 1))
        lua_pop(L, 1);
    }
    return 1;
  }

  int files_matching_prefix(lua_State* L)
  {
    auto mod_fs = static_cast<FileSource*>(lua_touserdata(L, lua_upvalueindex(1)));
    size_t path_len;
    auto path = luaL_checklstring(L, 1, &path_len);
    size_t prefix_len;
    auto prefix = luaL_checklstring(L, 2, &prefix_len);
    vector<string> files;
    mod_fs->getFiles(path, files);

    lua_createtable(L, static_cast<int>(files.size()), 0);
    int i = 1;
    for(auto& file : files)
    {
      if(file.size() >= prefix_len && memcmp(file.data(), prefix, prefix_len) == 0)
      {
        lua_pushlstring(L, file.c_str(), file.size());
        lua_rawseti(L, -2, i++);
      }
    }
    return 1;
  }

  int loadresource(lua_State* L, const char* name)
  {
    return WithResource<char>("lua", name, [=](const char* chunk, size_t len)
    {
      return luaL_loadbufferx(L, chunk, len, name, "t");
    });
  }

  class AbpContext
  {
  public:
    AbpContext(FileSource* mod_fs)
      : L(luaL_newstate())
    {
      lua_pushcfunction(L, luaopen_string);
      lua_call(L, 0, 0);

      lua_pushlightuserdata(L, mod_fs);
      lua_pushcclosure(L, loadfile, 1);
      lua_setglobal(L, "loadfile");

      lua_pushlightuserdata(L, mod_fs);
      lua_pushcclosure(L, files_matching_prefix, 1);
      lua_setglobal(L, "files_matching_prefix");     

      if(loadresource(L, "abp_loader.lua") != LUA_OK || lua_pcall(L, 0, 2, 0) != LUA_OK)
      {
        auto err = to_std_string(L, -1);
        lua_close(L);
        throw logic_error(err);
      }
    }

    ~AbpContext()
    {
      lua_close(L);
    }

    void import(const string& path)
    {
      lua_pushvalue(L, 1);
      lua_pushlstring(L, path.c_str(), path.size());
      int err_code = lua_pcall(L, 1, 0, 0);
      if(err_code != LUA_OK)
        report_lua_error(L);
    }

    struct iterator
    {
      iterator(lua_State* L, int i) : m_L(L), m_i(i) {}

      string operator*()
      {
        auto L = m_L;
        lua_rawgeti(L, 2, m_i + 1);
        auto result = to_std_string(L, -1);
        lua_pop(L, 1);
        return result;
      }

      iterator& operator++() { ++m_i; return *this; }
      bool operator!=(const iterator& other) const { return m_i != other.m_i; }

    private:
      lua_State* m_L;
      int m_i;
    };

    iterator begin()
    {
      return iterator(L, 0);
    }

    iterator end()
    {
      return iterator(L, static_cast<int>(lua_rawlen(L, 2)));
    }

  private:
    lua_State* L;
  };
}

namespace Essence { namespace Graphics
{
  unique_ptr<Model> LoadModel(FileSource* mod_fs, ShaderDatabase* shaders, const string& path, C6::D3::Device1& d3)
  {
    AbpContext ctx(mod_fs);
    ctx.import(path);
    vector<unique_ptr<const ChunkyFile>> files;
    for(string path : ctx)
    {
      auto file = mod_fs->readFile(path);
      if(file->getSize() == 0)
        throw runtime_error(path + " is empty.");
      if(auto chunky = ChunkyFile::Open(move(file)))
        files.push_back(move(chunky));
      else
        throw runtime_error(path + " is not a chunky file.");
    }
    return unique_ptr<Model>(new Model(mod_fs, shaders, move(files), d3));
  }
}}
