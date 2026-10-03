#pragma once

#include "Global.hpp"
#include "Constant.hpp"
#include "Buffer.hpp"
#include "Command.hpp"
#include "flecs.h"
#include "Vertex.hpp"

namespace Lu{
    namespace Core{
     
        
        using GeometryInfoId = uint32_t;

        struct GeometryInfo {
            uint32_t indexCount;
            uint32_t firstIndex;
            uint32_t vertexCount;
            int32_t  vertexOffset;
            glm::vec4 boundingSphere; // xyz = local center, w = radius
        };

        template<typename Vertex>
        struct GeometryPackage  {
        private:

            BufferRanges<Vertex> vertexBuffer;
            BufferRanges<uint32_t> indexBuffer;

            BufferInterface<Vertex> vertexInterface;
            BufferInterface<uint32_t> indexInterface;
            BufferGpuIndexed<GeometryInfo> infoBuffer;
            using GeometryEntity = flecs::entity_t;
            std::unordered_map<GeometryEntity, GeometryInfoId> entityToGeometryInfoId;
            std::vector<GeometryInfo> geometryInfos;

            Command command;
            
        public:
                GeometryPackage(uint32_t vertexBufferSize, uint32_t indexBufferSize,
                        uint32_t maxSubVertexBufferSize, uint32_t maxSubIndexBufferSize,
                        uint32_t infoBufferSize)
                        : vertexBuffer(vertexBufferSize,
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                            indexBuffer(indexBufferSize,
                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                            vertexInterface(maxSubVertexBufferSize,
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                            indexInterface(maxSubIndexBufferSize,
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                            command(),
                            infoBuffer(infoBufferSize,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
                {}

                void addGeometry(
                    const flecs::entity_t& entity,
                        const std::vector<Vertex>& vertices,
                        const std::vector<uint32_t>& indices){
                    LU_ASSERT(entityToGeometryInfoId.find(entity) == entityToGeometryInfoId.end(),
                        "GeometryPackage", "addGeometry", "Entity already has geometry.")
                    LU_ASSERT(!vertices.empty(), "GeometryPackage", "addGeometry", "Vertex list is empty.")
                    LU_ASSERT(!indices.empty(), "GeometryPackage", "addGeometry", "Index list is empty.")
                        LU_ASSERT(vertices.size() <= vertexInterface.size,
                        "GeometryPackage", "addGeometry", "Vertex staging capacity exceeded.")
                        LU_ASSERT(indices.size() <= indexInterface.size,
                        "GeometryPackage", "addGeometry", "Index staging capacity exceeded.")

                        const auto vertexRange = vertexBuffer.allocate(static_cast<uint32_t>(vertices.size()));
                        const auto indexRange = indexBuffer.allocate(static_cast<uint32_t>(indices.size()));
                        const GeometryInfoId infoId = infoBuffer.allocate();

                        GeometryInfo info{};
                        info.indexCount = indexRange.size;
                        info.firstIndex = indexRange.offset;
                        info.vertexCount = vertexRange.size;
                        info.vertexOffset = static_cast<int32_t>(vertexRange.offset);
                        if constexpr (requires(const Vertex& vertex) { vertex.position; }) {
                            glm::vec3 minPosition = vertices.front().position;
                            glm::vec3 maxPosition = minPosition;
                            for(const Vertex& vertex : vertices){
                                minPosition = glm::min(minPosition, vertex.position);
                                maxPosition = glm::max(maxPosition, vertex.position);
                            }
                            const glm::vec3 center = (minPosition + maxPosition) * 0.5f;
                            float radius = 0.0f;
                            for(const Vertex& vertex : vertices){
                                radius = glm::max(radius, glm::distance(vertex.position, center));
                            }
                            info.boundingSphere = glm::vec4(center, radius);
                        }

                        std::memcpy(vertexInterface.pMemory, vertices.data(), sizeof(Vertex) * vertices.size());
                        std::memcpy(indexInterface.pMemory, indices.data(), sizeof(uint32_t) * indices.size());

                        command.begin();
                        vertexInterface.recordCopyTo(vertexBuffer, vertexRange.size, command,
                                0, vertexRange.offset);
                        indexInterface.recordCopyTo(indexBuffer, indexRange.size, command,
                                0, indexRange.offset);
                        infoBuffer.upload(infoId, info, command);
                        command.end();
                        command.submit();
                        command.waitForFence();

                        if(infoId >= geometryInfos.size()) geometryInfos.resize(std::size_t{infoId} + 1);
                        geometryInfos[infoId] = info;

                        entityToGeometryInfoId.emplace(entity, infoId);
                }

                const GeometryInfoId getGeometryInfoId(GeometryEntity entity) const {
                    auto it = entityToGeometryInfoId.find(entity);
                    LU_ASSERT(it != entityToGeometryInfoId.end(), "GeometryPackage", "getGeometryInfoId", "No GeometryInfoId for given GeometryEntity");
                    return it->second;
                }

                const GeometryInfo& getGeometryInfo(GeometryInfoId id) const {
                    LU_ASSERT(id < geometryInfos.size(), "GeometryPackage", "getGeometryInfo", "GeometryInfoId is out of range.")
                    return geometryInfos[id];
                }

                const BufferGpuIndexed<GeometryInfo>& getGeometryInfoBuffer() const { return infoBuffer; }

                const VkBuffer& getVertexBuffer() const { return vertexBuffer.vkBuffer; }
                const VkBuffer& getIndexBuffer() const { return indexBuffer.vkBuffer; }
        };

    }
}