// The unit lab (docs/specs/unit_lab.md). A run is a battle of three units on the lab's own defs: the unit (slot 1), the ground
// target (2) and the air target (3). The targets are UNIT_DUMMY units the lab moves and brings back; the round is endless. A
// change goes into the run at the tick it is made and is recorded, so a step back plays the run again from its start with the
// same changes at the same ticks: the rules are deterministic, so that is the state it had.

#include "lab.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

internal NvVec3 unit_home(const BattleRules* rules)
{
    f32 field_width = (f32)rules->grid_width * rules->cell_size, field_length = (f32)rules->grid_length * rules->cell_size;
    return nv_vec3(field_width * 0.5f, 0.0f, field_length * 0.25f);
}

f32 lab_max_distance(const BattleRules* rules)
{
    f32 field_length = (f32)rules->grid_length * rules->cell_size;
    return fmaxf(1.0f, field_length - LAB_FIELD_MARGIN - unit_home(rules).z);
}

LabSettings lab_default_settings(const BattleDefs* project, u32 unit_def)
{
    NV_ASSERT(unit_def < project->unit_count);
    LabSettings settings = {0};
    settings.unit_def = unit_def;
    settings.walk = true;
    settings.distance = fminf(project->units[unit_def].weapon.range * 1.5f, lab_max_distance(&project->rules));
    settings.move_speed = 4.0f;
    settings.move_width = 16.0f;
    settings.target_health = 300.0f;
    settings.target_radius = 1.0f;
    settings.target_height = 2.0f;
    settings.air_altitude = 10.0f;
    settings.respawn_delay = 1.0f;
    return settings;
}

internal void set_target_def(UnitDef* def, const LabSettings* settings, UnitLayer layer)
{
    *def = (UnitDef){0};
    snprintf(def->name, sizeof(def->name), "%s", layer == UNIT_LAYER_AIR ? "Air target" : "Ground target");
    def->cost = 1;
    def->squad_size = 1;
    def->footprint = 1;
    def->health = settings->target_health;
    def->armor = settings->target_armor;
    def->radius = settings->target_radius;
    def->height = settings->target_height;
    def->layer = layer;
    def->altitude = layer == UNIT_LAYER_AIR ? settings->air_altitude : 0.0f;
}

// The lab's defs from a unit def, rules and settings. The seed stays the one the run started with.
internal void set_defs(Lab* lab, const UnitDef* unit, const BattleRules* rules, const LabSettings* settings)
{
    lab->defs.rules = *rules;
    lab->defs.units[LAB_DEF_UNIT] = *unit;
    set_target_def(&lab->defs.units[LAB_DEF_GROUND], settings, UNIT_LAYER_GROUND);
    set_target_def(&lab->defs.units[LAB_DEF_AIR], settings, UNIT_LAYER_AIR);
    lab->defs.unit_count = LAB_DEF_COUNT;
}

// The targets stand `distance` ahead of the unit's home (no farther than the field allows), the ground one to -X and the air
// one to +X, half the gap each.
internal void set_homes(Lab* lab)
{
    const BattleRules* rules = &lab->defs.rules;
    NvVec3 home = unit_home(rules);
    f32 z = home.z + fminf(lab->settings.distance, lab_max_distance(rules));
    lab->homes[LAB_GROUND] = nv_vec3(home.x - LAB_TARGET_GAP * 0.5f, 0.0f, z);
    lab->homes[LAB_AIR] = nv_vec3(home.x + LAB_TARGET_GAP * 0.5f, lab->settings.air_altitude, z);
}

internal void bring_back_target(Lab* lab, u32 target)
{
    Unit* unit = &lab->battle.units[LAB_SLOT_GROUND + target];
    unit->flags &= ~(u32)UNIT_DEAD;
    unit->health = lab->defs.units[LAB_DEF_GROUND + target].health;
    unit->damage_taken = 0.0f;
    unit->position = unit->previous_position = lab->homes[target];
    unit->velocity = nv_vec3(0.0f, 0.0f, 0.0f);
    lab->directions[target] = 1.0f;
    lab->spawn_ticks[target] = lab->battle.tick;
}

// The run's start: the start's settings, unit and rules, the units at their homes, nothing measured.
internal void play_from_start(Lab* lab)
{
    lab->settings = lab->start_settings;
    set_defs(lab, &lab->start_unit, &lab->start_rules, &lab->settings);
    set_homes(lab);
    memset(&lab->results, 0, sizeof(lab->results));
    lab->changes_applied = 0;

    Battle* battle = &lab->battle;
    battle_init(battle, &lab->defs);
    NvVec3 position = unit_home(&lab->defs.rules);
    if (lab->start_unit.layer == UNIT_LAYER_AIR)
        position.y = lab->start_unit.altitude;
    battle_add_unit(battle, LAB_DEF_UNIT, TEAM_PLAYER, position, lab->settings.walk ? 0u : (u32)UNIT_HOLD);
    battle_add_unit(battle, LAB_DEF_GROUND, TEAM_ENEMY, lab->homes[LAB_GROUND], UNIT_DUMMY);
    battle_add_unit(battle, LAB_DEF_AIR, TEAM_ENEMY, lab->homes[LAB_AIR], UNIT_DUMMY);
    b32 started = battle_start(battle);
    NV_ASSERT(started); // it has a player unit
    (void)started;
    battle->endless = true;
    for (u32 i = 0; i < LAB_TARGET_COUNT; ++i) {
        lab->directions[i] = 1.0f;
        lab->spawn_ticks[i] = 0;
        lab->death_ticks[i] = 0;
    }
}

void lab_start(Lab* lab, const BattleDefs* project, const LabSettings* settings)
{
    NV_ASSERT(settings->unit_def < project->unit_count);
    lab->start_settings = *settings;
    lab->start_unit = project->units[settings->unit_def];
    lab->start_rules = project->rules;
    lab->defs.seed = project->seed;
    lab->change_count = 0;
    lab->changes_full = false;
    lab->furthest_tick = 0;
    play_from_start(lab);
}

// A change goes into the run as it stands (docs/specs/unit_lab.md, "Changes and restarting"): what is alive keeps its health
// up to the new maximum, a shield its energy up to the new capacity, and the targets go to new homes when theirs moved.
internal void apply_change(Lab* lab, const LabChange* change)
{
    if (change->reset_results) {
        memset(&lab->results, 0, sizeof(lab->results));
        return;
    }
    NvVec3 old_homes[LAB_TARGET_COUNT] = {lab->homes[LAB_GROUND], lab->homes[LAB_AIR]};
    lab->settings = change->settings;
    set_defs(lab, &change->unit, &change->rules, &lab->settings);
    set_homes(lab);
    ++lab->results.changes;

    Battle* battle = &lab->battle;
    for (u32 slot = LAB_SLOT_UNIT; slot <= LAB_SLOT_AIR; ++slot) {
        Unit* unit = &battle->units[slot];
        const UnitDef* def = &lab->defs.units[unit->def];
        unit->health = fminf(unit->health, def->health);
        if (def->ability.kind == ABILITY_SHIELD)
            unit->ability.shield.energy = fminf(unit->ability.shield.energy, def->ability.shield.capacity);
    }
    Unit* unit = &battle->units[LAB_SLOT_UNIT];
    unit->flags = (unit->flags & ~(u32)UNIT_HOLD) | (lab->settings.walk ? 0u : (u32)UNIT_HOLD);
    unit->position.y = change->unit.layer == UNIT_LAYER_AIR ? change->unit.altitude : 0.0f;
    for (u32 i = 0; i < LAB_TARGET_COUNT; ++i) {
        if (old_homes[i].x == lab->homes[i].x && old_homes[i].y == lab->homes[i].y && old_homes[i].z == lab->homes[i].z)
            continue;
        Unit* target = &battle->units[LAB_SLOT_GROUND + i];
        if (!(target->flags & UNIT_DEAD)) {
            target->position = target->previous_position = lab->homes[i];
            lab->directions[i] = 1.0f;
        }
    }
}

// Puts a change into the run at this tick. A change after a step back drops the recorded ones it would come before.
internal void record_change(Lab* lab, const LabChange* change)
{
    // The changes after this tick are dropped, so the run beyond it is no longer the one it was.
    lab->change_count = lab->changes_applied;
    lab->furthest_tick = lab->battle.tick;
    if (lab->change_count < LAB_MAX_CHANGES) {
        lab->changes[lab->change_count++] = *change;
        lab->changes_applied = lab->change_count;
    } else {
        lab->changes_full = true;
    }
    apply_change(lab, change);
}

void lab_change(Lab* lab, const BattleDefs* project, const LabSettings* settings)
{
    if (settings->unit_def != lab->settings.unit_def) {
        lab_start(lab, project, settings);
        return;
    }
    LabChange change = {0};
    change.tick = lab->battle.tick;
    change.settings = *settings;
    change.unit = project->units[settings->unit_def];
    change.rules = project->rules;
    // Restart on change is not a change of the run: compare without it.
    LabSettings before = lab->settings, after = *settings;
    before.restart_on_change = after.restart_on_change = false;
    if (memcmp(&before, &after, sizeof(before)) == 0 && memcmp(&change.unit, &lab->defs.units[LAB_DEF_UNIT], sizeof(change.unit)) == 0
        && memcmp(&change.rules, &lab->defs.rules, sizeof(change.rules)) == 0) {
        lab->settings.restart_on_change = settings->restart_on_change;
        return;
    }
    if (settings->restart_on_change) {
        lab_start(lab, project, settings);
        return;
    }
    record_change(lab, &change);
}

void lab_reset_results(Lab* lab)
{
    LabChange change = {0};
    change.tick = lab->battle.tick;
    change.reset_results = true;
    record_change(lab, &change);
}

// Moving targets go along X at the move speed and turn at the ends of their way, or at the field's edge.
internal void steer_targets(Lab* lab)
{
    const LabSettings* settings = &lab->settings;
    f32 field_width = (f32)lab->defs.rules.grid_width * lab->defs.rules.cell_size;
    for (u32 i = 0; i < LAB_TARGET_COUNT; ++i) {
        Unit* target = &lab->battle.units[LAB_SLOT_GROUND + i];
        target->velocity = nv_vec3(0.0f, 0.0f, 0.0f);
        if ((target->flags & UNIT_DEAD) || !settings->targets_move || settings->move_speed <= 0.0f || settings->move_width <= 0.0f)
            continue;
        f32 x = target->position.x, half = settings->move_width * 0.5f, radius = settings->target_radius;
        if (x >= lab->homes[i].x + half || x >= field_width - radius)
            lab->directions[i] = -1.0f;
        else if (x <= lab->homes[i].x - half || x <= radius)
            lab->directions[i] = 1.0f;
        target->velocity.x = lab->directions[i] * settings->move_speed;
    }
}

// The results from the events battle_tick just added: shots go to the unit's target as it fired, hits and deaths to the
// target they are about (its def).
internal void measure(Lab* lab, u32 first_event)
{
    Battle* battle = &lab->battle;
    LabResults* results = &lab->results;
    u32 target_slot = battle->units[LAB_SLOT_UNIT].target.index;
    for (u32 i = first_event; i < battle->event_count; ++i) {
        const BattleEvent* event = &battle->events[i];
        switch (event->kind) {
        case BATTLE_EVENT_FIRE:
            if (event->team != TEAM_PLAYER)
                break;
            ++results->shots;
            if (target_slot >= LAB_SLOT_GROUND && target_slot <= LAB_SLOT_AIR)
                ++results->targets[target_slot - LAB_SLOT_GROUND].shots;
            break;
        case BATTLE_EVENT_HIT:
            if (event->team != TEAM_ENEMY)
                break;
            ++results->hits;
            ++results->targets[event->def - LAB_DEF_GROUND].hits;
            results->targets[event->def - LAB_DEF_GROUND].damage += event->size;
            break;
        case BATTLE_EVENT_DEATH:
            if (event->team == TEAM_ENEMY) {
                u32 target = event->def - LAB_DEF_GROUND;
                LabTargetResults* target_results = &results->targets[target];
                u32 ticks = battle->tick - lab->spawn_ticks[target];
                ++target_results->kills;
                target_results->kill_ticks += ticks;
                target_results->last_kill_ticks = ticks;
                lab->death_ticks[target] = battle->tick;
            }
            break;
        case BATTLE_EVENT_SHIELD_HIT:
            break;
        }
    }
    results->ticks_with_target += target_slot != 0;
}

void lab_tick(Lab* lab)
{
    Battle* battle = &lab->battle;
    // After a step back, the recorded changes go in again at their ticks.
    while (lab->changes_applied < lab->change_count && lab->changes[lab->changes_applied].tick <= battle->tick)
        apply_change(lab, &lab->changes[lab->changes_applied++]);
    steer_targets(lab);
    u32 first_event = battle->event_count;
    battle_tick(battle);
    // NOTE: events past BATTLE_MAX_EVENTS are dropped before they are counted; the view clears them every tick.
    measure(lab, first_event);
    u32 respawn_ticks = lab->settings.respawn_delay > 0.0f ? battle_seconds_to_ticks(lab->settings.respawn_delay) : 0;
    for (u32 i = 0; i < LAB_TARGET_COUNT; ++i)
        if ((battle->units[LAB_SLOT_GROUND + i].flags & UNIT_DEAD) && battle->tick - lab->death_ticks[i] >= respawn_ticks)
            bring_back_target(lab, i);
    if (battle->tick > lab->furthest_tick)
        lab->furthest_tick = battle->tick;
}

b32 lab_seek(Lab* lab, u32 tick)
{
    // Forward runs on from here, which needs no record; back plays the run again from its start.
    if (tick < lab->battle.tick) {
        if (lab->changes_full)
            return false;
        play_from_start(lab);
    }
    while (lab->battle.tick < tick) {
        lab_tick(lab);
        lab->battle.event_count = 0;
    }
    // The changes made at this very tick were made after it ran.
    while (lab->changes_applied < lab->change_count && lab->changes[lab->changes_applied].tick <= lab->battle.tick)
        apply_change(lab, &lab->changes[lab->changes_applied++]);
    lab->battle.events_dropped = 0;
    return true;
}

u32 lab_misses(const Lab* lab)
{
    u32 resolved = lab->results.hits + lab->battle.projectile_count;
    return lab->results.shots > resolved ? lab->results.shots - resolved : 0;
}

LabExpected lab_expected(const Lab* lab, u32 target)
{
    NV_ASSERT(target < LAB_TARGET_COUNT);
    const UnitDef* unit = &lab->defs.units[LAB_DEF_UNIT];
    const UnitDef* target_def = &lab->defs.units[LAB_DEF_GROUND + target];
    const WeaponDef* weapon = &unit->weapon;
    const BattleRules* rules = &lab->defs.rules;
    LabExpected expected = {0};
    if (!(weapon->targets & (1u << target_def->layer)))
        return expected;
    expected.ok = true;
    expected.damage_per_hit = fmaxf(weapon->damage - target_def->armor, weapon->damage * rules->min_damage_fraction);
    f32 cooldown = (f32)battle_seconds_to_ticks(weapon->cooldown) * BATTLE_TICK_SECONDS;
    expected.dps = expected.damage_per_hit / cooldown;
    if (expected.damage_per_hit <= 0.0f)
        return expected;
    expected.shots_to_kill = (u32)ceilf(target_def->health / expected.damage_per_hit - 0.0001f);
    // The last shell flies from where the unit fires: its stop distance when it walks, its home when it holds.
    NvVec3 home = unit_home(rules);
    f32 dx = lab->homes[target].x - home.x, dz = lab->homes[target].z - home.z;
    f32 distance = sqrtf(dx * dx + dz * dz);
    if (lab->settings.walk)
        distance = fminf(distance, weapon->range * rules->stop_fraction);
    f32 muzzle_height = (unit->layer == UNIT_LAYER_AIR ? unit->altitude : 0.0f) + weapon->muzzle.y;
    f32 aim_height = target_def->layer == UNIT_LAYER_AIR ? target_def->altitude + target_def->height * 0.5f : 0.0f;
    f32 rise = distance * tanf(weapon->launch_angle) - (aim_height - muzzle_height);
    f32 flight = rise > 0.0f ? sqrtf(2.0f * rise / rules->gravity) : 0.0f;
    expected.seconds_to_kill = (f32)(expected.shots_to_kill - 1) * cooldown + flight;
    return expected;
}
