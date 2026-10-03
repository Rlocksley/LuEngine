#pragma once

#include "Global.hpp"
#include <atomic>

namespace Lu::Core {

    // recordEcsFrame() is called only from the ECS thread. The renderer thread
    // reads its published value in recordRendererFrameAndPrint().
    class FramerateMonitor {
    public:
        void recordEcsFrame() {
            ++ecsFrameCount_;

            const auto now = std::chrono::steady_clock::now();
            const std::chrono::duration<double> elapsed = now - ecsSampleStart_;
            if (elapsed.count() >= sampleIntervalSeconds_) {
                ecsFramesPerSecond_.store(
                    static_cast<double>(ecsFrameCount_) / elapsed.count(),
                    std::memory_order_relaxed
                );
                ecsFrameCount_ = 0;
                ecsSampleStart_ = now;
            }
        }

        // Call only from the renderer thread.
        void recordRendererFrameAndPrint() {
            ++rendererFrameCount_;

            const auto now = std::chrono::steady_clock::now();
            const std::chrono::duration<double> elapsed = now - rendererSampleStart_;
            if (elapsed.count() >= sampleIntervalSeconds_) {
                const double rendererFps = rendererFrameCount_ / elapsed.count();
                const double ecsFps = ecsFramesPerSecond_.load(std::memory_order_relaxed);
                std::cout << "FPS | ECS: " << ecsFps
                          << " | Renderer: " << rendererFps << std::endl;

                rendererFrameCount_ = 0;
                rendererSampleStart_ = now;
            }
        }

    private:
        static constexpr double sampleIntervalSeconds_ = 1.0;

        std::atomic<double> ecsFramesPerSecond_{0.0};
        uint64_t ecsFrameCount_{0};
        std::chrono::steady_clock::time_point ecsSampleStart_{std::chrono::steady_clock::now()};

        uint64_t rendererFrameCount_{0};
        std::chrono::steady_clock::time_point rendererSampleStart_{std::chrono::steady_clock::now()};
    };

}
