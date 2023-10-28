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
#ifndef __YR_THREADPOOL_HPP__
#define __YR_THREADPOOL_HPP__

#include <cstdint>

#include <functional>
#include <deque>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace onart{

    union variant8 {
        uint8_t bytedata1[8];
        uint16_t bytedata2[4];
        uint32_t bytedata4[2];
#define TYPE_N_CAST(type, varname) type varname; inline variant8(type t):varname(t){} inline type& operator=(type t){return varname = t;}
        TYPE_N_CAST(int8_t, i8)
        TYPE_N_CAST(uint8_t, u8)
        TYPE_N_CAST(int16_t, i16)
        TYPE_N_CAST(uint16_t, u16)
        TYPE_N_CAST(int32_t, i32)
        TYPE_N_CAST(uint32_t, u32)
        TYPE_N_CAST(int64_t, i64)
        TYPE_N_CAST(uint64_t, u64)
        TYPE_N_CAST(float, f)
        TYPE_N_CAST(double, db)
        TYPE_N_CAST(void*, vp)
#undef TYPE_N_CAST
        inline variant8(const variant8& other):u64(other.u64) {}
        inline variant8():u64(0) {}
    };

    template<class T>
    class ReservedQueue{
        public:
        struct Node{
            T v = {};
            uint32_t next = ~0u;
        };
        inline void clear(){
            head = tail = ~0u;
            ind.resize(ind.capacity());
            for(size_t i = 0 ; i < ind.size() ; i++){
                ind[i] = (uint32_t)(ind.size() - 1 - i);
            }
        }
        inline ReservedQueue(size_t reserve = 256):data(reserve),ind(reserve){
            for(size_t i = 0 ; i < reserve ; i++){
                ind[i] = (uint32_t)(reserve - 1 - i);
            }
        }
        inline void enqueue(const T& v) {
            if(ind.size() == 0) {
                size_t newSize = data.size() * 2;
                for (size_t i = data.size(); i < newSize; i++) { ind[i] = uint32_t(newSize - 1 - i); }
                data.resize(newSize);
            }
            uint32_t newTail = ind.back();
            data[newTail].v = v;
            data[newTail].next = ~0u;

            if(empty()) {
                head = tail = newTail;
            }
            else{
                data[tail].next = newTail;
                tail = newTail;
            }
        }
        inline bool empty() const { return head == ~0u; }
        inline T dequeue() {
            if(empty()){
                return {};
            }
            else{
                T* ret = &data[head].v;
                ind.push_back(head);
                uint32_t formerHead = head;
                head = data[head].next;
                data[formerHead].next = ~0u;
                if(empty()) tail = ~0u;
                return *ret;
            }
        }
        inline const T* peek() const{
            if(empty()){
                return nullptr;
            }
            else{
                return &data[head].v;
            }
        }
        inline const T* next(const T* node) const {
            if((void*)node > (void*)(data.data() + data.size()) || (void*)node < (void*)data.data()) return nullptr;
            uint32_t next = reinterpret_cast<const Node*>(node)->next;
            if(next == ~0u) return nullptr;
            return &data[next].v;
        }
        inline void erase(const T* oneBeforeTarget) {
            if(oneBeforeTarget == nullptr) return (void)dequeue();
            if((void*)oneBeforeTarget > (void*)(data.data() + data.size()) || (void*)oneBeforeTarget < data.data()) return;
            Node* obt = const_cast<Node*>(reinterpret_cast<const Node*>(oneBeforeTarget));
            uint32_t next = obt->next;
            if(next == ~0u) return;
            data[next].next = ~0u;
            obt->next = data[next].next;
            ind.push_back(next);
        }
        private:
        uint32_t head = ~0u;
        uint32_t tail = ~0u;
        std::vector<Node> data;
        std::vector<uint32_t> ind;
    };

    /// @brief 작업을 비동기적으로 수행하기 위한 스레드 풀입니다. 스레드 수는 런타임에 정할 수 있으며 최대 스레드 수는 8입니다. 스레드 수를 0으로 정할 수도 있으며 이때는 post를 해도 아무 동작도 하지 않습니다.
    class ThreadPool{
        public:
            inline ThreadPool(size_t n = 1) {
                if(n > 8) n = 8;
                if(n == 0) return;
                workers.reserve(n);
                afterService.reserve(256);
                afterService2.reserve(256);
                for(size_t i = 0; i < n; i++){
                    workers.emplace_back([this, i](){execute(this, i);});
                }
            }
            inline ~ThreadPool() {
                stop = true;
                cond.notify_all();
                for(std::thread& t: workers) t.join();
            }
            /// @brief 스레드 풀에 진행 중이거나 대기 중인 작업이 있는지 리턴합니다.
            inline bool waiting(uint8_t strand = 0) const {
                if(strand){
                    return holders[strand] & 0xffff;
                }
                else{
                    return workCount.load();
                }
            }
            /// @brief 풀에 특정 함수를 요청합니다.
            /// @param work 스레드에서 실행할 함수입니다.
            /// @param completionHandler 함수가 완료되면 handleCompleted()에서 실행할 함수입니다. 주어지는 인수는 work 함수의 리턴값입니다.
            /// @param strand 동시 실행이 불가능한 그룹입니다. 즉 같은 strand값이 주어진 것끼리 같은 스레드에 배치됩니다. 0을 주면 그룹에 속하지 않아 어떤 스레드에도 배치될 수 있습니다.
            inline void post(const std::function<variant8()>& work, const std::function<void(variant8)>& completionHandler = {}, uint8_t strand = 0) {
                if(!work) return;
                workCount++;
                std::unique_lock<std::mutex> _(queueGuard);
                const bool toSignal = works.empty();
                works.enqueue({work, completionHandler, strand});
                if(toSignal) cond.notify_one();
            }
            /// @brief 완료된 동작에 대하여 등록한 후처리를 수행합니다.
            inline void handleCompleted(){
                asGuard.lock();
                afterService2.swap(afterService);
                asGuard.unlock();
                for(auto& f: afterService2){ f.handler(f.param); }
                afterService2.clear();
            }
            /// @brief 대기 중인 함수를 모두 제거합니다. 실행 중인 함수는 제거되지 않습니다.
            inline void cancelAll(){
                std::unique_lock<std::mutex> _(queueGuard);
                works.clear();
                workCount = 0;
            }
        private:
            inline static void execute(ThreadPool* pool, const size_t tid) {
                while(!pool->stop){
                    std::unique_lock<std::mutex> _(pool->queueGuard);
                    WorkWithStrand wws = pool->getWork(tid);
                    if (!wws.work) {
                        pool->cond.wait(_);
                        if (pool->stop) return;
                        continue;
                    }
                    _.unlock();
                    if(wws.work){
                        variant8 result = wws.work();
                        pool->release(wws.strand, tid);
                        if(wws.handler){
                            std::unique_lock<std::mutex> _(pool->asGuard);
                            pool->afterService.push_back({wws.handler, result});
                        }
                    }
                }
            }
            struct WorkWithStrand;
            inline WorkWithStrand getWork(size_t tid){
                const WorkWithStrand* prev = nullptr;
                const WorkWithStrand* node = works.peek();
                while(node){
                    if(node->strand) {
                        std::unique_lock<std::mutex> _(strandGuard);
                        if((holders[node->strand] >> 16) == tid) {
                            holders[node->strand]++;
                            works.erase(prev);
                            return *node;
                        }
                        else if((holders[node->strand] & 0xffff) == 0){
                            holders[node->strand] = ((uint32_t)tid << 16) + 1;
                            works.erase(prev);
                            return *node;
                        }
                        else{
                            prev = node;
                            node = works.next(node);
                        }
                    }
                    else{
                        works.erase(prev);
                        return *node;
                    }
                }
                return {};
            }
            inline void release(uint32_t strand, size_t tid) {
                workCount--;
                if(strand){
                    std::unique_lock<std::mutex> _(strandGuard);
                    holders[strand]--;
                }
            }
            std::mutex queueGuard;
            std::mutex strandGuard;
            std::mutex asGuard;
            std::condition_variable cond;
            struct WorkWithStrand{
                std::function<variant8()> work;
                std::function<void(variant8)> handler;
                uint32_t strand;
            };
            struct WorkCompleteHandler{
                std::function<void(variant8)> handler;
                variant8 param;
            };
            ReservedQueue<WorkWithStrand> works;
            std::vector<WorkCompleteHandler> afterService;
            std::vector<WorkCompleteHandler> afterService2;
            std::vector<std::thread> workers;
            uint32_t holders[256]{};
            std::atomic_uint32_t workCount{};
            bool stop = false;
    };
}

#endif