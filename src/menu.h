#pragma once

// The main menu's own rules (docs/specs/main_menu.md, "Background battle"). No GPU, ImGui or clock, so tests/battle_test.c runs
// them; the menu's drawing and window are menu_view.c.

#include "battle.h"

// Puts `battle` back in deployment with the player's side made from the stage's: each enemy squad mirrored through the field's
// middle, its footprint's corner (x, row) going to (grid_width - x - footprint_w, grid_length - row - footprint_d). A mirrored
// squad that does not fit (outside the player's zone, past the supply) is left out. Returns how many were placed.
u32 menu_place_mirrored(Battle* battle);
