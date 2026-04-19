#pragma once

#include "../EUINEO.h"
#include "../ui/ThemeTokens.h"
#include "../ui/UIBuilder.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>

namespace EUINEO {

class DatePickerNode : public UINode {
public:
    class Builder : public UIBuilderBase<DatePickerNode, Builder> {
    public:
        Builder(UIContext& context, DatePickerNode& node) : UIBuilderBase<DatePickerNode, Builder>(context, node) {}

        Builder& date(int year, int month, int day) {
            this->node_.trackComposeValue("year", year);
            this->node_.trackComposeValue("month", month);
            this->node_.trackComposeValue("day", day);
            this->node_.selectedYear_ = std::clamp(year, 1900, 2200);
            this->node_.selectedMonth_ = std::clamp(month, 1, 12);
            this->node_.selectedDay_ = std::clamp(day, 1, daysInMonth(this->node_.selectedYear_, this->node_.selectedMonth_));
            return *this;
        }

        Builder& fontSize(float value) {
            this->node_.trackComposeValue("fontSize", value);
            this->node_.fontSize_ = std::max(12.0f, value);
            return *this;
        }

        Builder& onChange(std::function<void(int, int, int)> handler) {
            this->node_.onChange_ = std::move(handler);
            return *this;
        }
    };

    explicit DatePickerNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "DatePickerNode";
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
        primitive_.width = 180.0f;
        primitive_.height = 36.0f;
        primitive_.rounding = 8.0f;
        selectedYear_ = 2026;
        selectedMonth_ = 4;
        selectedDay_ = 19;
        fontSize_ = 16.0f;
        onChange_ = {};
    }

private:
    struct PickerLayout {
        RectFrame overlay;
        RectFrame panel;
        RectFrame doneButton;
        RectFrame monthColumn;
        RectFrame dayColumn;
        RectFrame yearColumn;
        float centerY = 0.0f;
        float rowHeight = 42.0f;
    };

    static bool contains(const RectFrame& frame, float x, float y) {
        return x >= frame.x && x <= frame.x + frame.width &&
               y >= frame.y && y <= frame.y + frame.height;
    }

    static bool isLeapYear(int year) {
        return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    }

    static int daysInMonth(int year, int month) {
        static constexpr std::array<int, 12> days{{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}};
        if (month == 2 && isLeapYear(year)) {
            return 29;
        }
        return days[static_cast<std::size_t>(std::clamp(month, 1, 12) - 1)];
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

    static std::string formatDate(int year, int month, int day) {
        char buffer[16] = {};
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
        return std::string(buffer);
    }

    static const char* monthName(int month) {
        static constexpr std::array<const char*, 12> months{{
            "January", "February", "March", "April", "May", "June",
            "July", "August", "September", "October", "November", "December"
        }};
        return months[static_cast<std::size_t>(std::clamp(month, 1, 12) - 1)];
    }

    RectFrame overlayFrame() const {
        return RectFrame{0.0f, 0.0f, State.screenW, State.screenH};
    }

    PickerLayout makeLayout() const {
        PickerLayout layout;
        layout.overlay = overlayFrame();
        const float width = std::max(320.0f, std::min(430.0f, State.screenW - 24.0f));
        const float height = std::max(260.0f, std::min(308.0f, State.screenH - 24.0f));
        layout.panel = RectFrame{
            std::max(12.0f, (State.screenW - width) * 0.5f),
            std::max(12.0f, (State.screenH - height) * 0.5f),
            width,
            height
        };

        layout.doneButton = RectFrame{layout.panel.x + layout.panel.width - 78.0f, layout.panel.y + 16.0f, 58.0f, 30.0f};
        layout.rowHeight = 42.0f;
        layout.centerY = layout.panel.y + 172.0f;

        const float gap = 10.0f;
        const float wheelX = layout.panel.x + 22.0f;
        const float wheelY = layout.panel.y + 72.0f;
        const float wheelH = 196.0f;
        const float monthW = std::max(116.0f, (layout.panel.width - 44.0f - gap * 2.0f) * 0.43f);
        const float dayW = std::max(70.0f, (layout.panel.width - 44.0f - gap * 2.0f) * 0.22f);
        const float yearW = std::max(88.0f, layout.panel.width - 44.0f - gap * 2.0f - monthW - dayW);
        layout.monthColumn = RectFrame{wheelX, wheelY, monthW, wheelH};
        layout.dayColumn = RectFrame{wheelX + monthW + gap, wheelY, dayW, wheelH};
        layout.yearColumn = RectFrame{wheelX + monthW + gap + dayW + gap, wheelY, yearW, wheelH};
        return layout;
    }

    int columnAt(const PickerLayout& layout, float x, float y) const {
        if (contains(layout.monthColumn, x, y)) {
            return 0;
        }
        if (contains(layout.dayColumn, x, y)) {
            return 1;
        }
        if (contains(layout.yearColumn, x, y)) {
            return 2;
        }
        return -1;
    }

    int rowOffsetAt(const PickerLayout& layout, float y) const {
        return std::clamp(static_cast<int>(std::round((y - layout.centerY) / layout.rowHeight)), -3, 3);
    }

    int columnValue(int column) const {
        if (column == 0) {
            return selectedMonth_;
        }
        if (column == 1) {
            return selectedDay_;
        }
        return selectedYear_;
    }

    void setColumnValue(int column, int value) {
        int nextYear = selectedYear_;
        int nextMonth = selectedMonth_;
        int nextDay = selectedDay_;
        if (column == 0) {
            nextMonth = wrapValue(value, 1, 12);
            nextDay = std::min(nextDay, daysInMonth(nextYear, nextMonth));
        } else if (column == 1) {
            nextDay = wrapValue(value, 1, daysInMonth(nextYear, nextMonth));
        } else {
            nextYear = std::clamp(value, 1900, 2200);
            nextDay = std::min(nextDay, daysInMonth(nextYear, nextMonth));
        }
        applyDate(nextYear, nextMonth, nextDay);
    }

    void stepColumn(int column, int delta) {
        setColumnValue(column, columnValue(column) + delta);
    }

    void applyDate(int year, int month, int day) {
        year = std::clamp(year, 1900, 2200);
        month = std::clamp(month, 1, 12);
        day = std::clamp(day, 1, daysInMonth(year, month));
        if (selectedYear_ == year && selectedMonth_ == month && selectedDay_ == day) {
            return;
        }
        selectedYear_ = year;
        selectedMonth_ = month;
        selectedDay_ = day;
        if (onChange_) {
            onChange_(selectedYear_, selectedMonth_, selectedDay_);
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
        Renderer::DrawTextStr(formatDate(selectedYear_, selectedMonth_, selectedDay_),
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

        Renderer::DrawTextStr("Date", layout.panel.x + 22.0f, layout.panel.y + 37.0f,
                              ApplyOpacity(CurrentTheme->text, opacity), 20.0f / 24.0f);
        drawDoneButton(layout.doneButton, opacity);
        drawWheelColumn(layout.monthColumn, layout.centerY, layout.rowHeight, 0, opacity);
        drawWheelColumn(layout.dayColumn, layout.centerY, layout.rowHeight, 1, opacity);
        drawWheelColumn(layout.yearColumn, layout.centerY, layout.rowHeight, 2, opacity);
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
            if (text.empty()) {
                continue;
            }
            const float distance = static_cast<float>(std::abs(offset));
            const float font = Lerp(15.0f, 24.0f, offset == 0 ? 1.0f : 0.0f);
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
            return monthName(wrapValue(value + offset, 1, 12));
        }
        if (column == 1) {
            return std::to_string(wrapValue(value + offset, 1, daysInMonth(selectedYear_, selectedMonth_)));
        }
        const int year = value + offset;
        if (year < 1900 || year > 2200) {
            return std::string{};
        }
        return std::to_string(year);
    }

    int selectedYear_ = 2026;
    int selectedMonth_ = 4;
    int selectedDay_ = 19;
    float fontSize_ = 16.0f;
    std::function<void(int, int, int)> onChange_;
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
