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
#ifndef __YR_POOL_HPP__
#define __YR_POOL_HPP__

#include <cstdlib>
#include <vector>
#include <memory>
#include <type_traits>
#include <mutex>
#include <new>

#include "logger.hpp"

namespace onart{
    /// @brief 일정량의 데이터를 보유하는 메모리 풀입니다. shared_ptr를 하나씩 꺼낼 수 있으며, 더 꺼낼 것이 없는 경우 빈 포인터를 리턴합니다. 여기서 꺼낸 포인터에 대하여 명시적으로 delete를 수행할 수 없습니다.
    /// 풀은 복사와 이동이 불가능합니다. 풀은 스레드 안전합니다.
    /// @tparam T 개별 객체의 타입입니다.
    /// @tparam CAPACITY 최대 크기입니다. 런타임에 변할 수 없으며, 합리적인 최대 수가 정해져 있지 않은 경우 @ref DynamicPool 클래스를 사용합니다.
    template <class T, size_t CAPACITY = 256>
    struct Pool{
        inline Pool(){
            v = std::malloc(sizeof(T) * CAPACITY);
            stack = std::malloc(sizeof(decltype(stack[0])) * CAPACITY);
            for(int i = 0;i < CAPACITY;i++){
                stack[i] = CAPACITY - 1 - i;
            }
            tail = CAPACITY;
        }
        inline ~Pool(){
            if(!isFull()) {
                LOGWITH("FATAL ERROR: A POOL DESTROYED BEFORE THE ENTITIES INSIDE. You can Ignore this if this is called after the main function have returned");
            }
            std::free(v);
            std::free(stack);
            v = nullptr;
            stack = nullptr;
        }

        /// @brief 풀이 가득 찬 상태인지 확인합니다.
        bool isFull() const { std::unique_lock<std::mutex> _(lock); return tail == (CAPACITY - 1);}

        Pool(const Pool&) = delete;
        Pool(Pool&& src) = delete;
        Pool& operator=(const Pool&) = delete;
        Pool& operator=(Pool&& src) = delete;

        /// @brief 풀에서 객체 하나를 초기화하여 얻어옵니다.
        /// @return 풀이 비어 있어서 실패하면 빈 포인터를 리턴합니다.
        template <class... Args>
        inline std::shared_ptr<T> get(Args&&... args){
            std::unique_lock<std::mutex> _(lock);
            if(tail == (size_t) - 1) return std::shared_ptr<T>();
            new (&v[stack[tail]]) T(std::forward(args...));
            return std::shared_ptr<T>(&v[stack[tail--]],[this](T* p){
                ret(p);
            });
        }
    private:
        /// @brief 풀에 객체를 되돌려 놓습니다.
        inline void ret(T* p){
            if(v == nullptr) return;
            size_t idx = p - v;
            std::unique_lock<std::mutex> _(lock);
            stack[++tail] = idx;
        }
        T* v = nullptr;
        size_t* stack = nullptr;
        size_t tail = CAPACITY - 1;
        std::mutex lock;
    };

    /// @brief 
    /// @tparam T 개별 객체의 타입입니다. 이동 할당이 가능해야 합니다.
    /// @tparam CAPACITY 한 번에 할당할 단위입니다.
    template<class T, size_t CAPACITY = 256>
    struct DynamicPool{
        private:
            struct Node{
                Pool<T, CAPACITY> pool;
                Node* next = nullptr;
            };
            Node* head;
            std::mutex lock;
        public:
        inline DynamicPool(){ head = new Node; }

        template<class... Args>
        inline std::shared_ptr<T> get(Args&&... args){
            std::unique_lock<std::mutex> _(lock);
            Node* node = head;
            while(true){
                std::shared_ptr<T> p = node->pool.get(args...);
                if(p) return args;
                else {
                    if(node->next == nullptr) {
                        node->next = new Node;
                    }
                    node = node->next;
                }
            }
        }
        /// @brief 미사용 풀을 해제합니다.
        inline void shrink() {
            std::unique_lock<std::mutex> _(lock);
            Node* node = head;
            while(node->next){
                if(node->next->pool.isFull()) {
                    Node* dels = node->next;
                    node->next = node->next->next;
                    delete dels;
                }
                else {
                    node = node->next;
                }
            }
        }
    };
    
}

#endif