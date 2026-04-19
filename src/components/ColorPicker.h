#pragma once

#include "../EUINEO.h"
#include "../ui/ThemeTokens.h"
#include "../ui/UIBuilder.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace EUINEO {

class ColorPickerNode : public UINode {
public:
    class Builder : public UIBuilderBase<ColorPickerNode, Builder> {
    public:
        Builder(UIContext& context, ColorPickerNode& node) : UIBuilderBase<ColorPickerNode, Builder>(context, node) {}

        Builder& value(const Color& value) {
            this->node_.trackComposeValue("value", value);
            this->node_.value_ = Color(
                std::clamp(value.r, 0.0f, 1.0f),
                std::clamp(value.g, 0.0f, 1.0f),
                std::clamp(value.b, 0.0f, 1.0f),
                1.0f
            );
            return *this;
        }

        Builder& colors(std::vector<Color> value) {
            this->node_.trackComposeValue("colors", value);
            this->node_.colors_ = std::move(value);
            return *this;
        }

        Builder& fontSize(float value) {
            this->node_.trackComposeValue("fontSize", value);
            this->node_.fontSize_ = std::max(12.0f, value);
            return *this;
        }

        Builder& onChange(std::function<void(Color)> handler) {
            this->node_.onChange_ = std::move(handler);
            return *this;
        }
    };

    explicit ColorPickerNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "ColorPickerNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    bool usesCachedSurface() const override {
        return false;
    }

    bool blocksUnderlyingInput() const override {
        return popupPresentation_ && (open_ || openAnim_ > 0.001f);
    }

    bool interactiveContains(float x, float y) const override {
        const RectFrame frame = PrimitiveFrame(primitive_);
        if (!(open_ || openAnim_ > 0.001f)) {
            return contains(frame, x, y);
        }
        return contains(overlayFrame(), x, y);
    }

    bool wantsContinuousUpdate() const override {
        return activeSlider_ >= 0 ||
               (hoverAnim_ > 0.001f && hoverAnim_ < 0.999f) ||
               (openAnim_ > 0.001f && openAnim_ < 0.999f) ||
               (doneHover_ > 0.001f && doneHover_ < 0.999f);
    }

    RectFrame paintBounds() const override {
        if (open_ || openAnim_ > 0.001f) {
            return overlayFrame();
        }
        return clipPaintBounds(PrimitiveFrame(primitive_));
    }

    void update() override {
        const RectFrame frame = PrimitiveFrame(primitive_);
        const bool hoveredMain = primitive_.enabled && contains(frame, State.mouseX, State.mouseY);
        if (animateTowards(hoverAnim_, hoveredMain ? 1.0f : 0.0f, State.deltaTime * 15.0f)) {
            requestVisualRepaint(0.12f);
        }

        if (State.mouseClicked && primitive_.enabled && !open_ && hoveredMain) {
            open_ = true;
            State.mouseClicked = false;
            requestVisualRepaint(0.18f);
        }

        if (animateTowards(openAnim_, open_ ? 1.0f : 0.0f, State.deltaTime * 15.0f)) {
            requestVisualRepaint(0.18f);
        }
        updatePopupPresentation(open_ || openAnim_ > 0.001f);

        if (openAnim_ <= 0.001f) {
            activeSlider_ = -1;
            return;
        }

        const PickerLayout layout = makeLayout();
        const bool inDone = open_ && contains(layout.doneButton, State.mouseX, State.mouseY);
        if (animateTowards(doneHover_, inDone ? 1.0f : 0.0f, State.deltaTime * 18.0f)) {
            requestVisualRepaint(0.12f);
        }

        if (activeSlider_ >= 0 && State.mouseDown) {
            setChannelFromMouse(layout, activeSlider_);
            requestVisualRepaint(0.12f);
        }
        if (activeSlider_ >= 0 && !State.mouseDown) {
            activeSlider_ = -1;
            requestVisualRepaint(0.12f);
        }

        if (!open_ || !State.mouseClicked) {
            return;
        }

        if (inDone) {
            open_ = false;
            activeSlider_ = -1;
            State.mouseClicked = false;
            requestVisualRepaint(0.18f);
            return;
        }

        if (!contains(layout.panel, State.mouseX, State.mouseY)) {
            open_ = false;
            activeSlider_ = -1;
            State.mouseClicked = false;
            requestVisualRepaint(0.18f);
            return;
        }

        for (int channel = 0; channel < 3; ++channel) {
            if (contains(expand(sliderTrack(layout, channel), 10.0f, 12.0f), State.mouseX, State.mouseY)) {
                activeSlider_ = channel;
                setChannelFromMouse(layout, channel);
                State.mouseClicked = false;
                requestVisualRepaint(0.12f);
                return;
            }
        }

        const std::vector<Color> palette = resolvedColors();
        for (int index = 0; index < static_cast<int>(palette.size()); ++index) {
            if (!contains(swatchFrame(layout, index), State.mouseX, State.mouseY)) {
                continue;
            }
            applyColor(palette[static_cast<std::size_t>(index)]);
            State.mouseClicked = false;
            requestVisualRepaint(0.12f);
            return;
        }

        State.mouseClicked = false;
    }

    void draw() override {
        const RectFrame frame = PrimitiveFrame(primitive_);
        drawField(frame);
        if (openAnim_ > 0.001f) {
            drawPopup(makeLayout());
        }
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        if (popupPresentation_) {
            applyPopupPresentationDefaults(230);
        }
        primitive_.width = 180.0f;
        primitive_.height = 36.0f;
        primitive_.rounding = 8.0f;
        value_ = CurrentTheme->primary;
        colors_.clear();
        fontSize_ = 16.0f;
        onChange_ = {};
    }

private:
    struct PickerLayout {
        RectFrame overlay;
        RectFrame panel;
        RectFrame doneButton;
        RectFrame preview;
        RectFrame sliders;
        RectFrame swatches;
    };

    static bool contains(const RectFrame& frame, float x, float y) {
        return x >= frame.x && x <= frame.x + frame.width &&
               y >= frame.y && y <= frame.y + frame.height;
    }

    static RectFrame expand(const RectFrame& frame, float x, float y) {
        return RectFrame{frame.x - x, frame.y - y, frame.width + x * 2.0f, frame.height + y * 2.0f};
    }

    static int channelToInt(float value) {
        return std::clamp(static_cast<int>(std::round(std::clamp(value, 0.0f, 1.0f) * 255.0f)), 0, 255);
    }

    static std::string formatHex(const Color& color) {
        char buffer[10] = {};
        std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X",
                      channelToInt(color.r), channelToInt(color.g), channelToInt(color.b));
        return std::string(buffer);
    }

    RectFrame overlayFrame() const {
        return RectFrame{0.0f, 0.0f, State.screenW, State.screenH};
    }

    PickerLayout makeLayout() const {
        PickerLayout layout;
        layout.overlay = overlayFrame();
        const float width = std::max(320.0f, std::min(420.0f, State.screenW - 24.0f));
        const float height = std::max(292.0f, std::min(330.0f, State.screenH - 24.0f));
        layout.panel = RectFrame{
            std::max(12.0f, (State.screenW - width) * 0.5f),
            std::max(12.0f, (State.screenH - height) * 0.5f),
            width,
            height
        };
        layout.doneButton = RectFrame{layout.panel.x + layout.panel.width - 78.0f, layout.panel.y + 16.0f, 58.0f, 30.0f};
        layout.preview = RectFrame{layout.panel.x + 22.0f, layout.panel.y + 66.0f, layout.panel.width - 44.0f, 58.0f};
        layout.sliders = RectFrame{layout.panel.x + 26.0f, layout.panel.y + 146.0f, layout.panel.width - 52.0f, 98.0f};
        layout.swatches = RectFrame{layout.panel.x + 26.0f, layout.panel.y + 254.0f, layout.panel.width - 52.0f, 36.0f};
        return layout;
    }

    RectFrame sliderTrack(const PickerLayout& layout, int channel) const {
        return RectFrame{
            layout.sliders.x + 48.0f,
            layout.sliders.y + static_cast<float>(channel) * 31.0f + 10.0f,
            layout.sliders.width - 92.0f,
            6.0f
        };
    }

    RectFrame swatchFrame(const PickerLayout& layout, int index) const {
        const float size = 24.0f;
        const float gap = 8.0f;
        return RectFrame{
            layout.swatches.x + static_cast<float>(index) * (size + gap),
            layout.swatches.y + 4.0f,
            size,
            size
        };
    }

    float channelValue(int channel) const {
        if (channel == 0) {
            return value_.r;
        }
        if (channel == 1) {
            return value_.g;
        }
        return value_.b;
    }

    void setChannelValue(int channel, float nextValue) {
        Color next = value_;
        nextValue = std::clamp(nextValue, 0.0f, 1.0f);
        if (channel == 0) {
            next.r = nextValue;
        } else if (channel == 1) {
            next.g = nextValue;
        } else {
            next.b = nextValue;
        }
        next.a = 1.0f;
        applyColor(next);
    }

    void setChannelFromMouse(const PickerLayout& layout, int channel) {
        const RectFrame track = sliderTrack(layout, channel);
        if (track.width <= 0.0f) {
            return;
        }
        setChannelValue(channel, (State.mouseX - track.x) / track.width);
    }

    void applyColor(Color color) {
        color.r = std::clamp(color.r, 0.0f, 1.0f);
        color.g = std::clamp(color.g, 0.0f, 1.0f);
        color.b = std::clamp(color.b, 0.0f, 1.0f);
        color.a = 1.0f;
        if (std::abs(value_.r - color.r) < 0.0001f &&
            std::abs(value_.g - color.g) < 0.0001f &&
            std::abs(value_.b - color.b) < 0.0001f) {
            return;
        }
        value_ = color;
        if (onChange_) {
            onChange_(value_);
        }
    }

    std::vector<Color> resolvedColors() const {
        if (!colors_.empty()) {
            return colors_;
        }
        return {
            CurrentTheme->primary,
            Color(0.20f, 0.50f, 0.90f, 1.0f),
            Color(0.12f, 0.72f, 0.78f, 1.0f),
            Color(0.15f, 0.78f, 0.48f, 1.0f),
            Color(0.96f, 0.68f, 0.18f, 1.0f),
            Color(0.92f, 0.28f, 0.46f, 1.0f),
            Color(0.56f, 0.36f, 0.96f, 1.0f),
            Color(0.88f, 0.18f, 0.24f, 1.0f),
        };
    }

    void updatePopupPresentation(bool showPopup) {
        if (popupPresentation_ == showPopup) {
            return;
        }
        popupPresentation_ = showPopup;
        if (popupPresentation_) {
            applyPopupPresentationDefaults(230);
        } else {
            primitive_.renderLayer = RenderLayer::Content;
            primitive_.clipToParent = true;
        }
    }

    void drawField(const RectFrame& frame) const {
        PrimitiveClipScope clip(primitive_);
        DrawFieldChrome(primitive_, hoverAnim_, openAnim_, primitive_.rounding);
        Renderer::DrawRect(frame.x + 10.0f, frame.y + 8.0f, 20.0f, frame.height - 16.0f,
                           ApplyOpacity(value_, primitive_.opacity), 6.0f);
        const float textScale = fontSize_ / 24.0f;
        Renderer::DrawTextStr(formatHex(value_),
                              frame.x + 40.0f,
                              frame.y + frame.height * 0.5f + fontSize_ * 0.24f,
                              ApplyOpacity(CurrentTheme->text, primitive_.opacity),
                              textScale);
        Renderer::DrawTextStr(open_ ? "^" : "v",
                              frame.x + frame.width - 24.0f,
                              frame.y + frame.height * 0.5f + fontSize_ * 0.24f,
                              ApplyOpacity(CurrentTheme->text, primitive_.opacity),
                              textScale);
    }

    void drawPopup(const PickerLayout& layout) const {
        const float opacity = primitive_.opacity * std::clamp(openAnim_, 0.0f, 1.0f);
        Renderer::DrawRect(layout.overlay.x, layout.overlay.y, layout.overlay.width, layout.overlay.height,
                           ApplyOpacity(Color(0.0f, 0.0f, 0.0f, 0.30f), opacity), 0.0f);

        UIPrimitive popupPrimitive = primitive_;
        popupPrimitive.opacity = opacity;
        DrawPopupChrome(popupPrimitive, layout.panel, 22.0f);

        Renderer::DrawTextStr("Color", layout.panel.x + 22.0f, layout.panel.y + 37.0f,
                              ApplyOpacity(CurrentTheme->text, opacity), 20.0f / 24.0f);
        drawDoneButton(layout.doneButton, opacity);

        RectStyle previewStyle = MakeStyle(primitive_);
        previewStyle.color = ApplyOpacity(value_, opacity);
        previewStyle.rounding = 18.0f;
        previewStyle.shadowBlur = 18.0f;
        previewStyle.shadowOffsetY = 8.0f;
        previewStyle.shadowColor = ApplyOpacity(Color(value_.r, value_.g, value_.b, 0.24f), opacity);
        Renderer::DrawRect(layout.preview.x, layout.preview.y, layout.preview.width, layout.preview.height, previewStyle);

        const std::string hex = formatHex(value_);
        const float hexScale = 17.0f / 24.0f;
        const float hexWidth = Renderer::MeasureTextWidth(hex, hexScale);
        Renderer::DrawTextStr(hex, layout.preview.x + layout.preview.width - hexWidth - 18.0f, layout.preview.y + 36.0f,
                              ApplyOpacity(Color(1.0f, 1.0f, 1.0f, 0.94f), opacity), hexScale);

        drawSlider(layout, 0, "R", Color(0.92f, 0.20f, 0.22f, 1.0f), opacity);
        drawSlider(layout, 1, "G", Color(0.15f, 0.74f, 0.40f, 1.0f), opacity);
        drawSlider(layout, 2, "B", Color(0.20f, 0.46f, 0.92f, 1.0f), opacity);

        const std::vector<Color> palette = resolvedColors();
        for (int index = 0; index < static_cast<int>(palette.size()); ++index) {
            const RectFrame frame = swatchFrame(layout, index);
            if (frame.x + frame.width > layout.swatches.x + layout.swatches.width) {
                break;
            }
            Renderer::DrawRect(frame.x - 2.0f, frame.y - 2.0f, frame.width + 4.0f, frame.height + 4.0f,
                               ApplyOpacity(Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.08f), opacity),
                               10.0f);
            Renderer::DrawRect(frame.x, frame.y, frame.width, frame.height,
                               ApplyOpacity(palette[static_cast<std::size_t>(index)], opacity), 8.0f);
        }
    }

    void drawDoneButton(const RectFrame& frame, float opacity) const {
        const Color bg = Lerp(CurrentTheme->primary, Color(1.0f, 1.0f, 1.0f, 1.0f), doneHover_ * 0.12f);
        Renderer::DrawRect(frame.x, frame.y, frame.width, frame.height, ApplyOpacity(bg, opacity), 15.0f);
        const std::string text = "Done";
        const float scale = 13.0f / 24.0f;
        const float textWidth = Renderer::MeasureTextWidth(text, scale);
        Renderer::DrawTextStr(text, frame.x + (frame.width - textWidth) * 0.5f, frame.y + 20.0f,
                              ApplyOpacity(Color(1.0f, 1.0f, 1.0f, 1.0f), opacity), scale);
    }

    void drawSlider(const PickerLayout& layout, int channel, const std::string& label,
                    const Color& accent, float opacity) const {
        const RectFrame track = sliderTrack(layout, channel);
        const float value = channelValue(channel);
        const float y = track.y + track.height * 0.5f;
        Renderer::DrawTextStr(label, layout.sliders.x, y + 5.0f,
                              ApplyOpacity(CurrentTheme->text, opacity), 14.0f / 24.0f);
        Renderer::DrawRect(track.x, track.y, track.width, track.height,
                           ApplyOpacity(Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.12f), opacity),
                           track.height * 0.5f);
        Renderer::DrawRect(track.x, track.y, track.width * std::clamp(value, 0.0f, 1.0f), track.height,
                           ApplyOpacity(accent, opacity), track.height * 0.5f);

        const float knobRadius = activeSlider_ == channel ? 8.5f : 7.0f;
        const float knobX = track.x + track.width * std::clamp(value, 0.0f, 1.0f) - knobRadius;
        const float knobY = track.y + track.height * 0.5f - knobRadius;
        Renderer::DrawRect(knobX, knobY, knobRadius * 2.0f, knobRadius * 2.0f,
                           ApplyOpacity(Color(1.0f, 1.0f, 1.0f, 1.0f), opacity), knobRadius,
                           0.0f, 10.0f, 0.0f, 3.0f,
                           ApplyOpacity(Color(0.0f, 0.0f, 0.0f, 0.20f), opacity));

        const std::string valueText = std::to_string(channelToInt(value));
        const float scale = 12.0f / 24.0f;
        Renderer::DrawTextStr(valueText, track.x + track.width + 14.0f, y + 4.0f,
                              ApplyOpacity(Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.72f), opacity),
                              scale);
    }

    Color value_ = Color(0.20f, 0.50f, 0.90f, 1.0f);
    std::vector<Color> colors_;
    float fontSize_ = 16.0f;
    std::function<void(Color)> onChange_;
    bool open_ = false;
    bool popupPresentation_ = false;
    float hoverAnim_ = 0.0f;
    float openAnim_ = 0.0f;
    float doneHover_ = 0.0f;
    int activeSlider_ = -1;
};

} // namespace EUINEO
