#pragma once

#include "Global.hpp"
#include "Channel.hpp"
#include "flecs.h"

namespace Lu{
    namespace Core{

        class AssetManager{
            flecs::world* world{nullptr};
        public:
            void setWorld(flecs::world* _world){
                this->world = _world;
            }

            void load(const std::string& path, const std::string& prefix = "Mesh::");
        };

        inline AssetManager& GetAssetManager(){
            static AssetManager assetManager;
            return assetManager;
        }
    }
}