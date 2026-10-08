#pragma once

#include "Global.hpp"
#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "Shader.hpp"
#include "Buffer.hpp"
#include "PipelineConfig.hpp"
#include "PipelineBase.hpp"

namespace Lu{
    namespace Core{

        template<typename Vertex>
		class GraphicsPipeline : public PipelineBase {

            public:
                GraphicsPipeline() = default;
				virtual ~GraphicsPipeline(){}
				 
                void create(const VkPipelineLayout& pipelineLayout, const bool depthVertexMode, const GraphicsPipelineConfig& config);
        };

        template<typename Vertex>
        void GraphicsPipeline<Vertex>::create(const VkPipelineLayout& pipelineLayout, const bool depthVertexMode, const GraphicsPipelineConfig& config) {
			std::vector<VkPipelineShaderStageCreateInfo> shaderStages{};
			{
				LU_ASSERT(!config.vertexShader.empty(), "GraphicsPipeline", "build", "Vertex shader path must be set.");
				Shader vertexShader(config.vertexShader);
				VkPipelineShaderStageCreateInfo shaderStage{};
				shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				shaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
				shaderStage.module = vertexShader.createShaderModule();;
				shaderStage.pName = "main";
				shaderStages.push_back(shaderStage);
			}

			if(!config.fragmentShader.empty()){
				Shader fragmentShader(config.fragmentShader);
				VkPipelineShaderStageCreateInfo shaderStage{};
				shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				shaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
				shaderStage.module = fragmentShader.createShaderModule();
				shaderStage.pName = "main";
				shaderStages.push_back(shaderStage);
			}

			VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
			vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

			VkVertexInputBindingDescription vertexInputBinding{};
            vertexInputBinding = Vertex::getBindingDescription();

			std::vector<VkVertexInputAttributeDescription> vertexInputAttributes{};
			if(depthVertexMode){
                    vertexInputAttributes = Vertex::getDepthAttributeDescriptions();
			}else{
                    vertexInputAttributes = Vertex::getAttributeDescriptions();
			}

			const bool hasVertexBinding = (vertexInputBinding.stride > 0);
			vertexInputStateCreateInfo.vertexBindingDescriptionCount = hasVertexBinding ? 1u : 0u;
			vertexInputStateCreateInfo.pVertexBindingDescriptions = hasVertexBinding ? &vertexInputBinding : nullptr;
			vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
			vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributes.empty() ? nullptr : vertexInputAttributes.data();

			VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
			inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssemblyStateCreateInfo.topology = config.topology;
			inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

			VkViewport viewport{};
			viewport.x = 0.f;
			viewport.y = 0.f;
			viewport.width = static_cast<float>(vkExtent2D.width);
			viewport.height = static_cast<float>(vkExtent2D.height);
			viewport.minDepth = 0.f;
			viewport.maxDepth = 1.f;

			VkRect2D scissor{};
			scissor.offset = {0, 0};
			scissor.extent = config.extend2D.width > 0 && config.extend2D.height > 0 ? config.extend2D : vkExtent2D;

			VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
			viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportStateCreateInfo.viewportCount = 1;
			viewportStateCreateInfo.pViewports = &viewport;
			viewportStateCreateInfo.scissorCount = 1;
			viewportStateCreateInfo.pScissors = &scissor;

			VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
			rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
			rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
			rasterizationStateCreateInfo.polygonMode = config.polygonMode;
			rasterizationStateCreateInfo.cullMode = config.cullMode;
			rasterizationStateCreateInfo.frontFace = config.frontFace;
			rasterizationStateCreateInfo.lineWidth = 1.f;
			rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;

			VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{};
			multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampleStateCreateInfo.rasterizationSamples = config.sampleCount == VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM ? vkSampleCountFlagBits : config.sampleCount;
			multisampleStateCreateInfo.minSampleShading = config.minSampleShading;
			multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;

			VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
			depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencilStateCreateInfo.depthTestEnable = config.depthTestEnable ? VK_TRUE : VK_FALSE;
			depthStencilStateCreateInfo.depthWriteEnable = config.depthWriteEnable ? VK_TRUE : VK_FALSE;
			depthStencilStateCreateInfo.depthCompareOp = config.depthCompareOp;
			depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
			depthStencilStateCreateInfo.minDepthBounds = config.minDepthBounds;
			depthStencilStateCreateInfo.maxDepthBounds = config.maxDepthBounds;
			depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;

			VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
			colorBlendAttachmentState.colorWriteMask =
				VK_COLOR_COMPONENT_R_BIT |
				VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT |
				VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachmentState.blendEnable = config.blendEnable ? VK_TRUE : VK_FALSE;
			colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

			VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
			colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
			colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
			colorBlendStateCreateInfo.attachmentCount = 1;
			colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

			VkPipelineRenderingCreateInfo renderingInfo{};
			renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
			// Determine color attachment formats and count
			uint32_t colorCount = 0;
			const VkFormat* colorFormats = nullptr;
			if (config.hasColorAttachments) {
				if (config.colorAttachments.empty()) {
					colorCount = 1u;
					colorFormats = &vkSurfaceFormatKHR.format;
				} else {
					colorCount = static_cast<uint32_t>(config.colorAttachments.size());
					colorFormats = config.colorAttachments.data();
				}
			}
			renderingInfo.colorAttachmentCount = colorCount;
			renderingInfo.pColorAttachmentFormats = colorFormats;
			renderingInfo.depthAttachmentFormat = config.depthAttachmentFormat;
			renderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
			renderingInfo.viewMask = config.viewMask;

			VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
			graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			graphicsPipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
			graphicsPipelineCreateInfo.pStages = shaderStages.data();
			graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
			graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
			graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
			graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
			graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
			graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
			graphicsPipelineCreateInfo.pColorBlendState = config.hasColorAttachments ? &colorBlendStateCreateInfo : nullptr;
			graphicsPipelineCreateInfo.layout = pipelineLayout;
			graphicsPipelineCreateInfo.pNext = &renderingInfo;
			graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE; //Using dynamic rendering, so no render pass or subpass
			graphicsPipelineCreateInfo.subpass = 0;
			graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
			graphicsPipelineCreateInfo.basePipelineIndex = -1;

			LU_CHECK_VULKAN(
				vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &pipeline),
				"GraphicsPipeline::build",
				"vkCreateGraphicsPipelines"
			)

			for(auto& shaderStage : shaderStages){
				vkDestroyShaderModule(vkDevice, shaderStage.module, nullptr);
			}
		}

    }
}
