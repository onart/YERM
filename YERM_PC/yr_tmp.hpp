#ifndef __YR_TMP_HPP__
#define __YR_TMP_HPP__

#include <type_traits>

namespace onart{

    // meta-meta
#define DECLARE_MEM_CHECKER(f)\
    template <class T>\
    class __has##f{\
        using _1b = char;\
        struct _2b {_1b _[2];};\
        template<class C> static _1b test(decltype(&C::f));\
        template<class C> static _2b test(...);\
    public:\
        constexpr static bool hasv = sizeof(test<T>(0)) == sizeof(_1b);\
    };\
    template<class T> inline constexpr bool HAS_##f = __has##f<T>::hasv

#define DECLARE_MEM_SIG_CHECKER(f)\
    template <typename, typename T> struct has_##f : std::false_type {};\
    template <typename C, typename Ret, typename... Args>\
    struct has_##f<C, Ret(Args...)> {\
    private:\
        template <typename T>\
        static auto test(T*) -> decltype(std::declval<T>().f(std::declval<Args>()...), std::true_type{});\
        template <typename> static std::false_type test(...);\
    public:\
        using type = decltype(test<C>(nullptr));\
        static constexpr bool value = type::value;\
    };
}

#endif