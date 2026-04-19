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

class LineChartNode : public UINode {
public:
    class Builder : public UIBuilderBase<LineChartNode, Builder> {
    public:
        Builder(UIContext& context, LineChartNode& node) : UIBuilderBase<LineChartNode, Builder>(context, node) {}

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

        Builder& color(const Color& value) {
            this->node_.trackComposeValue("color", value);
            this->node_.lineColor_ = value;
            return *this;
        }

        Builder& fill(bool value) {
            this->node_.trackComposeValue("fill", value);
            this->node_.fill_ = value;
            return *this;
        }

        Builder& smooth(bool value) {
            this->node_.trackComposeValue("smooth", value);
            this->node_.smooth_ = value;
            return *this;
        }

        Builder& onPointClick(std::function<void(int)> handler) {
            this->node_.onPointClick_ = std::move(handler);
            return *this;
        }
    };

    explicit LineChartNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "LineChartNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    bool wantsContinuousUpdate() const override {
        for (float hover : pointHover_) {
            if (hover > 0.001f && hover < 0.999f) {
                return true;
            }
        }
        return false;
    }

    void update() override {
        ensureRuntimeState();
        hoveredIndex_ = -1;
        const RectFrame frame = PrimitiveFrame(primitive_);
        const RectFrame plot = plotFrame(frame);

        if (primitive_.enabled && containsPoint(State.mouseX, State.mouseY) && values_.size() >= 2 &&
            ChartDetail::Contains(ChartDetail::Expanded(plot, 18.0f, 20.0f), State.mouseX, State.mouseY)) {
            const float step = plot.width / static_cast<float>(values_.size() - 1);
            const int index = static_cast<int>(std::round((State.mouseX - plot.x) / std::max(1.0f, step)));
            if (index >= 0 && index < static_cast<int>(values_.size())) {
                hoveredIndex_ = index;
            }
        }

        for (int index = 0; index < static_cast<int>(pointHover_.size()); ++index) {
            const float target = index == hoveredIndex_ ? 1.0f : 0.0f;
            if (animateTowards(pointHover_[index], target, State.deltaTime * 16.0f)) {
                requestVisualRepaint(0.12f);
            }
        }

        if (hoveredIndex_ >= 0 && State.mouseClicked) {
            if (onPointClick_) {
                onPointClick_(hoveredIndex_);
            }
            State.mouseClicked = false;
        }
    }

    void draw() override {
        ensureRuntimeState();
        PrimitiveClipScope clip(primitive_);
        const RectFrame frame = PrimitiveFrame(primitive_);
        ChartDetail::DrawChartPanel(primitive_, frame);
        if (values_.size() < 2) {
            ChartDetail::DrawEmptyState(frame, primitive_, title_);
            return;
        }

        const RectFrame plot = plotFrame(frame);
        ChartDetail::DrawGrid(plot, primitive_);
        Renderer::DrawTextStr(title_, frame.x + 16.0f, frame.y + 27.0f,
                              ApplyOpacity(CurrentTheme->text, primitive_.opacity), 16.0f / 24.0f);

        const std::vector<Point2> points = makePoints(plot);
        const std::vector<Point2> path = smooth_ ? makeSmoothPath(points, plot) : points;

        if (fill_) {
            std::vector<Point2> area;
            area.reserve(path.size() + 2);
            area.push_back(Point2{path.front().x, plot.y + plot.height});
            area.insert(area.end(), path.begin(), path.end());
            area.push_back(Point2{path.back().x, plot.y + plot.height});
            const RectGradient gradient = RectGradient::Vertical(
                ChartDetail::WithAlpha(lineColor_, 0.20f),
                ChartDetail::WithAlpha(lineColor_, 0.025f)
            );
            Renderer::DrawPolygon(area, ChartDetail::WithAlpha(lineColor_, 0.12f), gradient);
        }

        ChartDetail::DrawPolyline(path, 3.0f, lineColor_, primitive_.opacity);

        for (std::size_t index = 0; index < points.size(); ++index) {
            const float hover = pointHover_[index];
            const float radius = 4.0f + hover * 2.0f;
            Renderer::DrawRect(points[index].x - radius, points[index].y - radius, radius * 2.0f, radius * 2.0f,
                               ApplyOpacity(ChartDetail::Lighten(lineColor_, hover * 0.25f), primitive_.opacity), radius);
        }

        drawXAxisLabels(plot);
        if (hoveredIndex_ >= 0 && hoveredIndex_ < static_cast<int>(points.size())) {
            ChartDetail::DrawTooltip(primitive_, points[hoveredIndex_], tooltipText(hoveredIndex_));
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
        title_ = "Line Chart";
        values_.clear();
        labels_.clear();
        lineColor_ = CurrentTheme->primary;
        fill_ = true;
        smooth_ = true;
        onPointClick_ = {};
    }

private:
    RectFrame plotFrame(const RectFrame& frame) const {
        return ChartDetail::Inset(frame, 18.0f, 48.0f, 18.0f, 30.0f);
    }

    void ensureRuntimeState() {
        if (pointHover_.size() != values_.size()) {
            pointHover_.assign(values_.size(), 0.0f);
        }
    }

    std::vector<Point2> makePoints(const RectFrame& plot) const {
        float minValue = 0.0f;
        float maxValue = 1.0f;
        ChartDetail::ResolveLineRange(values_, minValue, maxValue);

        std::vector<Point2> points;
        points.reserve(values_.size());
        const float step = plot.width / static_cast<float>(values_.size() - 1);
        for (std::size_t index = 0; index < values_.size(); ++index) {
            points.push_back(Point2{
                plot.x + step * static_cast<float>(index),
                ChartDetail::ValueY(plot, values_[index], minValue, maxValue)
            });
        }
        return points;
    }

    static Point2 catmullRom(const Point2& p0, const Point2& p1, const Point2& p2, const Point2& p3, float t) {
        const float t2 = t * t;
        const float t3 = t2 * t;
        return Point2{
            0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
                    (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                    (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
            0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t +
                    (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                    (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3)
        };
    }

    std::vector<Point2> makeSmoothPath(const std::vector<Point2>& points, const RectFrame& plot) const {
        if (points.size() < 3) {
            return points;
        }

        std::vector<Point2> path;
        path.reserve(points.size() * 10);
        path.push_back(points.front());
        for (std::size_t index = 0; index + 1 < points.size(); ++index) {
            const Point2& p0 = index == 0 ? points[index] : points[index - 1];
            const Point2& p1 = points[index];
            const Point2& p2 = points[index + 1];
            const Point2& p3 = index + 2 < points.size() ? points[index + 2] : points[index + 1];
            for (int step = 1; step <= 10; ++step) {
                Point2 point = catmullRom(p0, p1, p2, p3, static_cast<float>(step) / 10.0f);
                point.y = std::clamp(point.y, plot.y, plot.y + plot.height);
                path.push_back(point);
            }
        }
        return path;
    }

    void drawXAxisLabels(const RectFrame& plot) const {
        if (labels_.empty()) {
            return;
        }
        const float scale = 11.0f / 24.0f;
        const std::size_t count = std::min(labels_.size(), values_.size());
        if (count == 0) {
            return;
        }

        const float leftWidth = Renderer::MeasureTextWidth(labels_.front(), scale);
        Renderer::DrawTextStr(labels_.front(), plot.x - leftWidth * 0.45f, plot.y + plot.height + 18.0f,
                              ApplyOpacity(ChartDetail::SoftText(0.58f), primitive_.opacity), scale);
        if (count > 1) {
            const std::string& rightText = labels_[count - 1];
            const float rightWidth = Renderer::MeasureTextWidth(rightText, scale);
            Renderer::DrawTextStr(rightText, plot.x + plot.width - rightWidth * 0.55f, plot.y + plot.height + 18.0f,
                                  ApplyOpacity(ChartDetail::SoftText(0.58f), primitive_.opacity), scale);
        }
    }

    std::string tooltipText(int index) const {
        const std::string value = ChartDetail::FormatValue(values_[index]);
        if (index >= 0 && index < static_cast<int>(labels_.size())) {
            return labels_[index] + " " + value;
        }
        return value;
    }

    std::string title_;
    std::vector<float> values_;
    std::vector<std::string> labels_;
    Color lineColor_;
    bool fill_ = true;
    bool smooth_ = true;
    std::function<void(int)> onPointClick_;
    std::vector<float> pointHover_;
    int hoveredIndex_ = -1;
};

} // namespace EUINEO
