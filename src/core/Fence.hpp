#pragma once

#include "Global.hpp"

namespace Lu
{
    namespace Core
    {
        VkFence createFence(VkFenceCreateFlags flags);
        void destroyFence(VkFence fence);
    }
}