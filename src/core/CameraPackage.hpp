#pragma once

#include "Global.hpp"
#include "Buffer.hpp"
#include "Channel.hpp"
#include "Time.hpp"
#include <limits>

namespace Lu{
    namespace Core{
        struct Camera{
            glm::vec4 frustumPlanes[6]{
                glm::vec4(0.0f, 0.0f, 0.0f, -std::numeric_limits<float>::max())
            };
            glm::mat4 projection{glm::mat4(0.0f)};
            glm::mat4 view{glm::mat4(0.0f)};
            glm::mat4 viewProjection{glm::mat4(0.0f)};
            glm::mat4 inverseProjection{glm::mat4(0.0f)};
            glm::vec4 camPos{glm::vec4(0.0f)};      // xyz = world position, w = unused (matches std140 vec3 padded to vec4)
            glm::vec4 camDir{glm::vec4(0.0f)};      // xyz = direction, w = unused
            glm::vec2 screenSize{glm::vec2(0.0f)};  // Width, height in pixels
            float nearClip{0.0f};                   // Near plane distance
            float farClip{0.0f};                    // Far plane distance
            float deltaTime{0.f};
            float time{0.f};
        };

        class CameraPackage{
        private:
            Camera latestCamera{};
            DualBuffer<Camera> cameraBuffer;

            glm::mat4 createViewMatrix(const glm::vec3& cameraPosition, const glm::vec3& cameraDirection, const glm::vec3& upDirection);
            glm::mat4 createXMatrix();
            glm::mat4 createProjectionMatrix(float fov, float aspectRatio, float nearClip, float farClip);
            
        public:
            CameraPackage() : 
            cameraBuffer(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT){};

            void update(const EcsRequest::UpdateCamera& req);
            void copy(const uint32_t frameIndex, const Command& command);
            const DualBuffer<Camera>& getCameraBuffer() const {return cameraBuffer;}
        };

    }
}