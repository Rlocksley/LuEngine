#include "CameraPackage.hpp"
#include "PhysicalDevice.hpp"

namespace Lu{
    namespace Core{
        glm::mat4 CameraPackage::createViewMatrix(const glm::vec3& cameraPosition, const glm::vec3& cameraDirection, const glm::vec3& upDirection)
        {
           glm::vec3 cameraRight = glm::normalize(glm::cross(upDirection, cameraDirection));
            glm::vec3 cameraUp = glm::normalize(glm::cross(cameraRight, cameraDirection));

            glm::mat4 viewMatrix(1.0f);
            viewMatrix[0][0] = cameraRight.x;
            viewMatrix[0][1] = cameraRight.y;
            viewMatrix[0][2] = cameraRight.z;
            viewMatrix[1][0] = cameraUp.x;
            viewMatrix[1][1] = cameraUp.y;
            viewMatrix[1][2] = cameraUp.z;
            viewMatrix[2][0] = -cameraDirection.x;
            viewMatrix[2][1] = -cameraDirection.y;
            viewMatrix[2][2] = -cameraDirection.z;
            viewMatrix[3][0] = cameraPosition.x;
            viewMatrix[3][1] = cameraPosition.y;
            viewMatrix[3][2] = cameraPosition.z;

            viewMatrix = glm::inverse(viewMatrix);

            return viewMatrix;
        }

        glm::mat4 CameraPackage::createXMatrix()
        {
            glm::mat4 X = glm::mat4(1.f);
            X[1][1] = -1.f;
            X[2][2] = -1.f;
            return glm::inverse(X);
        }

        glm::mat4 CameraPackage::createProjectionMatrix(float fov, float aspectRatio, float nearClip, float farClip) 
        {
            glm::mat4 projectionMatrix(1.0f);

            float tanHalfFOV = tan(fov / 2.0f);
            float depthRangeInv = 1.0f / (farClip - nearClip);

            projectionMatrix[0][0] = 1.0f / (aspectRatio * tanHalfFOV);
            projectionMatrix[0][1] = 0.0f;
            projectionMatrix[0][2] = 0.0f;
            projectionMatrix[0][3] = 0.0f;
            projectionMatrix[1][0] = 0.0f;
            projectionMatrix[1][1] = 1.0f / tanHalfFOV;
            projectionMatrix[1][2] = 0.0f;
            projectionMatrix[1][3] = 0.0f;

            projectionMatrix[2][0] = 0.0f;
            projectionMatrix[2][1] = 0.0f;
            projectionMatrix[2][2] = farClip * depthRangeInv; // Vulkan clip depth [0, 1]
            projectionMatrix[2][3] = 1.0f;

            projectionMatrix[3][0] = 0.0f;
            projectionMatrix[3][1] = 0.0f;
            projectionMatrix[3][2] = (-farClip * nearClip) * depthRangeInv;
            projectionMatrix[3][3] = 0.0f;

            return projectionMatrix;
        }

        void CameraPackage::update(const EcsRequest::UpdateCamera& req){

            Camera cam{};

            // Build projection and view matrices. Include the X matrix used elsewhere
            // to match the existing viewProjection behavior.
            glm::mat4 proj = createProjectionMatrix(
                req.cameraData.fov,
                static_cast<float>(vkExtent2D.width) / static_cast<float>(vkExtent2D.height),
                req.cameraData.nearClip,
                req.cameraData.farClip
            );
            glm::mat4 X = createXMatrix();
            glm::mat4 view = createViewMatrix(req.cameraData.position, glm::normalize(req.cameraData.direction), glm::vec3(0.f, -1.f, 0.f));

            cam.projection = proj * X;
            cam.inverseProjection = glm::inverse(cam.projection);
            cam.view = view;
            cam.viewProjection = cam.projection * cam.view;

            cam.camPos = glm::vec4(req.cameraData.position, 0.0f);
            cam.camDir = glm::vec4(glm::normalize(req.cameraData.direction), 0.0f);
            cam.screenSize = glm::vec2(static_cast<float>(vkExtent2D.width), static_cast<float>(vkExtent2D.height));
            cam.nearClip = req.cameraData.nearClip;
            cam.farClip = req.cameraData.farClip;
            cam.deltaTime = Time::deltaTime;
            cam.time = Time::time;

            // Extract and normalize frustum planes from viewProjection
            const glm::vec4 row0(
                cam.viewProjection[0][0],
                cam.viewProjection[1][0],
                cam.viewProjection[2][0],
                cam.viewProjection[3][0]
            );
            const glm::vec4 row1(
                cam.viewProjection[0][1],
                cam.viewProjection[1][1],
                cam.viewProjection[2][1],
                cam.viewProjection[3][1]
            );
            const glm::vec4 row2(
                cam.viewProjection[0][2],
                cam.viewProjection[1][2],
                cam.viewProjection[2][2],
                cam.viewProjection[3][2]
            );
            const glm::vec4 row3(
                cam.viewProjection[0][3],
                cam.viewProjection[1][3],
                cam.viewProjection[2][3],
                cam.viewProjection[3][3]
            );

            cam.frustumPlanes[0] = row3 + row0; // Left
            cam.frustumPlanes[1] = row3 - row0; // Right
            cam.frustumPlanes[2] = row3 + row1; // Bottom
            cam.frustumPlanes[3] = row3 - row1; // Top
            cam.frustumPlanes[4] = row2;        // Near
            cam.frustumPlanes[5] = row3 - row2; // Far

            for (glm::vec4& plane : cam.frustumPlanes) {
                const float normalLength = glm::length(glm::vec3(plane));
                if (normalLength > 0.0f) {
                    plane /= normalLength;
                }
            }

            latestCamera = cam;
        }

        void CameraPackage::copy(const uint32_t frameIndex, const Command& command){
            cameraBuffer.push_back(frameIndex, latestCamera);
            cameraBuffer.copyToGpu(frameIndex, command);
        }
    }
}