#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "component/Transform.hpp"
#include "component/Material.hpp"
#include "component/MultiMesh.hpp"
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

        struct CreateMesh{
            flecs::entity_t parent;
            flecs::entity_t entity;            
            flecs::entity_t mesh;            
            flecs::entity_t pipe;
            Component::Material material;
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

        struct UpdateCamera{
            CameraData cameraData;
        };

        using EcsRequest = std::variant<CreateTransform, DestroyTransform, CreateMesh, DestroyMesh, CreateMultiMesh, DestroyMultiMesh, UpdateCamera>;

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