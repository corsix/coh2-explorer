struct command_t
{
  uint kind : K;
  float4 colour : C;
  float4 sprite : S;
  float4 rect : P;
};

struct ps_t
{
  float2 tex_coord : T;
  float4 colour : C;
  float4 pos : SV_POSITION;
};

cbuffer viewport_t
{
  float2 scale1;
  float2 translate1;
};

[maxvertexcount(4)]
void main(point command_t command[1], inout TriangleStream<ps_t> triangles )
{
  ps_t v;
  
  float4 uv = round(command[0].sprite * 255.0);
  uv.ar *= 4.0;
  uv.gb += uv.ar;
  uv = uv / 256.0;
  v.colour = command[0].colour;
  
  // top left
  v.pos.zw = float2(0, 1);
  v.tex_coord.xy = uv.ar;
  v.pos.xy = command[0].rect.xy * scale1 + translate1;
  triangles.Append(v);

  // bottom left
  v.tex_coord.xy = uv.ab;
  v.pos.xy = command[0].rect.xw * scale1 + translate1;
  triangles.Append(v);
  
  // top right
  v.tex_coord.xy = uv.gr;
  v.pos.xy = command[0].rect.zy * scale1 + translate1;
  triangles.Append(v);

  // bottom right
  v.tex_coord.xy = uv.gb;
  v.pos.xy = command[0].rect.zw * scale1 + translate1;
  triangles.Append(v);
}
