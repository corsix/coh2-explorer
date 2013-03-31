#include "stdafx.h"
#include "model_properties.h"
#include "essence_panel.h"
#include "model.h"
#include "chunky.h"
using namespace std;

namespace Essence { namespace Graphics
{
  namespace
  {
    template <typename T>
    const wchar_t* ToUnicodeStr(Arena& arena, T& str)
    {
      auto result = arena.mallocArray<wchar_t>(str.size() + 1);
      transform(str.begin(), str.end(), result, [](char c) -> wchar_t { return c; });
      return result;
    }

    float& UnboxFloat(const ChunkyString* value)
    {
      return *reinterpret_cast<float*>(const_cast<char*>(value->data()));
    }

    struct Listener : public C6::UI::PropertyListener
    {
      Listener(ModelVariable* variable)
        : m_variable(variable)
      {
      }

      void onPropertyChanged() override
      {
        m_variable->setValue(m_variable->getValue());
      }

    private:
      ModelVariable* m_variable;
    };
  }

  ModelProperties::ModelProperties(Arena& a, C6::UI::DC& dc, Panel& panel)
    : m_active_window(&panel)
  {
    for(auto& var : panel.getModel()->getVariables())
    {
      if(!var.second->affectsAnything())
        continue;

      auto listener = a.allocTrivial<Listener>(var.second);
      switch(var.second->getType())
      {
      case PropertyDataType::float1: {
        auto ui = a.alloc<C6::UI::ColourProperty>(a, dc, ToUnicodeStr(a, var.first));
        ui->appendScalar(UnboxFloat(var.second->getValue()), UnboxFloat(var.second->getPossibleValues()[0]), UnboxFloat(var.second->getPossibleValues()[1]));
        ui->addListener(listener);
        appendGroup(ui);
        break; }
      case PropertyDataType::string: {
        auto ui = a.alloc<C6::UI::EnumProperty>(a, dc, ToUnicodeStr(a, var.first), (void*&)var.second->getValue());
        for(auto value : var.second->getPossibleValues())
          ui->appendCase((void*)value, ToUnicodeStr(a, *value));
        ui->addListener(listener);
        appendGroup(ui);
        break; }
      }
    }
  }

  void ModelProperties::setActiveWindow(Window* active_window)
  {
    m_active_window = active_window;
  }
}}
