// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#pragma once

namespace ungula::eventbus
{

    /// Pure interface for system-state-change listeners.
    ///
    /// Implementations receive a "something changed" signal and decide
    /// what to do in their own context (typically a FreeRTOS task).
    /// The listener knows nothing about what changed — it queries the
    /// host system for the current state when it wakes up.
    class ISystemStateListener {
    public:
        virtual ~ISystemStateListener() = default;

        /// Start the listener (create task, begin processing)
        virtual bool start() = 0;

        /// Stop the listener (request task exit, clean up)
        virtual void stop() = 0;

        /// Signal that the system state has changed.
        /// Must be non-blocking. May be called from ISR context or main loop.
        /// Multiple rapid calls coalesce into a single pending notification.
        virtual void notifyStateChanged() = 0;
    };

} // namespace ungula::eventbus
