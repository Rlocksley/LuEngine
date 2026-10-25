#pragma once
#include "Global.hpp"
#include "flecs.h"

namespace Lu{
    namespace Component{
        // Tag: this Transform2D is mirrored by the GPU (Transform2DPackage). Same idea as TransformGpu.
        // Only the Transform2Ds that are really needed by the renderer carry it, i.e. the lowest Transform2D of a hierarchy:
        // the parent of an entity with a Text. Transform2Ds higher up in the hierarchy are plain CPU data that a
        // flecs system propagates to their children (world.system<...>().cascade().run(...)).
        struct Transform2DGpu{};

        struct Transform2DGpuDirty{};
    }
}
