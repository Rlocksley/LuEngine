#pragma once
#include "Global.hpp"
#include "MaterialInstance.hpp"

namespace Lu{
    namespace Core{

        struct SkeletonMesh{
            Material material{};
            uint32_t transformId{0};   
            uint32_t pipelineId{0};       
            uint32_t meshInfoId{0};
            uint32_t skeletonId{0};
            uint32_t nodeIndex{UINT32_MAX}; // UINT32_MAX for deformed, != UINT32_MAX for node-attached
            uint32_t valid{0};
        };


    }
}