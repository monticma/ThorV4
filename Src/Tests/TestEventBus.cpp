/// @file TestEventBus.cpp
/// @brief Tests unitaires et benchmark pour EventBus.
///
/// Compilation :
///   g++ -std=c++17 -pthread -I ../../Include Src/Tests/TestEventBus.cpp \
///       Src/Core/EventBus.cpp -o build/TestEventBus

#include "Core/EventBus.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <atomic>

static void testBasic()
{
    std::cout << "=== Basic Publish/Consume ===" << std::endl;
    EventBus bus;
    for (int i = 0; i < 3; ++i) {
        Event ev;
        ev.type = EventType::MotionComplete;
        ev.priority = EventPriority::Critical;
        ev.data.motion.axisIndex = i;
        assert(bus.publish(ev));
    }
    assert(bus.totalPublished() == 3);
    std::vector<Event> events;
    assert(bus.consume(10, events) == 3);
    for (int i = 0; i < 3; ++i)
        assert(events[i].data.motion.axisIndex == i);
    std::cout << "  PASSED" << std::endl;
}

static void testEmergency()
{
    std::cout << "=== Emergency Priority ===" << std::endl;
    EventBus bus;
    Event bg;
    bg.type = EventType::TelemetryTick;
    bg.priority = EventPriority::Background;
    bus.publish(bg);

    Event emerg;
    emerg.type = EventType::EmergencyStop;
    emerg.priority = EventPriority::Emergency;
    bus.publish(emerg);

    std::vector<Event> events;
    bus.consume(10, events);
    assert(events.size() == 2);
    assert(events[0].type == EventType::EmergencyStop);
    std::cout << "  PASSED" << std::endl;
}

static void testFifoOrder()
{
    std::cout << "=== FIFO Order ===" << std::endl;
    EventBus bus;
    for (int i = 0; i < 100; ++i) {
        Event ev;
        ev.type = EventType::MotionComplete;
        ev.priority = EventPriority::Normal;
        ev.data.motion.axisIndex = i;
        bus.publish(ev);
    }
    std::vector<Event> events;
    bus.consume(200, events);
    for (int i = 0; i < 100; ++i)
        assert(events[i].data.motion.axisIndex == i);
    std::cout << "  PASSED" << std::endl;
}

static void testDropOnFull()
{
    std::cout << "=== Drop On Full ===" << std::endl;
    EventBus bus;
    int pub = 0, drop = 0;
    for (int i = 0; i < 3000; ++i) {
        Event ev;
        ev.type = EventType::MotionComplete;
        ev.priority = EventPriority::Critical;
        if (bus.publish(ev)) ++pub; else ++drop;
    }
    assert(pub == 2048);
    assert(drop == 3000 - 2048);
    std::cout << "  Published=" << pub << " Dropped=" << drop << " PASSED" << std::endl;
}

static void testMultiProducer()
{
    std::cout << "=== Multi-Producer (4t x 500) ===" << std::endl;
    EventBus bus;
    std::atomic<bool> done{false};
    const int perThread = 500;

    std::thread consumer([&bus, &done]() {
        while (!done.load()) {
            std::vector<Event> batch;
            bus.consume(50, batch);
        }
        std::vector<Event> batch;
        while (bus.consume(50, batch) > 0) {}
    });

    auto producer = [&bus](int tid) {
        for (int i = 0; i < perThread; ++i) {
            Event ev;
            ev.type = EventType::MotionComplete;
            ev.priority = EventPriority::Critical;
            ev.sourceModule = tid;
            while (!bus.publish(ev))
                std::this_thread::yield();
        }
    };

    std::thread p0(producer, 0);
    std::thread p1(producer, 1);
    std::thread p2(producer, 2);
    std::thread p3(producer, 3);

    p0.join(); p1.join(); p2.join(); p3.join();
    done.store(true);
    consumer.join();

    std::cout << "  Pub=" << bus.totalPublished()
              << " Drop=" << bus.totalDropped()
              << " Cons=" << bus.totalConsumed() << std::endl;
    assert(bus.totalPublished() == 4 * perThread);
    assert(bus.totalDropped() == 0);
    std::cout << "  PASSED" << std::endl;
}

int main()
{
    testBasic();
    testEmergency();
    testFifoOrder();
    testDropOnFull();
    testMultiProducer();
    std::cout << std::endl << "ALL EventBus tests PASSED" << std::endl;
    return 0;
}
