#pragma once

#include "Global.hpp"
#include "Texture.hpp"

namespace Lu{
    namespace Core{

        struct TextureCpuInstance{
            TextureCpu texture{};
            uint32_t id;

            TextureCpuInstance(TextureCpu& texture, const uint32_t id) :
            id(id)
            {
                this->texture = std::move(texture);
            }
        };
    }
}