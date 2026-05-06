# UngulaEventbus

> **High-performance embedded C++ libraries for ESP32, STM32 and other MCUs** — system-state notification broker.

A lightweight system-state notification broker for embedded projects. No event queues, no message payloads, no topic routing. Just a simple signal: "the system state changed — hey, listener(s) go check what's different."

## What it does

Any part of the system can call `notify()` to signal that something changed, not what. Registered listeners wake up in their own FreeRTOS tasks, and -for example- read the current system state, compare it against what they last transmitted, and send an update only if something actually changed.

The notifier is non-blocking. The listeners do all their work asynchronously. Multiple rapid notifications coalesce into a single pending update — the listener always processes the latest state, never stale intermediate states.

## Components

### ISystemStateListener

Pure abstract interface. Three methods:

```cpp
class ISystemStateListener {
    virtual bool start() = 0;            // create task, begin processing
    virtual void stop() = 0;             // request task exit
    virtual void notifyStateChanged() = 0; // non-blocking wakeup signal
};
```

### SystemStateNotifier<MaxListeners>

Fixed-capacity broadcaster. Stores listener pointers, no dynamic allocation. Starts inactive — notifications are silently dropped until `activate()` is called (so the system can finish initialization first).

```cpp
ungula::eventbus::SystemStateNotifier<4> notifier;

notifier.subscribe(&wsListener);
notifier.subscribe(&cloudListener);
notifier.activate();    // now notifications flow
notifier.notify();      // broadcast to all listeners
```

### ESP32SystemStateListener<PollIntervalMs>

Abstract base class implementing `ISystemStateListener` using FreeRTOS task notifications. Template Method pattern — the base class owns the task loop, the derived class implements `handleStateChange()`.

```cpp
class MyListener : public ungula::eventbus::ESP32SystemStateListener<5000> {
  protected:
    void handleStateChange() override {
        // read current state, compare, send if different
    }
};
```

Template parameter `PollIntervalMs` is the max time between wakeups even without a notification — a safety net. Set to 0 to wait indefinitely (only wake on notification).

**Coalescing:** `notifyStateChanged()` uses `xTaskNotifyGive()`. The task waits with `ulTaskNotifyTake(pdTRUE, ...)` which resets the counter to 0. So 100 rapid notifications result in one wakeup.

## Conditional compilation

| Directive | What it enables |
| --- | --- |
| `ESP_PLATFORM` | `ESP32SystemStateListener` (FreeRTOS task-based listener) |
| `CONFIG_STATE_LISTENER_STACK` | Listener task stack size in bytes (default 8192) |
| `CONFIG_STATE_LISTENER_PRIORITY` | Listener task priority (default 2) |
| `CONFIG_STATE_LISTENER_CORE` | Listener task pinned core (default 1) |

The `ISystemStateListener` interface and `SystemStateNotifier` template are platform-independent — they work anywhere with a C++17 compiler.

Override task defaults via build flags:
```
-DCONFIG_STATE_LISTENER_STACK=12288
```

## Quick example

```cpp
#include <ungula/eventbus/system_state_notifier.h>
#include <ungula/eventbus/esp32_system_state_listener.h>

// Project-specific listener
class StatusPusher : public ungula::eventbus::ESP32SystemStateListener<5000> {
  public:
    StatusPusher(MyApp& app) : app_(app) {}

  protected:
    void handleStateChange() override {
        auto snap = buildSnapshot(app_);
        if (snap != lastSent_) {
            sendToWebSocket(snap);
            lastSent_ = snap;
        }
    }

  private:
    MyApp& app_;
    Snapshot lastSent_ = {};
};

// In setup():
ungula::eventbus::SystemStateNotifier<4> notifier;
StatusPusher pusher(app);

notifier.subscribe(&pusher);
pusher.start();
notifier.activate();

// Anywhere in the system when state changes:
notifier.notify();
```

## Tests

```shell
cd tests
./1_build.sh     # configure cmake (only needed once)
./2_run.sh       # build and run all tests
```

13 tests covering subscribe/unsubscribe, activate/deactivate, broadcast, capacity limits, and duplicate rejection.

## Acknowledgements

Thanks to Claude and ChatGPT for helping on generating this documentation.

## License

MIT License — see [LICENSE](license.txt) file.
