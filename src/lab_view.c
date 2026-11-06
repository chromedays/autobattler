// The unit lab's view (docs/specs/unit_lab.md): its camera, meshes, rings and paths, and the panel's lab sections. The lab's
// battle is drawn by battle_view.c's view_update like the round's; the rules are lab.c, changed here only through
// game_lab_settings_changed, game_lab_choose_unit, game_set_unit_value, lab_start and lab_reset_results.

#include "game.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

#define LAB_CAMERA_PITCH        (50.0f * NV_PI / 180.0f)
#define LAB_CAMERA_MIN_DISTANCE 40.0f
#define LAB_CAMERA_PER_DISTANCE 2.0f // the camera's distance for each meter between the unit and its targets

// The keys the Unit section edits, in the file's order (abproj.md, "unit <name>"). The labels go through T() where they are
// shown; strings_test finds them by their `.label =`.
global const UnitValueKey unit_value_keys[] = {
    {.block = DEFS_BLOCK_UNIT, .key = "cost", .label = "Cost", .kind = UNIT_VALUE_INTEGER,
     .offset = offsetof(UnitDef, cost), .speed = 1.0f, .min = 1.0f, .max = 10000.0f},
    {.block = DEFS_BLOCK_UNIT, .key = "squad", .label = "Squad", .kind = UNIT_VALUE_INTEGER,
     .offset = offsetof(UnitDef, squad_size), .speed = 0.1f, .min = 1.0f, .max = BATTLE_MAX_SQUAD_SIZE},
    {.block = DEFS_BLOCK_UNIT, .key = "footprint", .label = "Footprint (cells)", .kind = UNIT_VALUE_INTEGER,
     .offset = offsetof(UnitDef, footprint), .speed = 0.05f, .min = 1.0f, .max = BATTLE_MAX_FOOTPRINT},
    {.block = DEFS_BLOCK_UNIT, .key = "health", .label = "Health", .kind = UNIT_VALUE_NUMBER,
     .offset = offsetof(UnitDef, health), .speed = 1.0f, .min = 1.0f, .max = 100000.0f},
    {.block = DEFS_BLOCK_UNIT, .key = "armor", .label = "Armor", .kind = UNIT_VALUE_NUMBER,
     .offset = offsetof(UnitDef, armor), .speed = 0.1f, .min = 0.0f, .max = 1000.0f},
    {.block = DEFS_BLOCK_UNIT, .key = "speed", .label = "Speed (m/s)", .kind = UNIT_VALUE_NUMBER,
     .offset = offsetof(UnitDef, speed), .speed = 0.05f, .min = 0.0f, .max = 100.0f},
    {.block = DEFS_BLOCK_UNIT, .key = "radius", .label = "Radius (m)", .kind = UNIT_VALUE_NUMBER,
     .offset = offsetof(UnitDef, radius), .speed = 0.01f, .min = 0.05f, .max = 10.0f},
    {.block = DEFS_BLOCK_UNIT, .key = "height", .label = "Height (m)", .kind = UNIT_VALUE_NUMBER,
     .offset = offsetof(UnitDef, height), .speed = 0.01f, .min = 0.05f, .max = 20.0f},
    {.block = DEFS_BLOCK_UNIT, .key = "altitude", .label = "Altitude (m)", .kind = UNIT_VALUE_NUMBER,
     .offset = offsetof(UnitDef, altitude), .speed = 0.1f, .min = 0.1f, .max = 100.0f},
    {.block = DEFS_BLOCK_WEAPON, .key = "range", .label = "Range (m)", .kind = UNIT_VALUE_NUMBER,
     .offset = offsetof(UnitDef, weapon.range), .speed = 0.1f, .min = 0.1f, .max = 300.0f},
    {.block = DEFS_BLOCK_WEAPON, .key = "damage", .label = "Damage", .kind = UNIT_VALUE_NUMBER,
     .offset = offsetof(UnitDef, weapon.damage), .speed = 0.5f, .min = 0.0f, .max = 10000.0f},
    {.block = DEFS_BLOCK_WEAPON, .key = "cooldown", .label = "Cooldown (s)", .kind = UNIT_VALUE_NUMBER,
     .offset = offsetof(UnitDef, weapon.cooldown), .speed = 0.01f, .min = 0.04f, .max = 60.0f},
    {.block = DEFS_BLOCK_WEAPON, .key = "launch_angle", .label = "Launch angle (°)", .kind = UNIT_VALUE_ANGLE,
     .offset = offsetof(UnitDef, weapon.launch_angle), .speed = 0.2f, .min = 1.0f, .max = 89.0f},
    {.block = DEFS_BLOCK_WEAPON, .key = "spread", .label = "Spread (m)", .kind = UNIT_VALUE_NUMBER,
     .offset = offsetof(UnitDef, weapon.spread), .speed = 0.01f, .min = 0.0f, .max = 20.0f},
    {.block = DEFS_BLOCK_WEAPON, .key = "muzzle", .label = "Muzzle (x, y, z)", .kind = UNIT_VALUE_VECTOR,
     .offset = offsetof(UnitDef, weapon.muzzle), .speed = 0.01f, .min = -10.0f, .max = 10.0f},
    {.block = DEFS_BLOCK_ABILITY, .key = "radius", .label = "Shield radius (m)", .kind = UNIT_VALUE_NUMBER,
     .offset = offsetof(UnitDef, ability.shield.radius), .speed = 0.01f, .min = 0.05f, .max = 20.0f},
    {.block = DEFS_BLOCK_ABILITY, .key = "capacity", .label = "Shield capacity", .kind = UNIT_VALUE_NUMBER,
     .offset = offsetof(UnitDef, ability.shield.capacity), .speed = 0.5f, .min = 0.5f, .max = 10000.0f},
    {.block = DEFS_BLOCK_ABILITY, .key = "regen", .label = "Shield regen (/s)", .kind = UNIT_VALUE_NUMBER,
     .offset = offsetof(UnitDef, ability.shield.regen), .speed = 0.1f, .min = 0.0f, .max = 1000.0f},
    {.block = DEFS_BLOCK_ABILITY, .key = "regen_delay", .label = "Shield regen delay (s)", .kind = UNIT_VALUE_NUMBER,
     .offset = offsetof(UnitDef, ability.shield.regen_delay), .speed = 0.05f, .min = 0.0f, .max = 60.0f},
};

const UnitValueKey* lab_unit_value_key(u32 index)
{
    return index < NV_ARRAY_COUNT(unit_value_keys) ? &unit_value_keys[index] : NULL;
}

// The camera looks at the middle between the unit and its targets, from behind the unit (-Z), far enough to see both.
void lab_view_start(Game* game)
{
    const Lab* lab = &game->lab;
    NvVec3 unit = lab->battle.units[LAB_SLOT_UNIT].position;
    NvVec3 targets = nv_vec3_scale(nv_vec3_add(lab->homes[LAB_GROUND], lab->homes[LAB_AIR]), 0.5f);
    f32 dx = targets.x - unit.x, dz = targets.z - unit.z;
    f32 between = sqrtf(dx * dx + dz * dz);
    game->orbit.target = nv_vec3((unit.x + targets.x) * 0.5f, 0.0f, (unit.z + targets.z) * 0.5f);
    game->orbit.yaw = NV_PI;
    game->orbit.pitch = LAB_CAMERA_PITCH;
    game->orbit.distance = nv_clamp_f32(fmaxf(LAB_CAMERA_MIN_DISTANCE, between * LAB_CAMERA_PER_DISTANCE), game->orbit.min_distance,
                                        game->orbit.max_distance);
}

// A target's mesh: an upright cylinder as wide and tall as it is hit.
internal void set_target_mesh(Game* game, NvMeshId* slot, const UnitDef* def)
{
    NvVertex vertices[NV_MESH_CYLINDER_VERTICES(20)];
    u32 indices[NV_MESH_CYLINDER_INDICES(20)];
    NvMeshBuilder mesh = {.vertices = vertices, .vertex_capacity = NV_ARRAY_COUNT(vertices), .indices = indices,
                          .index_capacity = NV_ARRAY_COUNT(indices)};
    nv_mesh_append_cylinder(&mesh, nv_vec3(0.0f, def->height * 0.5f, 0.0f), def->radius, def->height * 0.5f, 20);
    NvMeshData data = nv_mesh_builder_data(&mesh);
    if (slot->index)
        nv_renderer_replace_mesh(&game->renderer, *slot, &data);
    else
        *slot = nv_renderer_add_mesh(&game->renderer, &data);
}

void lab_view_draw(Game* game)
{
    const Lab* lab = &game->lab;
    const LabSettings* settings = &lab->settings;
    NvRenderer* renderer = &game->renderer;
    // The meshes follow the lab's defs: remade in their slots when a size changed.
    for (u32 i = 0; i < LAB_DEF_COUNT; ++i) {
        const UnitDef* def = &lab->defs.units[i];
        if (game->lab_meshes[i].index && game->lab_mesh_sizes[i][0] == def->radius && game->lab_mesh_sizes[i][1] == def->height)
            continue;
        if (i == LAB_DEF_UNIT)
            view_set_unit_mesh(game, &game->lab_meshes[i], def);
        else
            set_target_mesh(game, &game->lab_meshes[i], def);
        game->lab_mesh_sizes[i][0] = def->radius;
        game->lab_mesh_sizes[i][1] = def->height;
    }

    // The unit's range on the ground, and where it stops when it walks.
    const Unit* unit = &lab->battle.units[LAB_SLOT_UNIT];
    const WeaponDef* weapon = &lab->defs.units[LAB_DEF_UNIT].weapon;
    NvVec3 ground = nv_vec3(unit->position.x, 0.05f, unit->position.z);
    view_draw_ring(renderer, ground, nv_vec3(1, 0, 0), nv_vec3(0, 0, 1), weapon->range, nv_vec3(0.9f, 0.75f, 0.3f));
    if (settings->walk)
        view_draw_ring(renderer, ground, nv_vec3(1, 0, 0), nv_vec3(0, 0, 1), weapon->range * lab->defs.rules.stop_fraction,
                       nv_vec3(0.35f, 0.3f, 0.15f));

    // The way moving targets go back and forth, at their height.
    if (settings->targets_move && settings->move_width > 0.0f) {
        f32 field_width = (f32)lab->defs.rules.grid_width * lab->defs.rules.cell_size;
        for (u32 i = 0; i < LAB_TARGET_COUNT; ++i) {
            NvVec3 home = lab->homes[i];
            f32 y = (i == LAB_AIR ? home.y : 0.0f) + 0.05f;
            f32 from = fmaxf(home.x - settings->move_width * 0.5f, settings->target_radius);
            f32 to = fminf(home.x + settings->move_width * 0.5f, field_width - settings->target_radius);
            nv_renderer_debug_line(renderer, nv_vec3(from, y, home.z), nv_vec3(to, y, home.z), nv_vec3(0.4f, 0.4f, 0.45f));
        }
    }
}

// The panel

internal b32 lab_number(const char* label, f32* value, f32 speed, f32 min, f32 max, const char* format)
{
    return igDragFloat(label, value, speed, min, max, format, ImGuiSliderFlags_AlwaysClamp);
}

internal b32 lab_checkbox(const char* label, b32* value)
{
    bool on = *value != 0;
    b32 changed = igCheckbox(label, &on);
    *value = on;
    return changed;
}

// What the lab tries: each change goes into the run (or starts it over, with Restart on change).
internal void conditions_section(Game* game)
{
    LabSettings* settings = &game->lab_settings;
    if (!igCollapsingHeader_TreeNodeFlags(TL("Lab"), ImGuiTreeNodeFlags_DefaultOpen))
        return;
    igPushItemWidth(igGetFontSize() * 8.0f);
    b32 changed = false;
    changed |= lab_checkbox(TL("Walk into range"), &settings->walk);
    changed |= lab_number(TL("Distance (m)"), &settings->distance, 0.1f, 1.0f, lab_max_distance(&game->defs.rules), "%.1f");
    changed |= lab_checkbox(TL("Targets move"), &settings->targets_move);
    igBeginDisabled(!settings->targets_move);
    changed |= lab_number(TL("Move speed (m/s)"), &settings->move_speed, 0.05f, 0.0f, 30.0f, "%.2f");
    changed |= lab_number(TL("Move width (m)"), &settings->move_width, 0.1f, 0.0f, 40.0f, "%.1f");
    igEndDisabled();
    changed |= lab_number(TL("Target health"), &settings->target_health, 1.0f, 1.0f, 100000.0f, "%.0f");
    changed |= lab_number(TL("Target armor"), &settings->target_armor, 0.1f, 0.0f, 1000.0f, "%.1f");
    changed |= lab_number(TL("Target radius (m)"), &settings->target_radius, 0.01f, 0.1f, 5.0f, "%.2f");
    changed |= lab_number(TL("Target height (m)"), &settings->target_height, 0.01f, 0.1f, 10.0f, "%.2f");
    changed |= lab_number(TL("Air altitude (m)"), &settings->air_altitude, 0.1f, 1.0f, 50.0f, "%.1f");
    changed |= lab_number(TL("Respawn delay (s)"), &settings->respawn_delay, 0.01f, 0.0f, 10.0f, "%.2f");
    igPopItemWidth();
    if (changed)
        game_lab_settings_changed(game);
}

// The lab's unit def as the project's text has it: a change writes the text (game_set_unit_value).
internal void unit_section(Game* game)
{
    if (!igCollapsingHeader_TreeNodeFlags(TL("Unit"), 0))
        return;
    const UnitDef* def = &game->defs.units[game->lab_settings.unit_def];
    igBeginDisabled(!game_rules_editable(game));
    igPushItemWidth(igGetFontSize() * 8.0f);
    for (u32 i = 0; i < NV_ARRAY_COUNT(unit_value_keys); ++i) {
        const UnitValueKey* key = &unit_value_keys[i];
        if (key->block == DEFS_BLOCK_ABILITY && def->ability.kind != ABILITY_SHIELD)
            continue;
        if (key->block == DEFS_BLOCK_UNIT && def->layer != UNIT_LAYER_AIR && key->offset == offsetof(UnitDef, altitude))
            continue;
        const u8* field = (const u8*)def + key->offset;
        char label[96];
        snprintf(label, sizeof(label), "%s###unit_value%u", T(key->label), i);
        f64 values[DEFS_UNIT_MAX_VALUES] = {0};
        u32 count = 1;
        b32 changed = false;
        switch (key->kind) {
        case UNIT_VALUE_INTEGER: {
            int value = (int)*(const u32*)field;
            changed = igDragInt(label, &value, key->speed, (int)key->min, (int)key->max, "%d", ImGuiSliderFlags_AlwaysClamp);
            values[0] = value;
        } break;
        case UNIT_VALUE_NUMBER: {
            f32 value = *(const f32*)field;
            changed = igDragFloat(label, &value, key->speed, key->min, key->max, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            values[0] = value;
        } break;
        case UNIT_VALUE_ANGLE: {
            f32 value = *(const f32*)field * (180.0f / NV_PI); // degrees, as the file writes it
            changed = igDragFloat(label, &value, key->speed, key->min, key->max, "%.1f", ImGuiSliderFlags_AlwaysClamp);
            values[0] = value;
        } break;
        case UNIT_VALUE_VECTOR: {
            NvVec3 vector = *(const NvVec3*)field;
            f32 value[3] = {vector.x, vector.y, vector.z};
            igPushItemWidth(igGetFontSize() * 11.0f);
            changed = igDragFloat3(label, value, key->speed, key->min, key->max, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            igPopItemWidth();
            values[0] = value[0];
            values[1] = value[1];
            values[2] = value[2];
            count = 3;
        } break;
        }
        if (changed) {
            game_set_unit_value(game, key->block, key->key, values, count);
            def = &game->defs.units[game->lab_settings.unit_def]; // the defs were read again
        }
    }
    igPopItemWidth();
    igEndDisabled();
}

// A table of the two targets: a row label, then one cell each. A target the unit cannot hit shows a dash.
internal b32 begin_target_table(const char* id, const char* title)
{
    if (!igBeginTable(id, 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg, (ImVec2_c){0, 0}, 0.0f))
        return false;
    igTableSetupColumn(title, 0, 1.3f, 0);
    igTableSetupColumn(T("Ground"), 0, 1.0f, 0);
    igTableSetupColumn(T("Air"), 0, 1.0f, 0);
    igTableHeadersRow();
    return true;
}

// What the run measured, and beside it what the definitions say, a column for each target.
internal void results_section(Game* game)
{
    Lab* lab = &game->lab;
    const LabResults* results = &lab->results;
    if (!igCollapsingHeader_TreeNodeFlags(TL("Results"), ImGuiTreeNodeFlags_DefaultOpen))
        return;
    LabExpected expected[LAB_TARGET_COUNT] = {lab_expected(lab, LAB_GROUND), lab_expected(lab, LAB_AIR)};
    f32 seconds_with_target = (f32)results->ticks_with_target * BATTLE_TICK_SECONDS;
    if (begin_target_table("lab_measured", T("Measured"))) {
        const char* rows[] = {T("Shots"), T("Hits"), T("Damage"), T("Kills"), T("Kill time, last (s)"), T("Kill time, mean (s)"),
                              T("DPS")};
        for (u32 row = 0; row < NV_ARRAY_COUNT(rows); ++row) {
            igTableNextRow(0, 0.0f);
            igTableNextColumn();
            igTextUnformatted(rows[row], NULL);
            for (u32 t = 0; t < LAB_TARGET_COUNT; ++t) {
                igTableNextColumn();
                const LabTargetResults* target = &results->targets[t];
                if (!expected[t].ok) {
                    igTextDisabled("-");
                    continue;
                }
                switch (row) {
                case 0: igText("%u", target->shots); break;
                case 1: igText("%u", target->hits); break;
                case 2: igText("%.0f", target->damage); break;
                case 3: igText("%u", target->kills); break;
                case 4: target->kills ? igText("%.2f", (f32)target->last_kill_ticks * BATTLE_TICK_SECONDS) : igTextDisabled("-"); break;
                case 5:
                    if (target->kills)
                        igText("%.2f", (f32)target->kill_ticks / (f32)target->kills * BATTLE_TICK_SECONDS);
                    else
                        igTextDisabled("-");
                    break;
                default: igText("%.1f", seconds_with_target > 0.0f ? target->damage / seconds_with_target : 0.0f); break;
                }
            }
        }
        igEndTable();
    }
    igText(T("Misses: %u"), lab_misses(lab));
    if (begin_target_table("lab_expected", T("Expected"))) {
        const char* rows[] = {T("Per hit"), T("Shots to kill"), T("Kill time (s)"), T("DPS")};
        for (u32 row = 0; row < NV_ARRAY_COUNT(rows); ++row) {
            igTableNextRow(0, 0.0f);
            igTableNextColumn();
            igTextUnformatted(rows[row], NULL);
            for (u32 t = 0; t < LAB_TARGET_COUNT; ++t) {
                igTableNextColumn();
                if (!expected[t].ok) {
                    igTextDisabled("-");
                    continue;
                }
                switch (row) {
                case 0: igText("%.1f", expected[t].damage_per_hit); break;
                case 1: igText("%u", expected[t].shots_to_kill); break;
                case 2: igText("%.2f", expected[t].seconds_to_kill); break;
                default: igText("%.1f", expected[t].dps); break;
                }
            }
        }
        igEndTable();
    }
    for (u32 t = 0; t < LAB_TARGET_COUNT; ++t)
        if (!expected[t].ok)
            igTextWrapped("%s", t == LAB_AIR ? T("The unit's weapon cannot hit the air target.") : T("The unit's weapon cannot hit the ground target."));
    if (results->changes)
        igTextWrapped(T("Results span %u changes"), results->changes);
    if (igButton(TL("Reset results"), (ImVec2_c){0, 0}))
        lab_reset_results(lab);
}

void lab_view_panel(Game* game)
{
    Lab* lab = &game->lab;
    u32 chosen = game->lab_settings.unit_def;
    if (view_unit_buttons(game, &chosen, true))
        game_lab_choose_unit(game, chosen);
    const UnitDef* def = &game->defs.units[game->lab_settings.unit_def];
    if (def->squad_size > 1)
        igTextWrapped(T("A squad of %u: the lab tries one of them."), def->squad_size);

    if (igButton(TL("Restart"), (ImVec2_c){0, 0})) {
        lab_start(lab, &game->defs, &game->lab_settings);
        nv_vfx_clear(&game->vfx);
        game->accumulator = 0.0f;
    }
    igSameLine(0.0f, -1.0f);
    if (lab_checkbox(TL("Restart on change"), &game->lab_settings.restart_on_change))
        game_lab_settings_changed(game);
    view_time_controls(game, lab->battle.tick > 0 && !lab->changes_full, true);
    if (lab->changes_full) {
        igPushStyleColor_Vec4(ImGuiCol_Text, igGetStyle()->Colors[ImGuiCol_TextDisabled]);
        igTextWrapped("%s", T("Too many changes to step back; Restart to step back again."));
        igPopStyleColor(1);
    }
    igText(T("Time: %.1f s (%u ticks)"), (f32)lab->battle.tick * BATTLE_TICK_SECONDS, lab->battle.tick);

    conditions_section(game);
    unit_section(game);
    results_section(game);
    igSeparator();
    igTextWrapped("%s", T("The unit fights two targets, one on the ground and one in the air, by the battle's rules. A change goes into the run as it goes; with Restart on change it starts the run over. Drag to move the view, drag with the right button or two fingers to turn it, and use the wheel or pinch to zoom."));
}
