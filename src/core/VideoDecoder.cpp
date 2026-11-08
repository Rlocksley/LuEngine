#include "VideoDecoder.hpp"

#include <chrono>
#include <cstring>
#include <stdexcept>

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg/pl_mpeg.h"

namespace Lu{
    namespace Core{

        // plm_decode() can decode several frames when it has to catch up, only the last one is converted
        static void onFrame(plm_t*, plm_frame_t* frame, void* user){
            *static_cast<void**>(user) = frame;
        }

        VideoDecoder::VideoDecoder(const std::string& filePath, bool loop, bool playing, float speed) :
            playing(playing), loop(loop), speed(speed < 0.0f ? 0.0f : speed)
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

        VideoDecoder::~VideoDecoder(){
            stop.store(true, std::memory_order_relaxed);
            if(worker.joinable()){
                worker.join();
            }
            if(plm != nullptr){
                plm_destroy(plm);
            }
        }

        void VideoDecoder::convertToBack(void* plmFrame){
            plm_frame_to_rgba(static_cast<plm_frame_t*>(plmFrame), backFrame.data(), static_cast<int>(width * 4));
        }

        void VideoDecoder::publishBack(){
            std::lock_guard<std::mutex> lock(frameMutex);
            frontFrame.swap(backFrame);
            ++frontSerial;
        }

        bool VideoDecoder::fetchFrame(uint8_t* dst, uint64_t& lastSerial){
            std::lock_guard<std::mutex> lock(frameMutex);
            if(frontSerial == lastSerial){
                return false;
            }
            std::memcpy(dst, frontFrame.data(), frontFrame.size());
            lastSerial = frontSerial;
            return true;
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
