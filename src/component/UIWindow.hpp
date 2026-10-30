#pragma once

#include "UICore.hpp"
#include "UIDisplay.hpp"

namespace Lu {
    namespace UI {

        // Window: Panel + title bar + title + close button + drag handle. A window is also a Panel (the module adds one if
        // the entity has none, so you can configure its colors with a Panel component). Put the content as
        // children of the window entity; their position is relative to the top-left corner of the window, the title bar
        // occupies the first titleHeight pixels. Clicking inside raises the window above its sibling windows.
        struct Window {
            std::string title;
            float titleHeight{36.0f};
            bool draggable{true};
            bool closable{true};
            bool destroyOnClose{false};  // false: the window gets the Hidden tag
            float fontSize{-1.0f};
            std::function<void(flecs::entity)> onClose{};
        };
    }
}
