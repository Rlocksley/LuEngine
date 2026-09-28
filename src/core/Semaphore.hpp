#pragma once

#include "Global.hpp"

namespace Lu
{
    namespace Core
    {
        VkSemaphore createSemaphore();
        void destroySemaphore(VkSemaphore semaphore);
    }
}