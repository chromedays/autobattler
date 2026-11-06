#pragma once

// The auto-battler's rules and the project file they read (docs/specs/battle.md, "Stage A rules"; docs/specs/abproj.md,
// "default.abproj"; docs/specs/unit_fields.md, the unit fields of the wiki's unit_stats.csv). Nothing here draws or reads the frame's time: the view steps the battle with battle_tick at 30 Hz and
// copies what it needs into nodes and effects, so tests can run whole rounds.

#include <engine/base.h>
#include <engine/math.h>

#include <stdbool.h>

// The tick rate is the engine's decision, not data: every time in the project file becomes whole ticks by it.
#define BATTLE_TICK_RATE      30
#define BATTLE_TICK_SECONDS   (1.0f / BATTLE_TICK_RATE)

// Capacities. The sizes the project file's rules can ask for stop here (the ranges of `grid` and `zone_rows`).
#define BATTLE_MAX_GRID_WIDTH  64
#define BATTLE_MAX_GRID_LENGTH 128
#define BATTLE_MAX_ZONE_ROWS   32
#define BATTLE_MAX_PLACES      (BATTLE_MAX_ZONE_ROWS * BATTLE_MAX_GRID_WIDTH) // one squad per cell of a zone, at the most
#define BATTLE_MAX_UNIT_DEFS   16
#define BATTLE_MAX_ROLES       32 // the roles a file may declare: one bit each in a unit's mask
#define BATTLE_NAME_SIZE       32 // a name is at most 31 bytes
#define BATTLE_MAX_SQUAD_SIZE  64 // units in a squad
#define BATTLE_MAX_FOOTPRINT   8  // cells on a side of the rectangle a squad stands on
#define BATTLE_MAX_TEAM_UNITS  2048
#define BATTLE_MAX_UNITS       (1 + 2 * BATTLE_MAX_TEAM_UNITS) // slot 0 is "none"
#define BATTLE_MAX_PROJECTILES 4096
#define BATTLE_MAX_EVENTS      4096
#define BATTLE_MAX_HITS        16 // hits_or_shots_per_attack

// Until the projectile rules follow a unit's projectile keys (docs/specs/unit_fields.md, "Interim"), every projectile attack fires
// a ballistic shell at this angle, from the middle of the unit's body.
#define BATTLE_INTERIM_LAUNCH_ANGLE (NV_PI / 4.0f)

// The newest format of the project file this build reads (docs/specs/abproj.md, "Versions").
#define ABPROJ_VERSION 1

// Definitions: what data/default.abproj says. Zero is empty.

// The rules of the `rules` block, times already in ticks.
typedef struct BattleRules {
    f32 cell_size;               // meters
    u32 grid_width, grid_length; // cells across, rows long
    u32 zone_rows;               // each side's deployment rows: the player's are 0 to zone_rows - 1, the enemy's the last zone_rows
    u32 round_ticks;             // the time limit
    u32 retarget_ticks;          // how often units pick targets again
    f32 gravity;                 // m/s²
    f32 stop_fraction;           // a unit stops once its target is within this share of its weapon's range
    f32 min_damage_fraction;     // armor never takes more than 1 minus this off a hit
} BattleRules;

// A unit's layer: ground units walk on the ground, air units fly at their `altitude_m`. A unit's targets are the layers it can hit:
// one TARGET_* bit per layer (TARGET_GROUND for UNIT_LAYER_GROUND, TARGET_AIR for UNIT_LAYER_AIR).
typedef enum UnitLayer { UNIT_LAYER_GROUND, UNIT_LAYER_AIR } UnitLayer;
enum { TARGET_GROUND = 1 << UNIT_LAYER_GROUND, TARGET_AIR = 1 << UNIT_LAYER_AIR };
// The file's `target_layer`: Ground, Air or Both.
typedef enum TargetLayer { TARGET_LAYER_GROUND, TARGET_LAYER_AIR, TARGET_LAYER_BOTH } TargetLayer;

typedef enum AttackType { ATTACK_MELEE, ATTACK_UNGUIDED, ATTACK_GUIDED } AttackType;
typedef enum ProjectileTrajectory { TRAJECTORY_STRAIGHT, TRAJECTORY_SCRIPTED_ARC } ProjectileTrajectory;
typedef enum GuidanceMode { GUIDANCE_NONE, GUIDANCE_PREDICTIVE_INTERCEPTION } GuidanceMode;
typedef enum TargetLossBehavior { TARGET_LOSS_NONE, TARGET_LOSS_CONTINUE_STRAIGHT } TargetLossBehavior;

// A projectile attack's keys (the wiki's Projectile Spec). The round does not follow them yet but aim_point_scatter: its shells
// are the interim ones (BATTLE_INTERIM_LAUNCH_ANGLE).
typedef struct ProjectileDef {
    f32 initial_speed, forward_acceleration; // m/s, m/s²
    f32 max_speed;                           // m/s; 0 when it has none
    f32 lifetime, max_travel;                // seconds, meters
    f32 collision_radius, gravity_scale;     // meters, a share of the rules' gravity
    f32 aim_point_scatter;                   // meters: the shell lands within this radius of its aim point
    f32 launch_spread;                       // radians
    u32 piercing_count;
    b32 interceptable, retargeting_allowed, target_leading;
    ProjectileTrajectory trajectory;
    f32 arc_height; // meters, a scripted arc's
    // Guided only.
    f32 turn_rate;      // radians per second
    f32 guidance_delay; // seconds
    GuidanceMode guidance_mode;
    TargetLossBehavior target_loss;
} ProjectileDef;

// A unit def is a row of the wiki's unit_stats.csv (docs/specs/unit_fields.md). It is placed as a squad (docs/specs/battle.md,
// "Squads"): `squad_size` units on `footprint_w` x `footprint_d` cells, for `cost` in all. Once the round starts each unit moves
// and fights on its own. The file's integers hp, armor, shield_hp and damage are kept as floats, as the round adds them up.
typedef struct UnitDef {
    char id[BATTLE_NAME_SIZE];   // the file's unit_id: what `place` and the editing functions name it by
    char name[BATTLE_NAME_SIZE]; // what the panel shows
    u32 cost;                    // of the whole squad
    u32 squad_size;              // at least 1
    u32 footprint_w, footprint_d; // cells along +X and +Z, at least 1
    UnitLayer layer;
    TargetLayer target_layer;
    u32 targets;                 // target_layer as TARGET_* bits: a shell passes the units of the other layers
    UnitLayer target_priority;   // with target_layer Both, the layer picked first (not followed yet)
    f32 radius, height;          // meters
    f32 altitude;                // an air unit's height in the air, meters; its body is above it
    f32 hp, armor;
    f32 shield_hp;               // 0 for none
    f32 move_speed;              // m/s
    f32 range, damage;           // meters, damage before armor
    f32 attack_interval;         // seconds
    u32 hits_per_attack;         // hits_or_shots_per_attack
    f32 splash_radius;           // meters; 0 is one target (not followed yet)
    AttackType attack_type;
    ProjectileDef projectile;    // all zero for a melee attack
} UnitDef;

typedef struct StagePlace {
    u8 def;              // index into BattleDefs.units
    u8 cell_x, cell_row; // the footprint's corner nearest the grid's origin
} StagePlace;

typedef struct BattleDefs {
    BattleRules rules;
    UnitDef units[BATTLE_MAX_UNIT_DEFS];
    u32 unit_count;
    u32 supply; // each side's
    u32 seed;   // for the random numbers of the whole round
    StagePlace enemy[BATTLE_MAX_PLACES];
    u32 enemy_count;
    // The roles of the `roles` block, for sorting units. The rules never read them, so they cannot change a round. unit_roles[i]
    // is a bit mask over `roles`: bit r is set when unit def i has role r.
    char roles[BATTLE_MAX_ROLES][BATTLE_NAME_SIZE];
    u32 role_count;
    u32 unit_roles[BATTLE_MAX_UNIT_DEFS];
    // The keys the file gives that the round does not follow yet (docs/specs/unit_fields.md, "Interim"), in the order they first
    // came, as "attack_type, initial_speed_mps"; empty when there are none. The loader warns of them once.
    char unfollowed[512];
    u32 error_count;        // errors found by the reads, all of them (only the first 20 are logged)
    char first_error[160];  // "default.abproj:12: unknown key 'healt'", for the panel
} BattleDefs;

// Reads the text of a project file (`text` need not end in a NUL). Returns false when anything in it is wrong: each error
// is logged with its file name and line (nv_log, source "battle"), counted in `defs->error_count`, and the first is kept
// in `defs->first_error`. Reading goes on to the end of the text to report every error. `defs` starts zeroed.
b32 defs_read_project(BattleDefs* defs, const char* file_name, const char* text, umm size);

// Editing the rules in a project's text (docs/specs/abproj.md, "Editing the rules"): the text must have been read without
// errors. A rule's values are those of its key in the `rules` block, as the file writes them (seconds, not ticks); `grid`
// has two, every other rule one.
#define BATTLE_RULE_MAX_VALUES 2
// Reads rule `key`'s values; returns how many, 0 when `key` is not a rule.
u32 defs_rule_get(const char* text, umm size, const char* key, f64 values[BATTLE_RULE_MAX_VALUES]);
// Writes the text with rule `key`'s values changed to `values` (`count` of them) into `out`, which may not overlap `text`; the
// rest of the line (its indentation, its comment, which keeps its column where it can) and every other line stay as they
// were. Returns the new text's size, 0 when `key` is not a rule, `count` is not its number of values or the text does not fit
// in `capacity` bytes. The values are not checked against the rules' ranges: read the result with defs_read_project.
umm defs_rule_set(const char* text, umm size, const char* key, const f64* values, u32 count, char* out, umm capacity);

// Editing a unit def in a project's text (docs/specs/unit_lab.md, "Editing the unit's values"): the text must have been read
// without errors. `unit` is its unit_id. Only the keys whose value is a number can be edited; a key's value is as the file writes
// it (degrees, seconds).
// Reads key `key`'s value in unit `unit`; returns 1, or 0 when the file does not give the key (an optional key left out) or it is
// not a number key of a unit.
u32 defs_unit_get(const char* text, umm size, const char* unit, const char* key, f64* value);
// Writes the text with the key's value changed to `value` into `out` (which may not overlap `text`), as defs_rule_set does. A
// key the unit does not give yet gets a line of its own after the unit's last key, at its keys' indentation. Returns the new
// text's size, 0 when the unit or the key is not there or the text does not fit in `capacity` bytes. The value is not checked:
// read the result with defs_read_project.
umm defs_unit_set(const char* text, umm size, const char* unit, const char* key, f64 value, char* out, umm capacity);

// Seconds as whole ticks, rounded up (with room for float error, so 1.5 s is 45 ticks and not 46); at least 1.
u32 battle_seconds_to_ticks(f32 seconds);

// The first row of the enemy's zone.
u32 battle_enemy_first_row(const BattleRules* rules);

// The grid a squad of `squad_size` units stands in on a footprint of `w` x `d` cells (the wiki's D-07): as near square as the
// footprint's shape lets it, columns = ceil(sqrt(squad_size * w / d)) and rows = ceil(squad_size / columns). Its units are
// min(w / columns, d / rows) * cell_size meters apart.
typedef struct SquadGrid {
    u32 columns, rows;
} SquadGrid;
SquadGrid battle_squad_grid(u32 squad_size, u32 w, u32 d);

// The simulation.

typedef enum BattleTeam { TEAM_PLAYER, TEAM_ENEMY } BattleTeam;

typedef struct UnitId { u32 index; } UnitId; // a slot in Battle.units; 0 = none

// UNIT_DUMMY: a target of the unit lab (docs/specs/unit_lab.md). It picks no target and never fires, and it moves by the
// velocity set on it before the tick (so leading aim reads it). UNIT_HOLD: a unit that does not move; it still turns to its
// target and fires at it within range.
enum { UNIT_DEAD = 1 << 0, UNIT_DUMMY = 1 << 1, UNIT_HOLD = 1 << 2 };

typedef struct Unit {
    u32 flags;
    u8 team;
    u8 def; // index into BattleDefs.units
    NvVec3 position, previous_position; // previous: the position before this tick, for drawing between ticks
    NvVec3 velocity;                    // what the unit moved at this tick, m/s; leading aim reads it
    f32 yaw, previous_yaw;              // radians; the unit faces (sin yaw, 0, cos yaw)
    f32 health;
    f32 damage_taken; // this tick; applied at the tick's end
    UnitId target;
    u32 cooldown_ticks; // until the weapon can fire
    f32 shield;         // shield HP left: it takes a whole hit before the health does (D-05) and does not refill (D-10)
} Unit;

typedef struct Projectile { // a shell
    u8 team;
    UnitId shooter;
    NvVec3 position, previous_position, velocity;
    f32 damage;  // before armor
    u32 targets; // the layers the shooter can hit: the shell passes the units of the others
} Projectile;

typedef enum BattleEventKind {
    BATTLE_EVENT_FIRE,       // position: where the shell starts, direction: where it goes
    BATTLE_EVENT_STRIKE,     // a melee attack: position: the target's middle, direction: from the attacker to it
    BATTLE_EVENT_HIT,        // a shell or a strike hurt a unit: position: where, size: the damage after armor
    BATTLE_EVENT_SHIELD_HIT, // a unit's shield took a hit: position: where, size: the shield HP it took
    BATTLE_EVENT_DEATH,      // position: where the unit died
} BattleEventKind;

typedef struct BattleEvent { // for the view only
    BattleEventKind kind;
    u8 team, def; // of the unit the event is about: the shooter, the one hit, the one that died
    NvVec3 position, direction;
    f32 size;
} BattleEvent;

typedef enum BattlePhase { BATTLE_DEPLOY, BATTLE_FIGHT, BATTLE_RESULT } BattlePhase;
typedef enum BattleOutcome { OUTCOME_NONE, OUTCOME_PLAYER, OUTCOME_ENEMY, OUTCOME_DRAW } BattleOutcome;

typedef struct Battle {
    const BattleDefs* defs;
    BattlePhase phase;
    b32 endless; // the round never ends: no side wins and there is no time limit (the unit lab)
    BattleOutcome outcome;
    u32 tick;
    NvRandom rng; // seeded at Start from the stage's seed
    // The player's deployment, by the corner of each squad's footprint nearest the grid's origin: unit def + 1, 0 = none (the
    // footprint's other cells are 0 too).
    u8 placed[BATTLE_MAX_ZONE_ROWS][BATTLE_MAX_GRID_WIDTH];
    u32 supply_used;
    u32 player_units; // the units the deployment makes
    Unit units[BATTLE_MAX_UNITS]; // [0] is unused; slots 1 to unit_count are the units, the player's first
    u32 unit_count;
    Projectile projectiles[BATTLE_MAX_PROJECTILES];
    u32 projectile_count;
    // Events pile up over ticks until the view has read them and set event_count back to 0; when the list is full,
    // events (and only events) are dropped and counted.
    BattleEvent events[BATTLE_MAX_EVENTS];
    u32 event_count, events_dropped;
    NvVec3 push[BATTLE_MAX_UNITS]; // scratch: this tick's separation, by unit
} Battle;

// Zeroes `battle` and starts an empty deployment. `defs` must stay valid and unchanged while the battle uses it.
void battle_init(Battle* battle, const BattleDefs* defs);

// The center of a cell on the ground. The player's side is -Z, the enemy's +Z.
NvVec3 battle_cell_center(const BattleRules* rules, u32 cell_x, u32 cell_row);

// Deployment (only in the BATTLE_DEPLOY phase; the others return false). A squad of a unit def is placed with its footprint's
// corner nearest the grid's origin on (cell_x, cell_row); the whole footprint must be in the player's zone and free, within the
// supply and BATTLE_MAX_TEAM_UNITS. Each change rebuilds `units`.
u32 battle_supply_left(const Battle* battle);
b32 battle_can_place(const Battle* battle, u32 def, s32 cell_x, s32 cell_row);
b32 battle_place(Battle* battle, u32 def, s32 cell_x, s32 cell_row);
b32 battle_remove(Battle* battle, s32 cell_x, s32 cell_row); // the squad on that cell (any of its footprint's); false when none
void battle_clear_placement(Battle* battle);                 // Reset

// The player's squad whose footprint covers a cell: its corner (as battle_place takes it). Fails (zeroed) when none does.
typedef struct BattleCell {
    b32 ok;
    s32 x, row;
} BattleCell;
BattleCell battle_squad_at(const Battle* battle, s32 cell_x, s32 cell_row);

// Puts one unit of a unit def at `position` (its height included), outside any squad: the unit lab's units. The player's faces
// +Z, the enemy's -Z. Only in deployment; the next placement or Retry rebuilds the units without it.
UnitId battle_add_unit(Battle* battle, u32 def, u32 team, NvVec3 position, u32 flags);

// Start needs at least one player unit. Retry goes back to deployment with the same placement, from any phase.
b32 battle_start(Battle* battle);
void battle_retry(Battle* battle);

// One 30 Hz tick (does nothing outside BATTLE_FIGHT).
void battle_tick(Battle* battle);

// Puts a started round (fighting or over) back at `tick`, by playing it again from its start with the same placement and seed:
// the rules are deterministic, so that is the state it had then. Events of the replayed ticks are dropped. The cost grows with
// `tick` (each step back replays the round so far).
void battle_seek(Battle* battle, u32 tick);

u32 battle_alive_count(const Battle* battle, u32 team);
// The sum over a team's living units of cost / squad size * health / hp.
f32 battle_remaining_value(const Battle* battle, u32 team);
// FNV-1a over the state a round's outcome depends on, so two runs can be compared.
u32 battle_hash(const Battle* battle);
