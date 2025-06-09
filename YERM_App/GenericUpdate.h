#ifndef __GENERIC_UPDATE_H__
#define __GENERIC_UPDATE_H__

#include <chrono>
#include <cstdint>
#include <vector>
#include <map>
#include <unordered_map>
#include <list>
#include <memory>
#include <type_traits>
#include "../YERM_PC/yr_tmp.hpp"

#include "Update.h"

namespace onart
{
    namespace tmp {
        DECLARE_MEM_SIG_CHECKER(update);
        DECLARE_MEM_SIG_CHECKER(onDestroy);
    }

    class Entity;

    struct uint_ptr {
    private:
        struct {
            std::atomic_uint32_t refCount;
            uint32_t value;
        }*block = nullptr;
    public:
        inline uint_ptr(uint32_t value) :block(new std::remove_reference_t<decltype(*block)>{ 1,value }) {}
        inline ~uint_ptr() { reset(); }
        inline uint_ptr(const uint_ptr& p) { operator=(p); }
        inline void reset() { if (block && (--block->refCount == 0)) { delete block; block = nullptr; } }
        inline uint_ptr& operator=(const uint_ptr& p) { 
            reset();
            block = p.block;
            block->refCount++;
            return *this;
        }
        inline uint32_t& operator*() { return block->value; }
        inline const uint32_t& operator*() const { return block->value; }
    };

    class ManagedManager: Updatee {
        friend class ManagerManager;
    protected:
        ManagedManager(size_t period, int priorityIndex);
        ~ManagedManager();
    private:
        virtual void finalize() = 0;
        std::list<ManagedManager*>::iterator managed;
    };

    template<class T, uint32_t periodNS = 0, int priorityIndex = 0>
    class Manager final: ManagedManager{
        friend class Entity;
        static_assert(std::is_default_constructible_v<T>);
        static_assert(std::is_swappable_v<T>);
        public:
            struct Pointer;

            struct ForwardIterator {
                friend class Manager;
            public:
                inline const Entity& getEntity() const { return std::get<1>((*components)[index]); }
                inline T& getValue() const { return std::get<0>((*components)[index]); }
                inline bool isEnd() const { return !components || index >= components->size(); }
                inline void operator++() { ++index; }
                inline ForwardIterator& operator=(const ForwardIterator& rhs) {
                    components = rhs.components;
                    index = rhs.index;
                }
                inline ForwardIterator() :components(nullptr), index(0) {}
            private:
                inline ForwardIterator(std::vector<std::tuple<T, Entity>>& v) :index(0), components(&v) {}
                std::vector<std::tuple<T, Entity>>* components;
                uint32_t index;
                // maintain Manager instance??
            };

            static ForwardIterator getIterator() {
                auto instance = getInstance();
                if (instance.use_count() == 1) { return ForwardIterator(); }
                return ForwardIterator(instance->components);
            }
        private:
            std::vector<std::tuple<T, Entity>> components;
            std::shared_ptr<Manager> _this;
            std::unordered_map<void*, uint_ptr> owning;
            bool isInLoop = false;

            Manager() :ManagedManager(periodNS, priorityIndex) {}

            inline static std::shared_ptr<Manager> getInstance(){
                static std::weak_ptr<Manager> p{};
                auto sp = p.lock();
                if(!sp){
                    sp = std::shared_ptr<Manager>(new Manager);
                    p = sp;
                    sp->_this = sp;
                }
                return sp;
            }            
            virtual void update(std::chrono::nanoseconds dt) {
                size_t removeStart = components.size();
                isInLoop = true;
                for (uint32_t i = 0; i < components.size(); i++) {
                    auto& [v, e] = components[i];
                    if (!e.isAlive()) {
                        owning.erase(e.block);
                        if (i != components.size() - 1) {
                            if constexpr (tmp::has_onDestroy<T, void(const Entity&)>::value) {
                                if (e.block) {
                                    std::get<0>(components[i]).onDestroy(e);
                                }
                            }
                            std::swap(components.back(), components[i]);
                            auto it = owning.find(std::get<1>(components[i]).block);
                            if (it != owning.end()) { *it->second = i; }
                        }
                        components.pop_back();
                        i--;
                        continue;
                    }
                    if constexpr (tmp::has_update<T, void(size_t, const Entity&)>::value) {
                        if constexpr (periodNS == 0) { v.update(dt.count(), e); }
                        else { v.update(periodNS, e); }
                    }
                }
                isInLoop = false;

                if (components.empty()) { _this.reset(); }
            }

            virtual void finalize() {
                components.clear();
                _this.reset();
            }

            inline Pointer addOrGet(const Entity& owner) {
                if (!owner.block) return Pointer{};
                auto it = owning.find(owner.block);
                if (it != owning.end()) {
                    return Pointer(owner, *it->second);
                }
                if (isInLoop) return Pointer(Entity(), uint_ptr(0));
                uint32_t pos = (uint32_t)components.size();
                auto& [v, e] = components.emplace_back();
                e = owner;
                uint_ptr ppos(pos);
                owning.insert({ owner.block, ppos });
                return Pointer(e, ppos);
            }

            inline Pointer get(const Entity& owner) {
                auto it = owning.find(owner.block);
                if (it != owning.end()) {
                    return Pointer(owner, it->second);
                }
                return Pointer{};
            }

            inline void remove(const Entity& owner) {
                auto it = owning.find(owner.block);
                if (it != owning.end()) {
                    uint_ptr& pos = it->second;
                    if (*pos < components.size()) {
                        Entity& e = std::get<1>(components[*pos]);
                        if (owner == e) {
                            if constexpr (tmp::has_onDestroy<T, void(const Entity&)>::value) {
                                std::get<0>(components[*pos]).onDestroy(e);
                            }
                            e.reset();
                        }
                    }
                    owning.erase(it);
                }
            }
    };

    class ManagerManager final {
        friend class ManagedManager;
    public:
        static void finalize();
    private:
        static std::list<ManagedManager*> mgrs;
    };

    template<class T, uint32_t periodNS = 0, int priority = 0>
    using Pointer = Manager<T, periodNS, priority>::template Pointer;

    class Entity final {
        template <class T, uint32_t periodNS, int priority>
        friend class Manager;
    public:
        template<class T, uint32_t periodNS = 0, int priority = 0>
        Pointer<T, periodNS, priority> addComponent() const;

        template<class T, uint32_t periodNS = 0, int priority = 0>
        void removeComponent() const;

        template<class T, uint32_t periodNS = 0, int priority = 0>
        Pointer<T, periodNS, priority> getComponent() const;

        void destroy();

        bool isAlive() const;

        inline bool operator==(const Entity& rhs) const { return block == rhs.block; }
        inline void operator=(Entity&& rhs) noexcept { block = rhs.block; rhs.block = nullptr; }
        Entity& operator=(const Entity&);

        Entity();
        ~Entity();
        Entity(const Entity&);
        inline Entity(Entity&& e) noexcept :block(e.block) { e.block = nullptr; }
        inline void swap(Entity&& e) { std::swap(block, e.block); }
        void reset();
        inline operator bool() const { return isAlive(); }
    private:
        void* block;
    };

    class ScopedEntity final {
    public:
        Entity* get();

        inline bool operator==(const ScopedEntity& rhs) const { return block == rhs.block; }
        inline void operator=(ScopedEntity&& rhs) noexcept { block = rhs.block; rhs.block = nullptr; }
        ScopedEntity& operator=(const ScopedEntity&);

        ScopedEntity();
        inline ScopedEntity(const ScopedEntity&);
        inline ScopedEntity(ScopedEntity&& e) noexcept :block(e.block) { e.block = nullptr; }
        inline void swap(ScopedEntity&& e) { std::swap(block, e.block); }
        ~ScopedEntity();
        void reset();
    private:
        void* block;
    };

    template<class T, uint32_t periodNS, int priority>
    struct Manager<T, periodNS, priority>::Pointer {
        friend class Manager;
    public:
        inline T* operator->() const {
            std::shared_ptr<Manager> instance = Manager::getInstance();
            uint32_t pos = *p;
            if (pos < instance->components.size()) {
                auto& [v, e] = instance->components[pos];
                if (this->e != e) return nullptr;
                return &v;
            }
            return nullptr;
        }
        inline operator bool() const { return operator->(); }
        inline const Entity& getEntity() const { return e; }
    private:
        Pointer(const Entity& e, const uint_ptr& pos) :e(e), p(pos) {}
        Pointer() :e(), p(~0u) {}
        Entity e;
        uint_ptr p;
    };

    template<class T, uint32_t periodNS, int priority>
    Pointer<T, periodNS, priority> Entity::addComponent() const { return Manager<T, periodNS, priority>::getInstance()->addOrGet(*this); }

    template<class T, uint32_t periodNS, int priority>
    void Entity::removeComponent() const { Manager<T, periodNS, priority>::getInstance()->remove(*this); }

    template<class T, uint32_t periodNS, int priority>
    Pointer<T, periodNS, priority> Entity::getComponent() const { return Manager<T, periodNS, priority>::getInstance()->get(*this); }
    
} // namespace onart


#endif