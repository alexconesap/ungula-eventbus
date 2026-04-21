// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#pragma once

// ============================================================================
// SystemStateNotifier — fixed-capacity broadcaster for state-change signals.
//
// Any part of the system calls notify() to signal "something changed."
// All registered listeners receive notifyStateChanged() — non-blocking,
// no payload, no knowledge of what changed.
//
// Usage:
//   SystemStateNotifier<4> notifier;
//   notifier.subscribe(&myWsListener);
//   notifier.subscribe(&myCloudListener);
//   notifier.activate();    // listeners start receiving notifications
//   notifier.notify();      // broadcast "state changed" to all
// ============================================================================

#include <cstddef>

#include "i_system_state_listener.h"

namespace ungula {

  template <size_t MaxListeners>
  class SystemStateNotifier {
    public:
      /// Subscribe a listener. Returns false if full or duplicate.
      bool subscribe(ISystemStateListener* listener) {
        if (listener == nullptr || count_ >= MaxListeners) {
          return false;
        }
        for (size_t idx = 0; idx < count_; ++idx) {
          if (listeners_[idx] == listener) {
            return false;  // duplicate
          }
        }
        listeners_[count_++] = listener;
        return true;
      }

      /// Unsubscribe a listener. Returns false if not found.
      bool unsubscribe(ISystemStateListener* listener) {
        for (size_t idx = 0; idx < count_; ++idx) {
          if (listeners_[idx] == listener) {
            for (size_t jdx = idx; jdx < count_ - 1; ++jdx) {
              listeners_[jdx] = listeners_[jdx + 1];
            }
            listeners_[--count_] = nullptr;
            return true;
          }
        }
        return false;
      }

      /// Broadcast "state changed" to all registered listeners.
      /// Does nothing if not activated. Non-blocking.
      void notify() {
        if (!active_) return;
        for (size_t idx = 0; idx < count_; ++idx) {
          listeners_[idx]->notifyStateChanged();
        }
      }

      /// Activate the notifier — listeners start receiving notifications.
      /// Call after the system is fully initialized.
      void activate() { active_ = true; }

      /// Deactivate — notifications are silently dropped.
      void deactivate() { active_ = false; }

      /// Check if the notifier is active
      bool isActive() const { return active_; }

      /// Number of registered listeners
      size_t listenerCount() const { return count_; }

      /// Maximum capacity
      static constexpr size_t maxListeners() { return MaxListeners; }

    private:
      ISystemStateListener* listeners_[MaxListeners] = {};
      size_t count_ = 0;
      bool active_ = false;
  };

}  // namespace ungula
