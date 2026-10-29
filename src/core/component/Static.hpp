#pragma once

namespace Lu {
    namespace Component {
        // Tag: "not updated every frame". Used by the transform systems:
        //   * the WorldTransform / WorldTransform2D of a Static entity is not recomputed every frame by the propagation
        //     system and not streamed over the triple buffer every frame.
        //   * it is still updated when the Transform is set again (or the Transform of an ancestor is set, or the entity is
        //     re-parented): the new WorldTransform is then sent once through EcsRequest::CreateTransform.
        // (A Static entity can be the parent of non-Static entities, and the other way round.)
        struct Static{};
    }
}
