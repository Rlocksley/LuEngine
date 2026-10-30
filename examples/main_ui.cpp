#include "App.hpp"
#include "Shape2D.hpp"
#include "UIModule.hpp"
#include <filesystem>
#include <iostream>

// =====================================================================================================================
// main_ui: a Civilization-style screen built ONLY from entity hierarchies.
//
// There is no factory function: a widget is an entity with a Transform2D (position, local to the parent), a Node (size)
// and a widget component (Window, Button, Slider, Toggle, Dropdown, TextInput, ScrollView, TabView, ProgressBar, Label,
// Panel, Image, ...). The UI module observes the widget components and creates the child entities that draw them
// (rectangles, knobs, texts) and, where a widget consists of other widgets (the close button of a Window, the tabs of a
// TabView, the options of a Dropdown, the scroll bar of a ScrollView), those child widgets.
//
//   UI                          Style (theme, valid for the whole tree below)
//   |- TopBar                   Panel + Layout(horizontal)
//   |   |- Gold / Science ...   Label
//   |- CityWindow               Window (draggable, closable, raised on click)
//   |   '- Tabs                 TabView
//   |       |- Production       TabPage + Layout(vertical): Label, ProgressBar, ScrollView -> Button*
//   |       '- Settings         TabPage + Layout(vertical): Slider, Toggle, Toggle(radio)*, Dropdown, TextInput
//   |- NextTurn                 Button + Tooltip
//   |- Status                   Label (changed by the callbacks)
//   '- ConfirmDialog            Window + Modal (shown by the Next Turn button)
// =====================================================================================================================

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

//UIDEMO_BEGIN
struct UIDemoModule {
    explicit UIDemoModule(flecs::world& world) {
        using namespace Lu::UI;
        using Lu::Component::Transform2D;

        // ---- root: the theme ------------------------------------------------------------------------------------
        flecs::entity ui = world.entity("UI")
            .set<Transform2D>({})
            .set<Node>({.size = {1280.f, 800.f}, .layer = 10})
            .set<Style>(Style{});   // all colors / sizes are the defaults of Style, change them here for the whole tree

        // ---- status line (changed by the callbacks below) ---------------------------------------------------------
        flecs::entity status = world.entity("Status").child_of(ui)
            .set<Transform2D>({.position = {20.f, 760.f}})
            .set<Label>({.text = "Click, drag, type, scroll - everything here is an entity.", .fontSize = 18.f});

        // ---- top bar: a Panel with a horizontal Layout ---------------------------------------------------------
        flecs::entity topBar = world.entity("TopBar").child_of(ui)
            .set<Transform2D>({.position = {0.f, 0.f}})
            .set<Node>({.size = {1280.f, 44.f}})
            .set<Panel>({})
            .set<Layout>({.direction = Direction::Horizontal, .spacing = 40.f, .padding = {20.f, 10.f, 20.f, 10.f}});
        world.entity("Gold").child_of(topBar).set<Transform2D>({}).set<Node>({.order = 0}).set<Label>({.text = "Gold: 214 (+7)"});
        world.entity("Science").child_of(topBar).set<Transform2D>({}).set<Node>({.order = 1}).set<Label>({.text = "Science: +12"});
        world.entity("Culture").child_of(topBar).set<Transform2D>({}).set<Node>({.order = 2}).set<Label>({.text = "Culture: +5"});
        world.entity("Turn").child_of(topBar).set<Transform2D>({}).set<Node>({.order = 3}).set<Label>({.text = "Turn 42"});

        // ---- the city window --------------------------------------------------------------------------------------
        flecs::entity city = world.entity("CityWindow").child_of(ui)
            .set<Transform2D>({.position = {60.f, 90.f}})
            .set<Node>({.size = {460.f, 560.f}})
            .set<Window>({.title = "Rome - Population 7"});

        flecs::entity tabs = world.entity("Tabs").child_of(city)
            .set<Transform2D>({.position = {12.f, 46.f}})
            .set<Node>({.size = {436.f, 502.f}})
            .set<TabView>({});

        // tab 1: production
        flecs::entity production = world.entity("Production").child_of(tabs)
            .set<TabPage>({.title = "Production", .order = 0})
            .set<Layout>({.direction = Direction::Vertical, .spacing = 10.f, .padding = {10.f, 12.f, 10.f, 10.f}});
        world.entity("Building").child_of(production)
            .set<Transform2D>({}).set<Node>({.order = 0})
            .set<Label>({.text = "Building: Warrior (3 turns)"});
        flecs::entity progress = world.entity("BuildProgress").child_of(production)
            .set<Transform2D>({}).set<Node>({.size = {400.f, 24.f}, .order = 1})
            .set<ProgressBar>({.value = 0.35f, .showText = true});
        flecs::entity queue = world.entity("Queue").child_of(production)
            .set<Transform2D>({}).set<Node>({.size = {400.f, 280.f}, .order = 2})
            .set<Panel>({})
            .set<ScrollView>({})
            .set<Layout>({.direction = Direction::Vertical, .spacing = 4.f, .padding = {6.f, 6.f, 22.f, 6.f}});
        const char* items[] = {"Warrior", "Settler", "Builder", "Scout", "Archer", "Slinger", "Monument", "Granary", "Library", "Barracks"};
        for (int i = 0; i < 10; ++i) {
            world.entity().child_of(queue)
                .set<Transform2D>({})
                .set<Node>({.size = {360.f, 34.f}, .order = i})
                .set<Button>({.text = std::string("Build ") + items[i], .onClick = [progress, status, name = std::string(items[i])](flecs::entity) {
                    progress.set<ProgressBar>({.value = 0.0f, .showText = true});
                    status.set<Label>({.text = "Production changed to " + name, .fontSize = 18.f});
                }});
        }

        // tab 2: settings
        flecs::entity settings = world.entity("Settings").child_of(tabs)
            .set<TabPage>({.title = "Settings", .order = 1})
            .set<Layout>({.direction = Direction::Vertical, .spacing = 14.f, .padding = {14.f, 14.f, 14.f, 14.f}});
        world.entity("VolumeLabel").child_of(settings).set<Transform2D>({}).set<Node>({.order = 0})
            .set<Label>({.text = "Master volume"});
        world.entity("Volume").child_of(settings)
            .set<Transform2D>({}).set<Node>({.size = {300.f, 24.f}, .order = 1})
            .set<Slider>({.value = 0.7f, .step = 0.05f, .onChange = [status](flecs::entity, float v) {
                status.set<Label>({.text = "Volume: " + std::to_string(static_cast<int>(v * 100.f)) + "%", .fontSize = 18.f});
            }})
            .set<Tooltip>({.text = "Drag the knob.\nSteps of 5%."});
        world.entity("Fullscreen").child_of(settings)
            .set<Transform2D>({}).set<Node>({.size = {300.f, 28.f}, .order = 2})
            .set<Toggle>({.text = "Fullscreen", .onChange = [status](flecs::entity, bool on) {
                status.set<Label>({.text = on ? "Fullscreen on" : "Fullscreen off", .fontSize = 18.f});
            }});
        world.entity("SizeSmall").child_of(settings).set<Transform2D>({}).set<Node>({.size = {300.f, 28.f}, .order = 3})
            .set<Toggle>({.checked = true, .text = "Small map", .radio = true, .group = 1});
        world.entity("SizeLarge").child_of(settings).set<Transform2D>({}).set<Node>({.size = {300.f, 28.f}, .order = 4})
            .set<Toggle>({.text = "Large map", .radio = true, .group = 1});
        world.entity("Name").child_of(settings)
            .set<Transform2D>({}).set<Node>({.size = {300.f, 40.f}, .order = 5})
            .set<TextInput>({.text = "Rome", .placeholder = "City name", .onSubmit = [city, status](flecs::entity, const std::string& text) {
                city.set<Window>({.title = text + " - Population 7"});
                status.set<Label>({.text = "City renamed to " + text, .fontSize = 18.f});
            }});
        world.entity("Difficulty").child_of(settings)
            .set<Transform2D>({}).set<Node>({.size = {300.f, 36.f}, .order = 6})
            .set<Dropdown>({.options = {"Settler", "Prince", "King", "Emperor", "Deity"}, .selected = 1,
                            .onChange = [status](flecs::entity, int i) {
                                status.set<Label>({.text = "Difficulty index " + std::to_string(i), .fontSize = 18.f});
                            }});

        // ---- next turn: Button + Tooltip, opens a modal dialog ------------------------------------------------------
        flecs::entity dialog = world.entity("ConfirmDialog").child_of(ui)
            .set<Transform2D>({.position = {460.f, 300.f}})
            .set<Node>({.size = {360.f, 170.f}, .layer = 2000})
            .set<Window>({.title = "Next turn?", .draggable = false, .closable = false})
            .add<Modal>()
            .add<Hidden>();
        world.entity("Question").child_of(dialog)
            .set<Transform2D>({.position = {24.f, 62.f}})
            .set<Label>({.text = "End your turn? Units that can still\nmove will be skipped."});
        world.entity("Yes").child_of(dialog)
            .set<Transform2D>({.position = {24.f, 114.f}})
            .set<Node>({.size = {150.f, 36.f}})
            .set<Button>({.text = "Yes", .onClick = [dialog, status](flecs::entity) {
                dialog.add<Hidden>();
                status.set<Label>({.text = "Turn ended.", .fontSize = 18.f});
            }});
        world.entity("No").child_of(dialog)
            .set<Transform2D>({.position = {186.f, 114.f}})
            .set<Node>({.size = {150.f, 36.f}})
            .set<Button>({.text = "No", .onClick = [dialog](flecs::entity) { dialog.add<Hidden>(); }});

        world.entity("NextTurn").child_of(ui)
            .set<Transform2D>({.position = {1060.f, 730.f}})
            .set<Node>({.size = {200.f, 50.f}})
            .set<Button>({.text = "Next Turn", .fontSize = 24.f, .onClick = [dialog](flecs::entity) { dialog.remove<Hidden>(); }})
            .set<Tooltip>({.text = "Ends the turn of all your units.\nThe game asks for confirmation."});
    }
};
//UIDEMO_END

int main(int argc, char** argv) {
    const std::string fontPath = findFont(argc, argv);

    Lu::App("LuEngine UI Demo - Civilization style screen", false, 1280, 800)
        // The shapes the UI module needs (unit shapes, widgets scale them): a square and a circle spanning (0,0)..(1,1)
        .registerShape2D<Lu::Shape2D::Rectangle>("Mesh2D::UIRect", glm::vec2(1.f))
        .registerShape2D<Lu::Shape2D::Circle>("Mesh2D::UICircle", 0.5f, 48u, glm::vec2(0.5f))
        .registerMesh2DPipe(Lu::GraphicsPipelineConfig{
            .name = "Mesh2DPipe::Basic",
            .capacity = 4096,
            .vertexShader = "shader/mesh2d_basic.vert.spv",
            .fragmentShader = "shader/mesh2d_basic.frag.spv",
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
        })
        .registerFont("Font::Main", fontPath, 64)
        .registerFontPipe(Lu::GraphicsPipelineConfig{
            .name = "FontPipe::Basic",
            .capacity = 4096,
            .vertexShader = "shader/font_basic.vert.spv",
            .fragmentShader = "shader/font_basic.frag.spv",
            .cullMode = VK_CULL_MODE_NONE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
        })
        .importModule<Lu::Module::UI>()
        .importModule<UIDemoModule>()
        .run();
}
