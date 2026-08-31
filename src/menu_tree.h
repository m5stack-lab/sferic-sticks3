#pragma once

#include "ui.h"

// Populate the registry with the built-in tools (ADR-0004). Call once at
// startup, after registry.begin().
void registerBuiltinTools();

// The root menu — built live from the registry, so tool on/off toggles
// (Settings > Tools) take effect on the next visit.
Screen* makeRootMenu();
