#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "component/Transform.hpp"
#include "component/Material.hpp"
#include "component/MultiMesh.hpp"
#include "component/Light.hpp"
#include "CameraData.hpp"

namespace Lu{

    namespace EcsRequest{
        struct CreateTransform{
            flecs::entity_t entity;            
            Component::Transform transform;
        };

        struct UpdateTransform{
            flecs::entity_t entity;            
            Component::Transform transform;
        };

        struct DestroyTransform{
            flecs::entity_t entity;            
        };

        // MeshTransform of an entity: gets its own slot in the GPU transform buffer.
        struct CreateMeshTransform{
            flecs::entity_t entity;            
            Component::MeshTransform transform;
        };

        struct UpdateMeshTransform{
            flecs::entity_t entity;            
            Component::MeshTransform transform;
        };

        struct DestroyMeshTransform{
            flecs::entity_t entity;            
        };

        struct Material{
            flecs::entity_t textures[5]{};
            glm::vec4 albedo{1.f, 1.f, 1.f, 1.f};
            glm::vec4 ambient{0.f, 0.f, 0.f, 0.f};
            glm::vec4 emission{0.f,0.f,0.f,0.f};
            float roughness{0.f};
            float metallic{0.f};

            Material(const Component::Material& material):
            albedo(material.albedo),
            ambient(material.ambient),
            emission(material.emission),
            roughness(material.roughness),
            metallic(material.metallic)
            {
                for(uint32_t i = 0; i < 5; i++){
                    textures[i] = material.textures[i].id();
                }
            }
        };

        struct CreateMesh{
            flecs::entity_t parent;
            flecs::entity_t entity;            
            flecs::entity_t mesh;            
            flecs::entity_t pipe;
            Material material;
        };


        struct DestroyMesh{
            flecs::entity_t entity;            
        };

        struct CreateMultiMesh{
            flecs::entity_t parent;
            flecs::entity_t entity;            
            flecs::entity_t mesh;            
            flecs::entity_t computePipe;
            flecs::entity_t pipe;
            glm::vec4 cullSphere;
            std::vector<Component::MultiMeshInstance> instances;
        };

        struct DestroyMultiMesh{
            flecs::entity_t entity;            
        };

        struct CreateLight{
            flecs::entity_t parent;
            flecs::entity_t entity;
            Component::Light light;
        };

        struct DestroyLight{
            flecs::entity_t entity;
        };

        struct UpdateCamera{
            CameraData cameraData;
        };

        // ---- skeletons ------------------------------------------------------------
        // CreateSkeleton / CreateSkeletonMesh are "upsert": the first request creates the
        // GPU object, later requests for the same entity update it (new asset, material,
        // pipeline, attach node, ...). Poses are NOT sent as requests but through the
        // skeleton pose triple buffer (see TripleBuffers.hpp) as UpdateSkeletonPose headers.
        struct CreateSkeleton{
            flecs::entity_t entity;     // parent entity (owns Transform + Skeleton)
            flecs::entity_t asset;      // skeleton asset entity (registered by loadSkeletonModel)
        };

        struct DestroySkeleton{
            flecs::entity_t entity;
        };

        struct CreateSkeletonMesh{
            flecs::entity_t parent;     // entity with Transform + Skeleton
            flecs::entity_t entity;
            flecs::entity_t mesh;       // skeleton geometry entity
            flecs::entity_t pipe;
            Material material;
            uint32_t nodeIndex;         // UINT32_MAX = deformed, otherwise attached to that node
        };

        struct DestroySkeletonMesh{
            flecs::entity_t entity;
        };

        // Header of one skeleton pose inside a SkeletonPoseSnapshot.
        struct UpdateSkeletonPose{
            flecs::entity_t entity{0};
            uint32_t firstPose{0};
            uint32_t nodeCount{0};
        };

        using EcsRequest = std::variant<CreateTransform, DestroyTransform, CreateMeshTransform, DestroyMeshTransform, CreateMesh, DestroyMesh, CreateMultiMesh, DestroyMultiMesh, CreateLight, DestroyLight, UpdateCamera,
            CreateSkeleton, DestroySkeleton, CreateSkeletonMesh, DestroySkeletonMesh>;

    }

    class Channel{
        private:
            std::vector<EcsRequest::EcsRequest> requestQueue;
            std::mutex queueMutex;

        public:
            void send(const EcsRequest::EcsRequest& request){
                std::lock_guard<std::mutex> lock(queueMutex);
                requestQueue.push_back(request);
            }

            void send(EcsRequest::EcsRequest&& request){
                std::lock_guard<std::mutex> lock(queueMutex);
                requestQueue.push_back(std::move(request));
            }

            void send(const std::vector<EcsRequest::EcsRequest>& requests){
                std::lock_guard<std::mutex> lock(queueMutex);
                requestQueue.reserve(requestQueue.size() + requests.size());
                requestQueue.insert(requestQueue.end(), requests.begin(), requests.end());
            }

            void send(std::vector<EcsRequest::EcsRequest>&& requests){
                std::lock_guard<std::mutex> lock(queueMutex);
                requestQueue.reserve(requestQueue.size() + requests.size());
                requestQueue.insert(requestQueue.end(),
                    std::make_move_iterator(requests.begin()),
                    std::make_move_iterator(requests.end()));
            }

            std::vector<EcsRequest::EcsRequest> drain(uint32_t maxRequests){
                std::vector<EcsRequest::EcsRequest> requests;
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    const size_t requestCount = requestQueue.size() < maxRequests
                        ? requestQueue.size()
                        : maxRequests;
                    requests.reserve(requestCount);
                    for(size_t index = 0; index < requestCount; ++index){
                        requests.push_back(std::move(requestQueue[index]));
                    }
                    requestQueue.erase(requestQueue.begin(), requestQueue.begin() + requestCount);
                }
                return requests;
            }
    };

    inline Channel& GetChannel(){
        static Channel channel;
        return channel;
    }
}
