#pragma once

#include "Global.hpp"
#include "Image.hpp"

namespace Lu
{
    namespace Core
    {
        struct Framebuffers
        {
            std::vector<Image> colorImages;
            std::vector<Image> depthImages;
            //std::vector<VkFramebuffer> vkFramebuffers{}; //dynamic  renderer dont need to have these
        };

        inline Framebuffers framebuffers;

        void createFramebuffers();
        void destroyFramebuffers();
    }
}