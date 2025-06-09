#include "Update.h"
#include <mutex>
#include "../YERM_PC/logger.hpp"
#include <map>

namespace onart
{

    static std::vector<Updatee**> updatee;
    static std::vector<Updatee**> newUpdatee;
    static std::mutex newUpdateeLock;
    static std::multimap<int, Updatee**> earlyUpdate;
    static std::multimap<int, Updatee**> lateUpdate;
    static std::vector<Updatee**> restHolder[2];

    void Updator::update(std::chrono::nanoseconds dt){
        {
            std::unique_lock _(newUpdateeLock);
            for (Updatee** p : newUpdatee) {
                if ((*p)->priority < 0) {
                    earlyUpdate.insert({ (*p)->priority, p });
                }
                else if ((*p)->priority > 0) {
                    lateUpdate.insert({ (*p)->priority, p });
                }
                else {
                    updatee.push_back(p);
                }
            }
            newUpdatee.clear();
        }

        for (auto it = earlyUpdate.begin(); it != earlyUpdate.end(); ) {
            Updatee** elem = it->second;
            if (*elem == nullptr) {
                it = earlyUpdate.erase(it);
                delete elem;
                continue;
            }
            if (!(*elem)->run(dt)) {
                restHolder[0].push_back(elem);
            }
            ++it;
        }

        size_t size = updatee.size();
        for (size_t i = 0; i < size; i++) {
            Updatee** elem = updatee[i];
            if (*elem && !(*elem)->run(dt)) {
                restHolder[0].push_back(elem);
            }
        }

        for (auto it = lateUpdate.begin(); it != lateUpdate.end(); ) {
            Updatee** elem = it->second;
            if (*elem == nullptr) {
                it = lateUpdate.erase(it);
                delete elem;
                continue;
            }
            if (!(*elem)->run(dt)) {
                restHolder[0].push_back(elem);
            }
            ++it;
        }

        auto* driver = &restHolder[0];
        auto* holder = &restHolder[1];
        size = driver->size();
        while (size) {
            for (size_t i = 0; i < size; i++) {
                Updatee** elem = updatee[i];
                if (*elem && !(*elem)->run(std::chrono::nanoseconds(0))) {
                    holder->push_back(elem);
                }
            }
            driver->clear();
            std::swap(holder, driver);
            size = driver->size();
        }

        size_t j = 0;
        for (size_t i = 0; i < size; i++) {
            Updatee** elem = updatee[i];
            if (*elem && !(*elem)->run(std::chrono::nanoseconds(0))) {
                updatee[i] = nullptr;
                delete elem;
                j++;
            }
            else if (j > 0) {
                std::swap(updatee[i], updatee[i - j]);
            }
        }
        updatee.resize(updatee.size() - j);
    }

    template<class T>
    inline static void freeVector(std::vector<T>& v) {
        std::vector<T> v2;
        v.swap(v2);
    }

    void Updator::finalize() {
#ifdef DEBUG
        auto notifyFree = [](Updatee** u) {
            if (u && *u)
            {
                LOGWITH("Updatee", (void*)u, "should have been freed");
            }
        };
        for (Updatee** u : updatee) { notifyFree(u); }
        for (Updatee** u : newUpdatee) { notifyFree(u); }
        for (auto& [k, u] : earlyUpdate) { notifyFree(u); }
        for (auto& [k, u] : lateUpdate) { notifyFree(u); }
#endif
        freeVector(updatee);
        freeVector(newUpdatee);
        earlyUpdate.clear();
        lateUpdate.clear();
    }


    Updatee::Updatee(std::chrono::nanoseconds period, uint32_t limit, int priority) : period(period), clock(period), limit(limit ? limit : UINT32_MAX), limitCounter(limit), priority(priority) {
        internal = new Updatee* { this };
        std::unique_lock _(newUpdateeLock);
        newUpdatee.push_back(internal);
    }

    Updatee::~Updatee() {
        *internal = nullptr;
    }

    bool Updatee::run(std::chrono::nanoseconds dt) {
        if (period.count()) {
            if (dt.count()) {
                clock -= dt;
                limitCounter = limit;
            }
            if (limitCounter > 0 && clock.count() <= 0) {
                clock += period;
                limitCounter--;
                bool ret = limitCounter == 0;
                update(period); // allow destruction here
                return ret;
            }
            return true;
        }
        else {
            update(dt);
            return true;
        }
    }
} // namespace onart
