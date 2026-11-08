#pragma once

#include "Global.hpp"

namespace Lu{

    struct GraphicsPipelineConfig{
        std::string name;
        uint32_t capacity{0};

        std::string vertexShader;
        std::string fragmentShader;

        VkExtent2D extend2D{0,0};
        VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
        VkPolygonMode polygonMode{VK_POLYGON_MODE_FILL};
        VkCullModeFlags cullMode{VK_CULL_MODE_BACK_BIT};
        VkFrontFace frontFace{VK_FRONT_FACE_CLOCKWISE};
        float depthBiasConstantFactor{0.0f};
        float depthBiasClamp{0.0f};
        float depthBiasSlopeFactor{0.0f};
        VkSampleCountFlagBits sampleCount{VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM};
        float minSampleShading{0.0f};
        bool depthTestEnable{true};
        bool depthWriteEnable{true};
        VkCompareOp depthCompareOp{VK_COMPARE_OP_LESS};
        float minDepthBounds{0.0f};
        float maxDepthBounds{0.0f};
        bool hasColorAttachments{true};
        bool blendEnable{true};
        std::vector<VkFormat> colorAttachments{};
        VkFormat depthAttachmentFormat{VK_FORMAT_D32_SFLOAT};
        uint32_t viewMask{0};
        // Colour blend factors (only used if blendEnable). Defaults = classic alpha blending.
        // Premultiplied alpha / additive effects (fire, glow, sparks):
        //   srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        //   dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
        VkBlendFactor srcColorBlendFactor{VK_BLEND_FACTOR_SRC_ALPHA};
        VkBlendFactor dstColorBlendFactor{VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA};
        // Alpha to coverage (needs MSAA): the alpha of the fragment shader output decides how many of the samples of the pixel
        // are covered. Anti-aliased edges / soft transitions that are independent of the draw order, with depth writes and
        // without blending (e.g. the SDF GUI shapes of shader/mesh2d_ui.frag). Usually together with blendEnable = false.
        bool alphaToCoverageEnable{false};
    };

    // Describes a font that is rasterized with FreeType into one atlas texture (App::registerFont).
    struct FontConfig{
        std::string name;                 // name of the flecs entity that Component::Text::font refers to
        std::string filePath;             // .ttf / .otf / ... file (anything FreeType can open)
        uint32_t pixelSize{48};           // size the glyphs are rasterized at; texts are scaled from it
        // true: signed distance field atlas (FreeType >= 2.11), stays sharp when scaled/rotated
        // false: plain 8 bit coverage bitmap, best at exactly Component::Text::size == pixelSize
        bool sdf{true};
        // inclusive unicode ranges that go into the atlas (default: printable ASCII + Latin-1 supplement)
        std::vector<std::pair<uint32_t, uint32_t>> codepointRanges{{32, 126}, {160, 255}};
    };

    // Describes a video that is decoded in real time into one texture (App::registerVideo).
    struct VideoConfig{
        std::string name;                 // name of the flecs entity that Component::VideoTexture::video refers to
        std::string filePath;             // MPEG-1 program stream (.mpg), no audio. See core/VideoDecoder.hpp for the ffmpeg command
        bool loop{true};
        bool autoplay{true};
        float speed{1.0f};                // playback speed, 1 = real time
    };

    // Describes a live camera stream captured into one texture (App::registerVideoStream).
    struct VideoStreamConfig{
        std::string name;                 // name of the flecs entity that Component::VideoTexture::video refers to
        int deviceIndex{0};               // camera index (0 = default / first camera)
        std::string deviceName{""};       // optional device name or backend hint
        uint32_t width{0};                // requested capture width (0 = default/auto)
        uint32_t height{0};               // requested capture height (0 = default/auto)
        double frameRate{0.0};            // requested capture framerate (0 = default/auto)
    };

    struct ComputePipelineConfig{
        std::string name;
        std::string computeShader;
    };
}
