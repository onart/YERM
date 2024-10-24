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
        void setOrthogonal();
        void setPerspective(float fovy);
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
            YRGraphics::pPipeline pipeline;
            std::vector<uint8_t> pushed; // per object push data
            std::vector<uint8_t> poub; // per object ub data
            std::unique_ptr<FreeRenderer> fr = nullptr;
            unsigned instaceCount = 1;
            unsigned meshRangeStart = 0;
            unsigned meshRangeCount = 0;
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
    public:
        IntermediateScene(uint32_t width, uint32_t height, int targetType);
        void resize(uint32_t width, uint32_t height);
        void addPred(IntermediateScene* scene);
        void removePred(IntermediateScene* scene);
    private:
        void draw();
        std::set<IntermediateScene*> pred;
        std::set<IntermediateScene*> succ;
        YRGraphics::pRenderPass target0;
    };

    class FinalScene: public Scene{
    public:
        FinalScene(uint32_t width, uint32_t height);
        void addPred(IntermediateScene* scene);
        void removePred(IntermediateScene* scene);
    private:
        void draw();
        YRGraphics::pRenderPass2Screen target1;
    };
}


#endif