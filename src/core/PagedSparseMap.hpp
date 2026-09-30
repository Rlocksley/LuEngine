// paged_sparse_map.hpp
// O(1) integer-key -> integer-value map with no hashing, no per-element allocation.
#pragma once

#include <concepts>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

namespace Lu {
    namespace Core{
/// Maps an unsigned integer key (e.g. entity index) to a Value, using lazily
/// allocated fixed-size pages. Lookup = two dependent loads, no hashing, no probing.
/// Memory is proportional to the number of *touched pages*, so sparse or large
/// entity ids don't blow up memory the way one flat vector would.
template <std::unsigned_integral Key,
          std::unsigned_integral Value,
          std::size_t PageBits = 12>              // 4096 entries per page
class PagedSparseMap {
public:
    static constexpr Value       kInvalid  = std::numeric_limits<Value>::max();
    static constexpr std::size_t kPageSize = std::size_t{1} << PageBits;
    static constexpr std::size_t kPageMask = kPageSize - 1;

    /// Value for `key`, or kInvalid if absent.
    [[nodiscard]] Value get(Key key) const noexcept {
        const std::size_t p = static_cast<std::size_t>(key) >> PageBits;
        if (p >= pages_.size()) [[unlikely]] return kInvalid;
        const Value* page = pages_[p].get();
        return page ? page[key & kPageMask] : kInvalid;
    }

    [[nodiscard]] bool contains(Key key) const noexcept { return get(key) != kInvalid; }

    /// Insert or overwrite. `value` must not be kInvalid.
    void set(Key key, Value value) {
        const std::size_t p = static_cast<std::size_t>(key) >> PageBits;
        if (p >= pages_.size()) pages_.resize(p + 1);
        if (!pages_[p]) {
            pages_[p] = std::make_unique<Value[]>(kPageSize);
            std::fill_n(pages_[p].get(), kPageSize, kInvalid);
        }
        pages_[p][key & kPageMask] = value;
    }

    void erase(Key key) noexcept {
        const std::size_t p = static_cast<std::size_t>(key) >> PageBits;
        if (p < pages_.size() && pages_[p]) pages_[p][key & kPageMask] = kInvalid;
    }

    void clear() noexcept { pages_.clear(); }

private:
    std::vector<std::unique_ptr<Value[]>> pages_;
};

}  // namespace Core
}  // namespace Lu
