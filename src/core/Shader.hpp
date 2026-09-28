#pragma once

#include "Global.hpp"

namespace Lu
{
    namespace Core
    {
        struct Shader
        {
        public:
            Shader(std::string filePath);
            ~Shader()  = default;

        private:
            std::vector<char> code{};

        public:
            VkShaderModule createShaderModule();
        };

        
    }
}