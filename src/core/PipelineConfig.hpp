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
    };

    struct ComputePipelineConfig{
        std::string name;
        std::string computeShader;
    };
}