#pragma once

#include "UICore.hpp"

namespace Lu {
    namespace UI {

        // Tabs: every child entity with a TabPage is a page. The module creates one tab Button per page (title) and shows only
        // the selected page; pages are placed below the tab row and get the rest of Node::size.
        struct TabView {
            int selected{0};
            float tabHeight{36.0f};
            float tabWidth{140.0f};
            std::function<void(flecs::entity, int)> onChange{};
        };

        struct TabPage {
            std::string title;
            int order{0};                // pages are sorted by order, then by entity id
        };
    }
}
