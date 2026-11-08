#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <memory>

struct plm_t;   // pl_mpeg (core/pl_mpeg/pl_mpeg.h), only included in VideoDecoder.cpp

namespace ccap {
    struct VideoFrame;
}

namespace Lu{
    namespace Core{

        // Software decoder of one MPEG-1 video file OR live video stream (e.g. from camera via ccap).
        // It has NO Vulkan / engine dependency: publishes the newest frame as tightly packed RGBA8
        // (sRGB encoded, alpha = 255). The render thread polls it with fetchFrame().
        //
        // File mode: worker thread decodes in real time (scaled by `speed`).
        // Stream mode: camera or external stream feeds frames via feedFrame().
        class VideoDecoder{
        public:
            // File mode: opens an MPEG-1 program stream (.mpg)
            VideoDecoder(const std::string& filePath, bool loop = true, bool playing = true, float speed = 1.0f);

            // Stream mode: live video / camera stream fed via feedFrame()
            VideoDecoder(uint32_t initialWidth = 0, uint32_t initialHeight = 0, double frameRate = 30.0);

            ~VideoDecoder();

            VideoDecoder(const VideoDecoder&) = delete;
            VideoDecoder& operator=(const VideoDecoder&) = delete;

            uint32_t getWidth() const { return width; }
            uint32_t getHeight() const { return height; }
            size_t getFrameByteSize() const { return static_cast<size_t>(width) * height * 4; }
            double getFrameRate() const { return frameRate; }
            double getDuration() const { return duration; }
            bool isStream() const { return isStreamMode; }

            // Copies the newest frame (getFrameByteSize() bytes) to `dst` if it is newer than `lastSerial`
            // and updates `lastSerial`. Returns false (dst untouched) if there is no new frame. Thread safe.
            // The first frame of the video/stream is available right after construction / first frame feed.
            bool fetchFrame(uint8_t* dst, uint64_t& lastSerial);

            // Feeds a captured camera frame (ccap::VideoFrame) into the decoder. Thread safe.
            // Automatically converts YUV/NV12/YUYV/UYVY/RGB/BGR/BGRA formats to tightly packed RGBA8
            // and accounts for frame orientation.
            void feedFrame(const std::shared_ptr<ccap::VideoFrame>& frame);

            // Feeds raw RGBA8 data directly into the decoder. Thread safe.
            void feedFrame(const uint8_t* rgbaData, uint32_t frameWidth, uint32_t frameHeight);

            // playing = true on a video that has ended (loop == false) restarts it.
            void setPlaying(bool value);
            void setLoop(bool value) { loop.store(value, std::memory_order_relaxed); }
            void setSpeed(float value) { speed.store(value < 0.0f ? 0.0f : value, std::memory_order_relaxed); }

        private:
            void threadMain();
            void convertToBack(void* plmFrame);
            void publishBack();

            bool isStreamMode{false};
            plm_t* plm{nullptr};
            uint32_t width{0};
            uint32_t height{0};
            double frameRate{0.0};
            double duration{0.0};

            std::atomic<bool> playing{true};
            std::atomic<bool> loop{true};
            std::atomic<float> speed{1.0f};
            std::atomic<bool> ended{false};
            std::atomic<bool> stop{false};

            std::mutex frameMutex;
            std::vector<uint8_t> frontFrame;    // newest published frame (guarded by frameMutex)
            uint64_t frontSerial{0};            // guarded by frameMutex
            std::vector<uint8_t> backFrame;     // buffer for decoding / conversion
            std::mutex feedMutex;               // guards backFrame during feedFrame
            void* lastDecoded{nullptr};         // newest plm_frame_t of the current plm_decode() call

            std::thread worker;
        };

    }
}
