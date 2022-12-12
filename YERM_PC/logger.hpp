// C++11 or newer needed
#ifndef __OA_LOGGER_HPP__
#define __OA_LOGGER_HPP__

#include <iostream>
#include <string>

template <class... T>
inline void __logMultiple(const T&...);

template<>
inline void __logMultiple(){ std::cout << std::endl; }

template <class F>
inline void __logMultiple(const F& first){ std::cout << first << std::endl; }

template <class F, class... T>
inline void __logMultiple(const F& first, const T &...extra){
  std::cout << first << ' ';
  __logMultiple(extra...);
}

template <class... T>
inline void __logPosition(const std::string& file, int line, const std::string& func, const T&... extra){
    std::cout << file << ':' << line << ' ';
    __logMultiple(func, extra...);
}

#define LOGHERE __logPosition(__FILE__, __LINE__, __func__)
#define LOGWITH(...) __logPosition(__FILE__, __LINE__, __func__, __VA_ARGS__)

#endif