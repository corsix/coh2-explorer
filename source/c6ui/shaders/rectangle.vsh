struct command_t
{
  uint kind : K;
  float4 colour : C;
  float4 colour_secondary : S;
  float4 rect : P;
};

command_t main(command_t v)
{
  return v;
}
