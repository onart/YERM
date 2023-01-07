// Copyright 2022 onart@github. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef __YR_TUPLE_HPP__
#define __YR_TUPLE_HPP__

#include <type_traits>
#include <cstddef>
#include <vector>

#define POSSIBLE_COMPONENT(t) virtual t * get_##t() { return nullptr; } virtual std::vector<t *> gets_##t() { return {}; }
#define TEMPLATE_VIRTUAL(t) t * get_##t() { return get<t>(); } std::vector<t *> gets_##t() { return gets<t>(); }

namespace onart {
    // something useful with the class below

    /// @brief 이 상수는 첫 번째 템플릿 인수 타입과 같은 것이 나머지 중에 있는 경우 참입니다.
    template <class, class...>
    inline constexpr bool is_one_of = false;

    /// @brief 이 상수는 첫 번째 템플릿 인수 타입과 같은 것이 나머지 중에 있는 경우 참입니다.
    template <class A, class B, class... T>
    inline constexpr bool is_one_of<A, B, T...> = std::is_same_v<A, B> || is_one_of<A, T...>;

    /// @brief 이 상수는 첫 번째 템플릿 인수 타입과 같은 것의 개수를 뜻합니다.
    template <class, class...>
    inline constexpr size_t count_of = 0;

    /// @brief 이 상수는 첫 번째 템플릿 인수 타입과 같은 것의 개수를 뜻합니다.
    template <class A, class B, class... T>
    inline constexpr size_t count_of <A, B, T...> = (int)std::is_same_v<A, B> + count_of<A, T...>;
    // something useful with the class below

    /// @brief 주어진 타입을 각각 멤버로 순서대로 가지고 있으며, 각각의 멤버에 대하여 같은 함수를 호출할 수 있는 자료구조입니다.
    /// 일반적인 std::tuple은 멤버가 메모리 상의 배치가 명시된 것의 역순으로 배치되었다는 점에서 다릅니다. 메모리 상 순서가 순차적으로 되면 유리할 경우에 이 클래스를 활용하세요.
    /// 그 외의 경우는 컴파일 속도가 약간 차이가 나므로 std::tuple이나 하단의 onart::rtuple을 사용합니다.
    template<class... T>
    struct ftuple;

    /// @brief 사용할 일 없는 클래스입니다. @ref ftuple 을 참고하세요.
    template<class F, size_t i = 0>
    struct ftupleBase {
    public:
        F first;
        ftupleBase() = default;
        ftupleBase(const ftupleBase&) = default;
        ftupleBase& operator=(const ftupleBase&) = default;
    protected:
        using Type = F;
        using firstType = ftupleBase<F, i>;
    };

    /// @brief 주어진 타입을 각각 멤버로 순서대로 가지고 있는 자료구조입니다.
    /// 일반적인 std::tuple은 멤버가 메모리 상의 배치가 명시된 것의 역순으로 배치되었다는 점에서 다릅니다. 메모리 상 순서가 순차적으로 되면 유리할 경우에 이 클래스를 활용하세요.
    /// 그 외의 경우는 컴파일 속도가 약간 차이가 나므로 std::tuple이나 하단의 onart::rtuple을 사용합니다.
    template<class F>
    struct ftuple<F> {
    public:
        F first;

        /// @brief 템플릿 인수로 주어진 번호(0부터)의 멤버의 참조를 리턴합니다. 번호가 주어진 타입의 수를 넘어가면 컴파일되지 않습니다.
        template<unsigned POS, std::enable_if_t<POS == 0, bool> = false>
        constexpr auto& get() {
            return first;
        }

        /// @brief 템플릿 인수로 주어진 타입의 멤버 중 첫 번째를 가리키는 포인터를 리턴합니다. 없으면 nullptr를 리턴합니다.
        template<class C>
        constexpr C* get() {
            if constexpr (std::is_same_v<C, F>) { return &first; }
            else { return nullptr; }
        }

        /// @brief 템플릿 인수로 주어진 타입의 멤버를 가리키는 포인터를 모두 std::vector에 담아 리턴합니다.
        template<class C>
        constexpr std::vector<C*> gets() {
            if constexpr (std::is_same_v<C, F>) { return { &first }; }
            else { return {}; }
        }

        /// @brief 템플릿 인수로 주어진 번호의 멤버의 메모리 상 오프셋을 리턴합니다. 번호가 주어진 타입의 수를 넘어가면 컴파일되지 않습니다.
        template<unsigned POS, std::enable_if_t<POS == 0, bool> = false>
        constexpr static size_t offset() {
            return 0;
        }
        ftuple() = default;
        ftuple(const ftuple&) = default;
        ftuple(const F& f) : first(f) {}
        ftuple(F&& f): first(f){}
        ftuple& operator=(const ftuple&) = default;
    protected:
        using Type = F;
        using firstType = ftuple<F>;
        using lastType = void;
    };

    /// @brief 주어진 타입을 각각 멤버로 순서대로 가지고 있으며, 각각의 멤버에 대하여 같은 함수를 호출할 수 있는 자료구조입니다.
    /// 일반적인 std::tuple은 멤버가 메모리 상의 배치가 명시된 것의 역순으로 배치되었다는 점에서 다릅니다. 메모리 상 순서가 순차적으로 되면 유리할 경우에 이 클래스를 활용하세요.
    /// 그 외의 경우는 컴파일 속도가 약간 차이가 나므로 std::tuple이나 하단의 onart::rtuple을 사용합니다.
    template<class F, class... T>
    struct ftuple<F, T...> : ftupleBase<F, sizeof...(T)>, ftuple<T...> {
    protected:
        using Type = F;
        using firstType = ftupleBase<F, sizeof...(T)>;
        using lastType = ftuple<T...>;
    private:
        template<unsigned POS, class FT>
        constexpr auto& get() {
            if constexpr (POS == 0) return FT::firstType::first;
            else return get<POS - 1, typename FT::lastType>();
        }

        template <class FT, class C>
        constexpr C* get() {
            if constexpr (std::is_same_v<typename FT::Type, C>) { return &(FT::firstType::first); }
            else { return get<typename FT::lastType, C>(); }
        }

        template <class FT, class C>
        constexpr void gets(std::vector<C*>& ret) {
            if constexpr (std::is_same_v<typename FT::Type, C>) { ret.push_back(&(FT::firstType::first)); }
            if constexpr (!std::is_same_v<typename FT::lastType, void>) gets<typename FT::lastType, C>(ret);
        }

        template <unsigned POS, class FT>
        constexpr static size_t offset() {
            using thisType = ftuple<F, T...>;
            if constexpr (POS == 0) return (::size_t)reinterpret_cast<char const volatile*>(&(((thisType*)0)->FT::firstType::first));   // warning: this one can't be calculated in compile time
            else return offset<POS - 1, typename FT::lastType>();
        }
    public:
        /// @brief 템플릿 인수로 주어진 번호(0부터)의 멤버의 참조를 리턴합니다. 번호가 주어진 템플릿 인수의 수를 넘어가면 컴파일되지 않습니다.
        template<unsigned POS, std::enable_if_t<POS <= sizeof...(T), bool> = false>
        constexpr auto& get() {
            static_assert(POS <= sizeof...(T), "Index exceeded");
            return get<POS, ftuple<F, T...>>();
        }

        /// @brief 템플릿 인수로 주어진 타입의 멤버 중 첫 번째를 가리키는 포인터를 리턴합니다. 없으면 nullptr를 리턴합니다.
        template <class C>
        constexpr C* get() {
            if constexpr (!is_one_of<C, F, T...>) { return nullptr; }
            else return get<ftuple<F, T...>, C>();
        }

        /// @brief 템플릿 인수로 주어진 타입의 멤버를 가리키는 포인터를 모두 std::vector에 담아 리턴합니다.
        template <class C>
        constexpr std::vector<C*> gets() {
            if constexpr (0 == count_of<C, F, T...>) { return {}; }
            else {
                std::vector<C*> ret;
                ret.reserve(count_of<C, F, T...>);
                gets<ftuple<F, T...>, C>(ret);
                return ret;
            }
        }

        /// @brief 템플릿 인수로 주어진 번호의 멤버의 오프셋을 리턴합니다. 번호가 주어진 템플릿 인수의 수를 넘어가면 컴파일되지 않습니다.
        template<unsigned POS, std::enable_if_t<POS <= sizeof...(T), bool> = false>
        constexpr static size_t offset() {
            return offset<POS, ftuple<F, T...>>();
        }

        ftuple() = default;
        ftuple(const ftuple&) = default;
        ftuple(F&& f, T&&... t): firstType{f}, lastType(t...){}
        ftuple(const F& f, const T&... t) : firstType{ f }, lastType(t...) {}
        ftuple& operator=(const ftuple&) = default;
    };
    
    /// @brief 주어진 타입을 각각 멤버로 순서대로 가지고 있으며, 각각의 멤버에 대하여 같은 함수를 호출할 수 있는 자료구조입니다.
    /// @ref ftuple과 다르게 멤버가 메모리 상에 역순으로 배치되어 있습니다. 메모리 상 순서가 순차적으로 되면 유리할 경우에는 ftuple을 활용하세요.
    template<class...>
    struct rtuple;

    /// @brief 주어진 타입을 각각 멤버로 순서대로 가지고 있으며, 각각의 멤버에 대하여 같은 함수를 호출할 수 있는 자료구조입니다.
    /// @ref ftuple과 다르게 멤버가 메모리 상에 역순으로 배치되어 있습니다. 메모리 상 순서가 순차적으로 되면 유리할 경우에는 ftuple을 활용하세요.
    template<class F>
    struct rtuple<F>{
    public:
        F first;

        /// @brief 템플릿 인수로 주어진 번호(0부터)의 멤버의 참조를 리턴합니다. 번호가 주어진 타입의 수를 넘어가면 컴파일되지 않습니다.
        template<unsigned POS, std::enable_if_t<POS == 0, bool> = false>
        constexpr auto& get(){
            return first;
        }

        /// @brief 템플릿 인수로 주어진 타입의 멤버 중 첫 번째를 가리키는 포인터를 리턴합니다. 없으면 nullptr를 리턴합니다.
        template<class C>
        constexpr C* get() {
            if constexpr (std::is_same_v<C, F>) { return &first; }
            else { return nullptr; }
        }

        /// @brief 템플릿 인수로 주어진 타입의 멤버를 가리키는 포인터를 모두 std::vector에 담아 리턴합니다.
        template<class C>
        constexpr std::vector<C*> gets() {
            if constexpr (std::is_same_v<C, F>) { return { &first }; }
            else { return {}; }
        }

        /// @brief 템플릿 인수로 주어진 번호의 멤버의 메모리 상 오프셋을 리턴합니다. 번호가 주어진 타입의 수를 넘어가면 컴파일되지 않습니다.
        template<unsigned POS, std::enable_if_t<POS == 0, bool> = false>
        constexpr static size_t offset() {
            return 0;
        }

        rtuple() = default;
        rtuple(const rtuple&) = default;
        rtuple(const F& f) : first(f) {}
        rtuple(F&& f): first(f) {}
        rtuple& operator=(const rtuple&) = default;
    protected:
        using Type = F;
        using fisrtType = rtuple<F>;
        using lastType = void;
    };
    
    /// @brief 주어진 타입을 각각 멤버로 순서대로 가지고 있는 자료구조입니다.
    /// @ref ftuple과 다르게 멤버가 메모리 상에 역순으로 배치되어 있습니다. 메모리 상 순서가 순차적으로 되면 유리할 경우에는 ftuple을 활용하세요.
    template<class F, class... T>
    struct rtuple<F, T...>: rtuple<T...>{
        protected:
            using Type = F;
            using firstType = rtuple<F>;
            using lastType = rtuple<T...>;
        private:
            template<unsigned POS, class FT>
            constexpr auto& get(){
                if constexpr (POS == 0) return FT::firstType::first;
                else return get<POS - 1, FT::lastType>();
            }
            template <class FT, class C>
            constexpr C* get() {
                if constexpr (std::is_same_v<typename FT::Type, C>) { return &(FT::firstType::first); }
                else { return get<typename FT::lastType, C>(); }
            }
            template <class FT, class C>
            constexpr void gets(std::vector<C*>& ret) {
                if constexpr (std::is_same_v<typename FT::Type, C>) { ret.push_back(&(FT::firstType::first)); }
                if constexpr (!std::is_same_v<typename FT::lastType, void>) gets<typename FT::lastType, C>(ret);
            }
            template <unsigned POS, class FT>
            constexpr static size_t offset() {
                using thisType = rtuple<F, T...>;
                if constexpr (POS == 0) return (::size_t)reinterpret_cast<char const volatile*>(&(((thisType*)0)->FT::firstType::first));   // warning: this one can't be calculated in compile time
                else return offset<POS - 1, typename FT::lastType>();
            }
        public:
            /// @brief 템플릿 인수로 주어진 번호(0부터)의 멤버의 참조를 리턴합니다. 번호가 주어진 템플릿 인수의 수를 넘어가면 컴파일되지 않습니다.
            template<unsigned POS, std::enable_if_t<POS <= sizeof...(T), bool> = false>
            constexpr auto& get(){
                return get<POS, rtuple<F, T...>>;
            }
            /// @brief 템플릿 인수로 주어진 타입의 멤버 중 첫 번째를 가리키는 포인터를 리턴합니다. 없으면 nullptr를 리턴합니다.
            template <class C>
            constexpr C* get(){
                if constexpr (!is_one_of<C, F, T...>) { return nullptr; }
                else return get<rtuple<F, T...>, C>();
            }
            /// @brief 템플릿 인수로 주어진 타입의 멤버를 가리키는 포인터를 모두 std::vector에 담아 리턴합니다.
            template <class C>
            constexpr std::vector<C*> gets() {
                if constexpr (0 == count_of<C, F, T...>) { return {}; }
                else {
                    std::vector<C*> ret;
                    ret.reserve(count_of<C, F, T...>);
                    gets<rtuple<F, T...>, C>(ret);
                    return ret;
                }
            }

            /// @brief 템플릿 인수로 주어진 번호의 멤버의 오프셋을 리턴합니다. 번호가 주어진 템플릿 인수의 수를 넘어가면 컴파일되지 않습니다.
            template<unsigned POS, std::enable_if_t<POS <= sizeof...(T), bool> = false>
            constexpr static size_t offset() {
                return offset<POS, rtuple<F, T...>>();
            }
            
            rtuple() = default;
            rtuple(const rtuple&) = default;
            rtuple(const F& f, const T&... t) : firstType{ f }, lastType(t...) {}
            rtuple(F&& f, T&&... t): firstType{f}, lastType(t...) {}
            rtuple& operator=(const rtuple&) = default;
    };
}

#endif