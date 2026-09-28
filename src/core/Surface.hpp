#pragma once

#include "Global.hpp"

namespace Lu
{
    namespace Core
    {
        inline VkSurfaceKHR vkSurfaceKHR;

        void createSurface();
        void destroySurface();
    }
}