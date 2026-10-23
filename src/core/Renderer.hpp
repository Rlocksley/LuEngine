#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Vertex.hpp"
#include "CameraPackage.hpp"
#include "TransformPackage.hpp"
#include "GeometryPackage.hpp"
#include "LightPackage.hpp"
#include "MeshPackage.hpp"
#include "MultiMeshPackage.hpp"
#include "SkeletonPackage.hpp"
#include "AnimationLibrary.hpp"
#include "TexturePackage.hpp"
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

            using SkeletonPipeEntity = flecs::entity_t;
            void createSkeletonPipe(const SkeletonPipeEntity entity, const GraphicsPipelineConfig& config);

            using SkeletonGeometryEntity = flecs::entity_t;
            void createSkeletonGeometry(const SkeletonGeometryEntity entity, const std::vector<Vertex::Skeleton>& vb, const std::vector<uint32_t>& ib);

            using SkeletonAssetEntity = flecs::entity_t;
            void createSkeletonAsset(const SkeletonAssetEntity entity, const SkeletonAsset& asset);

            using MultiMeshPipeEntity = flecs::entity_t;
            void createMultiMeshPipe(const MultiMeshPipeEntity entity, const GraphicsPipelineConfig& config);
            
            using MultiMeshComputePipeEntity = flecs::entity_t;
            void createMultiMeshComputePipe(const MultiMeshComputePipeEntity entity, const ComputePipelineConfig& config);

            using TextureEntity = flecs::entity_t;
            void createTexture(const TextureEntity entity, const std::string& filePath);
            void createTexture(const TextureEntity entity, TextureCpu&& texture);
        private:

            void processEcsRequests();
            void record();
            VkResult submit();
            
            uint32_t frameIndex{0};
            std::array<Lu::Core::Command, MAX_FRAMES_IN_FLIGHT> command;
            VkImageView depthMapView{VK_NULL_HANDLE};
            VkExtent2D depthMapExtent{};
            bool depthMapInitialized{false};
                        
          
            CameraPackage camera;
            TransformPackage transform;            
            LightPackage lights;
            GeometryPackage<Vertex::Mesh> meshGeometry;
            TexturePackage texture;
            MeshPackage mesh;
            MultiMeshPackage multiMesh;
            GeometryPackage<Vertex::Skeleton> skeletonGeometry;
            SkeletonPackage skeleton;
            std::deque<EcsRequest::CreateMultiMesh> pendingMultiMeshCreates;
            
            static void beginRendering(const Command& cmd);
            static void endRendering(const Command& cmd);
            void beginDepthRendering(const Command& cmd);
            void endDepthRendering(const Command& cmd);
            
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
