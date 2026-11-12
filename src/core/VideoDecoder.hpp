#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct plm_t;   // pl_mpeg (core/pl_mpeg/pl_mpeg.h), only included in VideoDecoder.cpp

namespace Lu{
    namespace Core{

        // Software decoder of one MPEG-1 video file (pl_mpeg: single header, no dependencies, no audio).
        // It has NO Vulkan / engine dependency: a worker thread decodes in real time (scaled by `speed`) and publishes
        // the newest frame as tightly packed RGBA8 (sRGB encoded, alpha = 255). The render thread polls it with fetchFrame().
        //
        //   convert any video once:  ffmpeg -i in.mp4 -an -c:v mpeg1video -b:v 4M -f mpeg out.mpg
        //
        // Throws std::runtime_error if the file cannot be opened / is not an MPEG-1 program stream.
        class VideoDecoder{
        public:
            VideoDecoder(const std::string& filePath, bool loop = true, bool playing = true, float speed = 1.0f);
            ~VideoDecoder();

            VideoDecoder(const VideoDecoder&) = delete;
            VideoDecoder& operator=(const VideoDecoder&) = delete;

            uint32_t getWidth() const { return width; }
            uint32_t getHeight() const { return height; }
            size_t getFrameByteSize() const { return static_cast<size_t>(width) * height * 4; }
            double getFrameRate() const { return frameRate; }
            double getDuration() const { return duration; }

            // Copies the newest frame (getFrameByteSize() bytes) to `dst` if it is newer than `lastSerial`
            // and updates `lastSerial`. Returns false (dst untouched) if there is no new frame. Thread safe.
            // The first frame of the file is available right after construction (serial 1).
            bool fetchFrame(uint8_t* dst, uint64_t& lastSerial);

            // playing = true on a video that has ended (loop == false) restarts it.
            void setPlaying(bool value);
            void setLoop(bool value) { loop.store(value, std::memory_order_relaxed); }
            void setSpeed(float value) { speed.store(value < 0.0f ? 0.0f : value, std::memory_order_relaxed); }

        private:
            void threadMain();
            void convertToBack(void* plmFrame);
            void publishBack();

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
            std::vector<uint8_t> backFrame;     // only touched by the decode thread
            void* lastDecoded{nullptr};         // newest plm_frame_t of the current plm_decode() call

            std::thread worker;
        };

    }
}
