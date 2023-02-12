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

    template<class T>
    class ReservedQueue{
        public:
        struct Node{
            T v = {};
            uint32_t next = ~0u;
        };
        inline ReservedQueue(size_t reserve = 256):data(reserve),ind(reserve){
            for(size_t i = 0 ; i < reserve ; i++){
                ind[i] = reserve - 1 - i;
            }
        }
        inline void enqueue(const T& v) {
            Node* newTail;
            if(ind.size() == 0) {
                size_t newSize = data.size() * 2;
                for(size_t i = data.size() ; i < newSize ; i++) { ind[i] = newSize - 1 - i; }
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
                head = data[head].next;
                if(empty()) tail = ~0u;
                return *ret;
            }
        }
        inline const T& peek() const{
            if(empty()){
                return {};
            }
            else{
                return data[head].v;
            }
        }
        private:
        uint32_t head = ~0u;
        uint32_t tail = ~0u;
        std::vector<Node> data;
        std::vector<uint32_t> ind;
    };

    /// @brief 작업을 비동기적으로 수행하기 위한 스레드 풀입니다. 스레스 수는 런타임에 정할 수 있으며 최대 스레드 수는 8입니다.
    class ThreadPool{
        public:
            inline ThreadPool(size_t n = 1) {
                if(n > 8) n = 8;
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
            /// @brief 풀에 특정 함수를 요청합니다.
            /// @param work 스레드에서 실행할 함수입니다.
            /// @param completionHandler 함수가 완료되면 handleCompleted()에서 실행할 함수입니다. 주어지는 인수는 work 함수의 리턴값입니다.
            /// @param strand 동시 실행이 불가능한 그룹입니다. 즉 같은 strand값이 주어진 것끼리 같은 스레드에 배치됩니다. 0을 주면 그룹에 속하지 않아 어떤 스레드에도 배치될 수 있습니다.
            inline void post(const std::function<void*()>& work, const std::function<void(void*)>& completionHandler = {}, uint8_t strand = 0) {
                if(!work) return;
                std::unique_lock<std::mutex> _(queueGuard);
                const bool toSignal = works.empty();
                works.enqueue({work, strand});
                completion.enqueue(completionHandler);
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
        private:
            inline static void execute(ThreadPool* pool, const size_t tid) {
                while(!pool->stop){
                    std::unique_lock<std::mutex> _(pool->queueGuard);
                    if(pool->works.empty()) {
                        pool->cond.wait(_);
                        if(pool->stop) return;
                    }
                    uint32_t strand = pool->hold(tid);
                    if(strand < 256){
                        std::function<void(void*)> handler = pool->completion.dequeue();
                        std::function<void*()> work = pool->works.dequeue().work;
                        _.unlock();
                        void* result = work();
                        pool->release(strand, tid);
                        if(handler){
                            std::unique_lock<std::mutex> _(pool->asGuard);
                            pool->afterService.push_back({handler, result});
                        }
                    }
                }
            }
            inline uint32_t hold(size_t tid){
                uint32_t strand = works.peek().strand;
                if(strand) {
                    std::unique_lock<std::mutex> _(strandGuard);
                    if((holders[strand] >> 16) == tid) {
                        holders[strand]++;
                    }
                    else if((holders[strand] & 0xffff) == 0){
                        holders[strand] = ((uint32_t)tid << 16) + 1;
                    }
                    else{
                        return 256;
                    }
                }
                return strand;
            }
            inline void release(uint32_t strand, size_t tid) {
                std::unique_lock<std::mutex> _(strandGuard);
                holders[strand]--;
            }
            std::mutex queueGuard;
            std::mutex strandGuard;
            std::mutex asGuard;
            std::condition_variable cond;
            struct WorkWithStrand{
                std::function<void*()> work;
                uint32_t strand;
            };
            struct WorkCompleteHandler{
                std::function<void(void*)> handler;
                void* param;
            };
            ReservedQueue<WorkWithStrand> works;
            ReservedQueue<std::function<void(void*)>> completion;
            std::vector<WorkCompleteHandler> afterService;
            std::vector<WorkCompleteHandler> afterService2;
            std::vector<std::thread> workers;
            std::atomic_uint32_t holders[256]{};
            bool stop = false;
    };
}

#endif