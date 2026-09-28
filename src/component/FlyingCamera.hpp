#pragma once

#include "Global.hpp"
#include "Channel.hpp"
#include "component/Transform.hpp"
#include "component/InputState.hpp"

namespace Lu
{
    namespace Component
    {
        struct FlyingCamera
        {
            glm::vec3 position;
            glm::vec2 angle;
            float speed;
            float rotationSpeed;
            float fov;
            float nearClip;
            float farClip;
            glm::vec3 direction;

        public:
            void update(Component::Transform& transform, Component::InputState& input, float deltaTime)
            {
                // apply mouse look
                angle += input.getCursorDeltaPosition() * (1.f * input.isMouseButtonDown(MouseButton::mouseRight)) * rotationSpeed;

                // compute forward direction from yaw (angle.x) and pitch (angle.y)
                direction = (glm::angleAxis(angle[0], glm::vec3(0.0f, -1.0f, 0.0f)) * glm::angleAxis(angle[1], glm::vec3(1.0f, 0.0f, 0.0f))) *
                            glm::vec3(0.0f, 0.0f, -1.0f);

                // movement input
                glm::vec3 deltaPosition = direction * (1.f * (input.isKeyDown(KeyCode::keyW) - input.isKeyDown(KeyCode::keyS)));
                glm::vec3 right = glm::cross(direction, glm::vec3(0.f, 1.f, 0.f));
                deltaPosition += right * (1.f * (input.isKeyDown(KeyCode::keyD) - input.isKeyDown(KeyCode::keyA)));

                if (deltaPosition.length() > 0.01f) {
                    glm::vec3 move = deltaPosition * (1.f / deltaPosition.length()) * speed * deltaTime;
                    position += move;
                }

                transform.position = position;
                // update transform rotation to match camera angles
                transform.rotation = glm::angleAxis(angle[0], glm::vec3(0.0f, -1.0f, 0.0f)) * glm::angleAxis(angle[1], glm::vec3(1.0f, 0.0f, 0.0f));
                
                GetChannel().send(
                    EcsRequest::UpdateCamera{
                        CameraData{
                            .position = transform.position,
                            .direction = direction,
                            .fov = fov,
                            .nearClip = nearClip,
                            .farClip = farClip
                        } 
                    }
                );
            }

            // new static lookAt()
            static glm::vec2 lookAt(const glm::vec3& position,
                                    const glm::vec3& viewTarget)
            {
                // 1) direction from cam → target
                glm::vec3 dir = glm::normalize(viewTarget - position);

                // 2) yaw  = rotation around Y so that forward (-Z) points towards dir
                //TBD test if -Z or +Z is forward and adjust accordingly
                float yaw   = std::atan2(dir.x, -dir.z);

                // 3) pitch = rotation around X so that forward (in Y) points towards dir
                float horizontalDist = std::sqrt(dir.x*dir.x + dir.z*dir.z);
                float pitch = std::atan2(dir.y, horizontalDist);

                return glm::vec2(yaw, pitch);
            }   
        };
    }

    namespace Module{
        struct FlyingCamera{
            FlyingCamera(flecs::world& world){
                world.component<Component::FlyingCamera>();

                world.system<Component::FlyingCamera, Component::Transform, Component::InputState>("Flying Camera Update")
                    .each(
                    [](flecs::iter it, size_t index, 
                    Component::FlyingCamera& camera, Component::Transform& transform, 
                    Component::InputState& input) {
                        camera.update(transform, input, it.delta_time());
                    });
            }
        };
    }
}