#pragma once

#include <algorithm>
#include <optional>
#include <stdint.h>
#include <vector>

template <typename T> struct Handle {
    uint32_t index{0};
    uint32_t gen{0};
};

template <typename T> class Pool {
public:
    Pool() = default;

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
            slot.state = SlotState::Alive;
            m_freeSlots.pop_back();

            return {static_cast<uint32_t>(slotIndex), slot.gen};
        }
    }

    const T* get(const Handle<T>& handle) const
    {
        if (handle.index >= m_slots.size())
            return nullptr;

        const Slot& slot = m_slots[static_cast<size_t>(handle.index)];

        return (slot.state == SlotState::Alive && handle.gen == slot.gen) ? &slot.obj : nullptr;
    }

    std::optional<T> retire(const Handle<T>& handle)
    {
        if (handle.index >= m_slots.size())
            return {};

        Slot& slot = m_slots[handle.index];

        if (slot.state == SlotState::Alive && handle.gen == slot.gen) {
            slot.state = SlotState::Pending;
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

        if (slot.state == SlotState::Pending) {
            slot.state = SlotState::Free;
            m_freeSlots.push_back(index);
        }
    }

    void reserve(const uint32_t size)
    {
        m_slots.reserve(size);
        m_freeSlots.reserve(size);
    };

    template <typename F> void forEachAlive(F&& f)
    {
        for (auto&& slot : m_slots) {
            if (slot.state == SlotState::Alive) {
                f(slot.obj);
            }
        }
    }

    template <typename F> void releaseAllAlive(F&& f)
    {
        for (size_t slotIndex = 0; slotIndex < m_slots.size(); ++slotIndex) {
            Slot& slot = m_slots[slotIndex];

            if (slot.state == SlotState::Alive) {
                f(slot.obj);

                slot.state = SlotState::Free;
                ++slot.gen;
                m_freeSlots.push_back(static_cast<uint32_t>(slotIndex));
            }
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
    enum class SlotState { Free, Alive, Pending };
    struct Slot {
        T obj;
        uint32_t gen{1};
        SlotState state{SlotState::Alive};
    };

    std::vector<Slot> m_slots;
    std::vector<uint32_t> m_freeSlots;
};
