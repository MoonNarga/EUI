#pragma once

#include "../EUINEO.h"
#include "../ui/ThemeTokens.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace EUINEO {
namespace ChartDetail {

constexpr float Pi = 3.14159265358979323846f;
constexpr float TwoPi = Pi * 2.0f;

inline bool Contains(const RectFrame& frame, float x, float y) {
    return x >= frame.x && x <= frame.x + frame.width &&
           y >= frame.y && y <= frame.y + frame.height;
}

inline RectFrame Inset(const RectFrame& frame, float left, float top, float right, float bottom) {
    return RectFrame{
        frame.x + left,
        frame.y + top,
        std::max(0.0f, frame.width - left - right),
        std::max(0.0f, frame.height - top - bottom)
    };
}

inline RectFrame Expanded(const RectFrame& frame, float x, float y) {
    return RectFrame{frame.x - x, frame.y - y, frame.width + x * 2.0f, frame.height + y * 2.0f};
}

inline Color WithAlpha(Color color, float alpha) {
    color.a = alpha;
    return color;
}

inline Color SoftText(float alpha) {
    return Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, alpha);
}

inline Color Lighten(const Color& color, float amount) {
    return Lerp(color, Color(1.0f, 1.0f, 1.0f, color.a), amount);
}

inline std::string FormatValue(float value) {
    const float rounded = std::round(value);
    if (std::abs(value - rounded) < 0.05f) {
        return std::to_string(static_cast<int>(rounded));
    }

    std::string text = std::to_string(value);
    const std::size_t dot = text.find('.');
    if (dot != std::string::npos && dot + 2 < text.size()) {
        text.resize(dot + 2);
    }
    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text.empty() ? "0" : text;
}

inline std::vector<Color> DefaultPalette() {
    return {
        CurrentTheme->primary,
        Color(0.18f, 0.78f, 0.60f, 1.0f),
        Color(0.96f, 0.62f, 0.18f, 1.0f),
        Color(0.88f, 0.30f, 0.46f, 1.0f),
        Color(0.48f, 0.42f, 0.96f, 1.0f),
        Color(0.20f, 0.70f, 0.92f, 1.0f),
    };
}

inline Color PaletteColor(const std::vector<Color>& colors, std::size_t index) {
    if (!colors.empty()) {
        return colors[index % colors.size()];
    }
    const std::vector<Color> palette = DefaultPalette();
    return palette[index % palette.size()];
}

inline float PositiveSum(const std::vector<float>& values) {
    float sum = 0.0f;
    for (float value : values) {
        sum += std::max(0.0f, value);
    }
    return sum;
}

inline void ResolveLineRange(const std::vector<float>& values, float& outMin, float& outMax) {
    if (values.empty()) {
        outMin = 0.0f;
        outMax = 1.0f;
        return;
    }

    auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
    outMin = *minIt;
    outMax = *maxIt;
    float span = outMax - outMin;
    if (std::abs(span) < 0.0001f) {
        span = std::max(1.0f, std::abs(outMax));
    }
    const float padding = span * 0.20f;
    outMin -= padding;
    outMax += padding;
}

inline void ResolveBarRange(const std::vector<float>& values, float& outMin, float& outMax) {
    if (values.empty()) {
        outMin = 0.0f;
        outMax = 1.0f;
        return;
    }

    auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
    outMin = std::min(0.0f, *minIt);
    outMax = std::max(0.0f, *maxIt);
    if (std::abs(outMax - outMin) < 0.0001f) {
        outMax += 1.0f;
    }
    outMax += (outMax - outMin) * 0.08f;
}

inline float ValueY(const RectFrame& plot, float value, float minValue, float maxValue) {
    const float t = (value - minValue) / std::max(0.0001f, maxValue - minValue);
    return plot.y + plot.height * (1.0f - std::clamp(t, 0.0f, 1.0f));
}

inline void DrawChartPanel(const UIPrimitive& primitive, const RectFrame& frame) {
    const float borderWidth = std::max(0.0f, primitive.borderWidth);
    const float radius = primitive.rounding > 0.0f ? primitive.rounding : 16.0f;
    RectStyle outer = MakeStyle(primitive);
    outer.color = ApplyOpacity(
        borderWidth > 0.0f ? primitive.borderColor : (primitive.background.a > 0.0f ? primitive.background : CurrentTheme->surface),
        primitive.opacity
    );
    outer.rounding = radius;
    Renderer::DrawRect(frame.x, frame.y, frame.width, frame.height, outer);

    if (borderWidth <= 0.0f || frame.width <= borderWidth * 2.0f || frame.height <= borderWidth * 2.0f) {
        return;
    }

    RectStyle inner = outer;
    inner.color = ApplyOpacity(primitive.background.a > 0.0f ? primitive.background : CurrentTheme->surface, primitive.opacity);
    inner.shadowBlur = 0.0f;
    inner.shadowColor = Color(0.0f, 0.0f, 0.0f, 0.0f);
    inner.rounding = std::max(0.0f, radius - borderWidth);
    Renderer::DrawRect(
        frame.x + borderWidth,
        frame.y + borderWidth,
        frame.width - borderWidth * 2.0f,
        frame.height - borderWidth * 2.0f,
        inner
    );
}

inline void DrawGrid(const RectFrame& plot, const UIPrimitive& primitive) {
    const Color line = ApplyOpacity(WithAlpha(CurrentTheme->border, CurrentTheme == &DarkTheme ? 0.34f : 0.45f), primitive.opacity);
    for (int index = 0; index <= 3; ++index) {
        const float y = plot.y + plot.height * (static_cast<float>(index) / 3.0f);
        Renderer::DrawRect(plot.x, y, plot.width, 1.0f, line, 0.0f);
    }
    Renderer::DrawRect(plot.x, plot.y + plot.height, plot.width, 1.0f,
                       ApplyOpacity(WithAlpha(CurrentTheme->text, 0.20f), primitive.opacity), 0.0f);
}

inline void DrawSegment(const Point2& start, const Point2& end, float thickness, const Color& color, float opacity) {
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.001f) {
        return;
    }

    RectStyle style;
    style.color = ApplyOpacity(color, opacity);
    style.rounding = thickness * 0.5f;
    style.transform.rotationDegrees = std::atan2(dy, dx) * 180.0f / Pi;
    Renderer::DrawRect(
        (start.x + end.x) * 0.5f - length * 0.5f,
        (start.y + end.y) * 0.5f - thickness * 0.5f,
        length,
        thickness,
        style
    );
}

inline Point2 SegmentNormal(const Point2& start, const Point2& end) {
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.0001f) {
        return Point2{0.0f, -1.0f};
    }
    return Point2{-dy / length, dx / length};
}

inline float Dot(const Point2& a, const Point2& b) {
    return a.x * b.x + a.y * b.y;
}

inline std::vector<Point2> BuildStrokePolygon(const std::vector<Point2>& points, float thickness) {
    std::vector<Point2> polygon;
    if (points.size() < 2 || thickness <= 0.0f) {
        return polygon;
    }

    const float half = thickness * 0.5f;
    std::vector<Point2> left;
    std::vector<Point2> right;
    left.reserve(points.size());
    right.reserve(points.size());

    for (std::size_t index = 0; index < points.size(); ++index) {
        Point2 offset{};
        if (index == 0) {
            const Point2 normal = SegmentNormal(points[index], points[index + 1]);
            offset = Point2{normal.x * half, normal.y * half};
        } else if (index + 1 == points.size()) {
            const Point2 normal = SegmentNormal(points[index - 1], points[index]);
            offset = Point2{normal.x * half, normal.y * half};
        } else {
            const Point2 prevNormal = SegmentNormal(points[index - 1], points[index]);
            const Point2 nextNormal = SegmentNormal(points[index], points[index + 1]);
            Point2 miter{prevNormal.x + nextNormal.x, prevNormal.y + nextNormal.y};
            const float miterLength = std::sqrt(miter.x * miter.x + miter.y * miter.y);
            if (miterLength <= 0.0001f) {
                offset = Point2{nextNormal.x * half, nextNormal.y * half};
            } else {
                miter.x /= miterLength;
                miter.y /= miterLength;
                const float denom = std::max(0.35f, std::abs(Dot(miter, nextNormal)));
                const float scale = std::min(half / denom, half * 2.6f);
                offset = Point2{miter.x * scale, miter.y * scale};
            }
        }

        left.push_back(Point2{points[index].x + offset.x, points[index].y + offset.y});
        right.push_back(Point2{points[index].x - offset.x, points[index].y - offset.y});
    }

    polygon.reserve(left.size() + right.size());
    polygon.insert(polygon.end(), left.begin(), left.end());
    for (auto it = right.rbegin(); it != right.rend(); ++it) {
        polygon.push_back(*it);
    }
    return polygon;
}

inline void DrawPolyline(const std::vector<Point2>& points, float thickness, const Color& color, float opacity) {
    if (points.size() < 2) {
        return;
    }
    const std::vector<Point2> stroke = BuildStrokePolygon(points, thickness);
    if (stroke.size() >= 3) {
        Renderer::DrawPolygon(stroke, ApplyOpacity(color, opacity));
    }
    const float radius = thickness * 0.5f;
    Renderer::DrawRect(points.front().x - radius, points.front().y - radius, thickness, thickness,
                       ApplyOpacity(color, opacity), radius);
    Renderer::DrawRect(points.back().x - radius, points.back().y - radius, thickness, thickness,
                       ApplyOpacity(color, opacity), radius);
}

inline void DrawEmptyState(const RectFrame& frame, const UIPrimitive& primitive, const std::string& title) {
    Renderer::DrawTextStr(title, frame.x + 16.0f, frame.y + 26.0f,
                          ApplyOpacity(CurrentTheme->text, primitive.opacity), 15.0f / 24.0f);
    Renderer::DrawTextStr("No data", frame.x + 16.0f, frame.y + frame.height * 0.56f,
                          ApplyOpacity(SoftText(0.52f), primitive.opacity), 14.0f / 24.0f);
}

inline void DrawTooltip(const UIPrimitive& primitive, const Point2& anchor, const std::string& text) {
    const float scale = 12.0f / 24.0f;
    const float textWidth = Renderer::MeasureTextWidth(text, scale);
    const float width = textWidth + 16.0f;
    const float height = 24.0f;
    float x = anchor.x - width * 0.5f;
    float y = anchor.y - height - 10.0f;
    x = std::clamp(x, 6.0f, std::max(6.0f, State.screenW - width - 6.0f));
    y = std::clamp(y, 6.0f, std::max(6.0f, State.screenH - height - 6.0f));

    UIPrimitive popup = primitive;
    popup.opacity = primitive.opacity;
    RectStyle style = MakePopupChromeStyle(popup, 8.0f);
    Renderer::DrawRect(x, y, width, height, style);
    Renderer::DrawTextStr(text, x + 8.0f, y + 16.5f, ApplyOpacity(CurrentTheme->text, primitive.opacity), scale);
}

} // namespace ChartDetail
} // namespace EUINEO
