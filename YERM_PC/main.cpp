#include "logger.hpp"
#include "yr_math.hpp"
#include "yr_game.h"
#include "yr_graphics.h"
#include "yr_gameapp.h"
#include "yr_visual.h"
#include "yr_2d.h"
#include "yr_constants.hpp"

#include <filesystem>
#include <cstdlib>

using namespace onart;
int main(int argc, char* argv[]){
#if BOOST_OS_WINDOWS
    system("chcp 65001");
#endif
    std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path());
    Game game;
    IntermediateScene* scn{};
    FinalScene* fscn{};
    game.setInit([&scn, &fscn]() {
        RenderPassCreationOptions opts;
        opts.width = 800;
        opts.height = 600;
        opts.canCopy = false;
        opts.subpassCount = 1;
        scn = new IntermediateScene(opts);
        fscn = new FinalScene(YRGraphics::createRenderPass2Screen(0, 0, {}));
        //fscn->addPred(scn);
        auto ve = fscn->createVisualElement();
        ve->pipeline = get2DDefaultPipeline();
        ve->instanceCount = 1;
        ve->texture = YRGraphics::createTexture(INT32_MIN, TEX0, sizeof(TEX0), {});
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
    });
    game.setUpdate([&scn, &fscn]() {
        //scn->draw();
        fscn->draw();
    });
    game.start();
}