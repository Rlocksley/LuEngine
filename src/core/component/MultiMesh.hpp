#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Material.hpp"
#include "Transform.hpp"
#include "../instance/MultiMeshInstance.hpp"

namespace Lu{
    namespace Component{
        struct MultiMeshGpuDirty{};

        struct MultiMeshInstance{
            Component::Transform transform{};
            Component::Material material{};
        };

        struct MultiMesh{
            flecs::entity meshId;
            flecs::entity computePipeId;
            flecs::entity pipelineId; 
            glm::vec4 cullSphere{0.f, 0.f, 0.f, 0.f}; //xyz = center, w = radius
            std::vector<MultiMeshInstance> instances{};

            MultiMesh() = default;

            MultiMesh(const flecs::entity meshId,
                    const flecs::entity computePipeId,
                      const flecs::entity pipelineId, 
                                        std::vector<MultiMeshInstance> instances,
                    const glm::vec4& cullSphere = glm::vec4(0.f)):
                meshId(meshId),
                computePipeId(computePipeId),
                pipelineId(pipelineId),
                cullSphere(cullSphere),
                instances(std::move(instances))
                {}
        };
    }
}