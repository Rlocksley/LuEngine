#pragma once

#include "UICore.hpp"

namespace Lu {
    namespace UI {

        // Vertical scroll area: it is a vertical Layout of its child nodes (add a Layout to configure spacing / padding,
        // the default one is used otherwise) shifted by `scroll`. The mouse wheel and a Slider child (scroll bar,
        // created by the module) change `scroll`. There is no clipping on the GPU side: children that are not fully
        // inside the viewport (Node::size of this entity) get the Clipped tag and are not drawn.
        struct ScrollView {
            float scroll{0.0f};          // pixels
            float wheelSpeed{48.0f};
            bool scrollbar{true};
            float scrollbarWidth{14.0f};
            float contentHeight{0.0f};   // written by the module
        };
    }
}
