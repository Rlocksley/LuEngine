#include "App.hpp"
#include "Shape2D.hpp"
#include "component/Transform2D.hpp"
#include "component/Transform2DGpu.hpp"
#include "component/Mesh2D.hpp"
#include "component/Text.hpp"
#include "Button.hpp"
#include "TextInput.hpp"
#include "UIModule.hpp"
#include <filesystem>
#include <iostream>

// =============================================================================
// main_ui: Demonstrates UI Button (press/hover) and TextInput components & module.
// =============================================================================

static std::string findFont(int argc, char** argv) {
    std::vector<std::string> candidates;
    if (argc > 1) candidates.push_back(argv[1]);
    candidates.insert(candidates.end(), {
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf"
    });
    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) return path;
    }
    LU_LOGE("main_ui", "findFont", "No font found. Pass a .ttf file as the first argument.");
    return {};
}

struct UIDemoModule {
    explicit UIDemoModule(flecs::world& world) {
        // 1. Window Background Panel
        world.entity("Panel::Mesh")
            .set(flecs::Parent{
                world.entity("Panel")
                .set<Lu::Component::Transform2D>({.position = {50.f, 40.f}, .positionZ = 1})
                .add<Lu::Component::Transform2DGpu>()
            })
            .set(Lu::Component::Mesh2D{
                .mesh = world.lookup("Mesh2D::Panel"),
                .pipeline = world.lookup("Mesh2DPipe::Basic"),
                .color = glm::vec4(0.12f, 0.13f, 0.17f, 1.0f)
            });

        // Title
        world.entity("Title::Text")
            .set(flecs::Parent{
                world.entity("Title")
                .set<Lu::Component::Transform2D>({.position = {70.f, 55.f}, .positionZ = 3})
                .add<Lu::Component::Transform2DGpu>()
            })
            .set(Lu::Component::Text{
                .text = "LuEngine UI Module Demo (Button & TextInput)",
                .font = world.lookup("Font::Main"),
                .pipeline = world.lookup("FontPipe::Basic"),
                .color = glm::vec4(0.95f, 0.95f, 0.95f, 1.0f),
                .size = 24.f,
                .align = Lu::TextAlign::Left
            });

        // 2. Status Label entity (will update when typing / clicking)
        auto statusLabel = world.entity("StatusLabel::Text")
            .set(flecs::Parent{
                world.entity("StatusLabel")
                .set<Lu::Component::Transform2D>({.position = {70.f, 360.f}, .positionZ = 3})
                .add<Lu::Component::Transform2DGpu>()
            })
            .set(Lu::Component::Text{
                .text = "Status: Ready. Type in the input field or click buttons.",
                .font = world.lookup("Font::Main"),
                .pipeline = world.lookup("FontPipe::Basic"),
                .color = glm::vec4(0.4f, 0.8f, 0.5f, 1.0f),
                .size = 18.f,
                .align = Lu::TextAlign::Left
            });

        // 3. TextInput field
        auto inputEntity = Lu::Module::UI::createTextInput(
            world,
            "NameInput",
            "",
            "Type your name here...",
            {.position = {70.f, 120.f}, .positionZ = 2},
            glm::vec2(320.0f, 42.0f),
            "Mesh2D::InputBox"
        );

        // Customize TextInput callbacks
        auto& textInputComp = inputEntity.get_mut<Lu::UI::TextInput>();
        textInputComp.onChange = [statusLabel](const std::string& currentText) {
            std::string msg = "Typing: \"" + currentText + "\"";
            auto& t = statusLabel.get_mut<Lu::Component::Text>();
            t.text = msg;
            t.color = glm::vec4(0.9f, 0.8f, 0.3f, 1.0f);
            statusLabel.modified<Lu::Component::Text>();
        };
        textInputComp.onSubmit = [statusLabel](const std::string& submittedText) {
            std::string msg = "Submitted text: \"" + submittedText + "\"!";
            auto& t = statusLabel.get_mut<Lu::Component::Text>();
            t.text = msg;
            t.color = glm::vec4(0.3f, 0.9f, 1.0f, 1.0f);
            statusLabel.modified<Lu::Component::Text>();
        };

        // 4. Button 1: Click Counter Button
        static int clickCount = 0;
        auto button1 = Lu::Module::UI::createButton(
            world,
            "CounterButton",
            "Click Me: 0",
            {.position = {70.f, 190.f}, .positionZ = 2},
            glm::vec2(180.0f, 44.0f),
            "Mesh2D::Button"
        );

        auto& btn1 = button1.get_mut<Lu::UI::Button>();
        btn1.normalColor = glm::vec4(0.20f, 0.45f, 0.75f, 1.0f);
        btn1.hoverColor = glm::vec4(0.30f, 0.55f, 0.88f, 1.0f);
        btn1.pressedColor = glm::vec4(0.12f, 0.32f, 0.60f, 1.0f);
        btn1.onClick = [statusLabel, button1](flecs::entity) {
            clickCount++;
            // Update button label directly on the Button component
            button1.get_mut<Lu::UI::Button>().setLabel("Click Me: " + std::to_string(clickCount));

            // Update status
            auto& st = statusLabel.get_mut<Lu::Component::Text>();
            st.text = "Counter Button clicked! Total clicks: " + std::to_string(clickCount);
            st.color = glm::vec4(0.4f, 1.0f, 0.6f, 1.0f);
            statusLabel.modified<Lu::Component::Text>();
        };

        // 5. Button 2: Clear Input Button
        auto button2 = Lu::Module::UI::createButton(
            world,
            "ClearButton",
            "Clear Input",
            {.position = {270.f, 190.f}, .positionZ = 2},
            glm::vec2(160.0f, 44.0f),
            "Mesh2D::Button"
        );

        auto& btn2 = button2.get_mut<Lu::UI::Button>();
        btn2.normalColor = glm::vec4(0.65f, 0.25f, 0.25f, 1.0f);
        btn2.hoverColor = glm::vec4(0.80f, 0.35f, 0.35f, 1.0f);
        btn2.pressedColor = glm::vec4(0.45f, 0.15f, 0.15f, 1.0f);
        btn2.onClick = [statusLabel, inputEntity](flecs::entity) {
            auto& input = inputEntity.get_mut<Lu::UI::TextInput>();
            input.clear();

            auto& st = statusLabel.get_mut<Lu::Component::Text>();
            st.text = "Input text cleared.";
            st.color = glm::vec4(1.0f, 0.4f, 0.4f, 1.0f);
            statusLabel.modified<Lu::Component::Text>();
        };

        // 6. Button 3: Toggle Enabled state
        static bool fieldsEnabled = true;
        auto button3 = Lu::Module::UI::createButton(
            world,
            "ToggleButton",
            "Disable Fields",
            {.position = {70.f, 260.f}, .positionZ = 2},
            glm::vec2(180.0f, 44.0f),
            "Mesh2D::Button"
        );

        auto& btn3 = button3.get_mut<Lu::UI::Button>();
        btn3.normalColor = glm::vec4(0.35f, 0.35f, 0.45f, 1.0f);
        btn3.hoverColor = glm::vec4(0.45f, 0.45f, 0.55f, 1.0f);
        btn3.pressedColor = glm::vec4(0.25f, 0.25f, 0.35f, 1.0f);
        btn3.onClick = [button1, button2, button3, inputEntity, statusLabel](flecs::entity self) {
            fieldsEnabled = !fieldsEnabled;

            button1.get_mut<Lu::UI::Button>().enabled = fieldsEnabled;
            button2.get_mut<Lu::UI::Button>().enabled = fieldsEnabled;
            inputEntity.get_mut<Lu::UI::TextInput>().enabled = fieldsEnabled;

            button3.get_mut<Lu::UI::Button>().setLabel(fieldsEnabled ? "Disable Fields" : "Enable Fields");

            auto& st = statusLabel.get_mut<Lu::Component::Text>();
            st.text = fieldsEnabled ? "Fields enabled." : "Fields disabled.";
            st.color = fieldsEnabled ? glm::vec4(0.4f, 1.0f, 0.6f, 1.0f) : glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
            statusLabel.modified<Lu::Component::Text>();
        };
    }
};

int main(int argc, char** argv) {
    const std::string fontPath = findFont(argc, argv);

    Lu::App("LuEngine UI Demo - Button & TextInput", false, 1280, 800)
        // 2D geometry registration
        .registerShape2D<Lu::Shape2D::Rectangle>("Mesh2D::Panel", glm::vec2(540.f, 400.f))
        .registerShape2D<Lu::Shape2D::RoundedRectangle>("Mesh2D::InputBox", glm::vec2(320.f, 42.f), 8.f, 8u)
        .registerShape2D<Lu::Shape2D::RoundedRectangle>("Mesh2D::Button", glm::vec2(180.f, 44.f), 10.f, 8u)
        // Mesh2D pipeline
        .registerMesh2DPipe(Lu::GraphicsPipelineConfig{
            .name = "Mesh2DPipe::Basic",
            .capacity = 1024,
            .vertexShader = "shader/mesh2d_basic.vert.spv",
            .fragmentShader = "shader/mesh2d_basic.frag.spv",
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
        })
        // Font & font pipeline
        .registerFont("Font::Main", fontPath, 64)
        .registerFontPipe(Lu::GraphicsPipelineConfig{
            .name = "FontPipe::Basic",
            .capacity = 4096,
            .vertexShader = "shader/font_basic.vert.spv",
            .fragmentShader = "shader/font_basic.frag.spv",
            .cullMode = VK_CULL_MODE_NONE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
        })
        // Import our non-core UI Module (flecs ecs)
        .importModule<Lu::Module::UI>()
        .importModule<UIDemoModule>()
        .run();
}
