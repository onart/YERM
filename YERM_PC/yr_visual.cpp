#include "yr_visual.h"
#include "yr_basic.hpp"

namespace onart {

	VisualElement* Scene::createVisualElement() {
		return ve.emplace_back(std::make_shared<shp_t<VisualElement>>()).get();
	}

	void IntermediateScene::addPred(IntermediateScene* sc) {
		sc->succ.insert(this);
		pred.insert(sc);
	}

	void IntermediateScene::draw() {
		if (sorter) { sorter(ve); }
		struct {
			YRGraphics::Pipeline* pipeline = nullptr;
		} state;
		target0->start();
		for (auto& elem : ve) {
			if (elem->fr) { 
				elem->fr->draw(target0.get());
				std::memset(&state, 0, sizeof(state));
				continue;
			}
			if (state.pipeline != elem->pipeline.get()) { state.pipeline = elem->pipeline.get(); target0->usePipeline(state.pipeline, 0); }
			if (elem->mesh1) { target0->invoke(elem->mesh0); }
			if (elem->instaceCount > 1) { target0->invoke(elem->mesh0, elem->mesh1, elem->instaceCount, 0, elem->meshRangeCount, elem->meshRangeCount); }
			else { target0->invoke(elem->mesh0, elem->meshRangeStart, elem->meshRangeCount); }
		}
		YRGraphics::RenderPass* prerequisites[16]{};
		size_t idx = 0;
		for (auto& pr : pred) { prerequisites[idx++] = pr->target0.get(); }

		target0->execute(succ.size(), pred.size(), prerequisites);
	}
}