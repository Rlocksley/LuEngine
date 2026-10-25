#pragma once

#include "Global.hpp"
#include "Buffer.hpp"
#include "Channel.hpp"
#include "Time.hpp"
#include "PhysicalDevice.hpp"

namespace Lu{
    namespace Core{

        // GPU side of the 2D camera, std140 uniform block `Camera2DBuffer` in the 2D shaders (160 bytes).
        struct Camera2D{
            glm::mat4 viewProjection{1.0f};  // world pixels -> clip space (x, y); depth comes from Transform2D::positionZ
            glm::mat4 view{1.0f};            // world pixels -> screen pixels
            glm::vec2 position{0.0f};
            glm::vec2 screenSize{0.0f};      // width, height in pixels
            float zoom{1.0f};
            float rotation{0.0f};
            float deltaTime{0.0f};
            float time{0.0f};
        };
        static_assert(sizeof(Camera2D) == 160);

        // Mirrors Component::Camera2D into a uniform buffer. The matrices are rebuilt in copy() every frame,
        // so a window resize, deltaTime and time are always current, even if the camera itself did not change.
        class Camera2DPackage{
        private:
            Camera2DData latest{};
            DualBuffer<Camera2D> cameraBuffer;

            Camera2D build() const {
                Camera2D cam{};
                const glm::vec2 screen(
                    static_cast<float>(vkExtent2D.width > 0 ? vkExtent2D.width : 1),
                    static_cast<float>(vkExtent2D.height > 0 ? vkExtent2D.height : 1));

                const glm::vec2 pivotPixels = latest.pivot * screen;
                const glm::mat4 view =
                    glm::translate(glm::mat4(1.0f), glm::vec3(pivotPixels, 0.0f)) *
                    glm::scale(glm::mat4(1.0f), glm::vec3(latest.zoom, latest.zoom, 1.0f)) *
                    glm::rotate(glm::mat4(1.0f), -latest.rotation, glm::vec3(0.0f, 0.0f, 1.0f)) *
                    glm::translate(glm::mat4(1.0f), glm::vec3(-latest.position, 0.0f));

                // screen pixels -> clip space. Vulkan clip y points down, like the pixel y: no flip needed.
                glm::mat4 projection(1.0f);
                projection[0][0] = 2.0f / screen.x;
                projection[1][1] = 2.0f / screen.y;
                projection[3][0] = -1.0f;
                projection[3][1] = -1.0f;

                cam.view = view;
                cam.viewProjection = projection * view;
                cam.position = latest.position;
                cam.screenSize = screen;
                cam.zoom = latest.zoom;
                cam.rotation = latest.rotation;
                cam.deltaTime = Time::deltaTime;
                cam.time = Time::time;
                return cam;
            }

        public:
            Camera2DPackage() :
            cameraBuffer(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT){}

            void update(const EcsRequest::UpdateCamera2D& req){
                latest = req.cameraData;
            }

            void copy(const uint32_t frameIndex, const Command& command){
                cameraBuffer.push_back(frameIndex, build());
                cameraBuffer.copyToGpu(frameIndex, command);
            }

            const DualBuffer<Camera2D>& getCameraBuffer() const { return cameraBuffer; }
        };
    }
}
