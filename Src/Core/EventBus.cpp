#include "Core/EventBus.h"

#include <chrono>

// =============================================================================
// EventQueue — Ring buffer Vyukov MPMC (single consumer optimisé)
// =============================================================================

class EventBus::EventQueue
{
public:
    static const int SIZE = 2048;

    struct Slot
    {
        std::atomic<uint64_t> sequence;
        Event                 event;
    };

    EventQueue()
    {
        for (int i = 0; i < SIZE; ++i) {
            mSlots[i].sequence.store(static_cast<uint64_t>(i),
                                      std::memory_order_relaxed);
        }
        mTail.store(0, std::memory_order_relaxed);
        mHead.store(0, std::memory_order_relaxed);
        mDropped.store(0, std::memory_order_relaxed);
    }

    bool publish(const Event& event)
    {
        uint64_t tail = mTail.load(std::memory_order_relaxed);

        while (true) {
            Slot& slot = mSlots[tail & (SIZE - 1)];
            uint64_t seq = slot.sequence.load(std::memory_order_acquire);
            int64_t diff = static_cast<int64_t>(seq) - static_cast<int64_t>(tail);

            if (diff == 0) {
                if (mTail.compare_exchange_weak(tail, tail + 1,
                    std::memory_order_relaxed)) {
                    slot.event = event;
                    slot.sequence.store(tail + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                mDropped.fetch_add(1, std::memory_order_relaxed);
                return false;
            } else {
                tail = mTail.load(std::memory_order_relaxed);
            }
        }
    }

    bool consume(Event& eventOut)
    {
        uint64_t head = mHead.load(std::memory_order_relaxed);
        Slot& slot = mSlots[head & (SIZE - 1)];
        uint64_t seq = slot.sequence.load(std::memory_order_acquire);
        int64_t diff = static_cast<int64_t>(seq) - static_cast<int64_t>(head + 1);

        if (diff == 0) {
            eventOut = slot.event;
            slot.sequence.store(head + SIZE, std::memory_order_release);
            mHead.store(head + 1, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    bool hasEvents() const
    {
        uint64_t head = mHead.load(std::memory_order_relaxed);
        const Slot& slot = mSlots[head & (SIZE - 1)];
        uint64_t seq = slot.sequence.load(std::memory_order_acquire);
        int64_t diff = static_cast<int64_t>(seq) - static_cast<int64_t>(head + 1);
        return diff == 0;
    }

    uint64_t droppedCount() const
    {
        return mDropped.load(std::memory_order_relaxed);
    }

private:
    alignas(64) Slot mSlots[SIZE];
    alignas(64) std::atomic<uint64_t> mTail;
    alignas(64) std::atomic<uint64_t> mHead;
    alignas(64) std::atomic<uint64_t> mDropped;
};

// =============================================================================
// EventBus
// =============================================================================

EventBus::EventBus()
    : mTotalPublished(0)
    , mTotalDropped(0)
    , mTotalConsumed(0)
    , mNextEventId(1)
{
    mEmergencyQueue = new EventQueue();
    for (int i = 0; i < kNumPriorities; ++i) {
        mQueues[i] = new EventQueue();
    }
}

EventBus::~EventBus()
{
    delete mEmergencyQueue;
    for (int i = 0; i < kNumPriorities; ++i) {
        delete mQueues[i];
    }
}

void EventBus::prepareEvent(Event& event)
{
    event.eventId = mNextEventId.fetch_add(1, std::memory_order_relaxed);

    // Timestamp : uniquement si l'appelant n'en a pas déjà défini un
    if (event.timestampUs == 0) {
        auto now = std::chrono::steady_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
        event.timestampUs = static_cast<uint64_t>(us);
    }

    if (event.generation == 0) {
        event.generation = 1;
    }
}

bool EventBus::publish(const Event& event)
{
    Event ev = event;
    prepareEvent(ev);

    EventQueue* targetQueue = nullptr;
    if (ev.priority == EventPriority::Emergency) {
        targetQueue = mEmergencyQueue;
    } else {
        int index = static_cast<int>(ev.priority) - 1;
        if (index >= 0 && index < kNumPriorities) {
            targetQueue = mQueues[index];
        }
    }

    if (targetQueue == nullptr) {
        mTotalDropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    bool ok = targetQueue->publish(ev);
    if (ok) {
        mTotalPublished.fetch_add(1, std::memory_order_relaxed);
    } else {
        mTotalDropped.fetch_add(1, std::memory_order_relaxed);
    }
    return ok;
}

int EventBus::consume(int maxPerPriority, std::vector<Event>& eventsOut)
{
    int totalConsumed = 0;
    Event ev;

    while (mEmergencyQueue->consume(ev)) {
        eventsOut.push_back(ev);
        ++totalConsumed;
    }

    for (int priorityIndex = 0; priorityIndex < kNumPriorities; ++priorityIndex) {
        int consumedThisPriority = 0;
        while (consumedThisPriority < maxPerPriority &&
               mQueues[priorityIndex]->consume(ev)) {
            eventsOut.push_back(ev);
            ++consumedThisPriority;
            ++totalConsumed;
        }
    }

    mTotalConsumed.fetch_add(totalConsumed, std::memory_order_relaxed);
    return totalConsumed;
}

bool EventBus::hasEmergencyEvents() const
{
    return mEmergencyQueue->hasEvents();
}

uint64_t EventBus::totalPublished() const
{
    return mTotalPublished.load(std::memory_order_relaxed);
}

uint64_t EventBus::totalDropped() const
{
    return mTotalDropped.load(std::memory_order_relaxed);
}

uint64_t EventBus::totalConsumed() const
{
    return mTotalConsumed.load(std::memory_order_relaxed);
}
