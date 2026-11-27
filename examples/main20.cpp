#include "App.hpp"
#include "Shape2D.hpp"
#include "module/ButtonModule.hpp"
#include "module/ScrollMatrixModule.hpp"
#include "component/Transform2D.hpp"
#include "component/Transform2DGpu.hpp"
#include "component/Mesh2D.hpp"
#include "component/Text2D.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace Demo {
static std::string findFont() {
    for (const auto& path : {"C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/segoeui.ttf"})
        if (std::filesystem::exists(path)) return path;
    return {};
}

// Apply one button's SDF treatment. The button owns a crisp body and a separate
// soft SDF layer; callbacks decide their colors, gradient, glow, and depression.
static void style(flecs::entity owner, glm::vec4 fill, float gradient,
                  glm::vec4 glow, float glowAlpha, float offsetY) {
    owner.children([&](flecs::entity part) {
        const auto* layerTransform = part.try_get<Lu::Component::Transform2D>();
        if (layerTransform && layerTransform->positionZ == 0) {
            if (auto* transform = part.try_get<Lu::Component::Transform2D>()) {
                auto updated = *transform;
                if (updated.position.y != -12.f) {
                    updated.position = {-12.f, -12.f};
                    part.set<Lu::Component::Transform2D>(updated);
                }
            }
            part.children([&](flecs::entity leaf) {
                if (auto* mesh = leaf.try_get<Lu::Component::Mesh2D>()) {
                    auto updated = *mesh;
                    const glm::vec4 color{glow.r, glow.g, glow.b, glowAlpha};
                    if (updated.color.r != color.r || updated.color.g != color.g ||
                        updated.color.b != color.b || updated.color.a != color.a ||
                        updated.params.x != 18.f || updated.params.y != 12.f ||
                        updated.params.z != 9.f || updated.params.w != 0.f) {
                        updated.color = color;
                        updated.params = {18.f, 12.f, 9.f, 0.f};
                        leaf.set<Lu::Component::Mesh2D>(updated);
                    }
                }
            });
        } else if (layerTransform && layerTransform->positionZ == 1) {
            if (auto* transform = part.try_get<Lu::Component::Transform2D>()) {
                auto updated = *transform;
                if (updated.position.y != offsetY) {
                    updated.position.y = offsetY;
                    part.set<Lu::Component::Transform2D>(updated);
                }
            }
            part.children([&](flecs::entity leaf) {
                if (auto* mesh = leaf.try_get<Lu::Component::Mesh2D>()) {
                    auto updated = *mesh;
                    if (updated.color.r != fill.r || updated.color.g != fill.g ||
                        updated.color.b != fill.b || updated.color.a != fill.a ||
                        updated.params.w != gradient) {
                        updated.color = fill;
                        updated.params.w = gradient;
                        leaf.set<Lu::Component::Mesh2D>(updated);
                    }
                }
            });
        }
    });
}


// A UIScrollMatrix whose elements are UIButtons (World of Warcraft style inventory): `columns` buttons per row, the
// rows scroll vertically. Like the UIScrollList the matrix does not know what its elements are: for every cell (i, j)
// it creates a slot entity and calls createItem, which puts a UIButton on the slot.
// Elements are stored row-major, so cell (i, j) has the flat index i * columns + j.
struct ScrollMatrixDemo {
    explicit ScrollMatrixDemo(flecs::world& world) {
        const auto mesh = world.lookup("Mesh2D::Button");
        const auto sdfClip = world.lookup("Mesh2DPipe::SDFClip");
        const auto font = world.lookup("Font::Main");
        const auto clipFont = world.lookup("FontPipe::Clip");
        const auto textPipe = world.lookup("FontPipe::Basic");

        auto titleTransform = world.entity("MatrixTitleTransform")
            .set<Lu::Component::Transform2D>({.position={56.f,28.f}, .positionZ=20})
            .add<Lu::Component::Transform2DGpu>();
        world.entity("MatrixTitleText").set(flecs::Parent{titleTransform})
            .set(Lu::Component::Text2D{.text="SCROLL MATRIX", .font=font, .pipeline=textPipe,
                .color={0.78f,0.88f,1.f,1.f}, .size=30.f});

        auto statusTransform = world.entity("MatrixStatusTransform")
            .set<Lu::Component::Transform2D>({.position={56.f,72.f}, .positionZ=20})
            .add<Lu::Component::Transform2DGpu>();
        auto status = world.entity("MatrixStatusText").set(flecs::Parent{statusTransform})
            .set(Lu::Component::Text2D{.text="Choose an item", .font=font, .pipeline=textPipe,
                .color={0.92f,0.88f,0.72f,1.f}, .size=22.f});
        auto setStatus = [status, font, textPipe](const std::string& message) {
            status.set<Lu::Component::Text2D>({.text=message, .font=font, .pipeline=textPipe,
                .color={0.92f,0.88f,0.72f,1.f}, .size=22.f});
        };

        auto hintTransform = world.entity("MatrixHintTransform")
            .set<Lu::Component::Transform2D>({.position={56.f,532.f}, .positionZ=3})
            .add<Lu::Component::Transform2DGpu>();
        world.entity("MatrixHint").set(flecs::Parent{hintTransform})
            .set(Lu::Component::Text2D{.text="Mouse wheel or drag the scrollbar", .font=font,
                .pipeline=textPipe, .color={0.62f,0.76f,0.86f,1.f}, .size=17.f});

        // 4 columns of 56x56 buttons; the viewport is wide enough for the columns plus the scrollbar.
        constexpr uint32_t columns = 4;
        constexpr uint32_t itemCount = 42;
        const glm::vec2 cellSize{56.f, 56.f};
        const glm::vec2 spacing{8.f, 8.f};
        const float padding = 8.f, scrollbarWidth = 8.f;
        const float viewWidth = 2.f * padding + columns * cellSize.x + (columns - 1) * spacing.x + scrollbarWidth + 4.f;

        // Labels in row-major order; kept in sync with the matrix on every add / remove.
        auto labels = std::make_shared<std::vector<std::string>>();
        for (uint32_t k = 0; k < itemCount; ++k) labels->push_back(std::to_string(k + 1));

        const glm::vec4 idle{0.10f,0.16f,0.24f,1.f};
        const glm::vec4 hover{0.16f,0.28f,0.38f,1.f};
        const glm::vec4 down{0.12f,0.62f,0.78f,1.f};

        // Builds one UIButton on a slot. Used by the factory and by the inserted elements.
        auto makeButton = [=](flecs::entity slot, uint32_t i, uint32_t j, glm::vec2 itemSize, const std::string& label) {
            slot.set(Lu::Component::UIButton{
                .mesh=mesh, .meshPipeline=sdfClip,
                .font=font, .fontPipeline=clipFont,
                .text=label, .color=idle, .textColor={0.91f,0.94f,0.98f,1.f},
                .fontSize=16.f, .meshParams={8.f,2.f,0.f,0.08f},
                .textTransform={.position={itemSize.x * 0.5f, itemSize.y * 0.5f - 9.f}, .positionZ=2},
                .size=itemSize,
                .onClicked=[=](flecs::entity e) {
                    setStatus("Clicked: item " + label + " at (" + std::to_string(i) + ", " + std::to_string(j) + ")");
                    style(e, down, -0.22f, {}, 0.f, 3.f);
                },
                .onHoverEnter=[=](flecs::entity e) { style(e, hover, 0.18f, {}, 0.f, -1.f); },
                .onHoverExit=[=](flecs::entity e) { style(e, idle, 0.08f, {}, 0.f, 0.f); },
                .onRelease=[=](flecs::entity e) { style(e, hover, 0.14f, {}, 0.f, -1.f); }
            });
        };

        auto matrixEntity = world.entity("ScrollMatrix")
            .set<Lu::Component::Transform2D>({.position={56.f,110.f}, .positionZ=10})
            .add<Lu::Component::Transform2DGpu>()
            .set(Lu::Component::UIScrollMatrix{
                .size={viewWidth,400.f}, .columns=columns, .itemCount=itemCount,
                .itemSize=cellSize, .spacing=spacing, .padding=padding,
                .mesh=mesh, .meshPipeline=sdfClip,
                .backgroundColor={0.05f,0.08f,0.12f,1.f}, .backgroundParams={12.f,0.f,0.f,0.f},
                .scrollbarWidth=scrollbarWidth,
                .trackColor={1.f,1.f,1.f,0.08f}, .trackParams={4.f,0.f,0.f,0.f},
                .thumbParams={4.f,0.f,0.f,0.f},
                // Any UI component can be the element: here every element is a UIButton.
                .createItem=[=](flecs::entity slot, uint32_t i, uint32_t j, glm::vec2 itemSize) {
                    makeButton(slot, i, j, itemSize, (*labels)[i * columns + j]);
                }
            });

        auto insertCount = std::make_shared<uint32_t>(0);
        auto random = std::make_shared<std::mt19937>(std::random_device{}());
        const glm::vec2 actionSize{236.f, 44.f};

        auto addAction = [&](const char* name, const char* text, glm::vec2 position,
                             std::function<void(flecs::entity)> action) {
            world.entity(name)
                .set<Lu::Component::Transform2D>({.position=position, .positionZ=10})
                .add<Lu::Component::Transform2DGpu>()
                .set(Lu::Component::UIButton{
                    .mesh=mesh, .meshPipeline=sdfClip,
                    .font=font, .fontPipeline=clipFont,
                    .text=text, .color=idle,
                    .textColor={0.91f,0.94f,0.98f,1.f}, .fontSize=17.f,
                    .meshParams={10.f,2.f,0.f,0.08f},
                    .textTransform={.position={actionSize.x * 0.5f, 12.f}, .positionZ=2},
                    .size=actionSize,
                    .onClicked=std::move(action),
                    .onHoverEnter=[=](flecs::entity e) { style(e, hover, 0.18f, {}, 0.f, -1.f); },
                    .onHoverExit=[=](flecs::entity e) { style(e, idle, 0.08f, {}, 0.f, 0.f); },
                    .onRelease=[=](flecs::entity e) { style(e, hover, 0.14f, {}, 0.f, -1.f); }
                });
        };

        addAction("InsertRandomMatrixItem", "Insert random item", {56.f, 570.f},
            [=](flecs::entity button) {
                style(button, down, -0.22f, {}, 0.f, 3.f);
                auto matrix = matrixEntity.get<Lu::Component::UIScrollMatrix>();
                // Keep the current (eased) scroll position when the matrix is rebuilt.
                if (const auto* state = matrixEntity.try_get<Lu::Component::UIScrollMatrixState>())
                    matrix.scroll = state->appliedScroll;

                // Random cell (i, j) in 0 .. count (count = append); later elements move one cell forward.
                const uint32_t flat = std::uniform_int_distribution<uint32_t>(0, matrix.elementCount())(*random);
                const auto [i, j] = matrix.cellOf(flat);
                const std::string label = "N" + std::to_string(++*insertCount);
                labels->insert(labels->begin() + flat, label);
                matrix.addItem(i, j, [=](flecs::entity slot, uint32_t ci, uint32_t cj, glm::vec2 itemSize) {
                    makeButton(slot, ci, cj, itemSize, label);
                });
                matrixEntity.set<Lu::Component::UIScrollMatrix>(matrix);
                setStatus("Inserted " + label + " at (" + std::to_string(i) + ", " + std::to_string(j) + ")");
            });

        addAction("RemoveRandomMatrixItem", "Remove random item", {310.f, 570.f},
            [=](flecs::entity button) {
                style(button, down, -0.22f, {}, 0.f, 3.f);
                auto matrix = matrixEntity.get<Lu::Component::UIScrollMatrix>();
                const uint32_t count = matrix.elementCount();
                if (count == 0) {
                    setStatus("The scroll matrix is empty");
                    return;
                }
                if (const auto* state = matrixEntity.try_get<Lu::Component::UIScrollMatrixState>())
                    matrix.scroll = state->appliedScroll;

                const uint32_t flat = std::uniform_int_distribution<uint32_t>(0, count - 1)(*random);
                const auto [i, j] = matrix.cellOf(flat);
                const std::string removed = (*labels)[flat];
                labels->erase(labels->begin() + flat);
                matrix.removeItem(i, j);
                matrixEntity.set<Lu::Component::UIScrollMatrix>(matrix);
                setStatus("Removed item " + removed + " at (" + std::to_string(i) + ", " + std::to_string(j) + ")");
            });
    }
};
}

int main() {
    Lu::App("LuEngine Scroll Matrix", false, 960, 640)
        .registerShape2D<Lu::Shape2D::Rectangle>("Mesh2D::Button", glm::vec2(1.f,1.f))
        .registerMesh2DPipe(Lu::GraphicsPipelineConfig{
            .name="Mesh2DPipe::SDFButton", .capacity=256,
            .vertexShader="shader/mesh2d_ui.vert.spv", .fragmentShader="shader/mesh2d_button.frag.spv",
            .depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL, .blendEnable=false, .alphaToCoverageEnable=true
        })
        .registerMesh2DPipe(Lu::GraphicsPipelineConfig{
            .name="Mesh2DPipe::SDFBackground", .capacity=256,
            .vertexShader="shader/mesh2d_ui.vert.spv", .fragmentShader="shader/mesh2d_background.frag.spv",
            .cullMode=VK_CULL_MODE_NONE,
            .depthWriteEnable=false, .depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL,
            .blendEnable=true
        })
        // Clipping pipelines used by the scroll matrix elements (Clip2D is inherited from the list).
        .registerMesh2DPipe(Lu::GraphicsPipelineConfig{
            .name="Mesh2DPipe::SDFClip", .capacity=1024,
            .vertexShader="shader/mesh2d_ui.vert.spv", .fragmentShader="shader/mesh2d_ui.frag.spv",
            .depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL, .blendEnable=false, .alphaToCoverageEnable=true
        })
        .registerFont("Font::Main", Demo::findFont(), 48)
        .registerFontPipe(Lu::GraphicsPipelineConfig{
            .name="FontPipe::Basic", .capacity=1024, .vertexShader="shader/font_basic.vert.spv",
            .fragmentShader="shader/font_basic.frag.spv", .cullMode=VK_CULL_MODE_NONE,
            .depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL
        })
        .registerFontPipe(Lu::GraphicsPipelineConfig{
            .name="FontPipe::Clip", .capacity=1024, .vertexShader="shader/font_clip.vert.spv",
            .fragmentShader="shader/font_clip.frag.spv", .cullMode=VK_CULL_MODE_NONE,
            .depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL
        })
        .importModule<Lu::Module::UIButton>()
        .importModule<Lu::Module::UIScrollMatrix>()
        .importModule<Demo::ScrollMatrixDemo>()
        .run();
}
