#pragma once

#include "Global.hpp"
#include "Constant.hpp"
#include "Buffer.hpp"
#include "Command.hpp"
#include "Texture.hpp"
#include "TexturePackage.hpp"
#include "PipelineConfig.hpp"
#include "VideoDecoder.hpp"
#include "flecs.h"
#include <ccap.h>

namespace Lu{
    namespace Core{

        // Videos = textures whose pixels change.
        //
        //   registerVideo       -> VideoDecoder (worker thread, RGBA frames) + one entry of the global texture array
        //   registerVideoStream -> ccap::Provider (camera capture) -> feeds VideoDecoder + one entry of global texture array
        //   every frame         -> newest decoded frame -> staging buffer of the frame in flight -> vkCmdCopyBufferToImage
        //
        // Because the video IS a slot of the global texture array, everything that samples that array shows it without
        // any shader change: Mesh2D::texture, Material::textures[..] of 3D meshes, skeleton meshes, ...
        // A frame is only uploaded when the decoder produced a new one (a 30 fps video on a 144 Hz renderer uploads 30x/s),
        // a paused video costs nothing.
        class VideoPackage{
        private:
            struct Video{
                std::unique_ptr<VideoDecoder> decoder;
                std::unique_ptr<ccap::Provider> camera; // live camera capture provider
                std::shared_ptr<Texture> texture;
                uint64_t serial{0};     // serial of the frame that is in the texture
                // one staging buffer per frame in flight: the buffer of a frame slot is only reused after its fence was waited
                std::array<std::unique_ptr<BufferInterface<uint8_t>>, MAX_FRAMES_IN_FLIGHT> staging;

                ~Video(){
                    if(camera){
                        camera->setNewFrameCallback(nullptr);
                        camera->close();
                    }
                }
            };

            using VideoEntity = flecs::entity_t;
            using TextureEntity = flecs::entity_t;

            std::vector<std::unique_ptr<Video>> videos;
            std::unordered_map<VideoEntity, Video*> entityToVideo;

        public:
            // Creates the decoder and registers the texture `textureEntity` (size of the video, first frame as content).
            void createVideo(const VideoEntity entity, const TextureEntity textureEntity,
                             const VideoConfig& config, TexturePackage& textures)
            {
                LU_ASSERT(entityToVideo.find(entity) == entityToVideo.end(), "VideoPackage", "createVideo", "Entity already is a video.")

                auto video = std::make_unique<Video>();
                try{
                    video->decoder = std::make_unique<VideoDecoder>(config.filePath, config.loop, config.autoplay, config.speed);
                }catch(const std::exception& e){
                    LU_LOGE("VideoPackage", "createVideo", e.what())
                }

                const uint32_t width = video->decoder->getWidth();
                const uint32_t height = video->decoder->getHeight();
                const size_t byteSize = video->decoder->getFrameByteSize();

                // the first frame is the initial content of the texture (uploaded by TexturePackage::copy)
                std::shared_ptr<uint8_t[]> firstFrame(new uint8_t[byteSize], std::default_delete<uint8_t[]>());
                const bool hasFirstFrame = video->decoder->fetchFrame(firstFrame.get(), video->serial);
                LU_ASSERT(hasFirstFrame, "VideoPackage", "createVideo", "The decoder has no first frame.")

                TextureCpu textureCpu;
                textureCpu.width = width;
                textureCpu.height = height;
                textureCpu.pixels = std::move(firstFrame);
                textureCpu.vkFormat = VK_FORMAT_R8G8B8A8_SRGB;   // video is sRGB encoded, the swapchain is sRGB too
                textures.createTexture(textureEntity, std::move(textureCpu));
                video->texture = textures.getTexture(textureEntity);

                entityToVideo[entity] = video.get();
                videos.push_back(std::move(video));

#ifdef LU_DEBUG
                LU_LOGI("VideoPackage", "createVideo", config.name + ": " + std::to_string(width) + "x" + std::to_string(height) +
                    ", " + std::to_string(videos.back()->decoder->getFrameRate()) + " fps, " +
                    std::to_string(videos.back()->decoder->getDuration()) + " s")
#endif
            }

            // Captures a live camera stream via ccap, feeds it into a VideoDecoder, and registers the texture `textureEntity`.
            void createVideoStream(const VideoEntity entity, const TextureEntity textureEntity,
                                   const VideoStreamConfig& config, TexturePackage& textures)
            {
                LU_ASSERT(entityToVideo.find(entity) == entityToVideo.end(), "VideoPackage", "createVideoStream", "Entity already is a video.")

                auto video = std::make_unique<Video>();
                auto camera = std::make_unique<ccap::Provider>();

                // Configure requested camera parameters before opening
                if(config.width > 0) camera->set(ccap::PropertyName::Width, config.width);
                if(config.height > 0) camera->set(ccap::PropertyName::Height, config.height);
                if(config.frameRate > 0.0) camera->set(ccap::PropertyName::FrameRate, config.frameRate);
                camera->set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::RGBA32);

                bool opened = false;
                if(!config.deviceName.empty()){
                    opened = camera->open(config.deviceName, true);
                } else {
                    opened = camera->open(config.deviceIndex, true);
                }

                if(!opened){
                    throw std::runtime_error("VideoPackage: cannot open camera device '" +
                        (config.deviceName.empty() ? std::to_string(config.deviceIndex) : config.deviceName) + "'");
                }

                // Grab first frame to determine video dimensions and initial texture content
                auto firstFrame = camera->grab(3000);
                if(!firstFrame){
                    throw std::runtime_error("VideoPackage: timed out waiting for first frame from camera '" + config.name + "'");
                }

                // Initialize the decoder for stream mode and feed the first frame
                video->decoder = std::make_unique<VideoDecoder>(firstFrame->width, firstFrame->height,
                    config.frameRate > 0.0 ? config.frameRate : 30.0);
                video->decoder->feedFrame(firstFrame);

                const uint32_t width = video->decoder->getWidth();
                const uint32_t height = video->decoder->getHeight();
                const size_t byteSize = video->decoder->getFrameByteSize();

                std::shared_ptr<uint8_t[]> firstPixels(new uint8_t[byteSize], std::default_delete<uint8_t[]>());
                const bool hasFirst = video->decoder->fetchFrame(firstPixels.get(), video->serial);
                LU_ASSERT(hasFirst, "VideoPackage", "createVideoStream", "The decoder has no first frame.")

                TextureCpu textureCpu;
                textureCpu.width = width;
                textureCpu.height = height;
                textureCpu.pixels = std::move(firstPixels);
                textureCpu.vkFormat = VK_FORMAT_R8G8B8A8_SRGB;
                textures.createTexture(textureEntity, std::move(textureCpu));
                video->texture = textures.getTexture(textureEntity);

                // Continuously feed camera frames into the decoder
                VideoDecoder* pDecoder = video->decoder.get();
                camera->setNewFrameCallback([pDecoder](const std::shared_ptr<ccap::VideoFrame>& frame) -> bool {
                    pDecoder->feedFrame(frame);
                    return true;
                });

                video->camera = std::move(camera);
                entityToVideo[entity] = video.get();
                videos.push_back(std::move(video));

#ifdef LU_DEBUG
                LU_LOGI("VideoPackage", "createVideoStream", config.name + ": " + std::to_string(width) + "x" + std::to_string(height))
#endif
            }

            void update(const VideoEntity entity, const bool playing, const bool loop, const float speed){
                auto it = entityToVideo.find(entity);
                if(it == entityToVideo.end()) return;   // not a registered video
                VideoDecoder& decoder = *it->second->decoder;
                decoder.setLoop(loop);
                decoder.setSpeed(speed);
                decoder.setPlaying(playing);
            }

            // Records the upload of every video that has a new frame. Called while recording the copy phase of a frame.
            void copy(const uint32_t frameIndex, const Command& cmd){
                LU_ASSERT(frameIndex < MAX_FRAMES_IN_FLIGHT, "VideoPackage", "copy", "Frame index is out of range.")

                for(auto& video : videos){
                    auto& staging = video->staging[frameIndex];
                    const size_t byteSize = video->decoder->getFrameByteSize();
                    if(!staging){
                        staging = std::make_unique<BufferInterface<uint8_t>>(
                            static_cast<uint32_t>(byteSize), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                    }

                    // decoder thread -> staging memory (host coherent), only if there is a new frame
                    if(video->decoder->fetchFrame(staging->pMemory, video->serial)){
                        video->texture->recordCopy(cmd.vkCommandBuffer, staging->vkBuffer, 0,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
                    }
                }
            }
        };

    }
}
