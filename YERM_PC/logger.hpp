// Unlicense.
// C++11 or newer needed
#ifndef __OA_LOGGER_HPP__
#define __OA_LOGGER_HPP__

#include "../externals/boost/predef/platform.h"
#include <iostream>
#include <sstream>
#include <string>

template <class... T>
inline void __getMultiple(std::ostream&, const T&...);

template<>
inline void __getMultiple(std::ostream& strm) { strm << std::endl; }

template<class F>
inline void __getMultiple(std::ostream& strm, const F& first) { strm << first << std::endl; }

template<class F, class... T>
inline void __getMultiple(std::ostream& strm, const F& first, const T&... extra) { 
    strm << first << ' ';
    __getMultiple(strm, extra...);
}

template <class... T>
inline std::string __getLogContent(const std::string& file, int line, const std::string& func, const T&... extra) {
    std::ostringstream strm;
    strm << file << ':' << line << ' ' << func;
    if constexpr(sizeof...(T) > 0) strm << ": ";
    __getMultiple(strm, extra...);
    return strm.str();
}

template <class... T>
inline void __logPosition(const std::string& file, int line, const std::string& func, const T&... extra){
    std::cout << file << ':' << line << ' ' << func;
    if constexpr(sizeof...(T) > 0) std::cout << ": ";
    __getMultiple(std::cout, extra...);
}

/// @brief 콘솔이 아닌 다른 곳에 로그를 하고 싶을 때 사용합니다.
template <class... T>
inline std::string toString(const T&... extra) {
    std::ostringstream strm;
    __getMultiple(strm, extra...);
    return strm.str();
}

#ifdef YR_NO_LOG
    #define LOGHERE
    #define LOGWITH(...)
    #define LOGRAW(...)
#else
    #ifdef BOOST_PLAT_ANDROID_AVAILABLE
    #include <android/log.h>
        
        #define LOGHERE __android_log_print(ANDROID_LOG_DEBUG, "", "%s:%d %s\n", __FILE__, __LINE__, __func__)
        #define LOGWITH(...) __android_log_print(ANDROID_LOG_DEBUG, "", "%s", __getLogContent(__FILE__, __LINE__, __func__, __VA_ARGS__).c_str())
        #define LOGRAW(...) __android_log_print(ANDROID_LOG_DEBUG, "", "%s", toString(__VA_ARGS__).c_str())
    #elif defined(YR_USE_WEBGPU)
    #include "../externals/wasm_webgpu/miniprintf.h"

        #define LOGHERE emscripten_mini_stdio_printf("%s:%d %s\n", __FILE__, __LINE__, __func__)
        #define LOGWITH(...) emscripten_mini_stdio_printf("%s", __getLogContent(__FILE__, __LINE__, __func__, __VA_ARGS__).c_str())
        #define LOGRAW(...) emscripten_mini_stdio_printf("%s", toString(__VA_ARGS__).c_str())
    #else
        #define LOGHERE __logPosition(__FILE__, __LINE__, __func__)
        #define LOGWITH(...) __logPosition(__FILE__, __LINE__, __func__, __VA_ARGS__)
        #define LOGRAW(...) __getMultiple(std::cout, __VA_ARGS__)
    #endif
#endif

#endif