#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <string>
#include <vector>

#include "VkEngine/Pool.h"

TEST_CASE("Create pool", "[Pool]")
{
    constexpr uint32_t size = 10;
    Pool<std::string> pool(size);

    const std::vector<uint32_t> expectedFreeSlots = {};

    REQUIRE(pool.freeSlotsIndices() == expectedFreeSlots);
    REQUIRE(pool.get({0, 1}) == nullptr);
}

// ---------------------------------------------------------------------------
// create / get
// ---------------------------------------------------------------------------

TEST_CASE("Created item can be retrieved", "[Pool]")
{
    Pool<std::string> pool(4);
    const auto handle = pool.create(std::string{"item"});

    REQUIRE(pool.get(handle) != nullptr);
    REQUIRE(*pool.get(handle) == "item");
}

TEST_CASE("Default handle is never valid", "[Pool]")
{
    Pool<std::string> pool(4);

    REQUIRE(pool.get(Handle<std::string>{}) == nullptr);

    pool.create(std::string{"item"});

    REQUIRE(pool.get(Handle<std::string>{}) == nullptr);
}

TEST_CASE("Handles to slots that were never created are invalid", "[Pool]")
{
    Pool<std::string> pool(4);
    pool.create(std::string{"item"});

    REQUIRE(pool.get({1, 1}) == nullptr);
    REQUIRE(pool.get({3, 1}) == nullptr);
    REQUIRE(pool.get({100, 1}) == nullptr);
}

TEST_CASE("Each create gets its own slot", "[Pool]")
{
    Pool<std::string> pool(4);
    const auto first = pool.create(std::string{"first"});
    const auto second = pool.create(std::string{"second"});

    REQUIRE(first.index != second.index);
    REQUIRE(*pool.get(first) == "first");
    REQUIRE(*pool.get(second) == "second");
}

TEST_CASE("Pool can grow past its reserved capacity", "[Pool]")
{
    Pool<std::string> pool(1);
    const auto first = pool.create(std::string{"first"});
    const auto second = pool.create(std::string{"second"});

    REQUIRE(*pool.get(first) == "first");
    REQUIRE(*pool.get(second) == "second");
}

// ---------------------------------------------------------------------------
// retire
// ---------------------------------------------------------------------------

TEST_CASE("Retire returns the stored object", "[Pool][retire]")
{
    Pool<std::string> pool(4);
    const auto handle = pool.create(std::string{"item"});

    const auto retired = pool.retire(handle);

    REQUIRE(retired.has_value());
    REQUIRE(*retired == "item");
}

TEST_CASE("Retired handle is no longer accessible", "[Pool][retire]")
{
    Pool<std::string> pool(4);
    const auto handle = pool.create(std::string{"item"});

    pool.retire(handle);

    REQUIRE(pool.get(handle) == nullptr);
}

TEST_CASE("Retired slot is not reused before free", "[Pool][retire]")
{
    Pool<std::string> pool(4);
    const auto retiredHandle = pool.create(std::string{"retired"});
    pool.retire(retiredHandle);

    const auto newHandle = pool.create(std::string{"new"});

    REQUIRE(newHandle.index != retiredHandle.index);
    REQUIRE(*pool.get(newHandle) == "new");
}

TEST_CASE("Retiring the same handle twice fails the second time", "[Pool][retire]")
{
    Pool<std::string> pool(4);
    const auto handle = pool.create(std::string{"item"});

    REQUIRE(pool.retire(handle).has_value());
    REQUIRE_FALSE(pool.retire(handle).has_value());
}

TEST_CASE("Retiring an invalid handle returns nothing", "[Pool][retire]")
{
    Pool<std::string> pool(4);
    const auto handle = pool.create(std::string{"item"});

    REQUIRE_FALSE(pool.retire(Handle<std::string>{}).has_value());
    REQUIRE_FALSE(pool.retire({handle.index, handle.gen + 1}).has_value());
    REQUIRE_FALSE(pool.retire({100, 1}).has_value());

    // the live item is untouched
    REQUIRE(pool.get(handle) != nullptr);
    REQUIRE(*pool.get(handle) == "item");
}

TEST_CASE("Retiring one item does not affect the others", "[Pool][retire]")
{
    Pool<std::string> pool(4);
    const auto first = pool.create(std::string{"first"});
    const auto second = pool.create(std::string{"second"});

    pool.retire(first);

    REQUIRE(pool.get(first) == nullptr);
    REQUIRE(pool.get(second) != nullptr);
    REQUIRE(*pool.get(second) == "second");
}

// ---------------------------------------------------------------------------
// free
// ---------------------------------------------------------------------------

TEST_CASE("Freed slot is reused by the next create", "[Pool][free]")
{
    Pool<std::string> pool(4);
    const auto oldHandle = pool.create(std::string{"old"});
    pool.retire(oldHandle);
    pool.free(oldHandle.index);

    const auto newHandle = pool.create(std::string{"new"});

    REQUIRE(newHandle.index == oldHandle.index);
    REQUIRE(newHandle.gen != oldHandle.gen);
    REQUIRE(pool.get(newHandle) != nullptr);
    REQUIRE(*pool.get(newHandle) == "new");
}

TEST_CASE("Old handle stays invalid after its slot is reused", "[Pool][free]")
{
    Pool<std::string> pool(4);
    const auto oldHandle = pool.create(std::string{"old"});
    pool.retire(oldHandle);
    pool.free(oldHandle.index);
    const auto newHandle = pool.create(std::string{"new"});

    REQUIRE(pool.get(oldHandle) == nullptr);
    REQUIRE_FALSE(pool.retire(oldHandle).has_value());

    // the stale retire must not have touched the new item
    REQUIRE(pool.get(newHandle) != nullptr);
    REQUIRE(*pool.get(newHandle) == "new");
}

TEST_CASE("Freeing the same slot twice does not hand it out twice", "[Pool][free]")
{
    Pool<std::string> pool(4);
    const auto handle = pool.create(std::string{"item"});
    pool.retire(handle);

    pool.free(handle.index);
    pool.free(handle.index);

    const auto first = pool.create(std::string{"first"});
    const auto second = pool.create(std::string{"second"});

    REQUIRE(first.index != second.index);
    REQUIRE(*pool.get(first) == "first");
    REQUIRE(*pool.get(second) == "second");
}

TEST_CASE("Freeing a live slot does nothing", "[Pool][free]")
{
    Pool<std::string> pool(4);
    const auto handle = pool.create(std::string{"live"});

    pool.free(handle.index);

    // still accessible, and not handed out to someone else
    REQUIRE(pool.get(handle) != nullptr);
    REQUIRE(*pool.get(handle) == "live");

    const auto other = pool.create(std::string{"other"});
    REQUIRE(other.index != handle.index);
    REQUIRE(*pool.get(handle) == "live");
}

TEST_CASE("Freeing a reused slot with a stale index does nothing", "[Pool][free]")
{
    Pool<std::string> pool(4);
    const auto oldHandle = pool.create(std::string{"old"});
    pool.retire(oldHandle);
    pool.free(oldHandle.index);
    const auto newHandle = pool.create(std::string{"new"}); // reuses the slot

    pool.free(oldHandle.index); // late / duplicated free from the deletion queue

    REQUIRE(pool.get(newHandle) != nullptr);
    const auto other = pool.create(std::string{"other"});
    REQUIRE(other.index != newHandle.index);
}

TEST_CASE("Freeing an out-of-range index does nothing", "[Pool][free]")
{
    Pool<std::string> pool(4);
    const auto handle = pool.create(std::string{"item"});

    pool.free(100);

    const auto other = pool.create(std::string{"other"});
    REQUIRE(other.index != handle.index);
    REQUIRE(other.index != 100);
}

TEST_CASE("Several pending slots are freed independently", "[Pool][free]")
{
    Pool<std::string> pool(4);
    const auto first = pool.create(std::string{"first"});
    const auto second = pool.create(std::string{"second"});
    pool.retire(first);
    pool.retire(second);

    pool.free(second.index);

    // only the freed slot can be reused, the other one is still pending
    const auto reused = pool.create(std::string{"reused"});
    REQUIRE(reused.index == second.index);

    const auto fresh = pool.create(std::string{"fresh"});
    REQUIRE(fresh.index != first.index);
}

// ---------------------------------------------------------------------------
// full lifecycle (what GpuResources will do across frames)
// ---------------------------------------------------------------------------

TEST_CASE("Full create / retire / free cycle many times on one slot", "[Pool][lifecycle]")
{
    Pool<std::string> pool(1);
    auto handle = pool.create(std::string{"gen"});
    const auto slotIndex = handle.index;

    for (int i = 0; i < 100; ++i) {
        const auto previous = handle;

        REQUIRE(pool.retire(handle).has_value());
        pool.free(handle.index);
        handle = pool.create(std::string{"gen"});

        REQUIRE(handle.index == slotIndex);
        REQUIRE(handle.gen != previous.gen);
        REQUIRE(pool.get(previous) == nullptr);
        REQUIRE(pool.get(handle) != nullptr);
    }
}
