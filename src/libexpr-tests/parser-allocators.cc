#include "nix/expr/tests/libexpr.hh"
#include "../libexpr/lexy/values/storage.hh"
#include "../libexpr/parser-allocators.hh"
#include <barrier>
#include <cstring>
#include <map>
#include <tuple>

namespace nix {

TEST(LexyArenaTest, AlignmentLifetimeAndRelease)
{
    struct Resource : std::pmr::memory_resource
    {
        std::map<void *, size_t> outstanding;

        void * do_allocate(size_t bytes, size_t alignment) override
        {
            auto p = std::pmr::new_delete_resource()->allocate(bytes, alignment);
            outstanding.emplace(p, bytes);
            return p;
        }

        void do_deallocate(void * p, size_t bytes, size_t alignment) override
        {
            EXPECT_EQ(outstanding.at(p), bytes);
            outstanding.erase(p);
            std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
        }

        bool do_is_equal(const std::pmr::memory_resource & other) const noexcept override
        {
            return this == &other;
        }
    } upstream;

    {
        nix::lex::parse_arena arena(&upstream);
        std::vector<std::tuple<unsigned char *, size_t, size_t>> allocations;
        for (size_t alignment : {1, 2, 8, 16, 64, 256, 4096}) {
            for (size_t size : {0, 1, 17, 16384, 65536, 131073}) {
                auto p = static_cast<unsigned char *>(arena.allocate(size, alignment));
                EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % alignment, 0u);
                std::memset(p, 0xab, size);
                allocations.emplace_back(p, size, alignment);
            }
        }
        for (auto [p, size, alignment] : allocations) {
            EXPECT_TRUE(std::all_of(p, p + size, [](auto byte) { return byte == 0xab; }));
            arena.deallocate(p, size, alignment);
        }
        EXPECT_THROW((void) arena.allocate(std::numeric_limits<size_t>::max(), 16), std::bad_alloc);
        EXPECT_NE(arena.allocate(32, 16), nullptr);
        EXPECT_FALSE(upstream.outstanding.empty());
    }
    EXPECT_TRUE(upstream.outstanding.empty());
}

TEST(LexyArenaTest, RecyclesReleasedBuffersWithoutOverwritingLiveValues)
{
    nix::lex::parse_arena arena;
    for (size_t size : {1, 7, 16, 17, 33, 128, 4096}) {
        auto live = static_cast<unsigned char *>(arena.allocate(size, alignof(std::max_align_t)));
        std::memset(live, 0xab, size);
        auto reusable = arena.allocate(size, 1);
        ASSERT_NE(live, reusable);
        arena.deallocate(reusable, size, 1);
        for (int i = 0; i < 1000; ++i) {
            auto next = arena.allocate(size, alignof(std::max_align_t));
            EXPECT_EQ(next, reusable);
            std::memset(next, 0xcd, size);
            EXPECT_TRUE(std::all_of(live, live + size, [](auto c) { return c == 0xab; }));
            arena.deallocate(next, size, alignof(std::max_align_t));
        }
        arena.deallocate(live, size, alignof(std::max_align_t));
    }
}

TEST(LexyArenaTest, NativeBatchesReuseStorageAcrossParsesAndUnwind)
{
    struct Owner : std::pmr::memory_resource
    {
        std::pmr::monotonic_buffer_resource storage;
        size_t allocations = 0, deallocations = 0;

        void * do_allocate(size_t bytes, size_t alignment) override
        {
            ++allocations;
            return storage.allocate(bytes, alignment);
        }

        void do_deallocate(void *, size_t, size_t) override
        {
            ++deallocations;
        }

        bool do_is_equal(const std::pmr::memory_resource & other) const noexcept override
        {
            return this == &other;
        }
    } owner;

    {
        ParserAllocators arenas(owner);
        std::vector<uint64_t *> values;
        try {
            for (uint64_t i = 0; i < 10000; ++i) {
                // Each acquisition models a separate file parse. Growing the
                // native tree must not require a new upstream chunk per file.
                auto parse = arenas.forCurrentThread();
                values.push_back(parse.new_object<uint64_t>(i));
            }
            throw std::runtime_error("unwind the parser");
        } catch (const std::runtime_error &) {
        }
        EXPECT_LT(owner.allocations, 100u);
        EXPECT_EQ(owner.deallocations, 0u);
        auto later = arenas.forCurrentThread();
        auto bytes = later.allocate_bytes(100000, 256);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(bytes) % 256, 0u);
        std::memset(bytes, 0xcd, 100000);
        EXPECT_EQ(owner.deallocations, 0u);
        for (uint64_t i = 0; i < values.size(); ++i)
            EXPECT_EQ(*values[i], i);
    }
    EXPECT_EQ(owner.deallocations, owner.allocations);
}

TEST(LexyArenaTest, NativeBatchesHaveIndependentThreadCursors)
{
    Exprs owner;
    std::array<std::pmr::memory_resource *, 4> resources{};
    std::array<std::vector<uint64_t *>, 4> values;
    std::barrier start{4};
    {
        std::vector<std::jthread> threads;
        for (size_t t = 0; t < values.size(); ++t) {
            threads.emplace_back([&, t] {
                auto alloc = owner.parserAllocator();
                resources[t] = alloc.resource();
                start.arrive_and_wait();
                for (uint64_t i = 0; i < 5000; ++i)
                    values[t].push_back(alloc.new_object<uint64_t>(t * 5000 + i));
            });
        }
    }
    // Native values remain live after their allocating threads have exited.
    for (size_t t = 0; t < values.size(); ++t) {
        for (size_t u = t + 1; u < values.size(); ++u)
            EXPECT_NE(resources[t], resources[u]);
        for (uint64_t i = 0; i < values[t].size(); ++i)
            EXPECT_EQ(*values[t][i], t * 5000 + i);
    }
    Exprs other;
    EXPECT_NE(owner.parserAllocator().resource(), other.parserAllocator().resource());
}

} // namespace nix
