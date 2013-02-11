#ifdef ARENA_WITH_VARIADIC_TEMPLATES
  template <typename T, typename... Args>
  T* METHOD(Args&&... args) {
    return new (static_cast<T*>(MALLOC)) T(std::forward<Args>(args)...);
  }
#else
  template <typename T>
  T* METHOD() {
    return new (static_cast<T*>(MALLOC)) T();
  }

  template <typename T, typename A1>
  T* METHOD(A1&& a1) {
    return new (static_cast<T*>(MALLOC)) T(std::forward<A1>(a1));
  }

  template <typename T, typename A1, typename A2>
  T* METHOD(A1&& a1, A2&& a2) {
    return new (static_cast<T*>(MALLOC)) T(std::forward<A1>(a1), std::forward<A2>(a2));
  }

  template <typename T, typename A1, typename A2, typename A3>
  T* METHOD(A1&& a1, A2&& a2, A3&& a3) {
    return new (static_cast<T*>(MALLOC)) T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3));
  }

  template <typename T, typename A1, typename A2, typename A3, typename A4>
  T* METHOD(A1&& a1, A2&& a2, A3&& a3, A4&& a4) {
    return new (static_cast<T*>(MALLOC)) T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3), std::forward<A4>(a4));
  }
#endif

#undef METHOD
#undef MALLOC
