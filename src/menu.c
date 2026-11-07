// The main menu's background battle (docs/specs/main_menu.md, "Background battle"): the player's side is the stage's enemy,
// mirrored. The round itself is battle.c's.

#include "menu.h"

u32 menu_place_mirrored(Battle* battle)
{
    battle_retry(battle);
    battle_clear_placement(battle);
    const BattleDefs* defs = battle->defs;
    const BattleRules* rules = &defs->rules;
    u32 placed = 0;
    for (u32 i = 0; i < defs->enemy_count; ++i) {
        const StagePlace* place = &defs->enemy[i];
        const UnitDef* def = &defs->units[place->def];
        s32 x = (s32)rules->grid_width - (s32)place->cell_x - (s32)def->footprint_w;
        s32 row = (s32)rules->grid_length - (s32)place->cell_row - (s32)def->footprint_d;
        placed += battle_place(battle, place->def, x, row); // refused where it does not fit: left out
    }
    return placed;
}
