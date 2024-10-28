#include "yr_visual.h"
#include "yr_basic.hpp"
#include "yr_sys.h"

namespace onart {

	Camera::Camera() {
		setLookAt({ 0,0,1 }, { 0,0,0 }, { 0,1,0 });
		setOrthogonal(0, 1);
	}

	void Camera::setLookAt(const vec3& eye, const vec3& at, const vec3& up) {
		view.eye = eye;
		view.at = at;
		view.up = up;
		view.matrix = mat4::lookAt(eye, at, up);
	}

	void Camera::setPerspective(float fovy, float near, float far) {
		proj.near = near;
		proj.far = far;
		proj.fovy = fovy;
		proj.matrix = mat4::perspective(fovy, proj.viewRange.y / proj.viewRange.x, near, far);
	}

	void Camera::setOrthogonal(float near, float far) {
		proj.fovy = 0;
		proj.near = near;
		proj.far = far;
		vec2 iv = vec2(1) / proj.viewRange;
		proj.matrix = mat4::TRS(vec3(0, 0, near), Quaternion(), vec3(iv.x, iv.y, 1 / (far - near)));
	}

	VisualElement* Scene::createVisualElement() {
		return ve.emplace_back(std::make_shared<shp_t<VisualElement>>()).get();
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
		target0->start();
		for (auto& elem : ve) {
			if (elem->fr) { 
				elem->fr->draw(target0.get());
				std::memset(&state, 0, sizeof(state));
				continue;
			}
			if (state.pipeline != elem->pipeline.get()) { state.pipeline = elem->pipeline.get(); target0->usePipeline(state.pipeline, 0); }

			// todo: avoid re-setting the same ones
			// postponed since vk <-> d3d11, opengl shader resource scheme are different
			if (elem->textureSet) { target0->bind(elem->textureBind, elem->textureSet); }
			else if (elem->texture) { target0->bind(elem->textureBind, elem->texture); }

			if (elem->pushed.size()) { target0->push(elem->pushed.data(), 0, elem->pushed.size()); }
			/* // todo: per-object dynamic uniform buffer scheme & per-frame(scene) uniform buffer scheme
			if (elem->poub.size()) {

			}
			*/
			if (elem->instaceCount > 1) { target0->invoke(elem->mesh0, elem->mesh1, elem->instaceCount, 0, elem->meshRangeCount, elem->meshRangeCount); }
			else { target0->invoke(elem->mesh0, elem->meshRangeStart, elem->meshRangeCount); }
		}
		YRGraphics::RenderPass* prerequisites[16]{};
		size_t idx = 0;
		for (auto& pr : pred) { prerequisites[idx++] = pr->target0.get(); }

		target0->execute(succ.size(), pred.size(), prerequisites);
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
		target0->start();
		for (auto& elem : ve) {
			if (elem->fr) {
				elem->fr->draw(target0.get());
				std::memset(&state, 0, sizeof(state));
				continue;
			}
			if (state.pipeline != elem->pipeline.get()) { state.pipeline = elem->pipeline.get(); target0->usePipeline(state.pipeline, 0); }

			// todo: avoid re-setting the same ones
			// postponed since vk <-> d3d11, opengl shader resource scheme are different
			if (elem->textureSet) { target0->bind(elem->textureBind, elem->textureSet); }
			else if (elem->texture) { target0->bind(elem->textureBind, elem->texture); }

			if (elem->pushed.size()) { target0->push(elem->pushed.data(), 0, elem->pushed.size()); }
			/* // todo: per-object dynamic uniform buffer scheme & per-frame(scene) uniform buffer scheme
			if (elem->poub.size()) {

			}
			*/
			if (elem->instaceCount > 1) { target0->invoke(elem->mesh0, elem->mesh1, elem->instaceCount, 0, elem->meshRangeCount, elem->meshRangeCount); }
			else { target0->invoke(elem->mesh0, elem->meshRangeStart, elem->meshRangeCount); }
		}
		YRGraphics::RenderPass* prerequisites[16]{};
		size_t idx = 0;
		for (auto& pr : pred) { prerequisites[idx++] = pr->target0.get(); }

		target0->execute(0, pred.size(), prerequisites);
	}
}