#pragma once

#include "Global.hpp"
#include "flecs.h"

namespace Lu{
    namespace Component{

        // A 2D mesh (GUI panel, button background, icon, ...). Put it on a CHILD of an entity that has a
        // Component::Transform2D AND a Component::Transform2DGpu, exactly like Component::Text:
        //
        //      auto panel = world.entity("Panel").set<Component::Transform2D>({.position = {20.f, 20.f}, .positionZ = 1})
        //                                         .add<Component::Transform2DGpu>();
        //      world.entity("PanelMesh").child_of(panel).set(Component::Mesh2D{
        //          .mesh = world.lookup("Mesh2D::Rect"), .pipeline = world.lookup("Mesh2DPipe::Basic"),
        //          .color = glm::vec4(0.2f, 0.2f, 0.25f, 1.f)});
        //
        // The vertices of the mesh are in local pixels, the Transform2D (position, positionZ = layer, rotation, scale)
        // and the Camera2D place them on the screen. Changing any field (set / modified) sends a new
        // EcsRequest::CreateMesh2D, which updates the mesh on the renderer side.
        struct Mesh2D{
            flecs::entity mesh;             // registered with App::registerMesh2D / App::registerShape2D
            flecs::entity pipeline;         // registered with App::registerMesh2DPipe
            flecs::entity texture;          // optional (App::registerTexture), none = untextured
            glm::vec4 color{1.f, 1.f, 1.f, 1.f};   // tint: multiplied with the vertex color (and the texture)
            glm::vec4 uvRect{0.f, 0.f, 1.f, 1.f};  // uv = uvRect.xy + vertexUv * uvRect.zw (sub rectangle of the texture)
            glm::vec4 params{0.f, 0.f, 0.f, 0.f};  // free for your own shaders (corner radius, border width, ...)
        };

        struct Mesh2DGpuDirty{};
    }
}
