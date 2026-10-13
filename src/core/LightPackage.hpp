#pragma once

#include "Global.hpp"
#include "Constant.hpp"
#include "Buffer.hpp"
#include "flecs.h"
#include "component/Light.hpp"
#include "instance/LightInstance.hpp"

namespace Lu{
    namespace Core{
        class LightPackage{
        private:
            using LightEntity = flecs::entity_t;
            std::vector<Light> lights;
            std::vector<LightEntity> lightEntities;
            DualBuffer<Light> lightBuffer;
            DualBuffer<uint32_t> lightCountBuffer;
            uint64_t revision{1};
            std::array<uint64_t, MAX_FRAMES_IN_FLIGHT> uploadedRevision{};

        public:
            LightPackage()
                : lightBuffer(MAX_LIGHTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                  lightCountBuffer(1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {}

            void createLight(LightEntity entity, const Component::Light& light, const uint32_t transformId) {
                const auto it = std::find(lightEntities.begin(), lightEntities.end(), entity);
                if(it == lightEntities.end()) {
                    LU_ASSERT(lights.size() < MAX_LIGHTS,
                        "LightPackage", "createLight", "Light capacity exceeded.")
                    lights.push_back(Light(light, transformId));
                    lightEntities.push_back(entity);
                } else {
                    lights[static_cast<size_t>(it - lightEntities.begin())] = Light(light, transformId);
                }
                ++revision;
            }

            void destroyLight(LightEntity entity) {
                const auto it = std::find(lightEntities.begin(), lightEntities.end(), entity);
                LU_ASSERT(it != lightEntities.end(),
                    "LightPackage", "destroyLight", "Light entity does not exist.")
                const size_t index = static_cast<size_t>(it - lightEntities.begin());
                lights.erase(lights.begin() + static_cast<ptrdiff_t>(index));
                lightEntities.erase(it);
                ++revision;
            }

            template<typename Cmd>
            void copy(uint32_t frameIndex, const Cmd& command) {
                LU_ASSERT(frameIndex < MAX_FRAMES_IN_FLIGHT,
                    "LightPackage", "copy", "Frame index is out of range.")
                if(uploadedRevision[frameIndex] == revision) return;

                lightBuffer.clearCpu(frameIndex);
                if(!lights.empty()) {
                    lightBuffer.push_range_back(frameIndex, lights);
                    lightBuffer.copyToGpu(frameIndex, command);
                }

                lightCountBuffer.clearCpu(frameIndex);
                lightCountBuffer.push_back(frameIndex, static_cast<uint32_t>(lights.size()));
                lightCountBuffer.copyToGpu(frameIndex, command);
                uploadedRevision[frameIndex] = revision;
            }

            const DualBuffer<Light>& getLightBuffer() const { return lightBuffer; }
            const DualBuffer<uint32_t>& getLightCountBuffer() const { return lightCountBuffer; }
        };
    }
}