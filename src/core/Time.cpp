#include "Time.hpp"

namespace Lu
{
    namespace Time
    {

        void createTime()
        {
            startTime = glfwGetTime();
            time = startTime;
            deltaTime = minDeltaTime;
        }

        void updateTime()
        {
            float lastTime = time;
            time = glfwGetTime();
            deltaTime = time - lastTime;
            deltaTime = 
            deltaTime * (minDeltaTime <= deltaTime && deltaTime <= maxDeltaTime) +
            minDeltaTime * (deltaTime < minDeltaTime) +
            maxDeltaTime * (maxDeltaTime < deltaTime);
        }
    }
}