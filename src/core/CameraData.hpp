#pragma once
#include "Global.hpp"

namespace Lu{
    struct CameraData{
        glm::vec3 position;
        glm::vec3 direction;
        float fov;
        float nearClip;
        float farClip;
    };
}