#pragma once
#include <algorithm>
#include <cstdint>
#include <deque>

namespace buffer {
struct Edge {
    bool down = false;
    bool p1 = true;
    int button = 1;
    std::uint64_t received = 0;
    std::uint64_t due = 0;
};

class Queue {
    std::deque<Edge> m_queue;
public:
    void clear() { m_queue.clear(); }
    bool empty() const { return m_queue.empty(); }
    std::size_t size() const { return m_queue.size(); }
    // Never collapse a press and release into a zero-length tap.
    void push(Edge e) {
        if (!m_queue.empty()) {
            auto const& previous = m_queue.back();
            e.due = std::max(e.due, previous.due + static_cast<std::uint64_t>(previous.down != e.down ? 1 : 0));
        }
        m_queue.push_back(e);
    }
    // Does not lose late edges: caller can always send when expired.
    template<class Dispatch>
    void flush(Dispatch&& fn) {
        while (!m_queue.empty()) {
            Edge e = m_queue.front();
            m_queue.pop_front();
            fn(e);
        }
    }
    template<class Dispatch>
    void fireDue(std::uint64_t tick, Dispatch&& fn) {
        while (!m_queue.empty() && m_queue.front().due <= tick) {
            Edge e = m_queue.front();
            m_queue.pop_front();
            fn(e);
        }
    }
};

inline std::uint64_t fixedTarget(std::uint64_t now, unsigned n) {
    return now + n;
}
inline std::uint64_t gridTarget(std::uint64_t now, unsigned n) {
    if (n == 0) return now;
    // Align to the next N-step grid boundary, not to an imaginary safe frame.
    auto v = ((now + n - 1) / n) * n;
    return v;
}
}
