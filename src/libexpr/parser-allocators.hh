#pragma once
/**
 * @file
 * Long-lived PMR cursors for constructing expressions on different threads.
 */

#include "nix/util/sync.hh"
#include <map>
#include <memory_resource>
#include <thread>

namespace nix {

/**
 * Repeated forCurrentThread() calls return the same monotonic resource for
 * that thread. Exprs owns this map so callers can reuse unused chunk space
 * across file parses, including after an exception.
 *
 * The map lock covers lookup/creation only. Each returned allocator may be used
 * only on its requesting thread. Separate threads have independent cursors;
 * the upstream resource must support concurrent chunk allocations. Map insertion
 * does not invalidate existing resources. Reuse of a departed thread's ID is
 * safe because its cursor is no longer in use.
 *
 * Resources are retained until destruction, even after a thread exits, because
 * expressions may still reference their chunks. The owner must outlive every
 * allocation and active parsing thread. Do not retain a thread-specific
 * allocator in a container that may later allocate on another thread.
 */
class ParserAllocators
{
    std::pmr::memory_resource & upstream;
    Sync<std::map<std::thread::id, std::pmr::monotonic_buffer_resource>> resources;

public:
    explicit ParserAllocators(std::pmr::memory_resource & upstream)
        : upstream(upstream)
    {
    }

    std::pmr::polymorphic_allocator<char> forCurrentThread()
    {
        auto locked = resources.lock();
        auto [entry, inserted] = locked->try_emplace(std::this_thread::get_id(), 256, &upstream);
        return {&entry->second};
    }
};

} // namespace nix
