#ifndef __YR_VISUAL_H__
#define __YR_VISUAL_H__

#include "yr_math.hpp"
#include "yr_graphics.h"
#include <set>

namespace onart
{

    class Camera{
        friend class Scene;
    public:
        void setViewRange(uint32_t width, uint32_t height);
        void setOrthogonal(float near, float far);
        void setPerspective(float fovy, float near, float far);
        void setLookAt(const vec3& eye, const vec3& at, const vec3& up);
    private:
        Camera();
        struct {
            vec3 eye;
            vec3 at;
            vec3 up;
            mat4 matrix;
        } view;
        struct {
            vec2 viewRange{128, 128};
            float fovy;
            float near;
            float far;
            mat4 matrix;
        } proj;
    };

    class FreeRenderer {
    public:
        virtual void draw(YRGraphics::RenderPass*) {}
        variant8 userData;
    };

    struct VisualElement{
        friend class Scene;
        public:
            YRGraphics::pMesh mesh0; // basic mesh
            YRGraphics::pMesh mesh1; // instance data
            YRGraphics::pTexture texture; // todo: plus lighting parameter
            YRGraphics::pTextureSet textureSet;
            YRGraphics::pPipeline pipeline;
            YRGraphics::pUniformBuffer ub;
            std::vector<uint8_t> pushed; // per object push data
            std::vector<uint8_t> poub; // per object ub data -> todo: remove this
            std::unique_ptr<FreeRenderer> fr = nullptr;
            unsigned instaceCount = 1;
            unsigned meshRangeStart = 0;
            unsigned meshRangeCount = 0;
            unsigned ubBind = 1;
            unsigned textureBind = 2;
        private:
            int ubIndex; // dynamic ub index
        protected:
            ~VisualElement() = default;
    };

    class Scene{
    protected:
        std::vector<std::shared_ptr<VisualElement>> ve;
    public:
        VisualElement* createVisualElement();
        Camera camera;
        std::function<void(decltype(ve)&)> sorter;
    };

    class IntermediateScene: public Scene{
    friend class FinalScene;
    public:
        IntermediateScene(const RenderPassCreationOptions&);
        void resize(uint32_t width, uint32_t height);
        void addPred(IntermediateScene* scene);
        void removePred(IntermediateScene* scene);
    private:
        void draw();
        ~IntermediateScene();
        std::set<IntermediateScene*> pred;
        std::set<IntermediateScene*> succ;
        std::set<FinalScene*> succ2;
        YRGraphics::pRenderPass target0;
    };

    class FinalScene: public Scene{
    public:
        FinalScene(const YRGraphics::pRenderPass2Screen&);
        void addPred(IntermediateScene* scene);
        void removePred(IntermediateScene* scene);
    private:
        void draw();
        std::set<IntermediateScene*> pred;
        YRGraphics::pRenderPass2Screen target0;
    };
}


#endif