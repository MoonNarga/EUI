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

namespace EUINEO {

class TimePickerNode : public UINode {
public:
    class Builder : public UIBuilderBase<TimePickerNode, Builder> {
    public:
        Builder(UIContext& context, TimePickerNode& node) : UIBuilderBase<TimePickerNode, Builder>(context, node) {}

        Builder& time(int hour, int minute) {
            this->node_.trackComposeValue("hour", hour);
            this->node_.trackComposeValue("minute", minute);
            this->node_.hour_ = std::clamp(hour, 0, 23);
            this->node_.minute_ = std::clamp(minute, 0, 59);
            return *this;
        }

        Builder& minuteStep(int value) {
            this->node_.trackComposeValue("minuteStep", value);
            this->node_.minuteStep_ = std::clamp(value, 1, 30);
            return *this;
        }

        Builder& fontSize(float value) {
            this->node_.trackComposeValue("fontSize", value);
            this->node_.fontSize_ = std::max(12.0f, value);
            return *this;
        }

        Builder& onChange(std::function<void(int, int)> handler) {
            this->node_.onChange_ = std::move(handler);
            return *this;
        }
    };

    explicit TimePickerNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "TimePickerNode";
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
        return draggingColumn_ >= 0 ||
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
            draggingColumn_ = -1;
            return;
        }

        const PickerLayout layout = makeLayout();
        const bool inDone = open_ && contains(layout.doneButton, State.mouseX, State.mouseY);
        if (animateTowards(doneHover_, inDone ? 1.0f : 0.0f, State.deltaTime * 18.0f)) {
            requestVisualRepaint(0.12f);
        }

        if (open_ && !State.scrollConsumed && std::abs(State.scrollDeltaY) > 0.001f) {
            const int column = columnAt(layout, State.mouseX, State.mouseY);
            if (column >= 0) {
                const int direction = State.scrollDeltaY > 0.0f ? -1 : 1;
                stepColumn(column, direction);
                State.scrollConsumed = true;
                requestVisualRepaint(0.16f);
            }
        }

        if (draggingColumn_ >= 0 && State.mouseDown) {
            const int delta = static_cast<int>(std::round((dragStartY_ - State.mouseY) / layout.rowHeight));
            setColumnValue(draggingColumn_, dragStartValue_ + delta);
            requestVisualRepaint(0.12f);
        }
        if (draggingColumn_ >= 0 && !State.mouseDown) {
            draggingColumn_ = -1;
            requestVisualRepaint(0.12f);
        }

        if (!open_ || !State.mouseClicked) {
            return;
        }

        if (inDone) {
            open_ = false;
            draggingColumn_ = -1;
            State.mouseClicked = false;
            requestVisualRepaint(0.18f);
            return;
        }

        if (!contains(layout.panel, State.mouseX, State.mouseY)) {
            open_ = false;
            draggingColumn_ = -1;
            State.mouseClicked = false;
            requestVisualRepaint(0.18f);
            return;
        }

        const int column = columnAt(layout, State.mouseX, State.mouseY);
        if (column >= 0) {
            const int offset = rowOffsetAt(layout, State.mouseY);
            if (offset != 0) {
                stepColumn(column, offset);
            }
            draggingColumn_ = column;
            dragStartY_ = State.mouseY;
            dragStartValue_ = columnValue(column);
            State.mouseClicked = false;
            requestVisualRepaint(0.16f);
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
        primitive_.width = 160.0f;
        primitive_.height = 36.0f;
        primitive_.rounding = 8.0f;
        hour_ = 9;
        minute_ = 30;
        minuteStep_ = 1;
        fontSize_ = 16.0f;
        onChange_ = {};
    }

private:
    struct PickerLayout {
        RectFrame overlay;
        RectFrame panel;
        RectFrame doneButton;
        RectFrame hourColumn;
        RectFrame minuteColumn;
        RectFrame periodColumn;
        float centerY = 0.0f;
        float rowHeight = 42.0f;
    };

    static bool contains(const RectFrame& frame, float x, float y) {
        return x >= frame.x && x <= frame.x + frame.width &&
               y >= frame.y && y <= frame.y + frame.height;
    }

    static int wrapValue(int value, int minValue, int maxValue) {
        const int span = maxValue - minValue + 1;
        if (span <= 0) {
            return minValue;
        }
        int shifted = (value - minValue) % span;
        if (shifted < 0) {
            shifted += span;
        }
        return minValue + shifted;
    }

    static std::string formatTime(int hour, int minute) {
        char buffer[8] = {};
        std::snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, minute);
        return std::string(buffer);
    }

    static std::string twoDigits(int value) {
        char buffer[8] = {};
        std::snprintf(buffer, sizeof(buffer), "%02d", value);
        return std::string(buffer);
    }

    RectFrame overlayFrame() const {
        return RectFrame{0.0f, 0.0f, State.screenW, State.screenH};
    }

    PickerLayout makeLayout() const {
        PickerLayout layout;
        layout.overlay = overlayFrame();
        const float width = std::max(304.0f, std::min(360.0f, State.screenW - 24.0f));
        const float height = std::max(256.0f, std::min(300.0f, State.screenH - 24.0f));
        layout.panel = RectFrame{
            std::max(12.0f, (State.screenW - width) * 0.5f),
            std::max(12.0f, (State.screenH - height) * 0.5f),
            width,
            height
        };

        layout.doneButton = RectFrame{layout.panel.x + layout.panel.width - 78.0f, layout.panel.y + 16.0f, 58.0f, 30.0f};
        layout.rowHeight = 42.0f;
        layout.centerY = layout.panel.y + 170.0f;

        const float gap = 12.0f;
        const float wheelX = layout.panel.x + 24.0f;
        const float wheelY = layout.panel.y + 72.0f;
        const float wheelH = 194.0f;
        const float colW = (layout.panel.width - 48.0f - gap * 2.0f) / 3.0f;
        layout.hourColumn = RectFrame{wheelX, wheelY, colW, wheelH};
        layout.minuteColumn = RectFrame{wheelX + colW + gap, wheelY, colW, wheelH};
        layout.periodColumn = RectFrame{wheelX + (colW + gap) * 2.0f, wheelY, colW, wheelH};
        return layout;
    }

    int resolvedMinuteStep() const {
        return std::clamp(minuteStep_, 1, 30);
    }

    int resolvedMinuteCount() const {
        const int step = resolvedMinuteStep();
        return std::max(1, (60 + step - 1) / step);
    }

    int displayHour12() const {
        const int h = hour_ % 12;
        return h == 0 ? 12 : h;
    }

    bool isPm() const {
        return hour_ >= 12;
    }

    int columnAt(const PickerLayout& layout, float x, float y) const {
        if (contains(layout.hourColumn, x, y)) {
            return 0;
        }
        if (contains(layout.minuteColumn, x, y)) {
            return 1;
        }
        if (contains(layout.periodColumn, x, y)) {
            return 2;
        }
        return -1;
    }

    int rowOffsetAt(const PickerLayout& layout, float y) const {
        return std::clamp(static_cast<int>(std::round((y - layout.centerY) / layout.rowHeight)), -3, 3);
    }

    int columnValue(int column) const {
        if (column == 0) {
            return displayHour12();
        }
        if (column == 1) {
            return minute_ / resolvedMinuteStep();
        }
        return isPm() ? 1 : 0;
    }

    void setColumnValue(int column, int value) {
        int nextHour = hour_;
        int nextMinute = minute_;
        if (column == 0) {
            const int hour12 = wrapValue(value, 1, 12);
            nextHour = isPm()
                ? (hour12 == 12 ? 12 : hour12 + 12)
                : (hour12 == 12 ? 0 : hour12);
        } else if (column == 1) {
            const int index = wrapValue(value, 0, resolvedMinuteCount() - 1);
            nextMinute = std::clamp(index * resolvedMinuteStep(), 0, 59);
        } else {
            const bool nextPm = wrapValue(value, 0, 1) == 1;
            const int hour12 = displayHour12();
            nextHour = nextPm
                ? (hour12 == 12 ? 12 : hour12 + 12)
                : (hour12 == 12 ? 0 : hour12);
        }
        applyTime(nextHour, nextMinute);
    }

    void stepColumn(int column, int delta) {
        setColumnValue(column, columnValue(column) + delta);
    }

    void applyTime(int hour, int minute) {
        hour = std::clamp(hour, 0, 23);
        minute = std::clamp(minute, 0, 59);
        if (hour_ == hour && minute_ == minute) {
            return;
        }
        hour_ = hour;
        minute_ = minute;
        if (onChange_) {
            onChange_(hour_, minute_);
        }
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
        const float textScale = fontSize_ / 24.0f;
        Renderer::DrawTextStr(formatTime(hour_, minute_),
                              frame.x + 12.0f,
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

        Renderer::DrawTextStr("Time", layout.panel.x + 22.0f, layout.panel.y + 37.0f,
                              ApplyOpacity(CurrentTheme->text, opacity), 20.0f / 24.0f);
        drawDoneButton(layout.doneButton, opacity);
        drawWheelColumn(layout.hourColumn, layout.centerY, layout.rowHeight, 0, opacity);
        drawWheelColumn(layout.minuteColumn, layout.centerY, layout.rowHeight, 1, opacity);
        drawWheelColumn(layout.periodColumn, layout.centerY, layout.rowHeight, 2, opacity);
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

    void drawWheelColumn(const RectFrame& frame, float centerY, float rowHeight, int column, float opacity) const {
        Renderer::DrawRect(frame.x, frame.y, frame.width, frame.height,
                           ApplyOpacity(Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.045f), opacity),
                           18.0f);

        const float bandY = centerY - rowHeight * 0.5f;
        Renderer::DrawRect(frame.x + 6.0f, bandY, frame.width - 12.0f, rowHeight,
                           ApplyOpacity(Color(CurrentTheme->primary.r, CurrentTheme->primary.g, CurrentTheme->primary.b, 0.10f), opacity),
                           10.0f);
        Renderer::DrawRect(frame.x + 12.0f, bandY, frame.width - 24.0f, 1.0f,
                           ApplyOpacity(Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.20f), opacity),
                           0.0f);
        Renderer::DrawRect(frame.x + 12.0f, bandY + rowHeight - 1.0f, frame.width - 24.0f, 1.0f,
                           ApplyOpacity(Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.20f), opacity),
                           0.0f);

        UIPrimitive clipPrimitive = primitive_;
        clipPrimitive.hasClipRect = true;
        clipPrimitive.clipRect = UIClipRect{frame.x, frame.y, frame.width, frame.height};
        PrimitiveClipScope clip(clipPrimitive);

        for (int offset = -2; offset <= 2; ++offset) {
            const std::string text = itemText(column, columnValue(column), offset);
            const float distance = static_cast<float>(std::abs(offset));
            const float font = offset == 0 ? 25.0f : 16.0f;
            const float scale = font / 24.0f;
            const float itemOpacity = opacity * std::clamp(1.0f - distance * 0.22f, 0.22f, 1.0f);
            const float textWidth = Renderer::MeasureTextWidth(text, scale);
            const float textX = frame.x + (frame.width - textWidth) * 0.5f;
            const float textY = centerY + static_cast<float>(offset) * rowHeight + font * 0.25f;
            Color color = offset == 0 ? CurrentTheme->text : Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.66f);
            Renderer::DrawTextStr(text, textX, textY, ApplyOpacity(color, itemOpacity), scale);
        }
    }

    std::string itemText(int column, int value, int offset) const {
        if (column == 0) {
            return std::to_string(wrapValue(value + offset, 1, 12));
        }
        if (column == 1) {
            const int index = wrapValue(value + offset, 0, resolvedMinuteCount() - 1);
            return twoDigits(std::clamp(index * resolvedMinuteStep(), 0, 59));
        }
        return wrapValue(value + offset, 0, 1) == 1 ? "PM" : "AM";
    }

    int hour_ = 9;
    int minute_ = 30;
    int minuteStep_ = 1;
    float fontSize_ = 16.0f;
    std::function<void(int, int)> onChange_;
    bool open_ = false;
    bool popupPresentation_ = false;
    float hoverAnim_ = 0.0f;
    float openAnim_ = 0.0f;
    float doneHover_ = 0.0f;
    int draggingColumn_ = -1;
    float dragStartY_ = 0.0f;
    int dragStartValue_ = 0;
};

} // namespace EUINEO
