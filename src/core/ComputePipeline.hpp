#pragma once

#include "PipelineConfig.hpp"
#include "PipelineBase.hpp"

namespace Lu {
    namespace Core {

        class ComputePipeline : public PipelineBase {
        public:
            ComputePipeline() = default;
            virtual ~ComputePipeline(){}

            void create(const VkPipelineLayout& pipelineLayout, const ComputePipelineConfig& config,
                        bool indirectBindable = false);
        };
    }
}