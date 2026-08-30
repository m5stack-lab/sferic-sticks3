#pragma once

#include "ui.h"

// Builds the root menu with the tool categories from
// docs/adr/0001-onboard-rf-and-tool-capabilities.md. Every leaf is a stub
// InfoScreen for now; a real tool replaces the action with `ui.push(new
// SomeToolScreen())`.
Screen* makeRootMenu();
