#pragma once

#include "Global.hpp"
#include "Channel.hpp"
#include "flecs.h"

namespace Lu{
    namespace Component{

        // Playback state of a registered video. Lives on the VIDEO entity (App::registerVideo creates it with the values of
        // the VideoConfig), so every surface that shows this video is paused / sped up together.
        //
        //      world.entity("Video::Intro").set<Component::VideoPlayback>({.playing = false});    // pause
        //      world.entity("Video::Intro").set<Component::VideoPlayback>({.playing = true});     // resume (restarts a finished video)
        //
        // Every set / modified is sent to the renderer as EcsRequest::UpdateVideo.
        struct VideoPlayback{
            bool playing{true};
            bool loop{true};
            float speed{1.0f};      // 1 = real time
        };

        // THE component to show a video: add it to a Component::Mesh2D entity (GUI / HUD / screen) or to a 3D
        // Component::Mesh entity (+ Component::Material) and that surface displays the video.
        //
        //      App(...).registerVideo("Video::Intro", "intro.mpg")
        //
        //      world.entity("Screen").child_of(panel)                    // panel: Transform2D + Transform2DGpu
        //          .set(Component::Mesh2D{.mesh = ..., .pipeline = ...})
        //          .set(Component::VideoTexture{.video = world.lookup("Video::Intro")});
        //
        // Mesh2D:  replaces Mesh2D::texture. Mesh2D::color still tints, Mesh2D::uvRect still selects the sub rectangle
        //          (uvRect = {0, 1, 1, -1} flips it vertically).
        // Mesh:    the video becomes the EMISSIVE texture of the Material (a lit screen / TV that glows): Material::emission
        //          is set to `emission`, the other Material fields are untouched, so albedo / ambient / textures[0..3] still apply.
        //          The video follows the UVs of the geometry.
        struct VideoTexture{
            flecs::entity video;        // registered with App::registerVideo
            float emission{1.0f};       // 3D meshes only: brightness of the screen (Material::emission = vec3(emission)), 0 = keep Material::emission
        };

        // The texture entity of a video, "<video name>::Texture" (created by App::registerVideo). Invalid if the video is unknown.
        inline flecs::entity videoTextureEntity(const VideoTexture& videoTexture){
            if(!videoTexture.video.is_valid()) return flecs::entity{};
            return videoTexture.video.lookup("Texture");
        }
    }

    namespace Module{
        struct Video{
            Video(flecs::world& world){
                world.component<Component::VideoPlayback>();
                world.component<Component::VideoTexture>();

                world.observer<const Component::VideoPlayback>()
                    .event(flecs::OnSet)
                    .each([](flecs::entity e, const Component::VideoPlayback& playback) {
                        GetChannel().send(EcsRequest::UpdateVideo{e.id(), playback.playing, playback.loop, playback.speed});
                    });
            }
        };
    }
}
