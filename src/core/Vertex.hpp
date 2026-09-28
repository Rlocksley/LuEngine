#pragma once
#include "Global.hpp"
#include <vector>

namespace Lu
{
    namespace Vertex
    {

        struct Mesh{
            glm::vec3 position;
            glm::vec3 normal;
            glm::vec4 color;
            glm::vec2 texCoord;

            static VkVertexInputBindingDescription getBindingDescription() {
                VkVertexInputBindingDescription bindingDescription{};
                bindingDescription.binding = 0;
                bindingDescription.stride = sizeof(Mesh);
                bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                return bindingDescription;
            }

            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
                std::vector<VkVertexInputAttributeDescription> attributeDescriptions(4);
                
                attributeDescriptions[0].binding = 0;
                attributeDescriptions[0].location = 0;
                attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
                attributeDescriptions[0].offset = offsetof(Vertex::Mesh, position);
                
                attributeDescriptions[1].binding = 0;
                attributeDescriptions[1].location = 1;
                attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
                attributeDescriptions[1].offset = offsetof(Vertex::Mesh, normal);
                
                attributeDescriptions[2].binding = 0;
                attributeDescriptions[2].location = 2;
                attributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
                attributeDescriptions[2].offset = offsetof(Vertex::Mesh, color);
                
                attributeDescriptions[3].binding = 0;
                attributeDescriptions[3].location = 3;
                attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
                attributeDescriptions[3].offset = offsetof(Vertex::Mesh, texCoord);
                
                
                return attributeDescriptions;
            }

            static std::vector<VkVertexInputAttributeDescription> getDepthAttributeDescriptions() {
                VkVertexInputAttributeDescription vertexAttributeDescr{};
                vertexAttributeDescr.binding = 0;
                vertexAttributeDescr.location = 0;
                vertexAttributeDescr.format = VK_FORMAT_R32G32B32_SFLOAT;
                vertexAttributeDescr.offset = offsetof(Vertex::Mesh, position);

                return {vertexAttributeDescr};
            }
        };

        struct Skeleton
        {
            glm::vec3 position;
            glm::vec3 normal;
            glm::vec4 color;
            glm::vec2 texCoord;
            glm::ivec4 bones = glm::ivec4(0);
            glm::vec4 weights = glm::vec4(0.0f);
            int nodeIndex = -1;

            static VkVertexInputBindingDescription getBindingDescription() {
                VkVertexInputBindingDescription bindingDescription{};
                bindingDescription.binding = 0;
                bindingDescription.stride = sizeof(Skeleton);
                bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                return bindingDescription;
            }

            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
                std::vector<VkVertexInputAttributeDescription> attributeDescriptions(7);
                
                attributeDescriptions[0].binding = 0;
                attributeDescriptions[0].location = 0;
                attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
                attributeDescriptions[0].offset = offsetof(Skeleton, position);
                
                attributeDescriptions[1].binding = 0;
                attributeDescriptions[1].location = 1;
                attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
                attributeDescriptions[1].offset = offsetof(Skeleton, normal);

                attributeDescriptions[2].binding = 0;
                attributeDescriptions[2].location = 2;
                attributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
                attributeDescriptions[2].offset = offsetof(Skeleton, color);
                
                attributeDescriptions[3].binding = 0;
                attributeDescriptions[3].location = 3;
                attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
                attributeDescriptions[3].offset = offsetof(Skeleton, texCoord);
                
                attributeDescriptions[4].binding = 0;
                attributeDescriptions[4].location = 4;
                attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SINT;
                attributeDescriptions[4].offset = offsetof(Skeleton, bones);
                
                attributeDescriptions[5].binding = 0;
                attributeDescriptions[5].location = 5;
                attributeDescriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
                attributeDescriptions[5].offset = offsetof(Skeleton, weights);
                
                attributeDescriptions[6].binding = 0;
                attributeDescriptions[6].location = 6;
                attributeDescriptions[6].format = VK_FORMAT_R32_SINT;
                attributeDescriptions[6].offset = offsetof(Skeleton, nodeIndex);
                
                return attributeDescriptions;
            }

            static std::vector<VkVertexInputAttributeDescription> getDepthAttributeDescriptions() {
                std::vector<VkVertexInputAttributeDescription> attrs(4);

                attrs[0].binding = 0;
                attrs[0].location = 0;
                attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
                attrs[0].offset = offsetof(Skeleton, position);

                attrs[1].binding = 0;
                attrs[1].location = 1;
                attrs[1].format = VK_FORMAT_R32G32B32A32_SINT;
                attrs[1].offset = offsetof(Skeleton, bones);

                attrs[2].binding = 0;
                attrs[2].location = 2;
                attrs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
                attrs[2].offset = offsetof(Skeleton, weights);

                attrs[3].binding = 0;
                attrs[3].location = 3;
                attrs[3].format = VK_FORMAT_R32_SINT;
                attrs[3].offset = offsetof(Skeleton, nodeIndex);

                return attrs;
            }
        };
    }
}