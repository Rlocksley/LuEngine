#pragma once

#include "UIDetail.hpp"

namespace Lu {
    namespace UI {
        namespace Detail {

            // =====================================================================================================
            //  Drawing (called for Dirty entities in the UI sync phase) and input logic of the basic widgets.
            // =====================================================================================================
            inline Interact interactOf(flecs::entity e) {
                const Interact* in = e.try_get<Interact>();
                return in ? *in : Interact{};
            }

            inline void syncPanel(Ctx& c, const Panel& panel) {
                c.defaultSize({200.0f, 120.0f});
                const float bw = panel.borderWidth >= 0.0f ? panel.borderWidth : c.style.borderWidth;
                c.bordered(Slot::PanelBorder, Slot::PanelFill, {0.0f, 0.0f}, c.size, 0, bw,
                           pick(panel.borderColor, c.style.panelBorder), pick(panel.color, c.style.panel));
            }

            inline void syncLabel(Ctx& c, const Label& label) {
                const float fs = c.fontSizeOr(label.fontSize);
                if (label.autoSize) {
                    // The label sizes its node to the text, until something else (a Layout, the user) gave the node another size.
                    const float w = label.maxWidth > 0.0f ? label.maxWidth : estimateWidth(label.text, fs) + 2.0f;
                    const glm::vec2 estimate(w, estimateHeight(label.text, fs, label.maxWidth));
                    if ((c.size.x <= 0.0f && c.size.y <= 0.0f) || c.size == c.parts.labelEstimate) c.setSize(estimate);
                    if (c.parts.labelEstimate != estimate) { c.parts.labelEstimate = estimate; c.partsChanged = true; }
                } else {
                    c.defaultSize({estimateWidth(label.text, fs) + 2.0f, fs * kLineHeight});
                }
                float x = 0.0f;
                if (label.align == TextAlign::Center) x = c.size.x * 0.5f;
                else if (label.align == TextAlign::Right) x = c.size.x;
                const float y = label.verticalCenter ? std::max(0.0f, (c.size.y - fs) * 0.5f) : 0.0f;
                c.text(Slot::LabelText, {x, y}, 0, label.text, pick(label.color, c.style.text), fs, label.align, label.maxWidth);
            }

            inline void syncImage(Ctx& c, const Image& image) {
                c.defaultSize({32.0f, 32.0f});
                c.image(Slot::ImageRect, {0.0f, 0.0f}, c.size, 0, image);
            }

            inline void syncProgress(Ctx& c, const ProgressBar& bar) {
                const bool horizontal = bar.orientation == Orientation::Horizontal;
                c.defaultSize(horizontal ? glm::vec2(200.0f, 22.0f) : glm::vec2(22.0f, 200.0f));
                const float bw = std::min(c.style.borderWidth, std::min(c.size.x, c.size.y) * 0.25f);
                c.bordered(Slot::ProgressBorder, Slot::ProgressTrack, {0.0f, 0.0f}, c.size, 0, bw,
                           c.style.panelBorder, pick(bar.trackColor, c.style.track));
                const glm::vec2 inner = c.size - glm::vec2(bw * 2.0f);
                const float v = std::clamp(bar.value, 0.0f, 1.0f);
                if (v > 0.0f && inner.x > 0.0f && inner.y > 0.0f) {
                    if (horizontal) c.rect(Slot::ProgressFill, {bw, bw}, {inner.x * v, inner.y}, 2, pick(bar.fillColor, c.style.accent));
                    else c.rect(Slot::ProgressFill, {bw, bw + inner.y * (1.0f - v)}, {inner.x, inner.y * v}, 2, pick(bar.fillColor, c.style.accent));
                } else {
                    c.hide(Slot::ProgressFill);
                }
                if (bar.showText) {
                    const float fs = std::min(c.style.fontSize, std::max(8.0f, c.size.y - 4.0f));
                    c.text(Slot::ProgressText, {c.size.x * 0.5f, std::max(0.0f, (c.size.y - fs) * 0.5f)}, 3,
                           std::to_string(static_cast<int>(std::round(v * 100.0f))) + "%", c.style.text, fs, TextAlign::Center);
                } else {
                    c.hide(Slot::ProgressText);
                }
            }

            // ---- Button -----------------------------------------------------------------------------------------------
            inline void syncButton(Ctx& c, const Button& button) {
                c.defaultSize({160.0f, 40.0f});
                const Interact in = interactOf(c.e);
                const ColorSet colors = pickSet(button.colors, c.style.button);
                const bool lit = in.enabled && (button.selected || in.hovered);
                c.bordered(Slot::ButtonBorder, Slot::ButtonFill, {0.0f, 0.0f}, c.size, 0, c.style.borderWidth,
                           lit ? c.style.accent : (in.enabled ? c.style.panelBorder : c.style.textDisabled),
                           stateColor(colors, in));
                const float fs = c.fontSizeOr(button.fontSize);
                c.text(Slot::ButtonText, {c.size.x * 0.5f, std::max(0.0f, (c.size.y - fs) * 0.5f)}, 3, button.text,
                       in.enabled ? pick(button.textColor, c.style.text) : c.style.textDisabled, fs, TextAlign::Center);
            }

            // ---- Toggle -----------------------------------------------------------------------------------------------
            inline void syncToggle(Ctx& c, const Toggle& toggle) {
                c.defaultSize({200.0f, 28.0f});
                const Interact in = interactOf(c.e);
                const float fs = c.fontSizeOr(toggle.fontSize);
                const float box = std::max(8.0f, std::min(c.size.y - 4.0f, 24.0f));
                const glm::vec2 boxPos(0.0f, (c.size.y - box) * 0.5f);
                const ColorSet colors = pickSet(toggle.colors, c.style.input);
                const Color border = in.enabled ? (in.hovered ? c.style.accent : c.style.panelBorder) : c.style.textDisabled;
                const float bw = std::min(c.style.borderWidth, box * 0.25f);
                if (toggle.radio) {
                    c.circle(Slot::ToggleBorder, boxPos, glm::vec2(box), 0, border);
                    c.circle(Slot::ToggleFill, boxPos + glm::vec2(bw), glm::vec2(box - bw * 2.0f), 1, stateColor(colors, in));
                    const float mark = box * 0.5f;
                    if (toggle.checked) c.circle(Slot::ToggleMark, boxPos + glm::vec2((box - mark) * 0.5f), glm::vec2(mark), 2, c.style.accent);
                    else c.hide(Slot::ToggleMark);
                } else {
                    c.bordered(Slot::ToggleBorder, Slot::ToggleFill, boxPos, glm::vec2(box), 0, bw, border, stateColor(colors, in));
                    const float mark = box * 0.5f;
                    if (toggle.checked) c.rect(Slot::ToggleMark, boxPos + glm::vec2((box - mark) * 0.5f), glm::vec2(mark), 2, c.style.accent);
                    else c.hide(Slot::ToggleMark);
                }
                c.text(Slot::ToggleText, {box + 10.0f, std::max(0.0f, (c.size.y - fs) * 0.5f)}, 3, toggle.text,
                       in.enabled ? pick(toggle.textColor, c.style.text) : c.style.textDisabled, fs, TextAlign::Left);
            }

            // ---- Slider -----------------------------------------------------------------------------------------------
            inline float sliderKnob(const Slider& slider, glm::vec2 size) {
                const float cross = slider.orientation == Orientation::Horizontal ? size.y : size.x;
                return std::max(4.0f, std::min(slider.knobSize, cross));
            }

            inline void syncSlider(Ctx& c, const Slider& slider) {
                const bool horizontal = slider.orientation == Orientation::Horizontal;
                c.defaultSize(horizontal ? glm::vec2(200.0f, 24.0f) : glm::vec2(24.0f, 200.0f));
                const Interact in = interactOf(c.e);
                const float k = sliderKnob(slider, c.size);
                const float tt = std::min(slider.trackThickness, horizontal ? c.size.y : c.size.x);
                const float v = std::clamp(slider.value, 0.0f, 1.0f);
                const float along = horizontal ? c.size.x : c.size.y;
                const float center = k * 0.5f + v * std::max(0.0f, along - k);
                const Color knob = (in.enabled && (in.pressed || in.hovered)) ? c.style.accent : pick(slider.knobColor, c.style.knob);
                if (horizontal) {
                    const float ty = (c.size.y - tt) * 0.5f;
                    c.rect(Slot::SliderBorder, {0.0f, ty}, {c.size.x, tt}, 0, in.enabled ? c.style.panelBorder : c.style.textDisabled);
                    c.rect(Slot::SliderTrack, {1.0f, ty + 1.0f}, {std::max(0.0f, c.size.x - 2.0f), std::max(0.0f, tt - 2.0f)}, 1, c.style.track);
                    c.rect(Slot::SliderFill, {1.0f, ty + 1.0f}, {std::max(0.0f, center - 1.0f), std::max(0.0f, tt - 2.0f)}, 2, pick(slider.fillColor, c.style.accent));
                    c.circle(Slot::SliderKnob, {center - k * 0.5f, (c.size.y - k) * 0.5f}, glm::vec2(k), 3, knob);
                } else {
                    const float tx = (c.size.x - tt) * 0.5f;
                    c.rect(Slot::SliderBorder, {tx, 0.0f}, {tt, c.size.y}, 0, in.enabled ? c.style.panelBorder : c.style.textDisabled);
                    c.rect(Slot::SliderTrack, {tx + 1.0f, 1.0f}, {std::max(0.0f, tt - 2.0f), std::max(0.0f, c.size.y - 2.0f)}, 1, c.style.track);
                    c.rect(Slot::SliderFill, {tx + 1.0f, 1.0f}, {std::max(0.0f, tt - 2.0f), std::max(0.0f, center - 1.0f)}, 2, pick(slider.fillColor, c.style.accent));
                    c.circle(Slot::SliderKnob, {(c.size.x - k) * 0.5f, center - k * 0.5f}, glm::vec2(k), 3, knob);
                }
            }

            // ---- TextInput ------------------------------------------------------------------------------------------
            inline std::string displayText(const TextInput& t, float maxVisible) {
                std::string shown = t.password ? std::string(t.text.size(), '*') : t.text;
                const size_t cursor = std::min(t.cursorPosition, shown.size());
                size_t anchor = shown.empty() ? 0 : shown.size() - 1;
                if (t.focused) {
                    shown.insert(cursor, 1, t.cursorVisible ? '|' : ' ');
                    anchor = cursor;
                }
                const size_t visible = static_cast<size_t>(std::max(1.0f, maxVisible));
                if (shown.size() > visible) {
                    const size_t start = anchor + 1 > visible ? anchor + 1 - visible : 0;
                    shown = shown.substr(start, visible);
                }
                return shown;
            }

            inline void syncTextInput(Ctx& c, const TextInput& t) {
                c.defaultSize({240.0f, 40.0f});
                const Interact in = interactOf(c.e);
                const ColorSet colors = pickSet(t.colors, c.style.input);
                Color fill = colors.normal;
                if (!in.enabled) fill = colors.disabled;
                else if (t.focused) fill = colors.pressed;
                else if (in.hovered) fill = colors.hover;
                const float fs = c.fontSizeOr(t.fontSize);
                c.bordered(Slot::InputBorder, Slot::InputFill, {0.0f, 0.0f}, c.size, 0, c.style.borderWidth,
                           t.focused ? c.style.accent : (in.enabled ? c.style.panelBorder : c.style.textDisabled), fill);
                const bool showPlaceholder = t.text.empty() && !t.focused;
                const std::string shown = showPlaceholder ? t.placeholder
                                                         : displayText(t, (c.size.x - 20.0f) / (fs * kCharWidth));
                const Color color = !in.enabled ? c.style.textDisabled
                                  : showPlaceholder ? pick(t.placeholderColor, c.style.textDim) : pick(t.textColor, c.style.text);
                c.text(Slot::InputText, {10.0f, std::max(0.0f, (c.size.y - fs) * 0.5f)}, 3, shown, color, fs, TextAlign::Left);
            }

            // =====================================================================================================
            //  Input logic
            // =====================================================================================================
            inline char keyToChar(int k, bool shift) {
                if (k >= GLFW_KEY_A && k <= GLFW_KEY_Z) return static_cast<char>((shift ? 'A' : 'a') + (k - GLFW_KEY_A));
                if (k >= GLFW_KEY_0 && k <= GLFW_KEY_9) {
                    constexpr char shifted[] = {')', '!', '@', '#', '$', '%', '^', '&', '*', '('};
                    return shift ? shifted[k - GLFW_KEY_0] : static_cast<char>('0' + (k - GLFW_KEY_0));
                }
                if (k >= GLFW_KEY_KP_0 && k <= GLFW_KEY_KP_9) return static_cast<char>('0' + (k - GLFW_KEY_KP_0));
                switch (k) {
                    case GLFW_KEY_SPACE: return ' ';
                    case GLFW_KEY_MINUS: return shift ? '_' : '-';
                    case GLFW_KEY_EQUAL: return shift ? '+' : '=';
                    case GLFW_KEY_LEFT_BRACKET: return shift ? '{' : '[';
                    case GLFW_KEY_RIGHT_BRACKET: return shift ? '}' : ']';
                    case GLFW_KEY_BACKSLASH: return shift ? '|' : '\\';
                    case GLFW_KEY_SEMICOLON: return shift ? ':' : ';';
                    case GLFW_KEY_APOSTROPHE: return shift ? '"' : '\'';
                    case GLFW_KEY_COMMA: return shift ? '<' : ',';
                    case GLFW_KEY_PERIOD: case GLFW_KEY_KP_DECIMAL: return shift ? '>' : '.';
                    case GLFW_KEY_SLASH: case GLFW_KEY_KP_DIVIDE: return shift ? '?' : '/';
                    case GLFW_KEY_GRAVE_ACCENT: return shift ? '~' : '`';
                    case GLFW_KEY_KP_MULTIPLY: return '*';
                    case GLFW_KEY_KP_SUBTRACT: return '-';
                    case GLFW_KEY_KP_ADD: return '+';
                    default: return '\0';
                }
            }

            inline void installBasicWidgets(flecs::world& world, const Phases& phases, std::shared_ptr<Pointer> pointer) {
                watchWidget<Panel>(world);
                watchWidget<Label>(world);
                watchWidget<Image>(world);
                watchWidget<ProgressBar>(world);
                watchWidget<Button>(world, true);
                watchWidget<Toggle>(world, true);
                watchWidget<Slider>(world, true);
                watchWidget<TextInput>(world, true, true);

                // ---- Button: click callback -----------------------------------------------------------------------------
                world.system<const Button, const Interact>("UIButton")
                    .kind(phases.input)
                    .each([](flecs::entity e, const Button& button, const Interact& in) {
                        if (in.clicked && in.enabled && button.onClick) button.onClick(e);
                    });

                // ---- Toggle: flip / radio group ---------------------------------------------------------------------------
                world.system<Toggle, const Interact>("UIToggle")
                    .kind(phases.input)
                    .each([](flecs::entity e, Toggle& toggle, const Interact& in) {
                        if (!in.clicked || !in.enabled) return;
                        if (toggle.radio && toggle.checked) return;
                        toggle.checked = !toggle.checked;
                        const Toggle snapshot = toggle;
                        if (toggle.checked && toggle.group != 0) {
                            std::vector<flecs::entity> siblings;
                            e.parent().children([&](flecs::entity s) { if (s.id() != e.id() && s.has<Toggle>()) siblings.push_back(s); });
                            for (flecs::entity s : siblings) {
                                Toggle other = s.get<Toggle>();
                                if (other.group == snapshot.group && other.checked) {
                                    other.checked = false;
                                    s.set<Toggle>(other);
                                    if (other.onChange) other.onChange(s, false);
                                }
                            }
                        }
                        e.modified<Toggle>();
                        if (snapshot.onChange) snapshot.onChange(e, snapshot.checked);
                    });

                // ---- Slider: drag -------------------------------------------------------------------------------------------
                world.system<Slider, const Interact, const Node>("UISlider")
                    .kind(phases.input)
                    .each([](flecs::entity e, Slider& slider, const Interact& in, const Node& node) {
                        if (!in.pressed || !in.enabled || node.size.x <= 0.0f || node.size.y <= 0.0f) return;
                        const bool horizontal = slider.orientation == Orientation::Horizontal;
                        const float k = sliderKnob(slider, node.size);
                        const float along = horizontal ? node.size.x : node.size.y;
                        const float pos = horizontal ? in.cursor.x : in.cursor.y;
                        float v = std::clamp((pos - k * 0.5f) / std::max(1.0f, along - k), 0.0f, 1.0f);
                        if (slider.step > 0.0f) v = std::clamp(std::round(v / slider.step) * slider.step, 0.0f, 1.0f);
                        if (std::abs(v - slider.value) < 1e-6f) return;
                        slider.value = v;
                        e.modified<Slider>();
                        if (slider.onChange) slider.onChange(e, v);
                    });

                // ---- TextInput: focus, typing, caret ------------------------------------------------------------------
                world.system<TextInput, const Interact, const Component::InputState>("UITextInput")
                    .kind(phases.input)
                    .each([pointer](flecs::iter& it, size_t index, TextInput& t, const Interact& in, const Component::InputState& input) {
                        flecs::entity e = it.entity(index);
                        const float dt = it.delta_time();
                        bool changed = false;     // redraw
                        bool edited = false;      // text changed

                        const bool focus = in.enabled && pointer->focused == e.id();
                        if (focus != t.focused) {
                            t.focused = focus;
                            t.cursorVisible = focus;
                            t.blinkTimer = 0.0f;
                            if (focus) t.cursorPosition = t.text.size();
                            changed = true;
                        }

                        if (t.focused) {
                            if (!t.readOnly) {
                                const bool shift = input.keys[GLFW_KEY_LEFT_SHIFT].down || input.keys[GLFW_KEY_RIGHT_SHIFT].down;
                                auto execute = [&](int k) {
                                    t.cursorPosition = std::min(t.cursorPosition, t.text.size());
                                    if (k == GLFW_KEY_BACKSPACE) {
                                        if (t.cursorPosition > 0) { t.text.erase(--t.cursorPosition, 1); edited = true; }
                                    } else if (k == GLFW_KEY_DELETE) {
                                        if (t.cursorPosition < t.text.size()) { t.text.erase(t.cursorPosition, 1); edited = true; }
                                    } else if (k == GLFW_KEY_LEFT) { if (t.cursorPosition > 0) --t.cursorPosition; }
                                    else if (k == GLFW_KEY_RIGHT) { if (t.cursorPosition < t.text.size()) ++t.cursorPosition; }
                                    else if (k == GLFW_KEY_HOME) t.cursorPosition = 0;
                                    else if (k == GLFW_KEY_END) t.cursorPosition = t.text.size();
                                    else if (k == GLFW_KEY_ENTER || k == GLFW_KEY_KP_ENTER) { if (t.onSubmit) t.onSubmit(e, t.text); }
                                    else if (k == GLFW_KEY_ESCAPE) { pointer->focused = 0; }
                                    else {
                                        const char ch = keyToChar(k, shift);
                                        if (ch != '\0' && (t.maxCharacters == 0 || t.text.size() < t.maxCharacters)) {
                                            t.text.insert(t.cursorPosition, 1, ch);
                                            ++t.cursorPosition;
                                            edited = true;
                                        }
                                    }
                                    t.cursorVisible = true;
                                    t.blinkTimer = 0.0f;
                                    changed = true;
                                };
                                int pressedKey = -1;
                                for (int k = GLFW_KEY_SPACE; k <= GLFW_KEY_LAST; ++k) {
                                    if (input.keys[static_cast<size_t>(k)].pressedThisFrame) { pressedKey = k; break; }
                                }
                                if (pressedKey != -1) {
                                    t.lastKey = pressedKey;
                                    t.repeatTimer = 0.0f;
                                    execute(pressedKey);
                                } else if (t.lastKey != -1 && input.keys[static_cast<size_t>(t.lastKey)].down) {
                                    t.repeatTimer += dt;
                                    if (t.repeatTimer >= 0.4f) { t.repeatTimer -= 0.04f; execute(t.lastKey); }
                                } else {
                                    t.lastKey = -1;
                                    t.repeatTimer = 0.0f;
                                }
                            }
                            t.blinkTimer += dt;
                            if (t.blinkTimer >= 0.5f) { t.blinkTimer = 0.0f; t.cursorVisible = !t.cursorVisible; changed = true; }
                        }

                        if (changed || edited) e.modified<TextInput>();
                        if (edited && t.onChange) t.onChange(e, t.text);
                    });
            }
        }
    }
}
