// dirty_tracker.hpp
// Tracks ids that must be re-uploaded for N frames in flight.
#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Lu {
    namespace Core{

/// Replaces `unordered_map<Id, framesRemaining>`.
///  - state_[id]  : flat per-id counter + "is in list" flag (O(1) lookup)
///  - list_       : dense list of currently dirty ids (cache-friendly iteration)
/// Removal (cancel) is lazy: the entry is dropped during the next collect().
template <std::unsigned_integral Id>
class DirtyTracker {
public:
    explicit DirtyTracker(std::uint8_t framesInFlight) : frames_(framesInFlight) {}

    /// (Re)start the countdown for `id`. Adds it to the list only once.
    void mark(Id id) {
        if (id >= state_.size()) [[unlikely]] state_.resize(std::size_t{id} + 1);
        State& s = state_[id];
        s.frames = frames_;
        if (!s.listed) {
            s.listed = 1;
            list_.push_back(id);
        }
    }

    /// Stop tracking `id` (e.g. on destroy). Safe if id was never marked.
    void cancel(Id id) noexcept {
        if (id < state_.size()) state_[id].frames = 0;   // dropped in next collect()
    }

    /// Calls emit(id) once per dirty id, decrements counters, drops expired ones.
    /// Single pass, in-place compaction, no allocation.
    template <std::invocable<Id> F>
    void collect(F&& emit) {
        std::size_t keep = 0;
        for (std::size_t i = 0, n = list_.size(); i < n; ++i) {
            const Id id = list_[i];
            State& s = state_[id];
            if (s.frames == 0) { s.listed = 0; continue; }     // cancelled
            emit(id);
            if (--s.frames == 0) { s.listed = 0; continue; }   // expired
            list_[keep++] = id;
        }
        list_.erase(list_.begin() + static_cast<std::ptrdiff_t>(keep), list_.end());
    }

    [[nodiscard]] std::size_t size() const noexcept { return list_.size(); }

    void reserve(std::size_t n) { state_.reserve(n); list_.reserve(n); }

private:
    struct State { std::uint8_t frames = 0; std::uint8_t listed = 0; };

    std::uint8_t       frames_;
    std::vector<State> state_;
    std::vector<Id>    list_;
};

}  // namespace Core
}  // namespace Lu