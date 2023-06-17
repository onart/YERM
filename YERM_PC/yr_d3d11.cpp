#include "yr_d3d11.h"

namespace onart {
	D3D11Machine* D3D11Machine::singleton = nullptr;
	thread_local unsigned D3D11Machine::reason = 0;

    D3D11Machine::D3D11Machine(Window* window) {
        if (singleton) {
            LOGWITH("Tried to create multiple GLMachine objects");
            return;
        }

        // ���� ��� glfwmakecontextcurrent�� yr_game.cpp���� ȣ��Ǿ� �ְ� ������ �����ߴٸ� ������ ������ ������� ���ؽ�Ʈ�� �Ѿ

        std::set<std::string> ext;
        int next; glGetIntegerv(GL_NUM_EXTENSIONS, &next);
        for (int k = 0; k < next; k++) ext.insert((const char*)glGetStringi(GL_EXTENSIONS, k));
        for (const char* arb : GL_DESIRED_ARB) {
            if (ext.find(arb) == ext.end()) {
                LOGWITH("No support for essential extension:", arb);
                return;
            }
        }
        checkTextureAvailable();

        int x, y;
        window->getFramebufferSize(&x, &y);
        createSwapchain(x, y);

        if constexpr (USE_OPENGL_DEBUG) {
            glEnable(GL_DEBUG_OUTPUT);
            glDebugMessageCallback(glOnError, 0);
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        singleton = this;
        UniformBuffer* push = createUniformBuffer(1, 128, 0, INT32_MIN + 1, 11);
        if (!push) {
            singleton = nullptr;
            return;
        }
        glBindBufferRange(GL_UNIFORM_BUFFER, 11, push->ubo, 0, 128);
    }
}