// Host regression test. Build and run from the repo root:
//   g++ -std=c++17 -I test/stub async.cpp test/host_test.cpp -o /tmp/async_test && /tmp/async_test
#include <assert.h>
#include <stdio.h>
#include "../async.h"

static unsigned long now = 0;
unsigned long millis() { return now; }

static Async* engine = nullptr;
static short selfId = -1;
static int calls = 0;

static void noop() {}
static void count() { calls++; }

// Pool slots that set* can still claim; leaves the pool as it found it.
static int freeSlots(Async& a) {
    short ids[64];
    int n = 0;
    while (n < 64 && (ids[n] = a.setTimeout(noop, 1000000)) >= 0) n++;
    for (int i = 0; i < n; i++) a.clearInterval(ids[i]);
    return n;
}

static void tick(Async& a, unsigned long ms) { now += ms; a.run(); }

// One-shot callback that clears its own id (JaamSiren pulse callbacks did this).
static void selfClearOnce() { calls++; engine->clearInterval(selfId); selfId = -1; }

// Loop callback that clears its own id on the first call.
static void selfClearLoop() { calls++; engine->clearInterval(selfId); }

// One-shot callback that schedules a follow-up timer from inside itself.
static short chainedId = -1;
static void chain() { chainedId = engine->setTimeout(count, 10); }

int main() {
    const int POOL = 20, PERMANENT = 11; // jaam_fusion: Async(20), 11 setInterval in setup()

    { // One-shot clearing itself must not decrement nNodes twice
        Async a(POOL); engine = &a; now = 0; calls = 0;
        for (int i = 0; i < PERMANENT; i++) assert(a.setInterval(noop, 1000) >= 0);
        for (int i = 0; i < 1000; i++) {
            selfId = a.setTimeout(selfClearOnce, 10);
            assert(selfId >= 0);
            tick(a, 10);
        }
        assert(calls == 1000);
        assert(freeSlots(a) == POOL - PERMANENT);
    }

    { // setTimeout from inside a one-shot callback must survive
        Async a(POOL); engine = &a; now = 0; calls = 0; chainedId = -1;
        assert(a.setTimeout(chain, 10) >= 0);
        tick(a, 10);
        assert(chainedId >= 0);
        tick(a, 10);
        assert(calls == 1);
        assert(freeSlots(a) == POOL);
    }

    { // Loop clearing itself stops and releases exactly one slot
        Async a(POOL); engine = &a; now = 0; calls = 0;
        selfId = a.setInterval(selfClearLoop, 10);
        for (int i = 0; i < 5; i++) tick(a, 10);
        assert(calls == 1);
        assert(freeSlots(a) == POOL);
    }

    { // jaam_fusion PR #96: external clearInterval releases the slot, repeated clear is a no-op
        Async a(POOL); engine = &a; now = 0;
        for (int i = 0; i < PERMANENT; i++) assert(a.setInterval(noop, 1000) >= 0);
        for (int day = 0; day < 30; day++) {
            short beep = a.setInterval(noop, 2000);
            assert(beep >= 0);
            tick(a, 2000);
            assert(a.clearInterval(beep));
            assert(a.clearInterval(beep));
        }
        assert(a.setTimeout(noop, 500) >= 0);
        assert(freeSlots(a) == POOL - PERMANENT - 1);
    }

    puts("async host tests: OK");
    return 0;
}
