#pragma once
#include "Global.hpp"
namespace Lu{
    namespace Core{
        const uint32_t MAX_FRAMES_PER_SECOND_ECS = 240; 
        const uint32_t MAX_FRAMES_IN_FLIGHT = 2;
        const uint32_t MAX_MESH_INFOS = 1024;
        const uint32_t MAX_MESH_VERTEX_BUFFER_SIZE = 1024 * 1024;
        const uint32_t MAX_MESH_INDEX_BUFFER_SIZE = 1024 * 1024;
        const uint32_t MAX_VERTICES_PER_MESH = 1024 * 512;
        const uint32_t MAX_INDICES_PER_MESH = 1024 * 512;
        const uint32_t MAX_ECS_REQUESTS_PROCESSED_PER_FRAME = 1024; 
        const uint32_t MAX_ECS_REQUEST_CREATE_MULTI_MESH_PER_FRAME = 5;
        const uint32_t MAX_TRANSFORMS = 1024 * 256;
        const uint32_t MAX_MESHES = 1024 * 256;
        const uint32_t MAX_MULTI_MESHES = 1024 * 2;
        const uint32_t MAX_MESH_PIPELINES = 256;
        const uint32_t MAX_MULTI_MESH_PIPELINES = 256;
        const uint32_t MAX_MULTI_MESH_COMPUTE_PIPELINES = 256;
        const uint32_t MAX_MULTI_MESH_INSTANCES = 10000;
        const uint32_t MAX_MULTI_MESH_UPLOAD_INSTANCES =
                        MAX_MULTI_MESH_INSTANCES * 
                        MAX_ECS_REQUEST_CREATE_MULTI_MESH_PER_FRAME * //check if this is correct 
                        MAX_FRAMES_IN_FLIGHT;
        const uint32_t MAX_TEXTURES = 256;
        const uint32_t MAX_LIGHTS = 1024;

        // ---- Skeletons ----------------------------------------------------
        // careful: MAX_BONES_PER_SKELETON must match the shaders
        // (skeleton_pose.comp, skeleton_basic.vert, skeleton_depth.vert, skeleton_cull.comp)
        const uint32_t MAX_BONES_PER_SKELETON = 128;          // nodes (bones + attach nodes) per skeleton asset
        const uint32_t MATS_PER_SKELETON = 2 * MAX_BONES_PER_SKELETON; // [skin matrices | node matrices]
        const uint32_t MAX_SKELETON_ASSETS = 256;
        const uint32_t MAX_SKELETONS = 4096;                  // live skeleton instances
        const uint32_t MAX_SKELETON_POSES_PER_FRAME = MAX_SKELETONS * 64; // node poses uploaded per frame
        const uint32_t MAX_SKELETON_MESHES = 1024 * 64;
        const uint32_t MAX_SKELETON_MESH_INFOS = 1024;
        const uint32_t MAX_SKELETON_VERTEX_BUFFER_SIZE = 1024 * 512;
        const uint32_t MAX_SKELETON_INDEX_BUFFER_SIZE = 1024 * 1024;
        const uint32_t MAX_SKELETON_MESH_PIPELINES = 64;


        //careful: 
        //for this constants 
        //you also have to change 
        //the constants in the shaders
        const uint32_t TILE_WIDTH = 16;
        const uint32_t TILE_HEIGHT = 16;
        const uint32_t MAX_LIGHTS_PER_TILE = 60;
    }
}