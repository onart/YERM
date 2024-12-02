#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <game-activity/GameActivity.cpp>
#include <game-text-input/gametextinput.cpp>
extern "C" {
#include <game-activity/native_app_glue/android_native_app_glue.c>
}

extern "C" {
void android_main(struct android_app* state);
};
#include "YERM_PC/logger.hpp"
#include "YERM_PC/yr_sys.h"
#include "YERM_PC/yr_game.h"
#include "YERM_PC/yr_2d.h"
#include "YERM_PC/yr_visual.h"

void android_main(struct android_app* app) {
    using namespace onart;
    Game game;
    IntermediateScene* scn{};
    FinalScene* fscn{};
    pVisualElement ve, ve2;
    game.setInit([&scn, &fscn, &ve, &ve2]() {
        RenderPassCreationOptions opts;
        opts.width = 400;
        opts.height = 300;
        opts.canCopy = false;
        opts.subpassCount = 1;
        scn = new IntermediateScene(opts);
        fscn = new FinalScene(YRGraphics::createRenderPass2Screen(0, 0, {}));
        fscn->addPred(scn);
        ve = VisualElement::create();
        fscn->insert(ve);
        ve->pipeline = get2DDefaultPipeline();
        ve->instanceCount = 1;
        ve->rtTexture = scn->getRenderpass();
        using testv_t = YRGraphics::Vertex<float[2], float[2]>;
        float verts[] = { -1,-1,0,0,-1,1,0,1,1,-1,1,0,1,1,1,1 };
        uint16_t inds[]{ 0,1,2,2,1,3 };

        MeshCreationOptions meshOpts;
        meshOpts.fixed = true;
        meshOpts.indexCount = 6;
        meshOpts.vertexCount = 4;
        meshOpts.singleIndexSize = 2;
        meshOpts.singleVertexSize = sizeof(testv_t);
        meshOpts.indices = inds;
        meshOpts.vertices = verts;
        ve->mesh0 = YRGraphics::createMesh(INT32_MIN, meshOpts);

        UniformBufferCreationOptions ubopts;
        ubopts.size = 64;
        scn->perFrameUB = YRGraphics::createUniformBuffer(INT32_MIN, ubopts);
        mat4 iden;
        scn->perFrameUB->update(&iden, 0, 0, 64);

        fscn->perFrameUB = YRGraphics::createUniformBuffer(INT32_MIN, ubopts);
        fscn->perFrameUB->update(&iden, 0, 0, 64);

        ve->pushed.resize(128);
        std::memcpy(ve->pushed.data(), &iden, 64);
        vec4 var(1, 1, 0, 0);
        std::memcpy(ve->pushed.data() + 64, &var, 16);
        var = vec4(1, 1, 1, 1);
        std::memcpy(ve->pushed.data() + 80, &var, 16);

        ve2 = VisualElement::create();
        scn->insert(ve2);
        ve2->pipeline = get2DInstancedPipeline();
        ve2->instanceCount = 1;
        ve2->mesh0 = ve->mesh0;
        ve2->pushed.resize(128);
        std::memcpy(ve2->pushed.data(), &var, 16);
        meshOpts.vertexCount = 1;
        meshOpts.fixed = false;
        meshOpts.indexCount = 0;
        meshOpts.singleVertexSize = 64;
        meshOpts.indices = nullptr;
        mat4 iden2;
        iden2._41 = 1.0f;
        iden2._42 = 1.0f;
        iden2._43 = 0.0f;
        iden2._44 = 0.0f;
        meshOpts.vertices = &iden2;
        ve2->mesh1 = YRGraphics::createMesh(INT32_MIN, meshOpts);
        ve2->texture = YRGraphics::createTexture(INT32_MIN, TEX0, sizeof(TEX0), {});
    });
    game.setUpdate([&scn, &fscn]() {
        scn->draw();
        fscn->draw();
    });
    game.start(app);
}