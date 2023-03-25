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
#include <cstdint>
#include <vector>
#include <memory>
#include <type_traits>
#include <mutex>
#include <new>
#include <forward_list>

#include "logger.hpp"

namespace onart{
    /// @brief 일정량의 데이터를 보유하는 메모리 풀입니다. shared_ptr를 하나씩 꺼낼 수 있으며, 더 꺼낼 것이 없는 경우 빈 포인터를 리턴합니다. 여기서 꺼낸 포인터에 대하여 명시적으로 delete를 수행할 수 없습니다.
    /// 풀은 복사와 이동이 불가능합니다. 풀은 스레드 안전하지 않습니다.
    /// @tparam T 개별 객체의 타입입니다.
    /// @tparam CAPACITY 최대 크기입니다. 런타임에 변할 수 없으며, 합리적인 최대 수가 정해져 있지 않은 경우 @ref DynamicPool 클래스를 사용합니다.
    template <class T, size_t CAPACITY = 256>
    struct Pool{
        inline Pool(){
            v = (T*)std::malloc(sizeof(T) * CAPACITY);
            stack = (size_t*)std::malloc(sizeof(decltype(stack[0])) * CAPACITY);
            for(int i = 0;i < CAPACITY;i++){
                stack[i] = CAPACITY - 1 - i;
            }
            tail = CAPACITY - 1;
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
        bool isFull() { return tail == (CAPACITY - 1);}

        Pool(const Pool&) = delete;
        Pool(Pool&& src) = delete;
        Pool& operator=(const Pool&) = delete;
        Pool& operator=(Pool&& src) = delete;

        /// @brief 풀에서 객체 하나를 초기화하여 shared_ptr로 얻어옵니다.
        /// @return 풀이 비어 있어서 실패하면 빈 포인터를 리턴합니다.
        template <class... Args>
        inline std::shared_ptr<T> get(Args&&... args){
            if(tail == (size_t) - 1) return std::shared_ptr<T>();
            new (&v[stack[tail]]) T(args...);
            return std::shared_ptr<T>(&v[stack[tail--]],[this](T* p){
                ret(p);
            });
        }

        /// @brief 풀에서 객체 하나를 초기화하여 shared_ptr로 얻어옵니다. 생성과 소멸은 스레드 안전합니다.
        /// @return 풀이 비어 있어서 실패하면 빈 포인터를 리턴합니다.
        template <class... Args>
        inline std::shared_ptr<T> lockedGet(Args&&... args){
            std::unique_lock _(lock);
            if(tail == (size_t) - 1) return std::shared_ptr<T>();
            new (&v[stack[tail]]) T(args...);
            return std::shared_ptr<T>(&v[stack[tail--]],[this](T* p){
                std::unique_lock _(lock);
                ret(p);
            });
        }

        /// @brief 풀에서 객체 하나를 초기화하여 기본 포인터 형태로 얻어옵니다.
        /// @return 풀이 비어 있어서 실패하면 빈 포인터를 리턴합니다.
        template<class... Args>
        inline T* getRaw(Args&&... args){
            if(tail == (size_t) - 1) return nullptr;
            new (&v[stack[tail]]) T(args...);
            return &v[stack[tail--]];
        }

        /// @brief 풀에 객체를 되돌려 놓습니다. 이 풀에서 나온 정상적인 포인터를 주는 경우 소멸자는 호출됩니다.
        /// @return 되돌려 놓기에 성공하면 true를 리턴합니다.
        inline bool returnRaw(T* p){
            size_t idx = p - v;
            if(idx < CAPACITY){
                p->~T();
                stack[++tail] = idx;
                return true;
            }
            return false;
        }

    private:
        /// @brief 풀에 객체를 되돌려 놓습니다.
        inline void ret(T* p){
            if(v == nullptr) return;
            size_t idx = p - v;
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
        public:
            inline DynamicPool() { head = new Node; }

        /// @brief shared_ptr 객체를 생성하여 리턴합니다.
        template<class... Args>
        inline std::shared_ptr<T> get(Args&&... args){
            Node* node = head;
            while(true){
                std::shared_ptr<T> p = node->pool.get(args...);
                if(p) return p;
                else {
                    if(node->next == nullptr) {
                        node->next = new Node;
                    }
                    node = node->next;
                }
            }
        }

        /// @brief 객체를 생성하여 일반 포인터를 리턴합니다. 일반 포인터는 명시적으로 returnRaw를 통해 돌려놓아야 합니다.
        template<class... Args>
        inline T* getRaw(Args&&... args){
            Node* node = head;
            while(true){
                T* p = node->pool.getRaw(args...);
                if(p) return p;
                else {
                    if(node->next == nullptr) {
                        node->next = new Node;
                    }
                    node = node->next;
                }
            }
        }

        /// @brief getRaw()로 생성된 객체를 되돌려 넣습니다. 정상적으로 되돌려 놓은 경우 소멸자는 호출됩니다.
        inline void returnRaw(T* p){
            Node* node = head;
            while(node && !node->pool.returnRaw(p)){
                node = node->next;
            }
        }


        /// @brief 미사용 풀을 해제합니다.
        inline void shrink() {
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
    
    /// @brief 상호 배제 없이 작동하는 메모리 풀로, 할당 스레드 하나, 해제 스레드 하나인 경우에 사용할 수 있습니다. 그 외의 경우에는 외부에서 동기화가 필요합니다.
    /// @tparam T 할당 객체 타입
    /// @tparam BLOCK 한 번에 할당될 메모리 블록 수
    template<class T, size_t BLOCK = 32>
    struct QueuePool{
        private:
            union Node{
                T t;
                Node* next = nullptr;
            };
            Node* head = nullptr;
            Node* tail = nullptr;
            std::forward_list<uint8_t*> space;
            inline void alloc(){
                uint8_t* newSpace = new uint8_t[BLOCK * sizeof(Node)];
                space.push_front(newSpace);
                Node* formerHead = head;
                head = reinterpret_cast<Node*>(newSpace);
                Node* nd = head;
                for(size_t i = 0; i < BLOCK; i++){
                    nd->next = head + i;
                    nd = nd->next;
                }
                nd->next = formerHead;
            }
            inline void dealloc(T* p){
                tail->next = reinterpret_cast<Node*>(p);
                tail->next->next = nullptr;
                tail = tail->next;
            }
        public:
            inline QueuePool(){
                uint8_t* newSpace = new uint8_t[BLOCK * sizeof(Node)];
                space.push_front(newSpace);
                head = reinterpret_cast<Node*>(newSpace);
                Node* nd = head;
                for(size_t i = 0; i < BLOCK; i++){
                    nd->next = head + i;
                    nd = nd->next;
                }
                tail = nd;
                tail->next = nullptr;
            }
            inline ~QueuePool(){
                for(uint8_t* block: space) delete[] block;
                space.clear();
            }
            template<class... Args>
            inline std::shared_ptr<T> get(Args&&... args){
                if(!head->next){
                    alloc();
                }
                new (&(head->t)) T(args...);
                std::shared_ptr<T> ret(&(head->t),[this](T* p){ dealloc(p); });
                head = head->next;
                return ret;
            }
    };
}

#endif