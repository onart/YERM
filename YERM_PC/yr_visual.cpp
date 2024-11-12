#include "yr_visual.h"
#include "yr_basic.hpp"
#include "yr_sys.h"

namespace onart {
	
	constexpr static int PER_OBJ_UB_DESCRIPTOR_BIND_INDEX = 1;
	constexpr static int PER_OBJ_TEXTURE_DESCRIPTOR_BIND_INDEX = 2;

	VisualElement* Scene::createVisualElement() {
		if (vePool.size()) {
			ve.emplace_back(std::move(vePool.back()));
			vePool.pop_back();
			return ve.back().get();
		}
		return ve.emplace_back(std::make_shared<shp_t<VisualElement>>()).get();
	}

	void Scene::setPoolCapacity(size_t size, bool shrink) { 
		poolSize = size;
		if (shrink && poolSize < vePool.size() ) {
			vePool.resize(poolSize);
		}
	}

	void Scene::clear(bool resetElements) {
		for (auto& elem : ve) {
			if (vePool.size() < poolSize) { 
				if (resetElements) { elem->reset(); }
				vePool.emplace_back(std::move(elem));
			}
		}
		ve.clear();
	}

	IntermediateScene::IntermediateScene(const RenderPassCreationOptions& opts) {		
		target0 = YRGraphics::createRenderPass(INT32_MIN, opts);
	}

	IntermediateScene::~IntermediateScene() {
		for (auto it = pred.begin(); it != pred.end(); ++it) { (*it)->succ.erase(this); }
		for (auto it = succ.begin(); it != succ.end(); ++it) { (*it)->removePred(this); }
		for (auto it = succ2.begin(); it != succ2.end(); ++it) { (*it)->removePred(this); }
	}

	void IntermediateScene::resize(uint32_t width, uint32_t height) {
		target0->resize(width, height);
	}

	void IntermediateScene::addPred(IntermediateScene* sc) {
		sc->succ.insert(this);
		pred.insert(sc);
	}

	void IntermediateScene::removePred(IntermediateScene* sc) {
		sc->succ.erase(this);
		pred.erase(sc);
	}

	void IntermediateScene::draw() {
		if (sorter) { sorter(ve); }
		struct {
			YRGraphics::Pipeline* pipeline = nullptr;
		} state;
		if (ve.size()) {
			target0->usePipeline(ve[0]->pipeline.get(), 0);
		}
		target0->start();
		target0->bind(0, perFrameUB.get());
		for (auto& elem : ve) {
			if (elem->fr) { 
				elem->fr->draw(target0.get());
				std::memset(&state, 0, sizeof(state));
				continue;
			}
			if (state.pipeline != elem->pipeline.get()) { state.pipeline = elem->pipeline.get(); target0->usePipeline(state.pipeline, 0); }

			if (elem->pushed.size()) { target0->push(elem->pushed.data(), 0, elem->pushed.size()); }
			if (elem->ubIndex >= 0) {
				if constexpr (YRGraphics::VULKAN_GRAPHICS) { 
					target0->bind(PER_OBJ_UB_DESCRIPTOR_BIND_INDEX, elem->ub.get(), elem->ubIndex);
				}
				else {
					elem->ub->update(elem->poub.data(), 0, 0, elem->poub.size());
					target0->bind(1, elem->ub.get(), elem->ubIndex);
				}
			}

			// todo: avoid re-setting the same ones
			// postponed since vk <-> d3d11, opengl shader resource scheme are different
			if constexpr (YRGraphics::VULKAN_GRAPHICS) {
				// If ub bound in this iteration.. reset texture
				if (elem->textureSet) { target0->bind(PER_OBJ_TEXTURE_DESCRIPTOR_BIND_INDEX, elem->textureSet); }
				else if (elem->texture) { target0->bind(PER_OBJ_TEXTURE_DESCRIPTOR_BIND_INDEX, elem->texture); }
			}
			else {
				if (elem->textureSet) { target0->bind(0, elem->textureSet); }
				else if (elem->texture) { target0->bind(0, elem->texture); }
			}

			if (elem->instanceCount > 1) { target0->invoke(elem->mesh0, elem->mesh1, elem->instanceCount, 0, elem->meshRangeCount, elem->meshRangeCount); }
			else { target0->invoke(elem->mesh0, elem->meshRangeStart, elem->meshRangeCount); }
		}
		YRGraphics::RenderPass* prerequisites[16]{};
		size_t idx = 0;
		for (auto& pr : pred) { prerequisites[idx++] = pr->target0.get(); }

		target0->execute(succ.size(), pred.size(), prerequisites);
	}

	void VisualElement::updatePOUB(const void* data, uint32_t offset, uint32_t size) {
		if constexpr (YRGraphics::VULKAN_GRAPHICS) { ub->update(data, ubIndex, offset, size); }
		else { std::memcpy(poub.data() + offset, data, size); }
	}

	FinalScene::FinalScene(const YRGraphics::pRenderPass2Screen& rp) : target0(rp) {}

	void FinalScene::addPred(IntermediateScene* sc) { 
		pred.insert(sc);
		sc->succ2.insert(this);
	}

	void FinalScene::removePred(IntermediateScene* sc) { 
		pred.erase(sc);
		sc->succ2.erase(this);
	}
	
	void FinalScene::draw() {
		if (sorter) { sorter(ve); }
		struct {
			YRGraphics::Pipeline* pipeline = nullptr;
		} state;
		if (ve.size()) {
			target0->usePipeline(ve[0]->pipeline.get(), 0);
		}
		target0->start();
		target0->bind(0, perFrameUB.get());
		for (auto& elem : ve) {
			if (elem->fr) {
				elem->fr->draw(target0.get());
				std::memset(&state, 0, sizeof(state));
				continue;
			}
			if (state.pipeline != elem->pipeline.get()) { state.pipeline = elem->pipeline.get(); target0->usePipeline(state.pipeline, 0); }

			if (elem->pushed.size()) { target0->push(elem->pushed.data(), 0, elem->pushed.size()); }
			if (elem->ubIndex >= 0) {
				if constexpr (YRGraphics::VULKAN_GRAPHICS) {
					target0->bind(PER_OBJ_UB_DESCRIPTOR_BIND_INDEX, elem->ub.get(), elem->ubIndex);
				}
				else {
					elem->ub->update(elem->poub.data(), 0, 0, elem->poub.size());
					target0->bind(1, elem->ub.get(), elem->ubIndex);
				}
			}

			// todo: avoid re-setting the same ones
			// postponed since vk <-> d3d11, opengl shader resource scheme are different
			if constexpr (YRGraphics::VULKAN_GRAPHICS) {
				// If ub bound in this iteration.. reset texture(set)
				if (elem->textureSet) { target0->bind(PER_OBJ_TEXTURE_DESCRIPTOR_BIND_INDEX, elem->textureSet); }
				else if (elem->texture) { target0->bind(PER_OBJ_TEXTURE_DESCRIPTOR_BIND_INDEX, elem->texture); }
			}
			else {
				if (elem->textureSet) { target0->bind(0, elem->textureSet); }
				else if (elem->texture) { target0->bind(0, elem->texture); }
			}
			if (elem->instanceCount > 1) { target0->invoke(elem->mesh0, elem->mesh1, elem->instanceCount, 0, elem->meshRangeCount, elem->meshRangeCount); }
			else { target0->invoke(elem->mesh0, elem->meshRangeStart, elem->meshRangeCount); }
		}
		YRGraphics::RenderPass* prerequisites[16]{};
		size_t idx = 0;
		for (auto& pr : pred) { prerequisites[idx++] = pr->target0.get(); }

		target0->execute(pred.size(), prerequisites);
	}
}