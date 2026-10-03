#include "ComputePipeline.hpp"

#include "Device.hpp"
#include "Shader.hpp"

namespace Lu {
	namespace Core {

		void ComputePipeline::create(const VkPipelineLayout& pipelineLayout, const ComputePipelineConfig& config,
								bool indirectBindable){

			PipelineBase::destroy();

			LU_ASSERT(!config.computeShader.empty(), "ComputePipeline", "create", "Compute shader path must be set.")

			Shader shader(config.computeShader);
			VkShaderModule shaderModule = shader.createShaderModule();

			VkPipelineShaderStageCreateInfo shaderStage{};
			shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
			shaderStage.module = shaderModule;
			shaderStage.pName = "main";

			VkComputePipelineCreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			VkPipelineCreateFlags2CreateInfo flags2Info{};
			if(indirectBindable){
				flags2Info.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO;
				flags2Info.flags = VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT;
				computePipelineCreateInfo.pNext = &flags2Info;
			}
			computePipelineCreateInfo.stage = shaderStage;
			computePipelineCreateInfo.layout = pipelineLayout;
			computePipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
			computePipelineCreateInfo.basePipelineIndex = -1;

			LU_CHECK_VULKAN(
				vkCreateComputePipelines(vkDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipeline),
				"ComputePipeline::build",
				"vkCreateComputePipelines"
			)

			vkDestroyShaderModule(vkDevice, shaderModule, nullptr);
		}
	}
}
