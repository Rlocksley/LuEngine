#pragma once
#include "Global.hpp"
namespace Lu{
    namespace Core{
        const uint32_t MAX_FRAMES_PER_SECOND_RENDERER = 120;
        const uint32_t MAX_FRAMES_PER_SECOND_ECS = 120; 
        const uint32_t MAX_FRAMES_IN_FLIGHT = 2;
        const uint32_t MAX_MESH_INFOS = 1024;
        const uint32_t MAX_MESH_VERTEX_BUFFER_SIZE = 1024 * 1024;
        const uint32_t MAX_MESH_INDEX_BUFFER_SIZE = 1024 * 1024;
        const uint32_t MAX_VERTICES_PER_MESH = 1024 * 512;
        const uint32_t MAX_INDICES_PER_MESH = 1024 * 512;
        const uint32_t MAX_ECS_REQUESTS_PROCESSED_PER_FRAME = 1024; 
        const uint32_t MAX_TRANSFORMS = 1024 * 256;
        const uint32_t MAX_MESHES = 1024 * 256;
        const uint32_t MAX_MESH_PIPELINES = 256;
    }
}