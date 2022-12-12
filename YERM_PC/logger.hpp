// C++11 or newer needed
#ifndef __OA_LOGGER_HPP__
#define __OA_LOGGER_HPP__

#include "../externals/boost/predef/platform.h"
#include <iostream>
#include <sstream>
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

template <class... T>
inline void __getMultiple(std::ostream&, const T&...);

template<>
inline void __getMultiple(std::ostream& strm) { strm << '\n'; }

template<class F>
inline void __getMultiple(std::ostream& strm, const F& first) { strm << first << '\n'; }

template<class F, class... T>
inline void __getMultiple(std::ostream& strm, const F& first, const T&... extra) { 
    strm << first << ' ';
    __getMultiple(strm, extra...);
}

template <class... T>
inline std::string __getLogContent(const std::string& file, int line, const std::string& func, const T&... extra) {
    std::string ret;
    std::ostringstream strm;
    
    strm << file << ':' << line << ' ';
    __getMultiple(strm, func, extra...);
    return strm.str();
}

#ifdef BOOST_PLAT_ANDROID_AVAILABLE
#include <android/log.h>
    
    #define LOGHERE __android_log_print(ANDROID_LOG_DEBUG, "", "%s:%d %s\n", __FILE__, __LINE__, __func__)
    #define LOGWITH(...) __android_log_print(ANDROID_LOG_DEBUG, "", "%s", __getLogContent(__FILE__, __LINE__, __func__, __VA_ARGS__).c_str())
#else
    #define LOGHERE __logPosition(__FILE__, __LINE__, __func__)
    #define LOGWITH(...) __logPosition(__FILE__, __LINE__, __func__, __VA_ARGS__)
#endif // DEBUG

#endif