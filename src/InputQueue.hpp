#pragma once

// This module is independent of Geode.  It owns ordering and scheduling of
// virtual jump edges; it never decides whether a predicted timing is safe.
#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <optional>

namespace noclip {

struct Edge {
    bool pressed = false;
    bool player1 = true;
    int button = 1;
    std::uint64_t received = 0;
    std::uint64_t due = 0;
};

class InputQueue {
public:
    struct State {
        std::deque<Edge> pending;
        bool rawHeld = false;
        bool deliveredHeld = false;
        unsigned overflowCount = 0;
    };
    static constexpr std::size_t kMaxPending = 32;
    std::array<State, 2> players;

    void clear() {
        players = {};
    }

    bool hasPending(int index) const {
        return !players[index].pending.empty();
    }

    // Call for *every* player jump edge.  A false return means forward that
    // event unchanged.  On overflow, the caller must flush() before forwarding.
    bool intercept(bool pressed, bool player1, int button,
                   std::uint64_t step, int predictedDelay) {
        auto& s = players[player1 ? 0 : 1];
        s.rawHeld = pressed;
        if (s.pending.empty() && predictedDelay <= 0) {
            s.deliveredHeld = pressed;
            return false;
        }
        if (s.pending.size() >= kMaxPending) {
            ++s.overflowCount;
            return false;
        }
        Edge edge{pressed, player1, button, step, step};
        if (s.pending.empty()) {
            edge.due = step + static_cast<std::uint64_t>(predictedDelay);
        } else {
            // A press already in flight must fire before a release.  Preserve
            // the physical press/release duration (minimum one physics tick).
            auto const& last = s.pending.back();
            auto elapsed = step > last.received ? step - last.received : 1u;
            edge.due = std::max(step, last.due + std::max<std::uint64_t>(1, elapsed));
        }
        s.pending.push_back(edge);
        return true;
    }

    std::optional<Edge> popDue(int index, std::uint64_t step) {
        auto& s = players[index];
        if (s.pending.empty() || s.pending.front().due > step)
            return std::nullopt;
        Edge front = s.pending.front();
        s.pending.pop_front();
        s.deliveredHeld = front.pressed;
        return front;
    }

    // Used on dual mode, mode transitions, disabled mods, and other unknown
    // states. Deliver pending edges in order, never silently discard an input.
    std::optional<Edge> popFirst(int index) {
        auto& s = players[index];
        if (s.pending.empty()) return std::nullopt;
        Edge front = s.pending.front();
        s.pending.pop_front();
        s.deliveredHeld = front.pressed;
        return front;
    }
};

} // namespace noclip