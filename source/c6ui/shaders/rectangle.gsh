struct command_t
{
    uint kind : K;
    float4 colour : C;
    float4 colour_secondary : S;
    float4 rect : P;
};

struct ps_t
{
  float4 colour : C;
  float4 colour_secondary : S;
  float4 tex_coord : T;
  float4 pos : SV_POSITION;
};

cbuffer viewport_t
{
  float2 scale1;
  float2 translate1;
};

float chebyshev_length(float4 r)
{
  float2 partial = max(abs(r.xy), abs(r.zw));
  return max(partial.x, partial.y);
}

[maxvertexcount(4)]
void main(point command_t command[1], inout TriangleStream<ps_t> triangles )
{
  ps_t v;
  
  uint kind = command[0].kind & 15;
  v.colour = command[0].colour;
  v.colour_secondary = command[0].colour_secondary;
  
  float colour_dist = chebyshev_length(v.colour_secondary - v.colour);
  v.tex_coord.w = .5 / (colour_dist * 256);
  
  // top left
  v.pos.zw = float2(0, 1);
  v.tex_coord.xy = command[0].rect.xy;
  v.tex_coord.z = 0;
  v.pos.xy = v.tex_coord.xy * scale1 + translate1;
  triangles.Append(v);

  // bottom left
  v.tex_coord.y = command[0].rect.w;
  v.pos.xy = v.tex_coord.xy * scale1 + translate1;
  v.tex_coord.z = kind ? 1 : 0;
  triangles.Append(v);
  
  // top right
  v.tex_coord.xy = command[0].rect.zy;
  v.pos.xy = v.tex_coord.xy * scale1 + translate1;
  v.tex_coord.z = kind ? 0 : 1;
  triangles.Append(v);

  // bottom right
  v.tex_coord.y = command[0].rect.w;
  v.tex_coord.z = 1;
  v.pos.xy = v.tex_coord.xy * scale1 + translate1;
  triangles.Append(v);
}
