#include "logger.hpp"
#include "yr_game.h"
#include "../YERM_App/Update.h"
#include "../YERM_App/GenericUpdate.h"

#include <filesystem>
#include <cstdlib>
#include "yr_visual.h"
#include "yr_2d.h"

using namespace onart;
int main(int argc, char* argv[]){
#if BOOST_OS_WINDOWS
    system("chcp 65001");
#endif
    std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path());
    Game game;
    struct printer {
        printer() {
            fc = new FinalScene(YRGraphics::createRenderPass2Screen(0, 0, {}));

            ve = VisualElement::create();
            fc->insert(ve);
            ve->instanceCount = 1;
            ve->pipeline = get2DDefaultPipeline();
            ve->mesh0 = get2DDefaultQuad();

            {
                uint32_t _1x1 = 0xffffffee;
                TextureCreationOptions texOpts;
                texOpts.nChannels = 4;
                texOpts.linearSampled = false;
                ve->texture = YRGraphics::createTextureFromColor(INT32_MIN, (const uint8_t*)&_1x1, 1, 1, texOpts);
                ve->texture = YRGraphics::createTexture(INT32_MIN, TEX0, sizeof(TEX0), texOpts);
            }

            {
                UniformBufferCreationOptions ubopts;
                ubopts.size = 64;
                mat4 iden;
                fc->perFrameUB = YRGraphics::createUniformBuffer(INT32_MIN, ubopts);
                fc->perFrameUB->update(&iden, 0, 0, 64);

                ve->pushed.resize(128);
                std::memcpy(ve->pushed.data(), &iden, 64);
                vec4 var(1, 1, 0, 0);
                std::memcpy(ve->pushed.data() + 64, &var, 16);
                var = vec4(1, 1, 1, 1);
                std::memcpy(ve->pushed.data() + 80, &var, 16);
            }
        }
        void update(size_t dt, const Entity& e) { 
            fc->draw();
        };
        ~printer() {
            delete fc;
        }
        FinalScene* fc;
        pVisualElement ve;
    };
    Entity e;
    game.setInit([&e]() {
        e.addComponent<printer>();
    });
    game.setUpdate([]() {
        Updator::update(std::chrono::nanoseconds(Game::intDT));
    });
    game.setFinalize([]() {
        ManagerManager::finalize();
        Updator::finalize();
    });
    game.start();
}