#pragma once

#include "component/Transform2D.hpp"
#include "UICore.hpp"

namespace Lu {
    namespace Component {

        enum class UIButtonMode : uint8_t { Momentary, Toggle, Radio, Hold };

        struct UIButton {
            flecs::entity mesh;
            flecs::entity meshPipeline;
            flecs::entity mesh2;
            flecs::entity meshPipeline2;
            flecs::entity font;
            flecs::entity fontPipeline;
            flecs::entity texture;
            glm::vec4 uvRect{0.f, 0.f, 1.f, 1.f};
            std::string text;
            glm::vec4 color{1.f, 1.f, 1.f, 1.f};        
            glm::vec4 textColor{1.f, 1.f, 1.f, 1.f}; 
            float fontSize{-1.0f};      
            glm::vec4 meshParams{0.f, 0.f, 0.f, 0.f};
            glm::vec4 mesh2Param{0.f, 0.f, 0.f, 0.f};
            glm::vec4 mesh2Color{0.f, 0.f, 0.f, 0.f};
            float lineSpacing{0.f}; 
            float maxWidth{0}; // 0 = no word wrapping
            Lu::TextAlign align{Lu::TextAlign::Center};
           
            Transform2D textTransform; // relative to the button's Transform2D;
           
            bool selected{false};
            bool pressed{false};
            bool enabled{true};
            UIButtonMode mode{UIButtonMode::Momentary};
            uint32_t radioGroup{0};
            glm::vec2 size{160.f, 48.f};
           
            std::function<void(flecs::entity)> onClicked{};
            std::function<void(flecs::entity)> onHover{};
            std::function<void(flecs::entity)> onHoverEnter{};
            std::function<void(flecs::entity)> onHoverExit{};
            std::function<void(flecs::entity)> onRelease{};
        };
    }
}
