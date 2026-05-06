// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#pragma once
#ifndef __cplusplus
#error UngulaEventbus requires a C++ compiler
#endif

// Ungula Event Brokers Library
// Include this header to activate the library in Arduino

#include "ungula/eventbus/i_system_state_listener.h"
#include "ungula/eventbus/system_state_notifier.h"

#ifdef ESP_PLATFORM
#include "ungula/eventbus/esp32_system_state_listener.h"
#endif
