#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Vertex.hpp"
#include "CameraPackage.hpp"
#include "TransformPackage.hpp"
#include "GeometryPackage.hpp"
#include "MeshPackage.hpp"
#include "MultiMeshPackage.hpp"
#include "Channel.hpp"
#include "FramerateMonitor.hpp"

namespace Lu{
    namespace Core{

    class Renderer{
        public:
            Renderer();
            ~Renderer();
            void run(FramerateMonitor& framerateMonitor);

            using MeshPipeEntity = flecs::entity_t;
            void createMeshPipe(const MeshPipeEntity entity, const GraphicsPipelineConfig& config);

            using MeshGeometryEntity = flecs::entity_t;
            void createMeshGeometry(const MeshGeometryEntity entity, const std::vector<Vertex::Mesh>& vb, const std::vector<uint32_t> ib);

            using MultiMeshPipeEntity = flecs::entity_t;
            void createMultiMeshPipe(const MultiMeshPipeEntity entity, const GraphicsPipelineConfig& config);
            void createMultiMeshComputePipe(const MultiMeshPipeEntity entity, const ComputePipelineConfig& config);

        private:

            void processEcsRequests();
            void record();
            VkResult submit();
            
            uint32_t frameIndex{0};
            std::array<Lu::Core::Command, MAX_FRAMES_IN_FLIGHT> command;
                        
          
            CameraPackage camera;
            TransformPackage transform;            
            GeometryPackage<Vertex::Mesh> meshGeometry;
            MeshPackage mesh;
            MultiMeshPackage multiMesh;
            std::deque<EcsRequest::CreateMultiMesh> pendingMultiMeshCreates;

            static void beginRendering(const Command& cmd);
            static void endRendering(const Command& cmd);
            
            static void copyBarrier(const Command& cmd);
            static void transferBarrier(const Command& cmd);
            static void cullBarrier(const Command& cmd);
            
            static void recordBarrier(
                VkCommandBuffer commandBuffer,
                VkPipelineStageFlags srcStage,
                VkPipelineStageFlags dstStage,
                VkAccessFlags srcAccess,
                VkAccessFlags dstAccess
            );
        };


        
    }
}
