#pragma once

// Small, dependency-free, read-only trajectory scorer. It never modifies
// Geometry Dash state. This model is intentionally limited to classic wave
// motion through currently static rectangular hazard hitboxes.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace noclip::wave {

struct Rect {
    float left = 0.f;
    float bottom = 0.f;
    float right = 0.f;
    float top = 0.f;
};

struct State {
    float x = 0.f;
    float y = 0.f;
    float dxPerFrame = 0.f;
    float halfWidth = 3.5f;
    float halfHeight = 3.5f;
    bool held = false;
    bool gravityReversed = false;
};

struct Decision {
    bool evaluated = false;
    bool immediatePathHits = false;
    int delayFrames = 0; // 0 means forward now; never exceed maxDelay.
};

inline bool valid(Rect r) {
    return std::isfinite(r.left) && std::isfinite(r.right)
        && std::isfinite(r.bottom) && std::isfinite(r.top)
        && r.left < r.right && r.bottom < r.top;
}

inline bool overlaps(Rect a, Rect b) {
    return a.left < b.right && b.left < a.right
        && a.bottom < b.top && b.bottom < a.top;
}

// Continuous segment against expanded AABB. Expanding the obstacle by the
// wave hitbox dimensions catches in-between-frame collisions without swept
// AABB corner false positives on diagonal wave motion.
inline bool sweptHit(
    float x0, float y0, float x1, float y1, State const& s, Rect hazard
) {
    float loX = hazard.left - s.halfWidth;
    float hiX = hazard.right + s.halfWidth;
    float loY = hazard.bottom - s.halfHeight;
    float hiY = hazard.top + s.halfHeight;
    float tMin = 0.f;
    float tMax = 1.f;
    auto clipAxis = [&](float start, float delta, float lo, float hi) {
        if (std::abs(delta) < .000001f) return start >= lo && start <= hi;
        float a = (lo - start) / delta;
        float b = (hi - start) / delta;
        if (a > b) std::swap(a, b);
        tMin = std::max(tMin, a);
        tMax = std::min(tMax, b);
        return tMin <= tMax;
    };
    return clipAxis(x0, x1 - x0, loX, hiX)
        && clipAxis(y0, y1 - y0, loY, hiY);
}

inline bool pathHits(
    State const& initial, bool nextHeld, int inputAfterFrames,
    int horizonFrames, std::vector<Rect> const& hazards
) {
    float x = initial.x;
    float y = initial.y;
    const float rise = std::abs(initial.dxPerFrame);
    for (int frame = 0; frame < horizonFrames; ++frame) {
        bool held = frame >= inputAfterFrames ? nextHeld : initial.held;
        float xNext = x + initial.dxPerFrame;
        float yNext = y + (held ? 1.f : -1.f)
            * (initial.gravityReversed ? -1.f : 1.f) * rise;
        for (auto const& hazard : hazards) {
            if (sweptHit(x, y, xNext, yNext, initial, hazard)) return true;
        }
        x = xNext;
        y = yNext;
    }
    return false;
}

// Never delay an already-safe trajectory. Only delay if changing the input
// now predicts collision AND a later input stays clear throughout the horizon.
// Select the earliest safe delay. If none exists, let the original input pass.
inline Decision chooseDelay(
    State const& initial, bool nextHeld, int maxDelay, int horizonFrames,
    std::vector<Rect> const& hazards
) {
    if (!std::isfinite(initial.x) || !std::isfinite(initial.y)
        || !std::isfinite(initial.dxPerFrame)
        || std::abs(initial.dxPerFrame) < .1f
        || initial.halfWidth <= 0.f || initial.halfHeight <= 0.f
        || maxDelay < 0 || horizonFrames < maxDelay + 2
        || horizonFrames > 120 || maxDelay > 20
        || hazards.size() > 1024 || nextHeld == initial.held) {
        return {};
    }
    for (auto const& rect : hazards) {
        if (!valid(rect)) return {};
    }

    Decision out;
    out.evaluated = true;
    out.immediatePathHits = pathHits(initial, nextHeld, 0, horizonFrames, hazards);
    if (!out.immediatePathHits) return out;
    for (int delay = 1; delay <= maxDelay; ++delay) {
        if (!pathHits(initial, nextHeld, delay, horizonFrames, hazards)) {
            out.delayFrames = delay;
            return out;
        }
    }
    return out;
}

} // namespace noclip::wave
