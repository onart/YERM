#ifndef __YR_GRAPHICS_PARAM_H__
#define __YR_GRAPHICS_PARAM_H__

#include <cstdint>

namespace onart
{
    struct __devnull{
        __devnull() {}
        template<class T>
        void operator=(const T&){}
    };

    // enum

    /// @brief 렌더 타겟의 유형입니다.
    enum RenderTargetType { 
        /// @brief 색 버퍼 1개를 보유합니다.
        RTT_COLOR1 = 0b1,
        /// @brief 색 버퍼 2개를 보유합니다.
        RTT_COLOR2 = 0b11,
        /// @brief 색 버퍼 3개를 보유합니다.
        RTT_COLOR3 = 0b111,
        /// @brief 깊이 버퍼를 보유합니다.
        RTT_DEPTH = 0b1000,
        /// @brief 스텐실 버퍼를 보유합니다.
        RTT_STENCIL = 0b10000,
    };

    enum ShaderStage {
        VERTEX = 0b1,
        FRAGMENT = 0b10,
        GEOMETRY = 0b100,
        TESS_CTRL = 0b1000,
        TESS_EVAL = 0b10000,
        GRAPHICS_ALL = VERTEX | FRAGMENT | GEOMETRY | TESS_CTRL | TESS_EVAL
    };

    /// @brief 이미지 파일로부터 텍스처를 생성할 때 줄 수 있는 옵션입니다.
    enum TextureFormatOptions {
        /// @brief 원본이 BasisU인 경우: 품질을 우선으로 트랜스코드합니다. 그 외: 그대로 사용합니다.
        IT_PREFER_QUALITY = 0,
        /// @brief 원본이 BasisU인 경우: 작은 용량을 우선으로 트랜스코드합니다. 원본이 비압축 형식인 경우: 하드웨어에서 가능한 경우 압축하여 사용니다. 그 외: 그대로 사용합니다.
        IT_PREFER_COMPRESS = 1,
    };

    enum class ShaderResourceType: uint8_t {
        NONE = 0,
        UNIFORM_BUFFER_1 = 1,
        DYNAMIC_UNIFORM_BUFFER_1 = 2,
        TEXTURE_1 = 3,
        TEXTURE_2 = 4,
        TEXTURE_3 = 5,
        TEXTURE_4 = 6,
        INPUT_ATTACHMENT_1 = 7,
        INPUT_ATTACHMENT_2 = 8,
        INPUT_ATTACHMENT_3 = 9,
        INPUT_ATTACHMENT_4 = 10,
    };

    enum class CompareOp {
        NEVER = 0,
        LESS = 1,
        EQUAL = 2,
        LTE = 3,
        GREATER = 4,
        NE = 5,
        GTE = 6,
        ALWAYS = 7,
    };

    enum class StencilOp {
        KEEP = 0,
        ZERO = 1,
        REPLACE = 2,
        PLUS1_CLAMP = 3,
        MINUS1_CLAMP = 4,
        INVERT = 5,
        PLUS1_WRAP = 6,
        MINUS1_WRAP = 7,
    };

    enum class BlendOperator {
        ADD = 0,
        SUBTRACT = 1,
        REVERSE_SUBTRACT = 2,
        MINIMUM = 3,
        MAXIMUM = 4,
    };

    enum class BlendFactor {
        ZERO = 0,
        ONE = 1,
        SRC_COLOR = 2,
        ONE_MINUS_SRC_COLOR = 3,
        DST_COLOR = 4,
        ONE_MINUS_DST_COLOR = 5,
        SRC_ALPHA = 6,
        ONE_MINUS_SRC_ALPHA = 7,
        DST_ALPHA = 8,
        ONE_MINUS_DST_ALPHA = 9,
        CONSTANT_COLOR = 10,
        ONE_MINUS_CONSTANT_COLOR = 11,
        CONSTANT_ALPHA = 12,
        ONE_MINUS_CONSTANT_ALPHA = 13,
        SRC_ALPHA_SATURATE = 14,
        SRC1_COLOR = 15,
        ONE_MINUS_SRC1_COLOR = 16,
        SRC1_ALPHA = 17,
        ONE_MINUS_SRC1_ALPHA = 18,
    };

    // structure
    /// @brief 텍스처 생성에 사용하는 옵션입니다.
    struct TextureCreationOptions {
        /// @brief @ref ImageTextureFormatOptions 기본값 IT_PREFER_QUALITY
        TextureFormatOptions opts = IT_PREFER_QUALITY;
        /// @brief 확대 또는 축소 샘플링 시 true면 bilinear 필터를 사용합니다. false면 nearest neighbor 필터를 사용합니다. 기본값 true
        bool linearSampled = true;
        /// @brief 원본 텍스처가 srgb 공간에 있는지 여부입니다. 기본값 false
        bool srgb = false;
        /// @brief 이미지의 채널 수를 지정합니다. 이 값은 BasisU 텍스처에 대하여 사용되며 그 외에는 이 값을 무시하고 원본 이미지의 채널 수를 사용합니다. 기본값 4
        int nChannels = 4;
        inline TextureCreationOptions(){}
    };

    struct UniformBufferCreationOptions {
        /// @brief 유니폼 버퍼의 크기입니다. 기본값 없음
        size_t size;
        /// @brief 유니폼 버퍼에 접근할 수 있는 셰이더 단계입니다. @ref ShaderStage 기본값 GRAPHICS_ALL
        uint32_t accessibleStages = ShaderStage::GRAPHICS_ALL;
        /// @brief 동적 유니폼 버퍼의 항목 수입니다. 1을 주면 동적 유니폼 버퍼로 만들어지지 않습니다. 기본값 1
        uint32_t count = 1;
    };

    struct MeshCreationOptions {
        /// @brief 정점 데이터입니다. 기본값 없음
        void* vertices;
        /// @brief 정점 수입니다. 기본값 없음
        size_t vertexCount;
        /// @brief 개별 정점의 크기입니다. 기본값 없음
        size_t singleVertexSize;
        /// @brief 인덱스 데이터입니다. 기본값 nullptr
        void* indices = nullptr;
        /// @brief 인덱스 수입니다. 기본값 0
        size_t indexCount = 0;
        /// @brief 개별 인덱스의 크기입니다. 2 또는 4여야 합니다.
        size_t singleIndexSize = 0;
        /// @brief false인 경우 데이터를 수정할 수 있고 그러기 유리한 위치에 저장합니다. 기본값 true
        bool fixed = true;
    };

    struct RenderPassCreationOptions {
        /// @brief 타겟의 공통 크기입니다. 기본값 없음
        int width, height;
        /// @brief 서브패스 수입니다. Cube 대상의 렌더패스 생성 시에는 무시됩니다. 기본값 1
        uint32_t subpassCount = 1;
        /// @brief 타겟의 유형입니다. @ref RenderTargetType nullptr를 주면 모두 COLOR1로 취급되지만, nullptr를 주지 않는 경우에는 모든 것이 주어져야 합니다.
        /// Screen 대상의 RenderPass에서는 스왑체인인 마지막을 제외한 만큼 주어져야 합니다. 기본값 nullptr
        RenderTargetType* targets = nullptr;
        /// @brief 각 패스의 중간에 깊이 버퍼를 사용할 경우 그것을 input attachment로 사용할지 여부입니다. nullptr를 주면 일괄 false로 취급되며 그 외에는 모든 것이 주어져야 합니다. 기본값 nullptr
        bool* depthInput = nullptr;
        /// @brief true를 주면 최종 타겟을 텍스처로 사용할 때 linear 필터를 사용합니다. 기본값 true
        bool linearSampled = true;
        /// @brief screen 대상의 렌더패스의 최종 타겟에 depth 또는 stencil을 포함할지 결정합니다. 즉, RenderTargetType::DEPTH, RenderTargetType::STENCIL 이외에는 무시됩니다. 기본값 COLOR1
        RenderTargetType screenDepthStencil = RenderTargetType::RTT_COLOR1;
        /// @brief true일 경우 내용을 CPU 메모리로 읽어오거나 텍스처로 추출할 수 있습니다. RenderPass2Screen 및 RenderPass2Cube 생성 시에는 무시됩니다. 기본값 false
        bool canCopy = false;
        /// @brief 렌더패스 시작 시 모든 서브패스 타겟(색/깊이/스텐실)을 주어진 색으로 클리어합니다. 깊이/스텐실은 항상 1, 0으로 클리어합니다. vulkan API의 경우 autoclear를 사용하는 것이 더 성능이 높을 수 있습니다.
        struct {
            bool use = true;
            float color[4]{};
        } autoclear;
    };

    struct ShaderModuleCreationOptions {
        /// @brief SPIR-V 바이너리입니다. 기본값 없음
        const void* source;
        /// @brief source의 크기(바이트)입니다. 기본값 없음
        size_t size;
        /// @brief 대상 셰이더 단계입니다. 기본값 없음
        ShaderStage stage;
    };

    struct PipelineLayoutOptions {
        ShaderResourceType pos0 = ShaderResourceType::NONE;
        ShaderResourceType pos1 = ShaderResourceType::NONE;
        ShaderResourceType pos2 = ShaderResourceType::NONE;
        ShaderResourceType pos3 = ShaderResourceType::NONE;
        bool usePush = false;
    };

    struct DepthStencilTesting {
        CompareOp comparison = CompareOp::LESS;
        bool depthTest = false;
        bool depthWrite = false;
        bool stencilTest = false;
        struct StencilWorks {
            StencilOp onFail = StencilOp::KEEP;
            StencilOp onDepthFail = StencilOp::KEEP;
            StencilOp onPass = StencilOp::KEEP;
            CompareOp compare = CompareOp::ALWAYS;
            uint32_t reference = 0;
            uint32_t writeMask = 0xff;
            uint32_t compareMask = 0xff;
        }stencilFront, stencilBack;
    };

    struct AlphaBlend {
        BlendOperator colorOp = BlendOperator::ADD;
        BlendOperator alphaOp = BlendOperator::ADD;
        BlendFactor srcColorFactor = BlendFactor::ONE;
        BlendFactor dstColorFactor = BlendFactor::ZERO;
        BlendFactor srcAlphaFactor = BlendFactor::ONE;
        BlendFactor dstAlphaFactor = BlendFactor::ZERO;
        inline constexpr bool operator== (const AlphaBlend& other) const { 
#define COMP_ATTR(name) (name == other.name)
            return COMP_ATTR(colorOp) && COMP_ATTR(alphaOp) && COMP_ATTR(srcColorFactor) && COMP_ATTR(dstColorFactor) && COMP_ATTR(srcAlphaFactor) && COMP_ATTR(dstAlphaFactor);
#undef COMP_ATTR
        }
        inline constexpr bool operator!=(const AlphaBlend& other) const { return !operator==(other); }
        inline static constexpr AlphaBlend overwrite() { return AlphaBlend{}; }
        inline static constexpr AlphaBlend normal() { return AlphaBlend{ BlendOperator::ADD, BlendOperator::ADD, BlendFactor::SRC_ALPHA, BlendFactor::ONE_MINUS_SRC_ALPHA, BlendFactor::ONE, BlendFactor::ONE_MINUS_SRC_ALPHA }; }
        inline static constexpr AlphaBlend pma() { return AlphaBlend{ BlendOperator::ADD, BlendOperator::ADD, BlendFactor::ONE, BlendFactor::ONE_MINUS_SRC_ALPHA, BlendFactor::ONE, BlendFactor::ONE_MINUS_SRC_ALPHA }; }
    };

    /// @brief 복사 영역을 지정합니다.
    struct TextureArea2D {
        /// @brief 복사 영역의 x좌표(px)를 설정합니다. 왼쪽이 0입니다. 기본값 0
        uint32_t x = 0;
        /// @brief 복사 영역의 y좌표(px)를 설정합니다. 위쪽이 0입니다. 기본값 0
        uint32_t y = 0;
        /// @brief 복사 영역의 가로 길이(px)를 설정합니다. 0이면 x, y, height에 무관하게 전체가 복사됩니다. 기본값 0
        uint32_t width = 0;
        /// @brief 복사 영역의 세로 길이(px)를 설정합니다. 0이면 x, y, width에 무관하게 전체가 복사됩니다. 기본값 0
        uint32_t height = 0;
    };

    struct RenderTarget2TextureOptions {
        /// @brief 0~2: 타겟의 해당 번호의 색 버퍼를 복사합니다. 3~: 현재 지원하지 않습니다. 기본값 0
        uint32_t index = 0;
        /// @brief true인 경우 결과 텍스처의 샘플링 방식이 linear로 수행됩니다. 기본값 false
        bool linearSampled = false;
        /// @brief 복사 영역을 지정합니다. @ref TextureArea2D
        TextureArea2D area;
    };

}


#endif
