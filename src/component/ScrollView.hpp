#pragma once

#include "UICore.hpp"

namespace Lu {
    namespace UI {

        // Vertical scroll area: it is a vertical Layout of its child nodes (add a Layout to configure spacing / padding,
        // the default one is used otherwise) shifted by `scroll`. The mouse wheel and the ScrollBar child (created by the
        // module) change `scroll`. Node::size of this entity is the viewport.
        //
        // Clipping: with `clip` on and the two clipping pipelines registered (Mesh2DPipe::UIClip with shader/mesh2d_clip.*,
        // FontPipe::UIClip with shader/font_clip.*; names in Style) everything inside the view that sticks out of the
        // viewport is cut on the GPU: a child that is partly visible is drawn partly. Without the clipping pipelines (or
        // with clip = false) there is no way to cut: children that are not completely inside the viewport get the Clipped
        // tag and are not drawn. Clicks outside of the viewport never reach the children.
        // (Nested scroll views: the innermost one clips.)
        struct ScrollView {
            float scroll{0.0f};          // pixels
            float wheelSpeed{48.0f};
            bool clip{true};
            bool scrollbar{true};
            float scrollbarWidth{14.0f};
            float contentHeight{0.0f};   // written by the module
        };
    }
}
