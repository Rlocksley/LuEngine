#pragma once

#include "Global.hpp"
#include "Channel.hpp"
#include "component/Transform.hpp"
#include "component/InputState.hpp"

namespace Lu
{
    namespace Component
    {
        // A third-person orbit camera, similar to the character camera used by WoW.
        struct CharacterCamera
        {
            flecs::entity target{};

            float yaw{0.0f};
            float pitch{0.15f};
            float distance{6.0f};
            float minDistance{2.0f};
            float maxDistance{20.0f};
            float zoomSpeed{2.0f};
            float rotationSpeed{0.005f};
            float fov{glm::radians(60.0f)};
            float nearClip{0.1f};
            float farClip{500.0f};
            glm::vec3 targetOffset{0.0f, 1.5f, 0.0f};

            void update(Transform& cameraTransform, const InputState& input)
            {
                if (!target.is_alive() || !target.has<Transform>()) return;

                if (input.isMouseButtonDown(MouseButton::mouseRight))
                {
                    const glm::vec2 delta = input.getCursorDeltaPosition();
                    yaw -= delta.x * rotationSpeed;
                    pitch -= delta.y * rotationSpeed;
                }

                constexpr float maxPitch = glm::radians(89.0f);
                pitch = glm::clamp(pitch, -maxPitch, maxPitch);

                distance -= input.scroll.deltaScroll.y * zoomSpeed;
                distance = glm::clamp(distance, minDistance, maxDistance);

                const glm::quat orientation =
                    glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f)) *
                    glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));
                const glm::vec3 focusPoint = target.get<Transform>().position + targetOffset;
                const glm::vec3 cameraPosition = focusPoint + orientation * glm::vec3(0.0f, 0.0f, -distance);
                const glm::vec3 direction = glm::normalize(focusPoint - cameraPosition);

                cameraTransform.position = cameraPosition;
                cameraTransform.rotation = orientation;

                GetChannel().send(EcsRequest::UpdateCamera{
                    CameraData{
                        .position = cameraPosition,
                        .direction = direction,
                        .fov = fov,
                        .nearClip = nearClip,
                        .farClip = farClip
                    }
                });
            }
        };
    }

    namespace Module
    {
        struct CharacterCamera
        {
            explicit CharacterCamera(flecs::world& world)
            {
                world.component<Component::CharacterCamera>();
                world.system<Component::CharacterCamera, Component::Transform, const Component::InputState>("Character Camera Update")
                    .each([](Component::CharacterCamera& camera, Component::Transform& transform, const Component::InputState& input)
                    {
                        camera.update(transform, input);
                    });
            }
        };
    }
}
