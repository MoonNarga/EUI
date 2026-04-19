#pragma once

#include "ChartCommon.h"
#include "../ui/UIBuilder.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace EUINEO {

class PieChartNode : public UINode {
public:
    class Builder : public UIBuilderBase<PieChartNode, Builder> {
    public:
        Builder(UIContext& context, PieChartNode& node) : UIBuilderBase<PieChartNode, Builder>(context, node) {}

        Builder& title(std::string value) {
            this->node_.trackComposeValue("title", value);
            this->node_.title_ = std::move(value);
            return *this;
        }

        Builder& values(std::vector<float> value) {
            this->node_.trackComposeValue("values", value);
            this->node_.values_ = std::move(value);
            return *this;
        }

        Builder& labels(std::vector<std::string> value) {
            this->node_.trackComposeValue("labels", value);
            this->node_.labels_ = std::move(value);
            return *this;
        }

        Builder& colors(std::vector<Color> value) {
            this->node_.trackComposeValue("colors", value);
            this->node_.colors_ = std::move(value);
            return *this;
        }

        Builder& onSliceClick(std::function<void(int)> handler) {
            this->node_.onSliceClick_ = std::move(handler);
            return *this;
        }
    };

    explicit PieChartNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "PieChartNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    bool wantsContinuousUpdate() const override {
        for (float hover : sliceHover_) {
            if (hover > 0.001f && hover < 0.999f) {
                return true;
            }
        }
        return false;
    }

    void update() override {
        ensureRuntimeState();
        hoveredIndex_ = -1;
        const float total = ChartDetail::PositiveSum(values_);
        if (total > 0.0001f && primitive_.enabled && containsPoint(State.mouseX, State.mouseY)) {
            const RectFrame frame = PrimitiveFrame(primitive_);
            Point2 center{};
            float radius = 0.0f;
            resolvePie(frame, center, radius);
            const float dx = State.mouseX - center.x;
            const float dy = State.mouseY - center.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance <= radius + 8.0f) {
                hoveredIndex_ = sliceIndexAt(std::atan2(dy, dx), total);
            }
        }

        for (int index = 0; index < static_cast<int>(sliceHover_.size()); ++index) {
            const float target = index == hoveredIndex_ ? 1.0f : 0.0f;
            if (animateTowards(sliceHover_[index], target, State.deltaTime * 16.0f)) {
                requestVisualRepaint(0.12f);
            }
        }

        if (hoveredIndex_ >= 0 && State.mouseClicked) {
            if (onSliceClick_) {
                onSliceClick_(hoveredIndex_);
            }
            State.mouseClicked = false;
        }
    }

    void draw() override {
        ensureRuntimeState();
        PrimitiveClipScope clip(primitive_);
        const RectFrame frame = PrimitiveFrame(primitive_);
        ChartDetail::DrawChartPanel(primitive_, frame);
        Renderer::DrawTextStr(title_, frame.x + 16.0f, frame.y + 27.0f,
                              ApplyOpacity(CurrentTheme->text, primitive_.opacity), 16.0f / 24.0f);

        const float total = ChartDetail::PositiveSum(values_);
        if (total <= 0.0001f) {
            ChartDetail::DrawEmptyState(frame, primitive_, title_);
            return;
        }

        Point2 center{};
        float radius = 0.0f;
        resolvePie(frame, center, radius);
        float cursor = startAngle();
        for (int index = 0; index < static_cast<int>(values_.size()); ++index) {
            const float share = std::max(0.0f, values_[index]) / total;
            if (share <= 0.0f) {
                continue;
            }
            const float next = cursor + share * ChartDetail::TwoPi;
            drawSlice(center, radius, cursor, next, index);
            cursor = next;
        }

        if (hoveredIndex_ >= 0) {
            ChartDetail::DrawTooltip(primitive_, Point2{State.mouseX, State.mouseY}, tooltipText(hoveredIndex_, total));
        }
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        primitive_.width = 260.0f;
        primitive_.height = 180.0f;
        primitive_.rounding = 16.0f;
        primitive_.background = CurrentTheme->surface;
        primitive_.borderWidth = 1.0f;
        primitive_.borderColor = CurrentTheme->border;
        primitive_.shadow.blur = CurrentTheme == &DarkTheme ? 12.0f : 8.0f;
        primitive_.shadow.offsetY = CurrentTheme == &DarkTheme ? 6.0f : 4.0f;
        primitive_.shadow.color = CurrentTheme == &DarkTheme
            ? Color(0.0f, 0.0f, 0.0f, 0.20f)
            : Color(0.10f, 0.14f, 0.22f, 0.10f);
        title_ = "Pie Chart";
        values_.clear();
        labels_.clear();
        colors_.clear();
        onSliceClick_ = {};
    }

private:
    static float startAngle() {
        return -ChartDetail::Pi * 0.5f;
    }

    void ensureRuntimeState() {
        if (sliceHover_.size() != values_.size()) {
            sliceHover_.assign(values_.size(), 0.0f);
        }
    }

    void resolvePie(const RectFrame& frame, Point2& center, float& radius) const {
        const float available = std::min(frame.width - 42.0f, frame.height - 64.0f);
        radius = std::max(24.0f, available * 0.5f);
        center.x = frame.x + frame.width * 0.5f;
        center.y = frame.y + 46.0f + (frame.height - 62.0f) * 0.5f;
    }

    int sliceIndexAt(float absoluteAngle, float total) const {
        float angle = absoluteAngle - startAngle();
        while (angle < 0.0f) {
            angle += ChartDetail::TwoPi;
        }
        while (angle >= ChartDetail::TwoPi) {
            angle -= ChartDetail::TwoPi;
        }

        float cursor = 0.0f;
        for (int index = 0; index < static_cast<int>(values_.size()); ++index) {
            const float share = std::max(0.0f, values_[index]) / total;
            const float next = cursor + share * ChartDetail::TwoPi;
            if (angle >= cursor && angle <= next) {
                return index;
            }
            cursor = next;
        }
        return -1;
    }

    void drawSlice(const Point2& center, float radius, float start, float end, int index) const {
        const float hover = index < static_cast<int>(sliceHover_.size()) ? sliceHover_[index] : 0.0f;
        const float mid = (start + end) * 0.5f;
        const float offset = hover * 5.0f;
        const Point2 shiftedCenter{
            center.x + std::cos(mid) * offset,
            center.y + std::sin(mid) * offset
        };
        const float segmentRadius = radius + hover * 3.0f;
        const int steps = std::max(8, static_cast<int>((end - start) * segmentRadius / 7.0f));

        std::vector<Point2> points;
        points.reserve(static_cast<std::size_t>(steps) + 2);
        points.push_back(shiftedCenter);
        for (int step = 0; step <= steps; ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(steps);
            const float angle = Lerp(start, end, t);
            points.push_back(Point2{
                shiftedCenter.x + std::cos(angle) * segmentRadius,
                shiftedCenter.y + std::sin(angle) * segmentRadius
            });
        }

        Color color = ChartDetail::PaletteColor(colors_, static_cast<std::size_t>(index));
        color = ChartDetail::Lighten(color, hover * 0.18f);
        Renderer::DrawPolygon(points, ApplyOpacity(color, primitive_.opacity), 1.0f,
                              ApplyOpacity(ChartDetail::WithAlpha(CurrentTheme->surface, 0.70f), primitive_.opacity));
    }

    std::string tooltipText(int index, float total) const {
        const float percent = total > 0.0f ? std::max(0.0f, values_[index]) / total * 100.0f : 0.0f;
        const std::string value = ChartDetail::FormatValue(percent) + "%";
        if (index >= 0 && index < static_cast<int>(labels_.size())) {
            return labels_[index] + " " + value;
        }
        return value;
    }

    std::string title_;
    std::vector<float> values_;
    std::vector<std::string> labels_;
    std::vector<Color> colors_;
    std::function<void(int)> onSliceClick_;
    std::vector<float> sliceHover_;
    int hoveredIndex_ = -1;
};

} // namespace EUINEO
