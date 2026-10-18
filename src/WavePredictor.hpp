#pragma once

// Noclip wave micro-predictor. No overlay, no mutation of GD state.
// This is an approximate static-AABB predictor, NOT GD's real collision engine.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
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

enum class Verdict {
    Unsupported,  // Can't safely predict this scenario
    AlreadySafe,  // Pressing/releasing NOW is predicted safe
    TooEarly,     // Earliest predicted collision is far outside timing window
    NoCorrection, // No delay in the window avoids the immediate obstacle
    Buffered      // delayFrames gives the predicted safest frame
};

inline char const* verdictName(Verdict v) {
    switch (v) {
        case Verdict::Unsupported: return "unsupported";
        case Verdict::AlreadySafe: return "already-safe";
        case Verdict::TooEarly: return "collision-too-far";
        case Verdict::NoCorrection: return "no-safe-frame";
        case Verdict::Buffered: return "buffered";
    }
    return "unknown";
}

struct Decision {
    bool evaluated = false;
    bool immediatePathHits = false;
    int delayFrames = 0;
    int firstCollisionFrame = -1;
    int comparedFrames = 0;
    int safeCandidates = 0;
    Verdict verdict = Verdict::Unsupported;
};

inline bool valid(Rect r) {
    return std::isfinite(r.left) && std::isfinite(r.right)
        && std::isfinite(r.bottom) && std::isfinite(r.top)
        && r.left < r.right && r.bottom < r.top;
}

// Swept point against rectangle expanded by player half-size.
// Conservative for rotated spikes, but checks in-between physics frames.
inline bool sweptHit(
    float x0, float y0, float x1, float y1, State const& s, Rect obstacle
) {
    float xLo = obstacle.left - s.halfWidth;
    float xHi = obstacle.right + s.halfWidth;
    float yLo = obstacle.bottom - s.halfHeight;
    float yHi = obstacle.top + s.halfHeight;
    float tMin = 0.f;
    float tMax = 1.f;
    auto clipAxis = [&](float start, float movement, float lo, float hi) {
        if (std::abs(movement) < 0.000001f) return start >= lo && start <= hi;
        float a = (lo - start) / movement;
        float b = (hi - start) / movement;
        if (a > b) std::swap(a, b);
        tMin = std::max(tMin, a);
        tMax = std::min(tMax, b);
        return tMin <= tMax;
    };
    return clipAxis(x0, x1 - x0, xLo, xHi)
        && clipAxis(y0, y1 - y0, yLo, yHi);
}

inline float distanceToRect(float x, float y, State const& state, Rect r) {
    float dx = std::max({r.left - state.halfWidth - x, 0.f,
                         x - r.right - state.halfWidth});
    float dy = std::max({r.bottom - state.halfHeight - y, 0.f,
                         y - r.top - state.halfHeight});
    return std::sqrt(dx * dx + dy * dy);
}

struct Trace {
    int firstCollision = -1;
    int hitIndex = -1;
    float clearance = std::numeric_limits<float>::infinity();
    bool crossesSpecial = false;
};

// Test a tap edge arriving in 0..window physics steps.
// After the edge, assume a held state until the next user input, which is
// not yet known. Later unrelated obstacles must not decide this click.
inline Trace trace(
    State const& initial, bool nextHeld, int inputAfterFrames,
    int frames, std::vector<Rect> const& obstacles,
    std::vector<Rect> const& specials = {}
) {
    Trace result;
    float x = initial.x;
    float y = initial.y;
    float dyPerFrame = std::abs(initial.dxPerFrame)
        * (initial.gravityReversed ? -1.f : 1.f);
    for (int frame = 0; frame < frames; ++frame) {
        bool held = frame >= inputAfterFrames ? nextHeld : initial.held;
        float nextX = x + initial.dxPerFrame;
        float nextY = y + (held ? dyPerFrame : -dyPerFrame);
        for (auto const& special : specials) {
            if (sweptHit(x, y, nextX, nextY, initial, special))
                result.crossesSpecial = true;
        }
        for (std::size_t i = 0; i < obstacles.size(); ++i) {
            auto const& box = obstacles[i];
            if (sweptHit(x, y, nextX, nextY, initial, box)) {
                result.firstCollision = frame;
                result.hitIndex = static_cast<int>(i);
                return result;
            }
            // Three samples approximate smallest clearance, providing a
            // sensible midpoint for a multi-frame safe interval.
            result.clearance = std::min(result.clearance,
                distanceToRect(x, y, initial, box));
            result.clearance = std::min(result.clearance,
                distanceToRect((x + nextX) * .5f,
                               (y + nextY) * .5f, initial, box));
            result.clearance = std::min(result.clearance,
                distanceToRect(nextX, nextY, initial, box));
        }
        x = nextX;
        y = nextY;
    }
    return result;
}

inline bool pathHits(
    State const& initial, bool nextHeld, int inputAfterFrames, int frames,
    std::vector<Rect> const& obstacles
) {
    return trace(initial, nextHeld, inputAfterFrames, frames, obstacles)
        .firstCollision >= 0;
}

// Pick a local obstacle window rather than demanding that a wave stays
// in the same direction through an entire multi-click zigzag.
// Only attempt corrections when the imminent collision is close enough to
// the user's frame window; do not rewrite clicks 10+ frames before a hazard.
inline Decision chooseDelay(
    State const& initial, bool nextHeld, int maxDelay, int horizonFrames,
    std::vector<Rect> const& obstacles,
    std::vector<Rect> const& specials = {}
) {
    Decision out;
    if (!std::isfinite(initial.x) || !std::isfinite(initial.y)
        || !std::isfinite(initial.dxPerFrame)
        || std::abs(initial.dxPerFrame) < .1f
        || !(initial.halfWidth > 0.f) || !(initial.halfHeight > 0.f)
        || maxDelay < 0 || maxDelay > 20
        || horizonFrames < maxDelay + 2 || horizonFrames > 120
        || obstacles.size() > 1024 || specials.size() > 1024
        || nextHeld == initial.held) return out;

    for (auto const& r : obstacles) if (!valid(r)) return out;
    for (auto const& r : specials) if (!valid(r)) return out;

    out.evaluated = true;
    Trace immediate = trace(initial, nextHeld, 0, horizonFrames, obstacles, specials);
    if (immediate.crossesSpecial) {
        out.evaluated = false;
        return out;
    }
    if (immediate.firstCollision < 0) {
        out.verdict = Verdict::AlreadySafe;
        return out;
    }
    out.immediatePathHits = true;
    out.firstCollisionFrame = immediate.firstCollision;
    if (immediate.firstCollision > maxDelay + 3) {
        out.verdict = Verdict::TooEarly;
        return out;
    }

    auto const& first = obstacles[static_cast<std::size_t>(immediate.hitIndex)];
    // Only require surviving the *current* obstacle. A collision 15 frames
    // later may simply need the next wave click, not a different timing now.
    int exitFrame = static_cast<int>(
        std::ceil((first.right + initial.halfWidth - initial.x)
                  / initial.dxPerFrame)) + 1;
    int localEnd = std::max(immediate.firstCollision + 2, exitFrame);
    localEnd = std::clamp(localEnd, 2, horizonFrames);
    localEnd = std::min(localEnd, immediate.firstCollision + maxDelay + 3);
    out.comparedFrames = localEnd;

    // Test ALL candidate delays, not just first safe. Choose the safest
    // geometric clearance, with earliest delay as a tie-breaker.
    float bestClearance = -1.f;
    int bestDelay = 0;
    for (int delay = 1; delay <= maxDelay; ++delay) {
        auto candidate = trace(initial, nextHeld, delay, localEnd,
                               obstacles, specials);
        if (candidate.firstCollision >= 0 || candidate.crossesSpecial)
            continue;
        ++out.safeCandidates;
        if (bestDelay == 0
            || candidate.clearance > bestClearance + .001f) {
            bestClearance = candidate.clearance;
            bestDelay = delay;
        }
    }
    if (bestDelay) {
        out.verdict = Verdict::Buffered;
        out.delayFrames = bestDelay;
    } else {
        out.verdict = Verdict::NoCorrection;
    }
    return out;
}

} // namespace noclip::wave
