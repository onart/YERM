#include "yr_visual.h"
#include "yr_basic.hpp"
#include "yr_sys.h"
#include <algorithm>

namespace onart {
	
	constexpr static int PER_OBJ_UB_DESCRIPTOR_BIND_INDEX = 1;
	constexpr static int PER_OBJ_TEXTURE_DESCRIPTOR_BIND_INDEX = 2;

	void Scene::insert(const std::shared_ptr<VisualElement>& e) {
		ve.push_back(e);
		e->sceneRefs++;
	}

	void Scene::clear() {
		for (auto& elem : ve) {
			if(elem) elem->sceneRefs--;
		}
		ve.clear();
	}

	Scene::~Scene() {
		clear();
	}

	template<class RP>
	void Scene::draw(RP& target0) {
		if (sorter) { sorter(ve); }
		struct {
			YRGraphics::Pipeline* pipeline = nullptr;
		} state;
		if (ve.size()) {
			target0->usePipeline(ve[0]->pipeline.get(), 0);
		}
		target0->start();
		target0->bind(0, perFrameUB.get());
		size_t size = ve.size();
		size_t invalidHead = 0;
		for (size_t i = 0; i < size; i++) {
			VisualElement* elem = ve[i].get();
			if (!elem || elem->getSceneRefCount() == ve[i].use_count()) {
				if (elem) { elem->sceneRefs--; }
				ve[i].reset();
				continue;
			}
			if (i != invalidHead) { ve[invalidHead] = std::move(ve[i]); }
			invalidHead++;
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
				if (elem->rtTexture) { target0->bind(PER_OBJ_TEXTURE_DESCRIPTOR_BIND_INDEX, elem->rtTexture.get()); }
				else if (elem->textureSet) { target0->bind(PER_OBJ_TEXTURE_DESCRIPTOR_BIND_INDEX, elem->textureSet); }
				else if (elem->texture) { target0->bind(PER_OBJ_TEXTURE_DESCRIPTOR_BIND_INDEX, elem->texture); }
			}
			else {
				if (elem->rtTexture) { target0->bind(0, elem->rtTexture.get()); }
				else if (elem->textureSet) { target0->bind(0, elem->textureSet); }
				else if (elem->texture) { target0->bind(0, elem->texture); }
			}

			if (elem->mesh1) { target0->invoke(elem->mesh0, elem->mesh1, elem->instanceCount, 0, elem->meshRangeCount, elem->meshRangeCount); }
			else { target0->invoke(elem->mesh0, elem->meshRangeStart, elem->meshRangeCount); }
		}
		if (invalidHead < ve.size()) {
			ve.resize(invalidHead);
		}
	}

	IntermediateScene::IntermediateScene(const RenderPassCreationOptions& opts) {		
		target0 = YRGraphics::createRenderPass(INT32_MIN, opts);
	}

	IntermediateScene::~IntermediateScene() {
		clear();
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
		Scene::draw(target0);
		YRGraphics::RenderPass* prerequisites[16]{};
		size_t idx = 0;
		for (auto& pr : pred) { 
			if (pr->ve.size()) {
				prerequisites[idx++] = pr->target0.get();
			}
		}

		target0->execute(succ.size() + succ2.size(), idx, prerequisites);
	}

	std::shared_ptr<VisualElement> VisualElement::create() {
		return std::make_shared<shp_t<VisualElement>>();
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
		Scene::draw(target0);
		YRGraphics::RenderPass* prerequisites[16]{};
		size_t idx = 0;
		for (auto& pr : pred) { 
			if (pr->ve.size()) {
				prerequisites[idx++] = pr->target0.get();
			}
		}
		target0->execute(idx, prerequisites);
	}

	FinalScene::~FinalScene() {
		clear();
		for (auto it = pred.begin(); it != pred.end(); ++it) { (*it)->succ2.erase(this); }
	}
}