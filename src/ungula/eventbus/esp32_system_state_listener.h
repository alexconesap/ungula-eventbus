// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#pragma once

// ============================================================================
// ESP32SystemStateListener — abstract base for async state-change listeners.
//
// Implements the Template Method pattern:
//   - Base class owns the FreeRTOS task and the wait/coalesce loop
//   - Derived class implements handleStateChange() with its own logic
//
// Coalescing: multiple notifyStateChanged() calls while already pending
// collapse into a single handleStateChange() invocation. The derived class
// always sees the latest system state, never stale intermediate states.
//
// Template parameter:
//   PollIntervalMs — max time between wakeups even without notification.
//                    Acts as a safety net. Set to 0 to wait indefinitely.
//
// Usage:
//   class MyListener : public ESP32SystemStateListener<5000> {
//     protected:
//       void handleStateChange() override { /* read state, send it */ }
//   };
// ============================================================================

#ifdef ESP_PLATFORM

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "i_system_state_listener.h"

// Default task parameters. Override with -DCONFIG_STATE_LISTENER_STACK=12288 etc.
#ifndef CONFIG_STATE_LISTENER_STACK
#define CONFIG_STATE_LISTENER_STACK 8192
#endif
#ifndef CONFIG_STATE_LISTENER_PRIORITY
#define CONFIG_STATE_LISTENER_PRIORITY 2
#endif
#ifndef CONFIG_STATE_LISTENER_CORE
#define CONFIG_STATE_LISTENER_CORE 1
#endif

namespace ungula::eventbus {


template <uint32_t PollIntervalMs = 5000>
class ESP32SystemStateListener : public ISystemStateListener {
    public:
        virtual ~ESP32SystemStateListener() {
            stop();
        }

        bool start() override {
            if (taskHandle_ != nullptr) {
                return true;  // already running
            }
            running_ = true;
            BaseType_t result =
                    xTaskCreatePinnedToCore(taskEntry, taskName(), stackSize(), this,
                                            taskPriority(), &taskHandle_, taskCore());
            if (result != pdPASS) {
                running_ = false;
                taskHandle_ = nullptr;
                return false;
            }
            return true;
        }

        void stop() override {
            if (taskHandle_ == nullptr) {
                return;
            }
            running_ = false;
            // Wake the task so it can exit the loop
            xTaskNotifyGive(taskHandle_);
            // Wait for the task to finish (up to 2 seconds)
            for (int attempt = 0; attempt < 200 && taskHandle_ != nullptr; ++attempt) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        void notifyStateChanged() override {
            if (!running_ || taskHandle_ == nullptr) {
                return;
            }
            // Non-blocking: xTaskNotifyGive increments a counter.
            // The task reads it with ulTaskNotifyTake(pdTRUE, ...) which
            // resets to 0 — so multiple rapid calls coalesce into one wakeup.
            xTaskNotifyGive(taskHandle_);
        }

    protected:
        /// Derived classes implement this to handle a state change.
        /// Called in the listener's own task context — safe to do I/O, build JSON, etc.
        virtual void handleStateChange() = 0;

        /// Override these for custom task parameters
        virtual const char* taskName() const {
            return "state_listener";
        }
        virtual uint32_t stackSize() const {
            return CONFIG_STATE_LISTENER_STACK;
        }
        virtual UBaseType_t taskPriority() const {
            return CONFIG_STATE_LISTENER_PRIORITY;
        }
        virtual BaseType_t taskCore() const {
            return CONFIG_STATE_LISTENER_CORE;
        }

    private:
        TaskHandle_t taskHandle_ = nullptr;
        volatile bool running_ = false;

        static void taskEntry(void* param) {
            auto* self = static_cast<ESP32SystemStateListener*>(param);
            self->taskLoop();
        }

        void taskLoop() {
            while (running_) {
                // Wait for notification or timeout
                TickType_t ticks =
                        (PollIntervalMs > 0) ? pdMS_TO_TICKS(PollIntervalMs) : portMAX_DELAY;

                // ulTaskNotifyTake with pdTRUE resets the counter to 0,
                // so N rapid notifications become one wakeup.
                ulTaskNotifyTake(pdTRUE, ticks);

                if (!running_) {
                    break;
                }

                handleStateChange();
            }

            // Clean exit
            taskHandle_ = nullptr;
            vTaskDelete(nullptr);
        }
};

}  // namespace ungula::eventbus
#endif  // ESP_PLATFORM
