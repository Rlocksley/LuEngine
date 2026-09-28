#pragma once
#include "Global.hpp"
#include "Device.hpp"

namespace Lu {
    namespace Core {
        class PipelineBase {
        public:
        PipelineBase() = default;
        virtual ~PipelineBase() {
            destroy();
        }

        void destroy(){
            if(pipeline != VK_NULL_HANDLE){
                vkDestroyPipeline(vkDevice, pipeline, nullptr);
                pipeline = VK_NULL_HANDLE;
            }
        }

        const VkPipeline& getVkPipeline() const { return pipeline; }

        protected:
            VkPipeline pipeline{VK_NULL_HANDLE};
        };
    }
}