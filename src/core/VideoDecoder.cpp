#include "VideoDecoder.hpp"

#include <chrono>
#include <cstring>
#include <stdexcept>

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg/pl_mpeg.h"

#include <ccap.h>
#include <ccap_convert.h>

namespace Lu{
    namespace Core{

        // plm_decode() can decode several frames when it has to catch up, only the last one is converted
        static void onFrame(plm_t*, plm_frame_t* frame, void* user){
            *static_cast<void**>(user) = frame;
        }

        VideoDecoder::VideoDecoder(const std::string& filePath, bool loop, bool playing, float speed) :
            isStreamMode(false), playing(playing), loop(loop), speed(speed < 0.0f ? 0.0f : speed)
        {
            plm = plm_create_with_filename(filePath.c_str());
            if(plm == nullptr){
                throw std::runtime_error("VideoDecoder: cannot open '" + filePath + "'");
            }
            if(!plm_has_headers(plm) || plm_get_width(plm) <= 0 || plm_get_height(plm) <= 0){
                plm_destroy(plm);
                plm = nullptr;
                throw std::runtime_error("VideoDecoder: '" + filePath + "' is not an MPEG-1 program stream. Convert it with: "
                    "ffmpeg -i in.mp4 -an -c:v mpeg1video -b:v 4M -f mpeg out.mpg");
            }

            plm_set_audio_enabled(plm, FALSE);
            plm_set_loop(plm, loop ? TRUE : FALSE);
            plm_set_video_decode_callback(plm, onFrame, &lastDecoded);

            width = static_cast<uint32_t>(plm_get_width(plm));
            height = static_cast<uint32_t>(plm_get_height(plm));
            frameRate = plm_get_framerate(plm);
            duration = plm_get_duration(plm);

            // pl_mpeg only writes R, G and B: the alpha bytes stay 255 for the whole life of both buffers
            frontFrame.assign(getFrameByteSize(), 255);
            backFrame.assign(getFrameByteSize(), 255);

            // first frame synchronously: the texture is never empty / black and can be created with the right size
            plm_frame_t* first = plm_decode_video(plm);
            if(first == nullptr){
                plm_destroy(plm);
                plm = nullptr;
                throw std::runtime_error("VideoDecoder: '" + filePath + "' contains no decodable video frame");
            }
            convertToBack(first);
            publishBack();

            worker = std::thread(&VideoDecoder::threadMain, this);
        }

        VideoDecoder::VideoDecoder(uint32_t initialWidth, uint32_t initialHeight, double frameRate) :
            isStreamMode(true), width(initialWidth), height(initialHeight), frameRate(frameRate), duration(0.0),
            playing(true), loop(true), speed(1.0f)
        {
            if(width > 0 && height > 0){
                frontFrame.assign(getFrameByteSize(), 255);
                backFrame.assign(getFrameByteSize(), 255);
            }
        }

        VideoDecoder::~VideoDecoder(){
            stop.store(true, std::memory_order_relaxed);
            if(worker.joinable()){
                worker.join();
            }
            if(plm != nullptr){
                plm_destroy(plm);
                plm = nullptr;
            }
        }

        void VideoDecoder::convertToBack(void* plmFrame){
            plm_frame_to_rgba(static_cast<plm_frame_t*>(plmFrame), backFrame.data(), static_cast<int>(width * 4));
        }

        void VideoDecoder::publishBack(){
            std::lock_guard<std::mutex> lock(frameMutex);
            if(frontFrame.size() != backFrame.size()){
                frontFrame = backFrame;
            } else {
                frontFrame.swap(backFrame);
            }
            ++frontSerial;
        }

        bool VideoDecoder::fetchFrame(uint8_t* dst, uint64_t& lastSerial){
            std::lock_guard<std::mutex> lock(frameMutex);
            if(frontSerial == lastSerial || frontFrame.empty()){
                return false;
            }
            std::memcpy(dst, frontFrame.data(), frontFrame.size());
            lastSerial = frontSerial;
            return true;
        }

        void VideoDecoder::feedFrame(const std::shared_ptr<ccap::VideoFrame>& frame){
            if(!frame || !playing.load(std::memory_order_relaxed)){
                return;
            }

            std::lock_guard<std::mutex> feedLock(feedMutex);

            const uint32_t frameWidth = frame->width;
            const uint32_t frameHeight = frame->height;
            if(frameWidth == 0 || frameHeight == 0) return;

            if(width != frameWidth || height != frameHeight){
                width = frameWidth;
                height = frameHeight;
            }

            const size_t byteSize = getFrameByteSize();
            if(backFrame.size() != byteSize){
                backFrame.assign(byteSize, 255);
            }

            const int dstStride = static_cast<int>(width * 4);
            uint8_t* dst = backFrame.data();
            const bool flip = (frame->orientation == ccap::FrameOrientation::BottomToTop);
            const int effHeight = flip ? -static_cast<int>(height) : static_cast<int>(height);

            switch(frame->pixelFormat){
                case ccap::PixelFormat::RGBA32: {
                    if(!flip && static_cast<uint32_t>(frame->stride[0]) == width * 4){
                        std::memcpy(dst, frame->data[0], byteSize);
                    } else {
                        const uint8_t* src = frame->data[0];
                        const int srcStride = static_cast<int>(frame->stride[0]);
                        for(uint32_t y = 0; y < height; ++y){
                            const uint32_t srcY = flip ? (height - 1 - y) : y;
                            std::memcpy(dst + y * dstStride, src + srcY * srcStride, width * 4);
                        }
                    }
                    break;
                }
                case ccap::PixelFormat::BGRA32: {
                    ccap::bgraToRgba(frame->data[0], frame->stride[0], dst, dstStride, width, effHeight);
                    break;
                }
                case ccap::PixelFormat::RGB24: {
                    ccap::rgbToRgba(frame->data[0], frame->stride[0], dst, dstStride, width, effHeight);
                    break;
                }
                case ccap::PixelFormat::BGR24: {
                    ccap::bgrToRgba(frame->data[0], frame->stride[0], dst, dstStride, width, effHeight);
                    break;
                }
                case ccap::PixelFormat::NV12:
                case ccap::PixelFormat::NV12f: {
                    ccap::nv12ToRgba32(frame->data[0], frame->stride[0],
                                       frame->data[1], frame->stride[1],
                                       dst, dstStride, width, effHeight);
                    break;
                }
                case ccap::PixelFormat::I420:
                case ccap::PixelFormat::I420f: {
                    ccap::i420ToRgba32(frame->data[0], frame->stride[0],
                                       frame->data[1], frame->stride[1],
                                       frame->data[2], frame->stride[2],
                                       dst, dstStride, width, effHeight);
                    break;
                }
                case ccap::PixelFormat::YUYV:
                case ccap::PixelFormat::YUYVf: {
                    ccap::yuyvToRgba32(frame->data[0], frame->stride[0], dst, dstStride, width, effHeight);
                    break;
                }
                case ccap::PixelFormat::UYVY:
                case ccap::PixelFormat::UYVYf: {
                    ccap::uyvyToRgba32(frame->data[0], frame->stride[0], dst, dstStride, width, effHeight);
                    break;
                }
                default: {
                    if(frame->data[0] != nullptr && frame->sizeInBytes >= byteSize){
                        std::memcpy(dst, frame->data[0], byteSize);
                    }
                    break;
                }
            }

            publishBack();
        }

        void VideoDecoder::feedFrame(const uint8_t* rgbaData, uint32_t frameWidth, uint32_t frameHeight){
            if(!rgbaData || !playing.load(std::memory_order_relaxed)) return;
            std::lock_guard<std::mutex> feedLock(feedMutex);
            if(width != frameWidth || height != frameHeight){
                width = frameWidth;
                height = frameHeight;
            }
            const size_t byteSize = getFrameByteSize();
            if(backFrame.size() != byteSize){
                backFrame.assign(byteSize, 255);
            }
            std::memcpy(backFrame.data(), rgbaData, byteSize);
            publishBack();
        }

        void VideoDecoder::setPlaying(bool value){
            playing.store(value, std::memory_order_relaxed);
        }

        void VideoDecoder::threadMain(){
            using Clock = std::chrono::steady_clock;
            auto last = Clock::now();

            while(!stop.load(std::memory_order_relaxed)){
                const auto now = Clock::now();
                double dt = std::chrono::duration<double>(now - last).count();
                last = now;

                if(playing.load(std::memory_order_relaxed)){
                    if(ended.load(std::memory_order_relaxed)){
                        // playing was switched on again after the end of a non looping video: restart
                        plm_rewind(plm);
                        ended.store(false, std::memory_order_relaxed);
                        dt = 0.0;
                    }

                    plm_set_loop(plm, loop.load(std::memory_order_relaxed) ? TRUE : FALSE);
                    if(dt > 0.1) dt = 0.1;   // do not fast forward after a stall / debugger break
                    dt *= static_cast<double>(speed.load(std::memory_order_relaxed));

                    lastDecoded = nullptr;
                    plm_decode(plm, dt);
                    if(lastDecoded != nullptr){
                        convertToBack(lastDecoded);
                        publishBack();
                    }

                    if(plm_has_ended(plm)){
                        // keep showing the last frame; setPlaying(true) restarts
                        ended.store(true, std::memory_order_relaxed);
                        playing.store(false, std::memory_order_relaxed);
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }

    }
}
