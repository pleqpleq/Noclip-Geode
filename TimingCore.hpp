#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace noclip {

// This is deliberately geometry-independent: no claim of predicting the
// objectively perfect frame. A target has to have been learned/recorded.
struct Anchor {
    std::string levelKey;
    bool player1 = true;
    bool down = true;
    int mode = 0;
    float x = 0.f;
};

struct Deferred {
    bool down = true;
    int button = 1;
    bool player1 = true;
    bool hasTarget = false;
    float targetX = 0.f;
    int direction = 1;
    int receivedStep = 0;
    int expiresStep = 0;
};

inline int normalizeDirection(float distancePerStep) {
    return distancePerStep < 0.f ? -1 : 1;
}

inline bool isWithinEarlyWindow(
    float nowX, float targetX, float distancePerStep, int windowFrames
) {
    if (windowFrames <= 0 || !std::isfinite(distancePerStep)
        || std::abs(distancePerStep) < .03f) {
        return false;
    }
    float ahead = (targetX - nowX) * normalizeDirection(distancePerStep);
    float step = std::abs(distancePerStep);
    // Don't delay presses already at/past the target, nor far-away ones.
    return ahead > .20f * step && ahead <= windowFrames * step + .25f * step;
}

inline bool shouldDispatch(
    Deferred const& event, float currentX, float distancePerStep, int step
) {
    if (step >= event.expiresStep) return true;
    if (!event.hasTarget) return false;
    if (!std::isfinite(currentX)) return true;
    // Snap to the learned location, allowing small float/physics variance.
    float tolerance = .10f * std::abs(distancePerStep);
    return (currentX - event.targetX) * event.direction >= -tolerance;
}

}  // namespace noclip
