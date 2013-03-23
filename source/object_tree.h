#pragma once
#include "c6ui/tree_control.h"

namespace Essence { namespace Graphics { class Model; }}

class ObjectTree : public C6::UI::BooleanTreeControl
{
public:
  ObjectTree(Arena& arena, C6::UI::DC& dc, Essence::Graphics::Model& model, bool* visibility_states);
};
