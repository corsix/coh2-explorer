struct command_t
{
  uint kind : K;
  float4 colour : C;
  float4 sprite : S;
  float4 rect : P;
};

command_t main(command_t v)
{
  v.sprite = float4(0.f, 256.f / 255.f, 256.f / 255.f, 0.f);
  return v;
}
