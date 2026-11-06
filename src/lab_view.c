// The unit lab's view (docs/specs/unit_lab.md): its camera, meshes, rings, paths and dimensions, and the panel's lab sections. The lab's
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
     .offset = offsetof(UnitDef, cost), .speed = 1.0f, .min = 1.0f, .max = 10000.0f, .battle_only = true},
    {.block = DEFS_BLOCK_UNIT, .key = "squad", .label = "Squad", .kind = UNIT_VALUE_INTEGER,
     .offset = offsetof(UnitDef, squad_size), .speed = 0.1f, .min = 1.0f, .max = BATTLE_MAX_SQUAD_SIZE, .battle_only = true},
    {.block = DEFS_BLOCK_UNIT, .key = "footprint", .label = "Footprint (cells)", .kind = UNIT_VALUE_INTEGER,
     .offset = offsetof(UnitDef, footprint), .speed = 0.05f, .min = 1.0f, .max = BATTLE_MAX_FOOTPRINT, .battle_only = true},
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
}

// Where a world point is on the canvas, in CSS pixels (ImGui's), as the camera is placed now. Fails behind the camera. A point
// outside the viewport is still given: what is drawn there is under the panel or off the canvas.
typedef struct ScreenPoint {
    b32 ok;
    ImVec2_c point;
} ScreenPoint;

internal ScreenPoint screen_point(Game* game, NvVec3 point)
{
    const NvSceneOutput* out = &game->layout.scene;
    NvMat4 view, projection;
    nv_renderer_camera_matrices(game->scene, *out, &view, &projection);
    NvMat4 view_projection = nv_mat4_mul(projection, view);
    f32 w = view_projection.e[3] * point.x + view_projection.e[7] * point.y + view_projection.e[11] * point.z + view_projection.e[15];
    if (w <= 1e-4f)
        return (ScreenPoint){0};
    NvVec3 clip = nv_mat4_transform_point(view_projection, point);
    f32 ratio = nv_window_pixel_ratio(&game->window);
    f32 x = (f32)out->image.x + (clip.x / w * 0.5f + 0.5f) * (f32)out->width * out->pixel_width;
    f32 y = (f32)out->image.y + (0.5f - clip.y / w * 0.5f) * (f32)out->height * out->pixel_height;
    return (ScreenPoint){.ok = true, .point = {x / ratio, y / ratio}};
}

// Colors (ImGui's ABGR) of the dimensions, each the color of what it measures.
#define DIMENSION_RANGE_COLOR    0xFF4DBFE6u // the range ring's
#define DIMENSION_STOP_COLOR     0xFF59A6BFu
#define DIMENSION_DISTANCE_COLOR 0xFFE6E6EEu
#define DIMENSION_SIZE_COLOR     0xFFFFD999u
#define DIMENSION_THICKNESS      1.5f
#define DIMENSION_CAP            6.0f  // pixels to each side of the line at its ends
#define DIMENSION_GAP_PAD        4.0f  // pixels between the text and the line on either side of it
#define DIMENSION_MIN_SIZE       20.0f // pixels a body's height or width must take on screen to be drawn
#define DIMENSION_BODY_GAP       0.3f  // meters between a body and its height's line

// A dimension line between two world points, as a drawing has one: a cap across each end and `text` in a gap in the middle,
// |-----20.0 m-----|. When the line is too short on screen for the text, the text goes beside it, on its `side` (+1 or -1:
// the screen's (-y, x) of the line from `from` to `to`, or the other). A line shorter than `min_length` pixels on screen is not
// drawn at all.
internal void draw_dimension(Game* game, ImDrawList* draw, NvVec3 from, NvVec3 to, const char* text, u32 color, f32 min_length, f32 side)
{
    // The constants are pixels at the UI's own size; they grow with it, as the text does.
    f32 ui = game_ui_scale(game);
    f32 cap = DIMENSION_CAP * ui, gap_pad = DIMENSION_GAP_PAD * ui, thickness = DIMENSION_THICKNESS * ui;
    min_length *= ui;
    ScreenPoint low = screen_point(game, from), high = screen_point(game, to);
    if (!low.ok || !high.ok)
        return;
    ImVec2_c a = low.point, b = high.point;
    f32 dx = b.x - a.x, dy = b.y - a.y, length = sqrtf(dx * dx + dy * dy);
    if (length < fmaxf(1.0f, min_length))
        return;
    ImVec2_c along = {dx / length, dy / length}, across = {-along.y, along.x};
    for (u32 end = 0; end < 2; ++end) {
        ImVec2_c p = end ? b : a;
        ImDrawList_AddLine(draw, (ImVec2_c){p.x - across.x * cap, p.y - across.y * cap},
                           (ImVec2_c){p.x + across.x * cap, p.y + across.y * cap}, color, thickness);
    }
    ImVec2_c size = igCalcTextSize(text, NULL, false, -1.0f);
    ImVec2_c middle = {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
    // How far the text's box reaches along the line from its middle.
    f32 half_gap = (fabsf(along.x) * size.x + fabsf(along.y) * size.y) * 0.5f + gap_pad;
    ImVec2_c text_pos;
    if (length > 2.0f * half_gap + 2.0f * cap) {
        ImDrawList_AddLine(draw, a, (ImVec2_c){middle.x - along.x * half_gap, middle.y - along.y * half_gap}, color, thickness);
        ImDrawList_AddLine(draw, (ImVec2_c){middle.x + along.x * half_gap, middle.y + along.y * half_gap}, b, color, thickness);
        text_pos = (ImVec2_c){middle.x - size.x * 0.5f, middle.y - size.y * 0.5f};
    } else {
        ImDrawList_AddLine(draw, a, b, color, thickness);
        // Beside the line: off its middle across it, by half the text's box that way.
        f32 reach = ((fabsf(across.x) * size.x + fabsf(across.y) * size.y) * 0.5f + cap + gap_pad) * side;
        text_pos = (ImVec2_c){middle.x + across.x * reach - size.x * 0.5f, middle.y + across.y * reach - size.y * 0.5f};
    }
    ImDrawList_AddRectFilled(draw, (ImVec2_c){text_pos.x - 3.0f * ui, text_pos.y - 1.0f * ui},
                             (ImVec2_c){text_pos.x + size.x + 3.0f * ui, text_pos.y + size.y + 1.0f * ui}, 0x99000000u, 3.0f * ui, 0);
    ImDrawList_AddText_Vec2(draw, text_pos, color, text, NULL);
}

// A body's height beside it (on the screen's right of it) and its width across its top (its base is where an air unit's
// altitude ends), when they are large enough on screen to read. When their text goes beside them, the height's goes right and
// the width's up, away from each other and from the body.
internal void draw_body_dimensions(Game* game, ImDrawList* draw, NvVec3 base, const UnitDef* def, NvVec3 right)
{
    char text[48];
    NvVec3 side = nv_vec3_add(base, nv_vec3_scale(right, def->radius + DIMENSION_BODY_GAP));
    snprintf(text, sizeof(text), T("Height %.2f m"), def->height);
    draw_dimension(game, draw, side, nv_vec3_add(side, nv_vec3(0.0f, def->height, 0.0f)), text, DIMENSION_SIZE_COLOR, DIMENSION_MIN_SIZE,
                   1.0f);
    NvVec3 top = nv_vec3(base.x, base.y + def->height, base.z);
    snprintf(text, sizeof(text), T("Width %.2f m"), def->radius * 2.0f);
    draw_dimension(game, draw, nv_vec3_sub(top, nv_vec3_scale(right, def->radius)), nv_vec3_add(top, nv_vec3_scale(right, def->radius)),
                   text, DIMENSION_SIZE_COLOR, DIMENSION_MIN_SIZE, -1.0f);
}

// Every frame in the lab, after the camera is placed for the frame (view_update), from where the units are drawn (between
// ticks): the range rings and the targets' ways as debug lines, and the dimensions (the range, the stop distance, the
// distance to the target, air units' heights, the bodies' sizes) on ImGui's background list, behind the panel.
void lab_view_overlay(Game* game)
{
    const Lab* lab = &game->lab;
    const Battle* battle = &lab->battle;
    const LabSettings* settings = &lab->settings;
    NvRenderer* renderer = &game->renderer;
    if (game->unit_nodes_made < LAB_SLOT_AIR)
        return; // the view has not drawn the lab's units yet
    NvVec3 drawn[LAB_SLOT_AIR + 1];
    for (u32 slot = LAB_SLOT_UNIT; slot <= LAB_SLOT_AIR; ++slot)
        drawn[slot] = nv_scene_get(game->scene, game->unit_nodes[slot])->position;

    // The unit's range on the ground, and where it stops when it walks.
    const WeaponDef* weapon = &lab->defs.units[LAB_DEF_UNIT].weapon;
    f32 stop = weapon->range * lab->defs.rules.stop_fraction;
    NvVec3 ground = nv_vec3(drawn[LAB_SLOT_UNIT].x, 0.05f, drawn[LAB_SLOT_UNIT].z);
    u32 shown = ~game->lab_hidden;
    b32 show_stop = settings->walk && (shown & LAB_SHOW_STOP);
    if (shown & LAB_SHOW_RANGE)
        view_draw_ring(renderer, ground, nv_vec3(1, 0, 0), nv_vec3(0, 0, 1), weapon->range, nv_vec3(0.9f, 0.75f, 0.3f));
    if (show_stop)
        view_draw_ring(renderer, ground, nv_vec3(1, 0, 0), nv_vec3(0, 0, 1), stop, nv_vec3(0.35f, 0.3f, 0.15f));
    // The way moving targets go back and forth, at their height.
    if ((shown & LAB_SHOW_PATHS) && settings->targets_move && settings->move_width > 0.0f) {
        f32 field_width = (f32)lab->defs.rules.grid_width * lab->defs.rules.cell_size;
        for (u32 i = 0; i < LAB_TARGET_COUNT; ++i) {
            NvVec3 home = lab->homes[i];
            f32 y = (i == LAB_AIR ? home.y : 0.0f) + 0.05f;
            f32 from = fmaxf(home.x - settings->move_width * 0.5f, settings->target_radius);
            f32 to = fminf(home.x + settings->move_width * 0.5f, field_width - settings->target_radius);
            nv_renderer_debug_line(renderer, nv_vec3(from, y, home.z), nv_vec3(to, y, home.z), nv_vec3(0.4f, 0.4f, 0.45f));
        }
    }
    ImDrawList* draw = igGetBackgroundDrawList(NULL);
    char text[48];
    // The range and the stop distance across the unit's facing, to its two sides, so they stay off the way it fires.
    const Unit* unit = &battle->units[LAB_SLOT_UNIT];
    NvVec3 across = nv_vec3(cosf(unit->yaw), 0.0f, -sinf(unit->yaw)); // the unit's left: facing (sin yaw, 0, cos yaw) turned
    if (shown & LAB_SHOW_RANGE) {
        snprintf(text, sizeof(text), T("Range %.1f m"), weapon->range);
        draw_dimension(game, draw, ground, nv_vec3_add(ground, nv_vec3_scale(across, weapon->range)), text, DIMENSION_RANGE_COLOR, 0.0f,
                       1.0f);
    }
    if (show_stop) {
        snprintf(text, sizeof(text), T("Stop %.1f m"), stop);
        draw_dimension(game, draw, ground, nv_vec3_sub(ground, nv_vec3_scale(across, stop)), text, DIMENSION_STOP_COLOR, 0.0f, 1.0f);
    }
    // The distance to the unit's target, on the ground (as the rules measure it).
    if ((shown & LAB_SHOW_DISTANCE) && unit->target.index) {
        NvVec3 target = drawn[unit->target.index];
        NvVec3 there = nv_vec3(target.x, 0.05f, target.z);
        f32 dx = there.x - ground.x, dz = there.z - ground.z;
        snprintf(text, sizeof(text), T("Distance %.1f m"), sqrtf(dx * dx + dz * dz));
        draw_dimension(game, draw, ground, there, text, DIMENSION_DISTANCE_COLOR, 0.0f, 1.0f);
    }
    // Air units' heights over the ground, and every body's size.
    NvNode* camera = nv_scene_get(game->scene, game->camera);
    NvVec3 right = nv_quat_rotate(camera->rotation, nv_vec3(1.0f, 0.0f, 0.0f));
    right.y = 0.0f;
    right = nv_vec3_dot(right, right) > 1e-8f ? nv_vec3_normalize(right) : nv_vec3(1.0f, 0.0f, 0.0f);
    for (u32 slot = LAB_SLOT_UNIT; slot <= LAB_SLOT_AIR; ++slot) {
        const Unit* body = &battle->units[slot];
        if (body->flags & UNIT_DEAD)
            continue;
        const UnitDef* def = &battle->defs->units[body->def];
        NvVec3 base = drawn[slot];
        if ((shown & LAB_SHOW_ALTITUDE) && def->layer == UNIT_LAYER_AIR) {
            snprintf(text, sizeof(text), T("Altitude %.1f m"), base.y);
            draw_dimension(game, draw, nv_vec3(base.x, 0.0f, base.z), base, text, DIMENSION_DISTANCE_COLOR, 0.0f, 1.0f);
        }
        if (shown & LAB_SHOW_SIZES)
            draw_body_dimensions(game, draw, base, def, right);
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

// What the view draws over the units, a box each, two to a row. Not a change of the run.
internal void view_section(Game* game)
{
    if (!igCollapsingHeader_TreeNodeFlags(TL("View"), ImGuiTreeNodeFlags_DefaultOpen))
        return;
    const struct { u32 bit; const char* label; } boxes[] = {
        {LAB_SHOW_RANGE, TL("Range")},           {LAB_SHOW_STOP, TL("Stop distance")}, {LAB_SHOW_DISTANCE, TL("Distance to target")},
        {LAB_SHOW_ALTITUDE, TL("Altitude")},     {LAB_SHOW_SIZES, TL("Body sizes")},    {LAB_SHOW_PATHS, TL("Target paths")},
    };
    if (!igBeginTable("lab_view", 2, ImGuiTableFlags_SizingStretchSame, (ImVec2_c){0, 0}, 0.0f))
        return;
    for (u32 i = 0; i < NV_ARRAY_COUNT(boxes); ++i) {
        igTableNextColumn();
        b32 on = !(game->lab_hidden & boxes[i].bit);
        if (lab_checkbox(boxes[i].label, &on))
            game->lab_hidden = on ? game->lab_hidden & ~boxes[i].bit : game->lab_hidden | boxes[i].bit;
    }
    igEndTable();
}

// The lab's unit def as the project's text has it: a change writes the text (game_set_unit_value).
internal void unit_section(Game* game)
{
    if (!igCollapsingHeader_TreeNodeFlags(TL("Unit"), 0))
        return;
    const UnitDef* def = &game->defs.units[game->lab_settings.unit_def];
    igBeginDisabled(!game_rules_editable(game));
    igPushItemWidth(igGetFontSize() * 8.0f);
    // What the run reads first, then what only a battle does (the squad and what it costs), under its heading.
    b32 heading_shown = false;
    for (u32 n = 0; n < 2 * NV_ARRAY_COUNT(unit_value_keys); ++n) {
        u32 i = n % NV_ARRAY_COUNT(unit_value_keys);
        b32 battle_pass = n >= NV_ARRAY_COUNT(unit_value_keys);
        const UnitValueKey* key = &unit_value_keys[i];
        if (key->battle_only != battle_pass)
            continue;
        if (battle_pass && !heading_shown) {
            igSeparatorText(T("Battle only"));
            heading_shown = true;
        }
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
    if (view_unit_buttons(game, &chosen, true, false))
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
    view_section(game);
    unit_section(game);
    results_section(game);
    igSeparator();
    igTextWrapped("%s", T("The unit fights two targets, one on the ground and one in the air, by the battle's rules. A change goes into the run as it goes; with Restart on change it starts the run over. Drag to move the view, drag with the right button or two fingers to turn it, and use the wheel or pinch to zoom."));
}
