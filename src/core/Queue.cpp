#include "Queue.hpp"
#include "PhysicalDevice.hpp"
#include "Device.hpp"

namespace Lu
{
    namespace Core
    {
        void getQueue()
        {
            vkGetDeviceQueue
            (vkDevice,
            queueFamilyIndex,
            0,
            &vkQueue);

            #ifdef LU_DEBUG
            LU_LOGI("Queue", "got", "")
            #endif
        }
    }
}