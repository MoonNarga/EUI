#pragma once

#include "ChartCommon.h"
#include "../ui/UIBuilder.h"
#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace EUINEO {

class BarChartNode : public UINode {
public:
    class Builder : public UIBuilderBase<BarChartNode, Builder> {
    public:
        Builder(UIContext& context, BarChartNode& node) : UIBuilderBase<BarChartNode, Builder>(context, node) {}

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

        Builder& onBarClick(std::function<void(int)> handler) {
            this->node_.onBarClick_ = std::move(handler);
            return *this;
        }
    };

    explicit BarChartNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "BarChartNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    bool wantsContinuousUpdate() const override {
        for (float hover : barHover_) {
            if (hover > 0.001f && hover < 0.999f) {
                return true;
            }
        }
        return false;
    }

    void update() override {
        ensureRuntimeState();
        hoveredIndex_ = -1;
        if (primitive_.enabled && containsPoint(State.mouseX, State.mouseY)) {
            const RectFrame frame = PrimitiveFrame(primitive_);
            const RectFrame plot = plotFrame(frame);
            for (int index = 0; index < static_cast<int>(values_.size()); ++index) {
                if (ChartDetail::Contains(barSlot(plot, index), State.mouseX, State.mouseY)) {
                    hoveredIndex_ = index;
                    break;
                }
            }
        }

        for (int index = 0; index < static_cast<int>(barHover_.size()); ++index) {
            const float target = index == hoveredIndex_ ? 1.0f : 0.0f;
            if (animateTowards(barHover_[index], target, State.deltaTime * 16.0f)) {
                requestVisualRepaint(0.12f);
            }
        }

        if (hoveredIndex_ >= 0 && State.mouseClicked) {
            if (onBarClick_) {
                onBarClick_(hoveredIndex_);
            }
            State.mouseClicked = false;
        }
    }

    void draw() override {
        ensureRuntimeState();
        PrimitiveClipScope clip(primitive_);
        const RectFrame frame = PrimitiveFrame(primitive_);
        ChartDetail::DrawChartPanel(primitive_, frame);
        if (values_.empty()) {
            ChartDetail::DrawEmptyState(frame, primitive_, title_);
            return;
        }

        const RectFrame plot = plotFrame(frame);
        ChartDetail::DrawGrid(plot, primitive_);
        Renderer::DrawTextStr(title_, frame.x + 16.0f, frame.y + 27.0f,
                              ApplyOpacity(CurrentTheme->text, primitive_.opacity), 16.0f / 24.0f);

        float minValue = 0.0f;
        float maxValue = 1.0f;
        ChartDetail::ResolveBarRange(values_, minValue, maxValue);
        std::vector<RectFrame> bars;
        bars.reserve(values_.size());

        for (int index = 0; index < static_cast<int>(values_.size()); ++index) {
            const RectFrame slot = barSlot(plot, index);
            const float y = ChartDetail::ValueY(plot, values_[index], minValue, maxValue);
            const RectFrame bar{
                slot.x,
                y,
                slot.width,
                std::max(2.0f, plot.y + plot.height - y)
            };
            bars.push_back(bar);
            const float hover = barHover_[index];
            Color color = ChartDetail::PaletteColor(colors_, static_cast<std::size_t>(index));
            color = ChartDetail::Lighten(color, hover * 0.20f);
            Renderer::DrawRect(bar.x, bar.y, bar.width, bar.height,
                               ApplyOpacity(color, primitive_.opacity), std::min(8.0f, bar.width * 0.35f));
        }

        drawLabels(plot);
        if (hoveredIndex_ >= 0 && hoveredIndex_ < static_cast<int>(bars.size())) {
            const RectFrame bar = bars[hoveredIndex_];
            ChartDetail::DrawTooltip(
                primitive_,
                Point2{bar.x + bar.width * 0.5f, bar.y},
                tooltipText(hoveredIndex_)
            );
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
        title_ = "Bar Chart";
        values_.clear();
        labels_.clear();
        colors_.clear();
        onBarClick_ = {};
    }

private:
    RectFrame plotFrame(const RectFrame& frame) const {
        return ChartDetail::Inset(frame, 18.0f, 48.0f, 18.0f, 30.0f);
    }

    RectFrame barSlot(const RectFrame& plot, int index) const {
        const int count = std::max(1, static_cast<int>(values_.size()));
        const float slotWidth = plot.width / static_cast<float>(count);
        const float gap = std::min(10.0f, slotWidth * 0.28f);
        return RectFrame{
            plot.x + slotWidth * static_cast<float>(index) + gap * 0.5f,
            plot.y,
            std::max(2.0f, slotWidth - gap),
            plot.height
        };
    }

    void ensureRuntimeState() {
        if (barHover_.size() != values_.size()) {
            barHover_.assign(values_.size(), 0.0f);
        }
    }

    void drawLabels(const RectFrame& plot) const {
        const int count = std::min(static_cast<int>(labels_.size()), static_cast<int>(values_.size()));
        const float scale = 11.0f / 24.0f;
        for (int index = 0; index < count; ++index) {
            const RectFrame slot = barSlot(plot, index);
            const float textWidth = Renderer::MeasureTextWidth(labels_[index], scale);
            Renderer::DrawTextStr(labels_[index], slot.x + (slot.width - textWidth) * 0.5f, plot.y + plot.height + 18.0f,
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
    std::vector<Color> colors_;
    std::function<void(int)> onBarClick_;
    std::vector<float> barHover_;
    int hoveredIndex_ = -1;
};

} // namespace EUINEO
