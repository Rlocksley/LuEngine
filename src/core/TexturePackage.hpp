#pragma once

#include "Global.hpp"
#include "Constant.hpp"
#include "flecs.h"
#include "Buffer.hpp"
#include "Texture.hpp"
#include "instance/TextureInstance.hpp"
#include "Command.hpp"

namespace Lu{
    namespace Core{

        class TexturePackage{
        private:
            std::vector<TextureCpuInstance> textureCpuInstances;
            std::array<std::shared_ptr<Texture>, MAX_TEXTURES> textureArray;
            std::shared_ptr<Texture> fallbackTexture;
            TextureCpu fallbackTextureCpu;
            std::array<std::vector<BufferInterface<uint8_t>>, MAX_FRAMES_IN_FLIGHT> frameStaging;
            uint32_t textureCount{0};
            bool fallbackCopyPending{true};

            using TextureEntity = flecs::entity_t;
            using TextureId = uint32_t;
            std::unordered_map<TextureEntity, TextureId> entityToTextureId;

        public:
            TexturePackage() {
                std::shared_ptr<uint8_t[]> whitePixel(
                    new uint8_t[4]{255, 255, 255, 255},
                    std::default_delete<uint8_t[]>());
                fallbackTextureCpu = TextureCpu(1, 1, std::move(whitePixel));
                fallbackTexture = std::make_shared<Texture>(
                    fallbackTextureCpu.width,
                    fallbackTextureCpu.height,
                    fallbackTextureCpu.vkFormat,
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT);
                textureArray.fill(fallbackTexture);
            }

            void createTexture(TextureEntity entity, const std::string& filePath) {
                LU_ASSERT(entityToTextureId.find(entity) == entityToTextureId.end(),
                    "TexturePackage", "createTexture", "Entity already has a texture.")
                LU_ASSERT(textureCount < MAX_TEXTURES,
                    "TexturePackage", "createTexture", "Texture capacity exceeded.")

                TextureCpu cpuTexture(filePath);
                const TextureId id = textureCount;
                auto texture = std::make_shared<Texture>(
                    cpuTexture.width,
                    cpuTexture.height,
                    cpuTexture.vkFormat,
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT);
                textureArray[id] = std::move(texture);
                textureCpuInstances.emplace_back(cpuTexture, id);
                entityToTextureId.emplace(entity, id);
                ++textureCount;
            }

            void copy(uint32_t frameIndex, const Command& cmd) {
                LU_ASSERT(frameIndex < MAX_FRAMES_IN_FLIGHT,
                    "TexturePackage", "copy", "Frame index is out of range.")
                frameStaging[frameIndex].clear();

                if(fallbackCopyPending) {
                    BufferInterface<uint8_t> staging(4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                    std::memcpy(staging.pMemory, fallbackTextureCpu.pixels.get(), 4);
                    fallbackTexture->recordCopy(cmd.vkCommandBuffer, staging.vkBuffer);
                    frameStaging[frameIndex].push_back(std::move(staging));
                    fallbackCopyPending = false;
                }

                for(const TextureCpuInstance& textureInstance : textureCpuInstances) {
                    const uint32_t pixelByteCount =
                        textureInstance.texture.width * textureInstance.texture.height * 4;
                    BufferInterface<uint8_t> staging(
                        pixelByteCount,
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                    std::memcpy(staging.pMemory, textureInstance.texture.pixels.get(), pixelByteCount);
                    textureArray[textureInstance.id]->recordCopy(cmd.vkCommandBuffer, staging.vkBuffer);
                    frameStaging[frameIndex].push_back(std::move(staging));
                }
                textureCpuInstances.clear();
            }

            const std::array<std::shared_ptr<Texture>, MAX_TEXTURES>& getTextureArray() const{
                return textureArray;
            }

            TextureId getTextureId(TextureEntity entity) const{
                auto it = entityToTextureId.find(entity);
                LU_ASSERT(it != entityToTextureId.end(), "TexturePackage", "getTextureId", "No TextureId for TextureEntity");
                return it->second; 
            }
        };

    }
}