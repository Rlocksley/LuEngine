// made by Claude Sonnet 5.5 (free chat on claude.ai) this is crazy
// triple_buffer.hpp
// Lock-free, wait-free single-producer / single-consumer triple buffer (C++20).
#pragma once

#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace Lu {
namespace Core{
// Fixed value (not std::hardware_destructive_interference_size) to keep the ABI stable.
inline constexpr std::size_t kCacheLine = 64;

/// Triple buffer for exactly ONE writer thread and ONE reader thread.
///
/// Guarantees
///  - Neither side ever blocks or spins (wait-free, no allocation after construction).
///  - The reader always sees a *complete* snapshot, never a half-written one.
///  - The reader always gets the *most recent* published snapshot; older
///    unread snapshots are dropped (latest-value semantics, not a queue).
///
/// Usage contract
///  - Writer thread only: write_slot(), publish(), write(), push()
///  - Reader thread only: update(), read_slot(), read_if_new(), consume()
///  - After publish(), the slot you get from write_slot() holds the data of
///    an OLDER frame (buffers rotate). The writer must fully overwrite
///    whatever the reader is going to interpret. Containers may keep their
///    capacity, so steady state is allocation-free.
template <typename T>
class TripleBuffer {
public:
    using value_type = T;

    /// Default-construct all three slots.
    TripleBuffer() requires std::default_initializable<T>
        : TripleBuffer(std::in_place) {}

    /// Copy `initial` into all three slots (reader sees `initial` before the first publish).
    explicit TripleBuffer(const T& initial) requires std::copy_constructible<T>
        : TripleBuffer(std::in_place, initial) {}

    /// Construct every slot in place from the same arguments.
    template <typename... Args>
    explicit TripleBuffer(std::in_place_t, const Args&... args)
        : slots_{{Slot(std::in_place, args...),
                  Slot(std::in_place, args...),
                  Slot(std::in_place, args...)}} {}

    TripleBuffer(const TripleBuffer&)            = delete;
    TripleBuffer& operator=(const TripleBuffer&) = delete;
    TripleBuffer(TripleBuffer&&)                 = delete;
    TripleBuffer& operator=(TripleBuffer&&)      = delete;

    // ------------------------------------------------------------------ writer

    /// Slot owned exclusively by the writer. Valid until the next publish().
    [[nodiscard]] T& write_slot() noexcept { return slots_[write_idx_].value; }

    /// Make the current write slot visible to the reader and acquire a new one.
    void publish() noexcept {
        // release: slot contents happen-before the reader's acquire in update().
        // acquire: we take over a slot the reader has finished with.
        const std::uint8_t old = middle_.exchange(
            static_cast<std::uint8_t>(write_idx_ | kDirty), std::memory_order_acq_rel);
        write_idx_ = static_cast<std::uint8_t>(old & kIndexMask);
    }

    /// Convenience: fill the slot via callable, then publish.
    template <std::invocable<T&> F>
    void write(F&& fn) noexcept(std::is_nothrow_invocable_v<F, T&>) {
        std::forward<F>(fn)(write_slot());
        publish();
    }

    /// Convenience: assign a value, then publish.
    template <typename U>
        requires std::assignable_from<T&, U>
    void push(U&& value) {
        write_slot() = std::forward<U>(value);
        publish();
    }

    // ------------------------------------------------------------------ reader

    /// Fetch the newest published snapshot, if any. Returns true if the
    /// slot returned by read_slot() changed.
    [[nodiscard]] bool update() noexcept {
        // Only the writer sets the dirty bit and only we clear it, so a relaxed
        // pre-check is safe and keeps the common "nothing new" path read-only.
        if (!(middle_.load(std::memory_order_relaxed) & kDirty)) return false;

        const std::uint8_t old =
            middle_.exchange(read_idx_, std::memory_order_acq_rel);
        read_idx_ = static_cast<std::uint8_t>(old & kIndexMask);
        return true;
    }

    /// Slot owned exclusively by the reader. Stable until the next update().
    [[nodiscard]] const T& read_slot() const noexcept { return slots_[read_idx_].value; }

    /// update() + invoke callable with the snapshot only if new data arrived.
    template <std::invocable<const T&> F>
    bool read_if_new(F&& fn) {
        if (!update()) return false;
        std::forward<F>(fn)(read_slot());
        return true;
    }

    /// True if a newer snapshot is waiting (reader thread only).
    [[nodiscard]] bool has_new() const noexcept {
        return (middle_.load(std::memory_order_relaxed) & kDirty) != 0;
    }

private:
    static constexpr std::uint8_t kDirty     = 0x80;
    static constexpr std::uint8_t kIndexMask = 0x03;

    struct alignas(kCacheLine) Slot {
        template <typename... Args>
        explicit Slot(std::in_place_t, const Args&... args) : value(args...) {}
        T value;
    };

    static_assert(std::atomic<std::uint8_t>::is_always_lock_free);

    std::array<Slot, 3> slots_;

    // Each index lives on its own cache line to avoid false sharing.
    alignas(kCacheLine) std::uint8_t write_idx_ = 0;                 // writer-private
    alignas(kCacheLine) std::uint8_t read_idx_  = 2;                 // reader-private
    alignas(kCacheLine) std::atomic<std::uint8_t> middle_{1};        // shared hand-off
};




}  // namespace Core
} // namespace Lu