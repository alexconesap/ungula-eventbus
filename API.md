# UngulaEventbus

Non-blocking, fixed-capacity broadcaster for "system state changed" signals
in embedded C++17. The notifier carries no payload and no topic — it just
wakes registered listeners, which then query the host system for the
current state and act on it. Core types are platform-independent. An
optional ESP32/FreeRTOS task-based listener base class is included.

---

## LLM quick map

- **Primary include**: `#include <ungula/eventbus.h>`.
- **Arduino discovery include**: `#include <ungula_eventbus.h>` (forwarder only; host code should keep using the real header).
- **Namespace root**: `ungula::eventbus`.
- **Own source minimum**: `C++17`.
- **Effective minimum for consumers**: `C++17`.
- **Dependency impact**: None (no declared internal dependencies).
- **Supported architectures**: `esp32`.
- **Read order for coding agents**: `Usage` (working patterns) -> `API` (symbols/signatures) -> `Lifecycle`/`Error handling`/`Threading` notes in this file.

### Use-case index

- [Use case: portable broker (host tests, STM32, any C++17)](#use-case-portable-broker-host-tests-stm32-any-c17)
- [Use case: ESP32 listener with FreeRTOS task](#use-case-esp32-listener-with-freertos-task)
- [Use case: customising the listener task](#use-case-customising-the-listener-task)

### LLM rules

- Use only symbols and include paths documented in this file; do not infer extra public API from implementation files.
- Prefer the use-case patterns here over ad-hoc rewrites; keep dependency wiring and lifecycle order identical unless the task explicitly changes API design.
- Treat headers under `detail/`, `platform/`, and `platforms/` as internal unless this document calls them out as public.
- If required behavior is missing from the documented API, report the gap explicitly instead of inventing new public symbols.


## Usage

Single chain header pulls in the public surface. On Arduino-ESP32 the
ESP32 listener becomes available automatically because `ESP_PLATFORM` is
defined by the toolchain.

### Use case: portable broker (host tests, STM32, any C++17)

```cpp
#include <ungula/eventbus.h>

class MyListener : public ungula::eventbus::ISystemStateListener {
  public:
    bool start() override { return true; }
    void stop() override {}
    void notifyStateChanged() override {
        // read current state, act on it
    }
};

int main() {
    ungula::eventbus::SystemStateNotifier<4> notifier;
    MyListener a;
    MyListener b;

    notifier.subscribe(&a);
    notifier.subscribe(&b);
    notifier.activate();   // notifications were dropped before this

    notifier.notify();     // a.notifyStateChanged() and b.notifyStateChanged()
    return 0;
}
```

When to use this: any context where the host owns its own task model or
runs on a non-FreeRTOS target, or for unit tests of the broker itself.

### Use case: ESP32 listener with FreeRTOS task

```cpp
#include <Arduino.h>
#include <ungula/eventbus.h>

class StatusPusher : public ungula::eventbus::ESP32SystemStateListener<5000> {
  protected:
    void handleStateChange() override {
        // runs in the listener's own task; safe to do I/O here
    }
};

ungula::eventbus::SystemStateNotifier<4> notifier;
StatusPusher pusher;

void setup() {
    notifier.subscribe(&pusher);
    pusher.start();        // creates the FreeRTOS task
    notifier.activate();   // open the gate after init is done
}

void loop() {
    // anywhere state changes:
    notifier.notify();     // coalesced wake; pusher.handleStateChange() runs once
    delay(1000);
}
```

When to use this: ESP32 firmware where the listener should process state
changes asynchronously without blocking the caller of `notify()`.

### Use case: customising the listener task

```cpp
#include <ungula/eventbus.h>

class HighPriorityPusher : public ungula::eventbus::ESP32SystemStateListener<0> {
  protected:
    void handleStateChange() override { /* ... */ }

    const char* taskName() const override { return "hp_pusher"; }
    uint32_t stackSize() const override { return 12288; }
    UBaseType_t taskPriority() const override { return 5; }
    BaseType_t taskCore() const override { return 0; }
};
```

When to use this: when defaults (`state_listener`, 8192 B, prio 2, core 1)
do not match the project. `PollIntervalMs = 0` makes the task wait
indefinitely until a notification arrives.

---

## Public types

### `ungula::eventbus::ISystemStateListener`

Pure-abstract interface. Defined in `<ungula/eventbus/i_system_state_listener.h>`.

| Method | Contract |
| --- | --- |
| `virtual bool start() = 0` | Begin processing (e.g. create task). Return true on success. |
| `virtual void stop() = 0` | Request processing to end. |
| `virtual void notifyStateChanged() = 0` | Non-blocking wakeup. May be called from ISR or main loop. Multiple rapid calls must coalesce. |

The interface does not own any task or buffer — implementations decide.
The destructor is virtual and `default`.

### `ungula::eventbus::SystemStateNotifier<size_t MaxListeners>`

Header-only template. Fixed-capacity broadcaster. No heap. Defined in
`<ungula/eventbus/system_state_notifier.h>`.

Storage: `ISystemStateListener* listeners_[MaxListeners]`, plus a count
and an active flag. Default-constructed; no setup needed.

### `ungula::eventbus::ESP32SystemStateListener<uint32_t PollIntervalMs = 5000>`

Abstract base class implementing `ISystemStateListener` over a single
FreeRTOS task using task notifications. Defined in
`<ungula/eventbus/esp32_system_state_listener.h>`. Only compiled when
`ESP_PLATFORM` is defined.

Template parameter `PollIntervalMs` is the maximum idle time between
wakeups. `0` means wait indefinitely (`portMAX_DELAY`).

Build-time defaults (override with `-D...`):

| Macro | Default | Effect |
| --- | --- | --- |
| `CONFIG_STATE_LISTENER_STACK` | `8192` | Task stack bytes |
| `CONFIG_STATE_LISTENER_PRIORITY` | `2` | Task priority |
| `CONFIG_STATE_LISTENER_CORE` | `1` | Pinned core |

---

## Public functions / methods

### `SystemStateNotifier<N>::subscribe`

- **Purpose**: register a listener pointer.
- **Signature**: `bool subscribe(ISystemStateListener* listener)`
- **Parameters**: `listener` — non-null, not already registered.
- **Returns**: `true` if added; `false` if `nullptr`, capacity reached
  (`count_ >= MaxListeners`), or duplicate.
- **Side effects**: stores the pointer internally; does not take ownership.
- **Failure behavior**: silent boolean failure. Caller checks return.
- **Usage notes**: O(N) duplicate scan. Listener lifetime must outlive
  the notifier or be unsubscribed first.

### `SystemStateNotifier<N>::unsubscribe`

- **Purpose**: remove a previously registered listener.
- **Signature**: `bool unsubscribe(ISystemStateListener* listener)`
- **Returns**: `true` if removed; `false` if not found.
- **Side effects**: shifts remaining entries left; clears the freed slot.
- **Usage notes**: not thread-safe relative to `notify()` running on
  another core/task. Unsubscribe from the same context that calls
  `notify()`, or stop notifications first.

### `SystemStateNotifier<N>::notify`

- **Purpose**: broadcast "state changed" to every registered listener.
- **Signature**: `void notify()`
- **Side effects**: calls `notifyStateChanged()` on each listener in
  registration order.
- **Failure behavior**: no-op when not active (`active_ == false`).
- **Usage notes**: non-blocking only as far as listeners' implementations
  are non-blocking. The bundled `ESP32SystemStateListener` is non-blocking.

### `SystemStateNotifier<N>::activate` / `deactivate` / `isActive`

- `void activate()` — flips the gate; subsequent `notify()` calls go
  through. Use after the system has finished `setup()`.
- `void deactivate()` — silently drops further `notify()` calls.
- `bool isActive() const` — query the gate.

### `SystemStateNotifier<N>::listenerCount` / `maxListeners`

- `size_t listenerCount() const` — current registrations.
- `static constexpr size_t maxListeners()` — template parameter `N`.

### `ESP32SystemStateListener<P>::start`

- **Purpose**: create the listener's FreeRTOS task.
- **Signature**: `bool start() override`
- **Returns**: `true` on success, also `true` if already running. `false`
  if `xTaskCreatePinnedToCore` fails.
- **Side effects**: spawns one task, pinned to `taskCore()`, with stack
  `stackSize()`, priority `taskPriority()`, name `taskName()`.

### `ESP32SystemStateListener<P>::stop`

- **Purpose**: request the task to exit and wait briefly for it.
- **Signature**: `void stop() override`
- **Side effects**: clears the run flag, sends one notification to wake
  the task, polls up to ~2 s (200 × 10 ms) for `taskHandle_` to clear.
- **Usage notes**: also called by the destructor. After return, the task
  has either exited cleanly or the timeout expired. There is no error
  return for the timeout path.

### `ESP32SystemStateListener<P>::notifyStateChanged`

- **Purpose**: ISR/task-safe wakeup using `xTaskNotifyGive`.
- **Signature**: `void notifyStateChanged() override`
- **Side effects**: increments the FreeRTOS task notification value.
- **Failure behavior**: no-op if the task is not running.
- **Usage notes**: coalescing is implicit — the task uses
  `ulTaskNotifyTake(pdTRUE, ...)` which clears the counter, so N rapid
  calls become one `handleStateChange()`.

### `ESP32SystemStateListener<P>::handleStateChange` (protected, pure virtual)

- **Purpose**: derived class entry point. Runs in the listener's own task
  context — safe to do I/O, JSON building, network writes.
- **Signature**: `virtual void handleStateChange() = 0`

### `ESP32SystemStateListener<P>` task hooks (protected, virtual)

| Hook | Default |
| --- | --- |
| `const char* taskName() const` | `"state_listener"` |
| `uint32_t stackSize() const` | `CONFIG_STATE_LISTENER_STACK` |
| `UBaseType_t taskPriority() const` | `CONFIG_STATE_LISTENER_PRIORITY` |
| `BaseType_t taskCore() const` | `CONFIG_STATE_LISTENER_CORE` |

Override per subclass when defaults do not fit.

---

## Lifecycle

1. Construct the notifier and listener objects (statics, members of the
   app, or `new` at boot — never after `setup()`).
2. Call `notifier.subscribe(&listener)` for each listener.
3. Call `listener.start()` on every listener that owns a task
   (`ESP32SystemStateListener::start` creates the FreeRTOS task).
4. Call `notifier.activate()` once the rest of the system is ready to
   receive notifications.
5. During operation, anyone (including ISR) calls `notifier.notify()`.
6. To shut down: `notifier.deactivate()`, optionally `unsubscribe(...)`,
   then `listener.stop()`. The `ESP32SystemStateListener` destructor
   also calls `stop()`.

Violations:

- Calling `notify()` before `activate()` is silently dropped (intentional).
- Calling `notifyStateChanged()` on an `ESP32SystemStateListener` whose
  task was not started is a no-op.
- Subscribing the same pointer twice fails the second call.
- Subscribing a listener whose lifetime ends before `unsubscribe` is
  undefined behavior on the next `notify()`.

---

## Error handling

No exceptions, no error codes. Failures surface as boolean returns
(`subscribe`, `unsubscribe`, `start`) or silent no-ops (`notify` when
inactive, `notifyStateChanged` when the task is down). Call sites must
inspect the booleans where they matter.

---

## Threading / timing / hardware notes

- `SystemStateNotifier` is **not** thread-safe by itself.
  `subscribe`/`unsubscribe` mutate the array without a lock. Configure
  subscriptions during boot, before `activate()`. Treat the broker as
  configure-once.
- `notify()` walks the array and calls each listener's
  `notifyStateChanged()`. With the bundled `ESP32SystemStateListener`,
  that call is `xTaskNotifyGive` — safe from any task and from ISR.
- `ESP32SystemStateListener` runs exactly one task. Coalescing is
  guaranteed: the task wakes once for any burst of notifications received
  while it was processing the previous one.
- `PollIntervalMs > 0` adds a periodic safety wakeup. Use it when the
  system can miss a state change but the listener should re-check anyway.
  `PollIntervalMs = 0` waits forever for a notification.
- `stop()` is bounded at ~2 s. After that it returns even if the task
  has not exited. Plan accordingly during shutdown.
- Heap: `start()` calls `xTaskCreatePinnedToCore`, which allocates the
  stack from FreeRTOS heap. Per project policy, call it only at boot.

---

## Internals not part of the public API

- `SystemStateNotifier::listeners_`, `count_`, `active_` — private
  storage, do not access via casts or friendship.
- `ESP32SystemStateListener::taskHandle_`, `running_`, `taskEntry`,
  `taskLoop` — task-internal plumbing. Override the protected `taskName`,
  `stackSize`, `taskPriority`, `taskCore`, `handleStateChange` instead.
- `<freertos/FreeRTOS.h>` and `<freertos/task.h>` are pulled in by the
  ESP32 header. Do not call FreeRTOS APIs directly from
  `handleStateChange()` to manage the listener's own task — let the base
  class own it.

---

## LLM usage rules

- Use only the documented public API unless explicitly modifying the
  library.
- Subscribe and `start()` listeners during boot. Call `activate()` once,
  after the rest of the system is ready.
- Treat `notify()` as a fire-and-forget signal with no payload. Listeners
  must query the host system for actual state inside `handleStateChange()`.
- Do not protect the broker with locks at runtime; configure it once and
  leave the membership stable.
- Do not call `delay()`, `millis()`, or other Arduino timing APIs inside
  `handleStateChange()` — use `ungula::core::time` per project rules.
- Do not add logging calls inside listener implementations placed in
  reusable libraries; the host project owns logging.
- If a use case needs a payload, a queue, or topic routing, this library
  does not provide it — say so rather than inventing API.
- Preserve the terminology: notifier, listener, subscribe, activate,
  notify, handleStateChange.
