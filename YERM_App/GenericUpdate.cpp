#include "GenericUpdate.h"
#include "../YERM_PC/yr_basic.hpp"

#include <atomic>

namespace onart{

	struct _EntityBlock {
		std::atomic_uint32_t refCount;
		bool alive;
	};

	Entity::Entity() :block(new _EntityBlock{ 1, true }) {}

	Entity::Entity(const Entity& rhs) :block(nullptr) {
		operator=(rhs);
	}

	Entity::~Entity() {
		reset();
	}

	bool Entity::isAlive() const {
		if (!block) return false;
		return reinterpret_cast<_EntityBlock*>(block)->alive;
	}

	void Entity::destroy() {
		if (block) {
			reinterpret_cast<_EntityBlock*>(block)->alive = false;
		}
	}

	void Entity::reset() {
		if (block) {
			_EntityBlock* eb = reinterpret_cast<_EntityBlock*>(block);
			if (!--eb->refCount) { delete eb; }
			block = nullptr;
		}
	}

	Entity& Entity::operator=(const Entity& rhs) {
		reset();
		if (block = rhs.block) {
			reinterpret_cast<_EntityBlock*>(block)->refCount++;
		}
		return *this;
	}

	struct _ScopedEntityBlock {
		std::atomic_uint32_t refCount;
		Entity entity;
	};

	Entity* ScopedEntity::get() {
		if (!block) return nullptr;
		return &reinterpret_cast<_ScopedEntityBlock*>(block)->entity;
	}

	ScopedEntity::~ScopedEntity() {
		_EntityBlock* eb = reinterpret_cast<_EntityBlock*>(block);
		if (!--eb->refCount) { delete eb; }
		block = nullptr;
	}

	ScopedEntity::ScopedEntity() :block(new _ScopedEntityBlock{ 1 }) {}

	void ScopedEntity::reset() {
		if (block) {
			_ScopedEntityBlock* eb = reinterpret_cast<_ScopedEntityBlock*>(block);
			if (!--eb->refCount) { 
				eb->entity.destroy();
				delete eb;
			}
			block = nullptr;
		}
	}

	ScopedEntity& ScopedEntity::operator=(const ScopedEntity& rhs) {
		reset();
		if (block = rhs.block) {
			reinterpret_cast<_EntityBlock*>(block)->refCount++;
		}
		return *this;
	}

	ScopedEntity::ScopedEntity(const ScopedEntity& e) :block(nullptr) {
		operator=(e);
	}

	std::list<ManagedManager*> ManagerManager::mgrs;

	ManagedManager::ManagedManager(size_t period, int priorityIndex) :Updatee(std::chrono::nanoseconds(period), 0, priorityIndex) {
		ManagerManager::mgrs.push_back(this);
		managed = std::prev(ManagerManager::mgrs.end());
	}

	ManagedManager::~ManagedManager() {
		ManagerManager::mgrs.erase(managed);
	}

	void ManagerManager::finalize() {
		while (!mgrs.empty()) {
			mgrs.front()->finalize();
		}
	}
}