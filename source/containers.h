#pragma once
#include <stdint.h>

template <typename T>
struct range
{
public:
  T& begin() { return m_begin; }
  T& end() { return m_end; }
  const T& begin() const { return m_begin; }
  const T& end() const { return m_end; }

private:
  template <typename T>
  friend range<T> make_range(T, T);

  T m_begin;
  T m_end;
};

template <typename T>
range<T> make_range(T begin, T end)
{
  range<T> result;
  result.m_begin = std::move(begin);
  result.m_end = std::move(end);
  return result;
}

namespace std
{
template <typename T, typename U>
T begin(pair<T, U> p)
{
  return p.first;
}

template <typename T, typename U>
U end(pair<T, U> p)
{
  return p.second;
}
}
