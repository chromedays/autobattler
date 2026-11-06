#pragma once

// The unit lab (docs/specs/unit_lab.md): one unit of a unit def against two targets, a ground one and an air one, on the
// battle's own rules (battle_tick). Like battle.c it reads no GPU, ImGui or clock, so tests run it whole; the view steps it
// with lab_tick at the same 30 Hz and changes it only through the functions here.

#include "battle.h"

#define LAB_TARGET_GAP      6.0f // meters between the two targets across the field
#define LAB_FIELD_MARGIN    2.0f // meters the targets keep from the field's far edge
#define LAB_MAX_CHANGES     4096 // changes put into one run as it goes, for stepping back (a widget dragged makes one a frame)
#define LAB_TARGET_COUNT    2

// The lab's unit defs and the slots of its units.
enum { LAB_DEF_UNIT, LAB_DEF_GROUND, LAB_DEF_AIR, LAB_DEF_COUNT };
enum { LAB_SLOT_UNIT = 1, LAB_SLOT_GROUND = 2, LAB_SLOT_AIR = 3 };
// A target by its index in the results: 0 the ground target, 1 the air target (slot LAB_SLOT_GROUND + index).
enum { LAB_GROUND, LAB_AIR };

// What the lab tries, set in its panel and not saved.
typedef struct LabSettings {
    u32 unit_def; // index into the project's units
    b32 walk;     // walk into range as in a battle; off holds the unit where it stands
    f32 distance; // meters from the unit to the targets
    b32 targets_move;
    f32 move_speed, move_width; // m/s; meters, half to each side of the targets' homes
    f32 target_health, target_armor;
    f32 target_radius, target_height; // meters
    f32 air_altitude;                 // meters
    f32 respawn_delay;                // seconds
    b32 restart_on_change; // a change starts the run over instead of going into it
} LabSettings;

typedef struct LabTargetResults {
    u32 shots, hits, kills;
    f32 damage;            // after armor
    u32 kill_ticks;        // the times to kill, added up
    u32 last_kill_ticks;   // the last one
} LabTargetResults;

typedef struct LabResults {
    LabTargetResults targets[LAB_TARGET_COUNT];
    u32 shots, hits;       // all of them
    u32 ticks_with_target; // ticks the unit had a target: what DPS is over
    u32 changes;           // changes put into the run since the results were last reset
} LabResults;

// A change put into the run at a tick (before that tick ran): what the run's settings and the lab's copy of the unit and the
// rules became, or a reset of the results.
typedef struct LabChange {
    u32 tick;
    b32 reset_results; // only that: the rest is unused
    LabSettings settings;
    UnitDef unit;
    BattleRules rules;
} LabChange;

typedef struct Lab {
    BattleDefs defs; // the lab's own: the unit def, the ground target, the air target; the battle reads these
    Battle battle;
    LabSettings settings;
    LabResults results;
    // The run's start, which stepping back plays again from, and the changes put into it since, in tick order.
    LabSettings start_settings;
    UnitDef start_unit;
    BattleRules start_rules;
    LabChange changes[LAB_MAX_CHANGES];
    u32 change_count;
    u32 changes_applied; // the changes the run has passed: a step back leaves those after it to be played again
    b32 changes_full;    // a change did not fit, so the run cannot be played again: no stepping back
    u32 furthest_tick;   // the furthest the run has gone (the playback bar's timeline); a change after a step back brings it back
    // The targets.
    NvVec3 homes[LAB_TARGET_COUNT];
    f32 directions[LAB_TARGET_COUNT]; // +1 or -1 along X while they move
    u32 spawn_ticks[LAB_TARGET_COUNT];
    u32 death_ticks[LAB_TARGET_COUNT];
} Lab;

// What the definitions say the unit should do to a target, for the panel to set beside the results.
typedef struct LabExpected {
    b32 ok;               // false when the unit cannot hit that target's layer
    f32 damage_per_hit;   // after armor
    u32 shots_to_kill;    // attacks: shells, or melee strikes of hits_per_attack hits
    f32 dps;
    f32 seconds_to_kill;  // the attacks after the first, at the attack interval, and the last shell's flight
} LabExpected;

// The settings a lab starts with for unit def `unit_def` of a project.
LabSettings lab_default_settings(const BattleDefs* project, u32 unit_def);
// The farthest the targets may stand from the unit on a project's field.
f32 lab_max_distance(const BattleRules* rules);

// Starts a run over: the project's unit def `settings->unit_def` and rules are copied in, the units put at their homes, the
// results and the changes cleared. `lab` must stay where it is while it runs (its battle reads its defs).
void lab_start(Lab* lab, const BattleDefs* project, const LabSettings* settings);
// A change of the settings, of the project's unit def or of its rules (docs/specs/unit_lab.md, "Changes and restarting"):
// another unit def, or one with restart_on_change, starts the run over; otherwise it goes into the run as it is and is
// recorded for stepping back. A change of restart_on_change alone is only kept.
void lab_change(Lab* lab, const BattleDefs* project, const LabSettings* settings);
void lab_reset_results(Lab* lab);

// One 30 Hz tick: the targets' velocity, battle_tick, the results from its events and the targets that come back. The events
// stay for the view (it clears them).
void lab_tick(Lab* lab);
// Puts the run at `tick`: back by playing it again from its start with its changes, forward by running on from where it is.
// False (and nothing changes) for a step back when too many changes went into the run to play it again.
b32 lab_seek(Lab* lab, u32 tick);

u32 lab_misses(const Lab* lab); // shots that hit nothing: not hits and no longer in flight
LabExpected lab_expected(const Lab* lab, u32 target);
