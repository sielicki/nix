#pragma once

/**
 * @file
 * Scratch storage with a small initial buffer and uniform overflow blocks.
 * Ordinary overflow allocations use 64 KiB blocks; larger requests increase
 * the block size to fit the requested bytes and alignment. Deallocated buffers
 * up to 4 KiB with at most max_align_t alignment are recycled in power-of-two
 * size classes. Callers must not retain references to a deallocated buffer.
 * Live allocations never move. Larger or over-aligned buffers remain reserved
 * until arena destruction, as do decoded byte runs without an individual owner.
 */

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <limits>
#include <memory>
#include <memory_resource>
#include <new>

namespace nix::lex {

class parse_arena final : public std::pmr::memory_resource
{
    struct block
    {
        block * next;
        size_t bytes;
    };

    struct free_block
    {
        free_block * next;
    };

    static constexpr size_t minPooled = 16;
    static constexpr size_t maxPooled = 4096;
    static constexpr size_t poolCount = std::bit_width(maxPooled) - std::bit_width(minPooled) + 1;
    std::array<free_block *, poolCount> freeLists{};

    static size_t poolIndex(size_t bytes)
    {
        return std::bit_width(std::bit_ceil(std::max(bytes, minPooled))) - std::bit_width(minPooled);
    }

    alignas(std::max_align_t) std::array<std::byte, 16 * 1024> initial;
    void * cursor = initial.data();
    size_t remaining = initial.size();
    block * blocks = nullptr;
    std::pmr::memory_resource * upstream;

    void * do_allocate(size_t bytes, size_t alignment) override
    {
        if (bytes <= maxPooled && alignment <= alignof(std::max_align_t)) {
            auto index = poolIndex(bytes);
            if (auto result = freeLists[index]) {
                freeLists[index] = result->next;
                return result;
            }
            bytes = minPooled << index;
            // Pooled buffers are max_align_t-aligned and large enough to
            // hold a free-list link when returned to the pool.
            alignment = alignof(std::max_align_t);
        }
        if (!std::align(alignment, bytes, cursor, remaining)) {
            if (bytes > std::numeric_limits<size_t>::max() - sizeof(block) - (alignment - 1))
                throw std::bad_alloc();
            auto size = std::max(size_t(64 * 1024), bytes + sizeof(block) + alignment - 1);
            auto storage = upstream->allocate(size, alignof(block));
            blocks = new (storage) block{blocks, size};
            cursor = static_cast<std::byte *>(storage) + sizeof(block);
            remaining = size - sizeof(block);
            std::align(alignment, bytes, cursor, remaining);
        }
        auto result = cursor;
        cursor = static_cast<std::byte *>(cursor) + bytes;
        remaining -= bytes;
        return result;
    }

    void do_deallocate(void * p, size_t bytes, size_t alignment) override
    {
        if (bytes <= maxPooled && alignment <= alignof(std::max_align_t)) {
            auto & head = freeLists[poolIndex(bytes)];
            head = new (p) free_block{head};
        }
    }

    bool do_is_equal(const std::pmr::memory_resource & other) const noexcept override
    {
        return this == &other;
    }

public:
    explicit parse_arena(std::pmr::memory_resource * upstream = std::pmr::get_default_resource())
        : upstream(upstream)
    {
    }

    ~parse_arena() override
    {
        while (blocks) {
            auto current = blocks;
            blocks = current->next;
            upstream->deallocate(current, current->bytes, alignof(block));
        }
    }

    parse_arena(const parse_arena &) = delete;
    parse_arena & operator=(const parse_arena &) = delete;
};

} // namespace nix::lex
