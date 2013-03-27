#include "object_tree.h"
#include "model.h"
#include "chunky.h"
#include <regex>
#include <algorithm>
#include <unordered_set>
#include <stack>
using namespace std;
using namespace C6::UI;
using namespace Essence::Graphics;

namespace
{
  template <typename C>
  bool is_separator(C c)
  {
    return c == '_' || c == '.' || c == ' ';
  }

  template <typename T>
  static const wchar_t* ToUnicode(Arena& arena, size_t prefix_len, T&& str)
  {
    auto result = arena.mallocArray<wchar_t>(str.size() + 1 - prefix_len);
    for(size_t i = prefix_len; i < str.size(); ++i)
    {
      auto c = str.data()[i];
      if(c == '_')
        c = ' ';
      result[i - prefix_len] = c;
    }
    return result;
  }

  bool is_prefix(const string& p, const Essence::ChunkyString* str)
  {
    return p.size() <= str->size() && memcmp(p.data(), str->data(), p.size()) == 0;
  }

  bool in_range(char min, char test, char max)
  {
    return (static_cast<unsigned char>(test - min)) <= (static_cast<unsigned char>(max - min));
  }

  template <typename S, typename T>
  string longest_common_prefix(S&& s, T&& t)
  {
    size_t i = 0;
    for(size_t limit = (min)(s.size(), t.size()); i < limit; ++i)
      if(s.data()[i] != t.data()[i])
        break;
    return string(s.data(), s.data() + i);
  }

  bool is_undesirable(const char* cs)
  {
    auto head = *cs;
    auto tail = cs[1];

    if(is_separator(head) || in_range('0', head, '9'))
      return true;
    if(in_range('A', head, 'Z'))
    {
      if(in_range('A', tail, 'Z') || in_range('a', tail, 'z'))
        return true;
    }
    if(in_range('a', head, 'z'))
    {
      if(in_range('a', tail, 'z'))
        return true;
    }
    return false;
  }

  struct TreeItemEx : public TreeItem
  {
    using TreeItem::m_children;
    using TreeItem::m_has_children;
    using TreeItem::m_text;
    using TreeItem::m_tick_state;
  };

  struct category_t
  {
    category_t(Arena& arena_, string name_, TreeItem* item_)
      : arena(arena_)
      , name(move(name_))
      , item(static_cast<TreeItemEx*>(item_))
    {
    }

    ~category_t()
    {
      auto src = children.begin();
      for(auto& dst : item->m_children.recreate(&arena, children.size()))
      {
        dst = *src++;
        item->m_has_children = true;
      }
    }

    Arena& arena;
    string name;
    vector<TreeItem*> children;
    TreeItemEx* item;
  };

  class AutoCategoriser
  {
  public:
    AutoCategoriser(Arena& arena_, Mesh& mesh, bool* visibility_states, TreeItem* root)
      : arena(arena_)
    {
      for(auto object : mesh.getObjects())
      {
        objects.push_back(std::make_pair(object, visibility_states++));
        taken_names.emplace(object->getName()->begin(), object->getName()->end());
      }
      sortObjects();
      
      categories.push(category_t(arena, "", root));
      for(size_t i = 0, size = objects.size(); i < size; ++i)
      {
        auto& cur = objects[i];
        while(!is_prefix(categories.top().name, cur.first->getName()))
        {
          auto item = categories.top().item;
          auto name = move(categories.top().name);
          categories.pop();
          auto orig_text = item->m_text - categories.top().name.size();
          auto& children = categories.top().children;
          if(invent_category(name, *cur.first->getName()))
          {
            item->m_text = orig_text + categories.top().name.size();
            children.erase(remove(children.begin(), children.end(), item), children.end());
            categories.top().children.push_back(item);
          }
          if(is_separator(*item->m_text))
            ++item->m_text;
        }
        if(i + 1 < objects.size())
        {
          auto& next = objects[i + 1];
          invent_category(*cur.first->getName(), *next.first->getName());
        }
        auto name = cur.first->getName();
        auto prefix_len = categories.top().name.size();
        if(is_separator(name->data()[prefix_len]))
          ++prefix_len;
        auto item = arena.allocTrivial<TreeItem>(ToUnicode(arena, prefix_len, *name), reinterpret_cast<tristate_bool_t*>(cur.second));
        categories.top().children.push_back(item);
      }
      for(; !categories.empty(); categories.pop())
      {
        auto item = categories.top().item;
        if(is_separator(*item->m_text))
          ++item->m_text;
      }
    }

  private:
    Arena& arena;
    unordered_set<string> taken_names;
    stack<category_t> categories;
    vector<pair<Object*, bool*>> objects;

    void sortObjects()
    {
      std::sort(objects.begin(), objects.end(), [](decltype(objects[0])& lhs, decltype(objects[0])& rhs)
      {
        auto lhs_name = lhs.first->getName();
        auto rhs_name = rhs.first->getName();
        auto lhs_size = lhs_name->size();
        auto rhs_size = rhs_name->size();
        if(int result = memcmp(lhs_name->begin(), rhs_name->begin(), (std::min)(lhs_size, rhs_size)))
          return result < 0;
        else
          return lhs_size < rhs_size;
      });
    }

    template <typename T1, typename T2>
    bool invent_category(T1&& name1, T2&& name2)
    {
      auto longer = name1.size() > name2.size() ? name1.data() : name2.data();
      string prefix = longest_common_prefix(forward<T1>(name1), forward<T2>(name2));
      size_t len = prefix.size();
      while(len > 0 && (is_undesirable(longer + len - 1) || taken_names.count(string(prefix.begin(), prefix.begin() + len))))
        --len;
      prefix.resize(len);

      auto& top = categories.top();
      if(top.name.size() < prefix.size())
      {
        auto state = arena.allocTrivial<tristate_bool_t>();
        *state = *static_cast<TreeItemEx*>(top.item)->m_tick_state;
        auto item = arena.allocTrivial<TreeItem>(ToUnicode(arena, top.name.size(), prefix), state);
        top.children.push_back(item);
        categories.push(category_t(arena, move(prefix), item));
        return true;
      }
      else
      {
        return false;
      }
    }
  };

  class MeshItem : public TreeItem
  {
  public:
    MeshItem(Arena& arena, const string& text, Mesh& mesh, bool* visibility_states)
      : TreeItem(ToUnicode(arena, 0, text), arena.allocTrivial<tristate_bool_t>())
    {
      auto num_objects = mesh.getObjects().size();
      if(num_objects == 0)
        return;

      m_has_children = true;
      bool default_visible = (strstr(text.c_str(), "_Wreck") == nullptr && strstr(text.c_str(), "_wreck") == nullptr && strstr(text.c_str(), "_critical") == nullptr);
      *m_tick_state = static_cast<tristate_bool_t>(default_visible);
      fill(visibility_states, visibility_states + num_objects, default_visible);

      AutoCategoriser(arena, mesh, visibility_states, this);
    }
  };
}

ObjectTree::ObjectTree(Arena& arena, DC& dc, Model& model, bool* visibility_states)
  : BooleanTreeControl(dc)
{
  regex mm_regex("merged material-\\[([^,]*),(.*)\\]\\x00");
  for(auto mesh : model.getMeshes())
  {
    string mesh_name = regex_replace(mesh->getName(), mm_regex, string("$2"));
    bool default_visible = strstr(mesh_name.c_str(), "_Wreck") == nullptr;
    auto& item = *arena.allocTrivial<MeshItem>(arena, mesh_name, *mesh, visibility_states);
    appendRoot(item);
    expand(item);
    visibility_states += mesh->getObjects().size();
  }
}
