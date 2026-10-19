#pragma once

#include <algorithm>
#include <memory>
#include <stdint.h>

template <typename T> struct Handle {
    uint32_t index{0};
    uint32_t gen{0};
};

template <typename T> class Pool {
public:
    Pool(const uint32_t size)
    {
        m_slots.reserve(size);
        m_freeSlots.reserve(size);
    };

    ~Pool() = default;

    Handle<T> create(T&& obj)
    {
        if (m_freeSlots.empty()) {
            uint32_t slotIndex = static_cast<uint32_t>(m_slots.size());
            m_slots.push_back({.obj = std::move(obj)});

            return {slotIndex, 1};
        } else {
            size_t slotIndex = m_freeSlots[m_freeSlots.size() - 1];
            Slot& slot = m_slots[slotIndex];

            slot.obj = std::move(obj);
            m_freeSlots.pop_back();

            return {static_cast<uint32_t>(slotIndex), slot.gen};
        }
    }

    T* get(const Handle<T>& handle)
    {
        if (handle.index >= m_slots.size())
            return nullptr;

        Slot& slot = m_slots[static_cast<size_t>(handle.index)];

        return (!slot.pending && handle.gen == slot.gen) ? &slot.obj : nullptr;
    }

    std::optional<T> retire(const Handle<T>& handle)
    {
        if (handle.index >= m_slots.size())
            return {};

        Slot& slot = m_slots[handle.index];

        if (!slot.pending && handle.gen == slot.gen) {
            slot.pending = true;
            ++slot.gen;
            return std::move(slot.obj);
        }

        return {};
    }

    void free(const uint32_t index)
    {
        if (index >= m_slots.size())
            return;

        Slot& slot = m_slots[index];

        if (slot.pending) {
            slot.pending = false;
            m_freeSlots.push_back(index);
        }
    }

    uint32_t availableSlots() const
    {
        return static_cast<uint32_t>(m_freeSlots.size());
    }

    std::vector<uint32_t> freeSlotsIndices() const
    {
        std::vector<uint32_t> freeSlotsIndices;
        for (auto&& freeSlot : m_freeSlots) {
            freeSlotsIndices.push_back(freeSlot);
        }
        return freeSlotsIndices;
    }

    std::vector<uint32_t> slotsGeneration()
    {
        std::vector<uint32_t> slotsGen;
        for (auto&& slot : m_slots) {
            slotsGen.push_back(slot.gen);
        }
        return slotsGen;
    }

private:
    struct Slot {
        T obj;
        uint32_t gen{1};
        bool pending{false};
    };

    std::vector<Slot> m_slots;
    std::vector<uint32_t> m_freeSlots;
};
