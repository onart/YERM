#ifndef __YR_VISUAL_H__
#define __YR_VISUAL_H__

#include "yr_graphics.h"
#include <set>

namespace onart
{

    class FreeRenderer {
    public:
        virtual void draw(YRGraphics::RenderPass*) {}
#ifdef YR_USE_VULKAN
        virtual void draw(YRGraphics::RenderPass2Screen*) {}
#endif
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
            YRGraphics::pRenderPass rtTexture;
            YRGraphics::pUniformBuffer ub;
            std::vector<uint8_t> pushed; // per object push data
            std::vector<uint8_t> poub; // per object ub data (only for non-vulkan ver)
            std::unique_ptr<FreeRenderer> fr = nullptr;
            unsigned instanceCount = 1;
            unsigned meshRangeStart = 0;
            unsigned meshRangeCount = 0;
            int ubIndex = -1; // dynamic ub index
            static std::shared_ptr<VisualElement> create();
            void updatePOUB(const void* data, uint32_t offsetByte, uint32_t size);
            inline void reset() {
                mesh0 = {};
                mesh1 = {};
                texture = {};
                textureSet = {};
                pipeline = {};
                ub = {};
                {
                    std::vector<uint8_t> hollow;
                    pushed.swap(hollow);
                }
                {
                    std::vector<uint8_t> hollow;
                    poub.swap(hollow);
                }
                fr = {};
                instanceCount = 1;
                meshRangeStart = 0;
                meshRangeCount = 0;
                ubIndex = -1;
            }
            inline uint16_t getSceneRefCount() const { return sceneRefs; }
        protected:
            ~VisualElement() = default;
        private:
            uint16_t sceneRefs = 0;
    };

    using pVisualElement = std::shared_ptr<VisualElement>;

    class Scene{
    protected:
        template<class RP>
        void draw(RP&);
        std::vector<std::shared_ptr<VisualElement>> ve;
        size_t poolSize = 0;
    public:
        YRGraphics::pUniformBuffer perFrameUB;
        void insert(const std::shared_ptr<VisualElement>&);
        void clear();
        std::function<void(decltype(ve)&)> sorter{};
        ~Scene();
    };

    class IntermediateScene: public Scene{
    friend class FinalScene;
    public:
        IntermediateScene(const RenderPassCreationOptions&);
        void resize(uint32_t width, uint32_t height);
        void addPred(IntermediateScene* scene);
        void removePred(IntermediateScene* scene);
        void draw();
        inline YRGraphics::pRenderPass& getRenderpass() { return target0; }
        ~IntermediateScene();
    private:
        std::set<IntermediateScene*> pred;
        std::set<IntermediateScene*> succ;
        std::set<class FinalScene*> succ2;
        YRGraphics::pRenderPass target0;
    };

    class FinalScene: public Scene{
    public:
        FinalScene(const YRGraphics::pRenderPass2Screen&);
        void addPred(IntermediateScene* scene);
        void removePred(IntermediateScene* scene);
        void draw();
        ~FinalScene();
    private:
        std::set<IntermediateScene*> pred;
        YRGraphics::pRenderPass2Screen target0;
    };
}


#endif