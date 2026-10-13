#pragma once

#include "Global.hpp"
#include "Constant.hpp"
#include "Buffer.hpp"
#include "Command.hpp"
#include "flecs.h"
#include "Vertex.hpp"
#include <optional>

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

            BufferGpuIndexed<GeometryInfo> infoBuffer;
            using GeometryEntity = flecs::entity_t;
            std::unordered_map<GeometryEntity, GeometryInfoId> entityToGeometryInfoId;
            std::vector<GeometryInfo> geometryInfos;
            using VertexRange = typename BufferRanges<Vertex>::Range;
            using IndexRange = typename BufferRanges<uint32_t>::Range;
            struct GeometryCopy {
                VertexRange vertexRange;
                IndexRange indexRange;
                std::vector<Vertex> vertices;
                std::vector<uint32_t> indices;
            };
            std::vector<GeometryCopy> pendingGeometryCopies;
            struct StagingBatch {
                BufferInterface<Vertex> vertexInterface;
                BufferInterface<uint32_t> indexInterface;
                BufferInterface<GeometryInfo> infoInterface;

                StagingBatch(uint32_t vertexCount, uint32_t indexCount, uint32_t infoCount)
                    : vertexInterface(vertexCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                      indexInterface(indexCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                      infoInterface(infoCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT) {}
            };
            std::array<std::optional<StagingBatch>, MAX_FRAMES_IN_FLIGHT> frameStaging;
            uint32_t maxSubVertexBufferSize;
            uint32_t maxSubIndexBufferSize;
            uint32_t uploadedGeometryInfoCount{0};
            
        public:
                GeometryPackage(uint32_t vertexBufferSize, uint32_t indexBufferSize,
                        uint32_t maxSubVertexBufferSize, uint32_t maxSubIndexBufferSize,
                        uint32_t infoBufferSize)
                        : vertexBuffer(vertexBufferSize,
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                            indexBuffer(indexBufferSize,
                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                            infoBuffer(infoBufferSize,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                            maxSubVertexBufferSize(maxSubVertexBufferSize),
                            maxSubIndexBufferSize(maxSubIndexBufferSize)
                {}

                void addGeometry(
                    const flecs::entity_t& entity,
                        const std::vector<Vertex>& vertices,
                        const std::vector<uint32_t>& indices){
                    LU_ASSERT(entityToGeometryInfoId.find(entity) == entityToGeometryInfoId.end(),
                        "GeometryPackage", "addGeometry", "Entity already has geometry.")
                    LU_ASSERT(!vertices.empty(), "GeometryPackage", "addGeometry", "Vertex list is empty.")
                    LU_ASSERT(!indices.empty(), "GeometryPackage", "addGeometry", "Index list is empty.")
                        LU_ASSERT(vertices.size() <= maxSubVertexBufferSize,
                        "GeometryPackage", "addGeometry", "Vertex staging capacity exceeded.")
                        LU_ASSERT(indices.size() <= maxSubIndexBufferSize,
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

                        if(infoId >= geometryInfos.size()) geometryInfos.resize(std::size_t{infoId} + 1);
                        geometryInfos[infoId] = info;
                        pendingGeometryCopies.push_back({
                            vertexRange, indexRange, vertices, indices
                        });

                        entityToGeometryInfoId.emplace(entity, infoId);
                }

                template<typename Cmd>
                void copy(uint32_t frameIndex, const Cmd& cmd) {
                    LU_ASSERT(frameIndex < MAX_FRAMES_IN_FLIGHT,
                        "GeometryPackage", "recordCopies", "Frame index is out of range.")
                    frameStaging[frameIndex].reset();

                    const uint32_t geometryInfoCount = static_cast<uint32_t>(geometryInfos.size());
                    const uint32_t newGeometryInfoCount =
                        geometryInfoCount - uploadedGeometryInfoCount;
                    if(pendingGeometryCopies.empty() && newGeometryInfoCount == 0) return;

                    uint32_t vertexCount = 0;
                    uint32_t indexCount = 0;
                    for(const GeometryCopy& copy : pendingGeometryCopies) {
                        vertexCount += copy.vertexRange.size;
                        indexCount += copy.indexRange.size;
                    }

                    frameStaging[frameIndex].emplace(
                        vertexCount, indexCount, newGeometryInfoCount);
                    StagingBatch& staging = *frameStaging[frameIndex];

                    uint32_t vertexSourceOffset = 0;
                    uint32_t indexSourceOffset = 0;
                    for(const GeometryCopy& copy : pendingGeometryCopies) {
                        std::memcpy(staging.vertexInterface.pMemory + vertexSourceOffset,
                            copy.vertices.data(), sizeof(Vertex) * copy.vertices.size());
                        std::memcpy(staging.indexInterface.pMemory + indexSourceOffset,
                            copy.indices.data(), sizeof(uint32_t) * copy.indices.size());
                        staging.vertexInterface.recordCopyTo(vertexBuffer,
                            copy.vertexRange.size, cmd, vertexSourceOffset,
                            copy.vertexRange.offset);
                        staging.indexInterface.recordCopyTo(indexBuffer,
                            copy.indexRange.size, cmd, indexSourceOffset,
                            copy.indexRange.offset);
                        vertexSourceOffset += copy.vertexRange.size;
                        indexSourceOffset += copy.indexRange.size;
                    }

                    if(newGeometryInfoCount > 0) {
                        std::memcpy(staging.infoInterface.pMemory,
                            geometryInfos.data() + uploadedGeometryInfoCount,
                            sizeof(GeometryInfo) * newGeometryInfoCount);
                        infoBuffer.recordCopyFrom(staging.infoInterface, 0,
                            uploadedGeometryInfoCount, newGeometryInfoCount, cmd);
                    }

                    uploadedGeometryInfoCount = geometryInfoCount;
                    pendingGeometryCopies.clear();
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