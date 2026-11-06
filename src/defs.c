// Reads the project file (docs/specs/abproj.md): the line-based text of data/default.abproj, with its rules, units and
// stage. It takes text in memory, so tests can try strings, and it uses no GPU or ImGui. A bad line never stops the read:
// every error is reported (nv_log, with the file name and line) and the read fails at the end.

#include "battle.h"

#include <engine/log.h>
#include <engine/text.h>

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NO_LIMIT          1e30

typedef struct Reader {
    NvTextReader text;
    BattleDefs* defs;
} Reader;

// A key and its values, stored at `offset` in the struct being read.
typedef enum FieldKind {
    FIELD_INTEGER, // u32
    FIELD_WHOLE,   // f32 that the file writes as an integer
    FIELD_NUMBER,  // f32
    FIELD_ANGLE,   // f32, degrees in the file and radians when read
    FIELD_VECTOR,  // three f32 (no key takes one now; kept for keys to come)
    FIELD_TICKS,   // u32, seconds in the file and whole ticks when read
    FIELD_GRID,    // two integers, the width and the length, into a BattleRules (their own ranges)
    FIELD_TEXT,   // char[BATTLE_NAME_SIZE]: the rest of the line, spaces and all, 1 to 31 bytes
    FIELD_WORD,    // u32: one of `words` (case counts), stored as first_value + its index
    FIELD_BOOL,    // b32: true or false
} FieldKind;

// When a unit's key applies (docs/specs/unit_fields.md, "Keys"). What the others depend on is judged when the unit's block ends.
typedef enum FieldWhen {
    WHEN_ALWAYS,
    WHEN_AIR,        // altitude_m: required for unit_layer Air, an error for Ground
    WHEN_BOTH,       // target_priority: only with target_layer Both
    WHEN_PROJECTILE, // an error on a Melee unit; `required` holds for the others
    WHEN_GUIDED,     // a projectile key that `required` holds for only with GuidedProjectile (on other projectiles it is read and kept)
    WHEN_ARC,        // arc_height_m: a projectile key required for ScriptedArc, an error for Straight
} FieldWhen;

typedef struct Field {
    const char* key;
    FieldKind kind;
    u32 offset;
    f64 min, max; // the range of the value (in the file's units); NO_LIMIT for none
    b32 min_open, max_open; // the bound itself is not allowed
    b32 required;
    const char* const* words; // FIELD_WORD's
    u32 word_count, first_value;
    FieldWhen when;
    b32 unfollowed; // the round does not follow it yet (unit_fields.md, "Interim")
} Field;

global const char* const layer_words[] = {"Ground", "Air"};
global const char* const target_layer_words[] = {"Ground", "Air", "Both"};
global const char* const attack_words[] = {"Melee", "UnguidedProjectile", "GuidedProjectile"};
global const char* const trajectory_words[] = {"Straight", "ScriptedArc"};
global const char* const guidance_words[] = {"PredictiveInterception"};
global const char* const target_loss_words[] = {"ContinueStraight"};
global const char* const bool_words[] = {"false", "true"};

#define WORDS(list) .words = list, .word_count = NV_ARRAY_COUNT(list)
#define AT(member) .offset = offsetof(UnitDef, member)
#define PROJECTILE(member) .offset = offsetof(UnitDef, projectile.member)

// The columns of the wiki's unit_stats.csv, in its order (unit_id is the block's name; roles are read on their own).
global const Field unit_fields[] = {
    {.key = "name", .kind = FIELD_TEXT, AT(name), .required = true},
    {.key = "cost", .kind = FIELD_INTEGER, AT(cost), .min = 1, .max = 10000, .required = true},
    {.key = "squad_size", .kind = FIELD_INTEGER, AT(squad_size), .min = 1, .max = BATTLE_MAX_SQUAD_SIZE, .required = true},
    {.key = "unit_layer", .kind = FIELD_WORD, AT(layer), WORDS(layer_words), .required = true},
    {.key = "target_layer", .kind = FIELD_WORD, AT(target_layer), WORDS(target_layer_words), .required = true},
    {.key = "target_priority", .kind = FIELD_WORD, AT(target_priority), WORDS(layer_words), .when = WHEN_BOTH, .unfollowed = true},
    {.key = "footprint_w", .kind = FIELD_INTEGER, AT(footprint_w), .min = 1, .max = BATTLE_MAX_FOOTPRINT, .required = true},
    {.key = "footprint_d", .kind = FIELD_INTEGER, AT(footprint_d), .min = 1, .max = BATTLE_MAX_FOOTPRINT, .required = true},
    {.key = "radius_m", .kind = FIELD_NUMBER, AT(radius), .max = NO_LIMIT, .min_open = true, .required = true},
    {.key = "height_m", .kind = FIELD_NUMBER, AT(height), .max = NO_LIMIT, .min_open = true, .required = true},
    {.key = "altitude_m", .kind = FIELD_NUMBER, AT(altitude), .max = NO_LIMIT, .min_open = true, .when = WHEN_AIR},
    {.key = "hp", .kind = FIELD_WHOLE, AT(hp), .min = 1, .max = 1e9, .required = true},
    {.key = "armor", .kind = FIELD_WHOLE, AT(armor), .max = 1e9, .required = true},
    {.key = "shield_hp", .kind = FIELD_WHOLE, AT(shield_hp), .min = 1, .max = 1e9},
    {.key = "move_speed", .kind = FIELD_NUMBER, AT(move_speed), .max = NO_LIMIT, .required = true},
    {.key = "range", .kind = FIELD_NUMBER, AT(range), .max = NO_LIMIT, .min_open = true, .required = true},
    {.key = "damage", .kind = FIELD_WHOLE, AT(damage), .max = 1e9, .required = true},
    {.key = "attack_interval", .kind = FIELD_NUMBER, AT(attack_interval), .max = NO_LIMIT, .min_open = true, .required = true},
    {.key = "hits_or_shots_per_attack", .kind = FIELD_INTEGER, AT(hits_per_attack), .min = 1, .max = BATTLE_MAX_HITS, .required = true},
    {.key = "splash_radius", .kind = FIELD_NUMBER, AT(splash_radius), .max = NO_LIMIT, .required = true},
    {.key = "attack_type", .kind = FIELD_WORD, AT(attack_type), WORDS(attack_words), .required = true},
    {.key = "initial_speed_mps", .kind = FIELD_NUMBER, PROJECTILE(initial_speed), .max = NO_LIMIT, .min_open = true, .required = true,
     .when = WHEN_PROJECTILE, .unfollowed = true},
    {.key = "forward_acceleration_mps2", .kind = FIELD_NUMBER, PROJECTILE(forward_acceleration), .max = NO_LIMIT, .required = true,
     .when = WHEN_PROJECTILE, .unfollowed = true},
    {.key = "max_speed_mps", .kind = FIELD_NUMBER, PROJECTILE(max_speed), .max = NO_LIMIT, .min_open = true, .when = WHEN_PROJECTILE,
     .unfollowed = true},
    {.key = "lifetime_s", .kind = FIELD_NUMBER, PROJECTILE(lifetime), .max = NO_LIMIT, .min_open = true, .required = true,
     .when = WHEN_PROJECTILE, .unfollowed = true},
    {.key = "max_travel_m", .kind = FIELD_NUMBER, PROJECTILE(max_travel), .max = NO_LIMIT, .min_open = true, .required = true,
     .when = WHEN_PROJECTILE, .unfollowed = true},
    {.key = "collision_radius_m", .kind = FIELD_NUMBER, PROJECTILE(collision_radius), .max = NO_LIMIT, .min_open = true,
     .required = true, .when = WHEN_PROJECTILE, .unfollowed = true},
    {.key = "gravity_scale", .kind = FIELD_NUMBER, PROJECTILE(gravity_scale), .max = NO_LIMIT, .when = WHEN_PROJECTILE, .unfollowed = true},
    {.key = "turn_rate_deg_s", .kind = FIELD_ANGLE, PROJECTILE(turn_rate), .max = NO_LIMIT, .min_open = true, .required = true,
     .when = WHEN_GUIDED, .unfollowed = true},
    {.key = "guidance_delay_s", .kind = FIELD_NUMBER, PROJECTILE(guidance_delay), .max = NO_LIMIT, .required = true,
     .when = WHEN_GUIDED, .unfollowed = true},
    {.key = "guidance_mode", .kind = FIELD_WORD, PROJECTILE(guidance_mode), WORDS(guidance_words), .first_value = GUIDANCE_PREDICTIVE_INTERCEPTION,
     .required = true, .when = WHEN_GUIDED, .unfollowed = true},
    {.key = "aim_point_scatter_m", .kind = FIELD_NUMBER, PROJECTILE(aim_point_scatter), .max = NO_LIMIT, .when = WHEN_PROJECTILE},
    {.key = "launch_spread_deg", .kind = FIELD_ANGLE, PROJECTILE(launch_spread), .max = 90, .max_open = true, .when = WHEN_PROJECTILE,
     .unfollowed = true},
    {.key = "piercing_count", .kind = FIELD_INTEGER, PROJECTILE(piercing_count), .max = 1e6, .when = WHEN_PROJECTILE, .unfollowed = true},
    {.key = "interceptable", .kind = FIELD_BOOL, PROJECTILE(interceptable), .when = WHEN_PROJECTILE, .unfollowed = true},
    {.key = "trajectory_mode", .kind = FIELD_WORD, PROJECTILE(trajectory), WORDS(trajectory_words), .required = true,
     .when = WHEN_PROJECTILE, .unfollowed = true},
    {.key = "arc_height_m", .kind = FIELD_NUMBER, PROJECTILE(arc_height), .max = NO_LIMIT, .min_open = true, .when = WHEN_ARC,
     .unfollowed = true},
    {.key = "retargeting_allowed", .kind = FIELD_BOOL, PROJECTILE(retargeting_allowed), .when = WHEN_PROJECTILE, .unfollowed = true},
    {.key = "target_loss_behavior", .kind = FIELD_WORD, PROJECTILE(target_loss), WORDS(target_loss_words),
     .first_value = TARGET_LOSS_CONTINUE_STRAIGHT, .when = WHEN_GUIDED, .unfollowed = true},
    {.key = "target_leading", .kind = FIELD_BOOL, PROJECTILE(target_leading), .required = true, .when = WHEN_PROJECTILE, .unfollowed = true},
};

#define RULE(member) .offset = offsetof(BattleRules, member)

global const Field rules_fields[] = {
    {.key = "cell_size", .kind = FIELD_NUMBER, RULE(cell_size), .max = 10, .min_open = true, .required = true},
    {.key = "grid", .kind = FIELD_GRID, .required = true},
    {.key = "zone_rows", .kind = FIELD_INTEGER, RULE(zone_rows), .min = 1, .max = BATTLE_MAX_ZONE_ROWS, .required = true},
    {.key = "round_time", .kind = FIELD_TICKS, RULE(round_ticks), .max = 600, .min_open = true, .required = true},
    {.key = "gravity", .kind = FIELD_NUMBER, RULE(gravity), .max = 100, .min_open = true, .required = true},
    {.key = "retarget_interval", .kind = FIELD_TICKS, RULE(retarget_ticks), .max = 10, .min_open = true, .required = true},
    {.key = "stop_fraction", .kind = FIELD_NUMBER, RULE(stop_fraction), .max = 1, .min_open = true, .required = true},
    {.key = "min_damage_fraction", .kind = FIELD_NUMBER, RULE(min_damage_fraction), .max = 1, .required = true},
};

global const Field stage_fields[] = {
    {.key = "supply", .kind = FIELD_INTEGER, .offset = offsetof(BattleDefs, supply), .min = 1, .max = 100000, .required = true},
    {.key = "seed", .kind = FIELD_INTEGER, .offset = offsetof(BattleDefs, seed), .max = 4294967295.0},
};

#define FIELD_COUNT(fields) ((u32)NV_ARRAY_COUNT(fields))

internal u32 field_values(const Field* field)
{
    return field->kind == FIELD_VECTOR ? 3 : field->kind == FIELD_GRID ? 2 : 1;
}

internal const Field* find_field(const Field* fields, u32 count, NvTextToken key)
{
    for (u32 i = 0; i < count; ++i)
        if (nv_text_token_is(key, fields[i].key))
            return &fields[i];
    return NULL;
}

internal b32 in_range(const Field* field, f64 value)
{
    b32 above = field->min_open ? value > field->min : value >= field->min;
    b32 below = field->max_open ? value < field->max : value <= field->max;
    return above && below;
}

internal void report_range(Reader* reader, u32 line, const Field* field)
{
    char text[64] = "";
    u32 used = 0;
    if (field->min > -NO_LIMIT)
        used += (u32)snprintf(text + used, sizeof(text) - used, "%s %.10g", field->min_open ? ">" : ">=", field->min);
    if (field->max < NO_LIMIT)
        used += (u32)snprintf(text + used, sizeof(text) - used, "%s%s %.10g", used ? " and " : "", field->max_open ? "<" : "<=", field->max);
    nv_text_report(&reader->text, line, "'%s' must be %s", field->key, text);
}

// The text of a line after its key: from its first value to the end of its last, spaces between them kept.
internal NvTextToken line_rest(const NvTextLine* line)
{
    const NvTextToken* last = &line->tokens[line->token_count - 1];
    const char* begin = line->tokens[1].text;
    return (NvTextToken){.text = begin, .length = (u32)(last->text + last->length - begin)};
}

// The words a FIELD_WORD takes, for a message: "Ground, Air or Both".
internal void words_hint(const Field* field, char* out, umm capacity)
{
    umm used = 0;
    out[0] = 0;
    for (u32 i = 0; i < field->word_count && used < capacity; ++i) {
        const char* gap = i == 0 ? "" : i + 1 == field->word_count ? " or " : ", ";
        used += (umm)snprintf(out + used, capacity - used, "%s%s", gap, field->words[i]);
    }
}

// Reads a line `key value...` into the struct at `base`. `seen` holds a bit per field already given in this block. Returns
// whether the value was good (and stored).
internal b32 read_field(Reader* reader, const NvTextLine* line, const Field* field, u32 index, void* base, u64* seen)
{
    if (*seen & ((u64)1 << index)) {
        nv_text_report(&reader->text, line->number, "'%s' is given twice in this block", field->key);
        return false;
    }
    *seen |= (u64)1 << index; // a bad value is not "missing" on top of that
    u8* destination = (u8*)base + field->offset;
    if (field->kind == FIELD_TEXT) {
        if (line->token_count < 2) {
            nv_text_report(&reader->text, line->number, "'%s' takes a text", field->key);
            return false;
        }
        NvTextToken text = line_rest(line);
        if (text.length >= BATTLE_NAME_SIZE) {
            nv_text_report(&reader->text, line->number, "'%s' is longer than %d bytes", field->key, BATTLE_NAME_SIZE - 1);
            return false;
        }
        memcpy(destination, text.text, text.length);
        destination[text.length] = 0;
        return true;
    }
    if (field->kind == FIELD_WORD || field->kind == FIELD_BOOL) {
        const char* const* words = field->kind == FIELD_BOOL ? bool_words : field->words;
        u32 count = field->kind == FIELD_BOOL ? (u32)NV_ARRAY_COUNT(bool_words) : field->word_count;
        char hint[128];
        if (field->kind == FIELD_BOOL)
            snprintf(hint, sizeof(hint), "true or false");
        else
            words_hint(field, hint, sizeof(hint));
        if (line->token_count != 2) {
            nv_text_report(&reader->text, line->number, "'%s' takes %s", field->key, hint);
            return false;
        }
        for (u32 i = 0; i < count; ++i) {
            if (nv_text_token_is(line->tokens[1], words[i])) {
                u32 value = field->first_value + i;
                memcpy(destination, &value, sizeof(value));
                return true;
            }
        }
        nv_text_report(&reader->text, line->number, "unknown %s '%.*s' (%s)", field->key, nv_text_shown(line->tokens[1]), line->tokens[1].text, hint);
        return false;
    }
    u32 values = field_values(field);
    if (line->token_count != 1 + values) {
        nv_text_report(&reader->text, line->number, "'%s' takes %u value%s", field->key, values, values > 1 ? "s" : "");
        return false;
    }
    b32 integer = field->kind == FIELD_INTEGER || field->kind == FIELD_WHOLE || field->kind == FIELD_GRID;
    f64 numbers[3];
    for (u32 i = 0; i < values; ++i) {
        NvTextNumber number = nv_text_parse_number(&reader->text, line->number, line->tokens[1 + i], integer);
        if (!number.ok)
            return false;
        numbers[i] = number.value;
    }
    if (field->kind == FIELD_GRID) {
        if (numbers[0] < 1 || numbers[0] > BATTLE_MAX_GRID_WIDTH) {
            nv_text_report(&reader->text, line->number, "'grid' width must be 1 to %d", BATTLE_MAX_GRID_WIDTH);
            return false;
        }
        if (numbers[1] < 2 || numbers[1] > BATTLE_MAX_GRID_LENGTH) {
            nv_text_report(&reader->text, line->number, "'grid' length must be 2 to %d", BATTLE_MAX_GRID_LENGTH);
            return false;
        }
    } else {
        for (u32 i = 0; i < values; ++i) {
            if (!in_range(field, numbers[i])) {
                report_range(reader, line->number, field);
                return false;
            }
        }
    }
    switch (field->kind) {
    case FIELD_INTEGER: {
        u32 value = (u32)numbers[0];
        memcpy(destination, &value, sizeof(value));
    } break;
    case FIELD_WHOLE:
    case FIELD_NUMBER: {
        f32 value = (f32)numbers[0];
        memcpy(destination, &value, sizeof(value));
    } break;
    case FIELD_ANGLE: {
        f32 value = (f32)(numbers[0] * (NV_PI / 180.0));
        memcpy(destination, &value, sizeof(value));
    } break;
    case FIELD_VECTOR: {
        f32 value[3] = {(f32)numbers[0], (f32)numbers[1], (f32)numbers[2]};
        memcpy(destination, value, sizeof(value));
    } break;
    case FIELD_TICKS: {
        u32 value = battle_seconds_to_ticks((f32)numbers[0]);
        memcpy(destination, &value, sizeof(value));
    } break;
    case FIELD_GRID: {
        BattleRules* rules = base;
        rules->grid_width = (u32)numbers[0];
        rules->grid_length = (u32)numbers[1];
    } break;
    case FIELD_TEXT:
    case FIELD_WORD:
    case FIELD_BOOL:
        break; // read above
    }
    return true;
}

// What a block is still missing when it ends (its fields that are always required).
internal void check_required(Reader* reader, const Field* fields, u32 count, u64 seen, u32 header_line, const char* what, const char* name)
{
    for (u32 i = 0; i < count; ++i)
        if (fields[i].required && fields[i].when == WHEN_ALWAYS && !(seen & ((u64)1 << i)))
            nv_text_report(&reader->text, header_line, "%s '%s' is missing '%s'", what, name, fields[i].key);
}

// The project file

typedef enum BlockKind { BLOCK_ROOT, BLOCK_RULES, BLOCK_ROLES, BLOCK_UNIT, BLOCK_STAGE, BLOCK_SKIP } BlockKind;

// An open block. Its lines are indented deeper than its header, all by the same amount.
typedef struct Block {
    BlockKind kind;
    s32 header_indent;
    s32 child_indent; // -1 until the first line inside it
    u32 header_line;
    u64 seen;
} Block;

// The last top-level statement that was in its place; they come in this order (docs/specs/abproj.md, "Syntax").
typedef enum Top { TOP_NONE, TOP_VERSION, TOP_RULES, TOP_ROLES, TOP_UNIT, TOP_STAGE } Top;

#define UNIT_FIELD_COUNT FIELD_COUNT(unit_fields)
#define MAX_UNFOLLOWED   UNIT_FIELD_COUNT

typedef struct ProjectReader {
    Reader base;
    Block stack[2]; // root, then rules, roles, a unit or the stage
    u32 depth;
    Top last;
    b32 first_statement; // the next top-level statement is the file's first
    b32 stop;            // nothing more can be read (a newer format)

    UnitDef unit;   // the unit being read
    b32 unit_named; // its name is a good one, so what it lacks can be reported by name
    b32 unit_kept;  // there was room for it
    u32 unit_errors_start;
    u32 unit_roles; // the roles of the unit being read, as a mask over BattleDefs.roles
    b32 roles_seen; // it has a `roles` line
    u64 good;       // its fields read without an error (a bit per unit_fields entry)
    u32 field_lines[64]; // the line of each of its fields that it gives

    u32 rules_errors_start, zone_rows_line;
    b32 rules_ok;   // the rules block was read without an error, so the grid is known
    u32 stage_errors_start, places_seen; // `place` lines, also those that could not be judged against a wrong grid
    u32 enemy_units;                     // the units the enemy's places make
    u8 taken[BATTLE_MAX_ZONE_ROWS][BATTLE_MAX_GRID_WIDTH]; // enemy cells under a squad, by row in the zone
    u32 place_lines[BATTLE_MAX_PLACES];                   // the line of each enemy place

    const char* unfollowed[MAX_UNFOLLOWED]; // the keys the round does not follow yet, in the order they first came
    u32 unfollowed_count;
} ProjectReader;

// The index of a unit key in unit_fields.
internal u32 unit_field(const char* key)
{
    for (u32 i = 0; i < UNIT_FIELD_COUNT; ++i)
        if (strcmp(unit_fields[i].key, key) == 0)
            return i;
    NV_ASSERT(!"not a unit key");
    return 0;
}

internal b32 unit_good(const ProjectReader* reader, const char* key)
{
    return (reader->good & ((u64)1 << unit_field(key))) != 0;
}

internal void note_unfollowed(ProjectReader* reader, const char* key)
{
    for (u32 i = 0; i < reader->unfollowed_count; ++i)
        if (reader->unfollowed[i] == key)
            return;
    if (reader->unfollowed_count < MAX_UNFOLLOWED)
        reader->unfollowed[reader->unfollowed_count++] = key;
}

// What a unit's keys need of each other (docs/specs/unit_fields.md, "Checks when reading"), judged only against the keys they
// depend on that were read without an error, so that one mistake is one error.
internal void check_unit(ProjectReader* reader, const Block* block)
{
    Reader* base = &reader->base;
    const UnitDef* unit = &reader->unit;
    const char* name = unit->id;
    b32 attack_known = unit_good(reader, "attack_type");
    b32 layer_known = unit_good(reader, "unit_layer");
    b32 target_layer_known = unit_good(reader, "target_layer");
    b32 trajectory_known = unit_good(reader, "trajectory_mode");
    for (u32 i = 0; i < UNIT_FIELD_COUNT; ++i) {
        const Field* field = &unit_fields[i];
        b32 given = (block->seen & ((u64)1 << i)) != 0;
        u32 line = reader->field_lines[i];
        switch (field->when) {
        case WHEN_ALWAYS:
            break; // check_required
        case WHEN_AIR:
            if (layer_known && unit->layer == UNIT_LAYER_AIR && !given)
                nv_text_report(&base->text, block->header_line, "unit '%s' flies (unit_layer Air) and needs '%s'", name, field->key);
            if (layer_known && unit->layer == UNIT_LAYER_GROUND && given)
                nv_text_report(&base->text, line, "'%s' is only for air units", field->key);
            break;
        case WHEN_BOTH:
            if (target_layer_known && unit->target_layer != TARGET_LAYER_BOTH && given)
                nv_text_report(&base->text, line, "'%s' is only for target_layer Both", field->key);
            break;
        case WHEN_PROJECTILE:
        case WHEN_GUIDED:
        case WHEN_ARC:
            if (!attack_known)
                break;
            if (unit->attack_type == ATTACK_MELEE) {
                if (given)
                    nv_text_report(&base->text, line, "'%s' is for projectile attacks; unit '%s' is Melee", field->key, name);
                break;
            }
            if (field->when == WHEN_ARC) {
                if (trajectory_known && unit->projectile.trajectory == TRAJECTORY_SCRIPTED_ARC && !given)
                    nv_text_report(&base->text, block->header_line, "unit '%s' is missing '%s'", name, field->key);
                if (trajectory_known && unit->projectile.trajectory == TRAJECTORY_STRAIGHT && given)
                    nv_text_report(&base->text, line, "'%s' is only for ScriptedArc", field->key);
                break;
            }
            b32 applies = field->when == WHEN_PROJECTILE || unit->attack_type == ATTACK_GUIDED;
            if (field->required && applies && !given)
                nv_text_report(&base->text, block->header_line, "unit '%s' is missing '%s'", name, field->key);
            break;
        }
    }
}

// The keys of a unit that the round does not follow yet: the projectile keys it gives (but aim_point_scatter_m), and the attack's
// values the interim round does otherwise (unit_fields.md, "Interim").
internal void note_unit_unfollowed(ProjectReader* reader, u64 seen)
{
    const UnitDef* unit = &reader->unit;
    for (u32 i = 0; i < UNIT_FIELD_COUNT; ++i) {
        const Field* field = &unit_fields[i];
        b32 given = (seen & ((u64)1 << i)) != 0;
        b32 unfollowed = given && field->unfollowed;
        if (strcmp(field->key, "attack_type") == 0)
            unfollowed = given && unit->attack_type != ATTACK_MELEE;
        else if (strcmp(field->key, "hits_or_shots_per_attack") == 0)
            unfollowed = given && unit->attack_type != ATTACK_MELEE && unit->hits_per_attack > 1;
        else if (strcmp(field->key, "splash_radius") == 0)
            unfollowed = given && unit->splash_radius > 0.0f;
        if (unfollowed)
            note_unfollowed(reader, field->key);
    }
}

internal void close_block(ProjectReader* reader)
{
    Reader* base = &reader->base;
    BattleDefs* defs = base->defs;
    Block* block = &reader->stack[--reader->depth];
    switch (block->kind) {
    case BLOCK_RULES: {
        check_required(base, rules_fields, FIELD_COUNT(rules_fields), block->seen, block->header_line, "block", "rules");
        const BattleRules* rules = &defs->rules;
        if (rules->grid_length && rules->zone_rows && rules->zone_rows * 2 > rules->grid_length)
            nv_text_report(&base->text, reader->zone_rows_line, "'zone_rows' must be at most %u (half the grid's length)", rules->grid_length / 2);
        reader->rules_ok = base->text.errors == reader->rules_errors_start;
    } break;
    case BLOCK_UNIT: {
        UnitDef* unit = &reader->unit;
        if (reader->unit_named) {
            check_required(base, unit_fields, UNIT_FIELD_COUNT, block->seen, block->header_line, "unit", unit->id);
            check_unit(reader, block);
        }
        unit->targets = unit->target_layer == TARGET_LAYER_BOTH ? (TARGET_GROUND | TARGET_AIR)
                        : unit->target_layer == TARGET_LAYER_AIR ? TARGET_AIR : TARGET_GROUND;
        // A squad must fit in a zone, and its units must not stand on each other, either way its footprint may later be turned
        // (judged only against rules that were read right, and for a unit read without an error, so one mistake is one error).
        const BattleRules* rules = &defs->rules;
        if (reader->rules_ok && unit_good(reader, "footprint_w") && unit->footprint_w > rules->grid_width)
            nv_text_report(&base->text, reader->field_lines[unit_field("footprint_w")], "'footprint_w' must be at most %u (the grid's width)",
                           rules->grid_width);
        if (reader->rules_ok && unit_good(reader, "footprint_d") && unit->footprint_d > rules->zone_rows)
            nv_text_report(&base->text, reader->field_lines[unit_field("footprint_d")], "'footprint_d' must be at most %u (a zone's rows)",
                           rules->zone_rows);
        if (reader->rules_ok && reader->unit_named && base->text.errors == reader->unit_errors_start) {
            for (u32 turned = 0; turned < 2; ++turned) {
                u32 w = turned ? unit->footprint_d : unit->footprint_w, d = turned ? unit->footprint_w : unit->footprint_d;
                SquadGrid grid = battle_squad_grid(unit->squad_size, w, d);
                f32 across = (f32)w * rules->cell_size / (f32)grid.columns, along = (f32)d * rules->cell_size / (f32)grid.rows;
                f32 apart = fminf(across, along);
                if (apart < 2.0f * unit->radius) {
                    nv_text_report(&base->text, reader->field_lines[unit_field("squad_size")], "%u unit%s on %ux%u cells stand%s %.3g m apart%s, under their width (%.3g m)",
                                   unit->squad_size, unit->squad_size > 1 ? "s" : "", unit->footprint_w, unit->footprint_d,
                                   unit->squad_size > 1 ? "" : "s", apart, turned ? " when turned" : "", 2.0f * unit->radius);
                    break;
                }
            }
        }
        note_unit_unfollowed(reader, block->seen);
        if (reader->unit_kept) {
            defs->unit_roles[defs->unit_count] = reader->unit_roles;
            defs->units[defs->unit_count++] = *unit;
        }
    } break;
    case BLOCK_STAGE:
        check_required(base, stage_fields, FIELD_COUNT(stage_fields), block->seen, block->header_line, "block", "stage");
        if (reader->places_seen == 0 && base->text.errors == reader->stage_errors_start)
            nv_text_report(&base->text, block->header_line, "'stage' has no 'place' lines");
        if (defs->supply) {
            // The first place that takes the total over the supply is the one reported.
            u32 total = 0;
            for (u32 i = 0; i < defs->enemy_count; ++i) {
                total += defs->units[defs->enemy[i].def].cost;
                if (total > defs->supply) {
                    nv_text_report(&base->text, reader->place_lines[i], "the total cost is over the supply (%u)", defs->supply);
                    break;
                }
            }
        }
        break;
    case BLOCK_ROOT:
    case BLOCK_ROLES:
    case BLOCK_SKIP:
        break;
    }
}

internal void open_block(ProjectReader* reader, BlockKind kind, const NvTextLine* line)
{
    NV_ASSERT(reader->depth < NV_ARRAY_COUNT(reader->stack));
    reader->stack[reader->depth++] = (Block){.kind = kind, .header_indent = (s32)line->indent, .child_indent = -1, .header_line = line->number};
}

// Top-level statements

internal void read_version(ProjectReader* reader, const NvTextLine* line)
{
    Reader* base = &reader->base;
    if (reader->last != TOP_NONE) {
        nv_text_report(&base->text, line->number, "'abproj_version' must be the first statement");
        return;
    }
    reader->last = TOP_VERSION;
    if (line->token_count != 2) {
        nv_text_report(&base->text, line->number, "'abproj_version' takes one integer");
        return;
    }
    NvTextNumber number = nv_text_parse_number(&base->text, line->number, line->tokens[1], true);
    if (!number.ok)
        return;
    if (number.value < 1) {
        nv_text_report(&base->text, line->number, "'abproj_version' must be 1 to %d", ABPROJ_VERSION);
    } else if (number.value > ABPROJ_VERSION) {
        // A newer format may mean anything by what follows, so the rest is not read.
        nv_text_report(&base->text, line->number, "format %.0f is newer than this build reads (%d)", number.value, ABPROJ_VERSION);
        reader->stop = true;
    }
}

internal void read_unit_header(ProjectReader* reader, const NvTextLine* line)
{
    Reader* base = &reader->base;
    BattleDefs* defs = base->defs;
    reader->unit = (UnitDef){0};
    reader->unit_named = false;
    reader->unit_kept = false;
    reader->unit_roles = 0;
    reader->roles_seen = false;
    reader->good = 0;
    memset(reader->field_lines, 0, sizeof(reader->field_lines));
    reader->unit_errors_start = base->text.errors;
    open_block(reader, BLOCK_UNIT, line);
    if (line->token_count != 2) {
        nv_text_report(&base->text, line->number, "'unit' takes a unit_id");
        return;
    }
    if (!nv_text_parse_name(&base->text, line->number, line->tokens[1], reader->unit.id, BATTLE_NAME_SIZE))
        return;
    reader->unit_named = true;
    for (u32 i = 0; i < defs->unit_count; ++i) {
        if (strcmp(defs->units[i].id, reader->unit.id) == 0) {
            nv_text_report(&base->text, line->number, "unit '%s' is defined twice", reader->unit.id);
            return;
        }
    }
    if (defs->unit_count == BATTLE_MAX_UNIT_DEFS) {
        nv_text_report(&base->text, line->number, "too many units (at most %d)", BATTLE_MAX_UNIT_DEFS);
        return;
    }
    reader->unit_kept = true;
}

// A statement at the left edge. Each is allowed in one place only; one out of place is reported and its block skipped, so
// that a swap of two blocks is one error and not the errors of everything that depends on them.
internal void read_top_line(ProjectReader* reader, const NvTextLine* line)
{
    Reader* base = &reader->base;
    NvTextToken key = line->tokens[0];
    if (reader->first_statement) {
        reader->first_statement = false;
        if (!nv_text_token_is(key, "abproj_version")) {
            nv_text_report(&base->text, line->number, "the file must start with 'abproj_version <version>'");
            reader->last = TOP_VERSION; // read on as if it had, so a missing line is one error
        }
    }

    if (nv_text_token_is(key, "abproj_version")) {
        read_version(reader, line);
    } else if (nv_text_token_is(key, "rules")) {
        if (reader->last == TOP_VERSION) {
            if (line->token_count != 1)
                nv_text_report(&base->text, line->number, "'rules' takes no value");
            reader->last = TOP_RULES;
            reader->rules_errors_start = base->text.errors;
            open_block(reader, BLOCK_RULES, line);
        } else {
            nv_text_report(&base->text, line->number, "'rules' is given twice");
            open_block(reader, BLOCK_SKIP, line);
        }
    } else if (nv_text_token_is(key, "roles")) {
        if (reader->last == TOP_RULES) {
            if (line->token_count != 1)
                nv_text_report(&base->text, line->number, "'roles' takes no value");
            reader->last = TOP_ROLES;
            open_block(reader, BLOCK_ROLES, line);
        } else {
            nv_text_report(&base->text, line->number, reader->last == TOP_ROLES ? "'roles' is given twice" : "'roles' must come after 'rules' and before the units");
            open_block(reader, BLOCK_SKIP, line);
        }
    } else if (nv_text_token_is(key, "unit")) {
        if (reader->last == TOP_RULES || reader->last == TOP_ROLES || reader->last == TOP_UNIT) {
            reader->last = TOP_UNIT;
            read_unit_header(reader, line);
        } else {
            nv_text_report(&base->text, line->number, reader->last == TOP_STAGE ? "'unit' must come before 'stage'" : "'unit' must come after 'rules'");
            open_block(reader, BLOCK_SKIP, line);
        }
    } else if (nv_text_token_is(key, "stage")) {
        if (reader->last == TOP_UNIT) {
            if (line->token_count != 1)
                nv_text_report(&base->text, line->number, "'stage' takes no value");
            reader->last = TOP_STAGE;
            reader->stage_errors_start = base->text.errors;
            base->defs->seed = 1;
            open_block(reader, BLOCK_STAGE, line);
        } else {
            nv_text_report(&base->text, line->number, reader->last == TOP_STAGE   ? "'stage' is given twice"
                                       : reader->last == TOP_RULES ? "'stage' must come after the units"
                                                                   : "'stage' must come after 'rules' and the units");
            open_block(reader, BLOCK_SKIP, line);
        }
    } else {
        nv_text_report(&base->text, line->number, "unknown key '%.*s'", nv_text_shown(key), key.text);
        open_block(reader, BLOCK_SKIP, line);
    }
}

// Lines inside blocks

// A role's name: text of 1 to 31 bytes with no ',' (a unit's `roles` line lists them with commas). Copies it into `role`.
internal b32 read_role_name(Reader* reader, u32 line, NvTextToken text, char role[BATTLE_NAME_SIZE])
{
    for (u32 i = 0; i < text.length; ++i) {
        if (text.text[i] == ',') {
            nv_text_report(&reader->text, line, "a role's name cannot hold ','");
            return false;
        }
    }
    if (text.length >= BATTLE_NAME_SIZE) {
        nv_text_report(&reader->text, line, "a role's name is longer than %d bytes", BATTLE_NAME_SIZE - 1);
        return false;
    }
    memcpy(role, text.text, text.length);
    role[text.length] = 0;
    return true;
}

// A line of the `roles` block: one role's name (spaces and all), declared once.
internal void read_role(ProjectReader* reader, const NvTextLine* line)
{
    Reader* base = &reader->base;
    BattleDefs* defs = base->defs;
    const NvTextToken* last = &line->tokens[line->token_count - 1];
    NvTextToken text = {.text = line->tokens[0].text, .length = (u32)(last->text + last->length - line->tokens[0].text)};
    char role[BATTLE_NAME_SIZE];
    if (!read_role_name(base, line->number, text, role))
        return;
    for (u32 i = 0; i < defs->role_count; ++i) {
        if (strcmp(defs->roles[i], role) == 0) {
            nv_text_report(&base->text, line->number, "role '%s' is declared twice", role);
            return;
        }
    }
    if (defs->role_count == BATTLE_MAX_ROLES) {
        nv_text_report(&base->text, line->number, "too many roles (at most %d)", BATTLE_MAX_ROLES);
        return;
    }
    memcpy(defs->roles[defs->role_count++], role, BATTLE_NAME_SIZE);
}

// A unit's `roles` line: declared roles, with commas between them (`roles Chaff/Screen, Flanker`), each once.
internal void read_unit_roles(ProjectReader* reader, const NvTextLine* line)
{
    Reader* base = &reader->base;
    const BattleDefs* defs = base->defs;
    if (reader->roles_seen) {
        nv_text_report(&base->text, line->number, "'roles' is given twice in this block");
        return;
    }
    reader->roles_seen = true;
    if (line->token_count < 2) {
        nv_text_report(&base->text, line->number, "'roles' takes at least one role");
        return;
    }
    NvTextToken rest = line_rest(line);
    u32 mask = 0;
    const char* end = rest.text + rest.length;
    for (const char* item = rest.text; item <= end;) {
        const char* comma = item;
        while (comma < end && *comma != ',')
            ++comma;
        const char* begin = item;
        const char* stop = comma;
        while (begin < stop && *begin == ' ')
            ++begin;
        while (stop > begin && stop[-1] == ' ')
            --stop;
        if (begin == stop) {
            nv_text_report(&base->text, line->number, "'roles' has an empty role (between two commas, or at an end)");
            return;
        }
        NvTextToken role = {.text = begin, .length = (u32)(stop - begin)};
        u32 i = 0;
        while (i < defs->role_count && !nv_text_token_is(role, defs->roles[i]))
            ++i;
        if (i == defs->role_count) {
            nv_text_report(&base->text, line->number, "unknown role '%.*s' (declare it under 'roles')", nv_text_shown(role), role.text);
            return;
        }
        if (mask & (1u << i)) {
            nv_text_report(&base->text, line->number, "role '%.*s' is given twice", nv_text_shown(role), role.text);
            return;
        }
        mask |= 1u << i;
        item = comma + 1;
    }
    reader->unit_roles = mask;
}

internal void read_unit_line(ProjectReader* reader, const NvTextLine* line, Block* block)
{
    Reader* base = &reader->base;
    NvTextToken key = line->tokens[0];
    if (nv_text_token_is(key, "roles")) {
        read_unit_roles(reader, line);
        return;
    }
    const Field* field = find_field(unit_fields, UNIT_FIELD_COUNT, key);
    if (!field) {
        nv_text_report(&base->text, line->number, "unknown key '%.*s'", nv_text_shown(key), key.text);
        return;
    }
    u32 index = (u32)(field - unit_fields);
    b32 repeated = (block->seen & ((u64)1 << index)) != 0;
    if (!repeated)
        reader->field_lines[index] = line->number;
    if (read_field(base, line, field, index, &reader->unit, &block->seen))
        reader->good |= (u64)1 << index;
}

typedef struct UnitIndex {
    b32 ok;
    u32 index; // into BattleDefs.units
} UnitIndex;

// Fails when no unit has that unit_id.
internal UnitIndex find_unit(const BattleDefs* defs, NvTextToken id)
{
    for (u32 i = 0; i < defs->unit_count; ++i)
        if (nv_text_token_is(id, defs->units[i].id))
            return (UnitIndex){.ok = true, .index = i};
    return (UnitIndex){0};
}

internal void read_place(ProjectReader* reader, const NvTextLine* line)
{
    Reader* base = &reader->base;
    BattleDefs* defs = base->defs;
    const BattleRules* rules = &defs->rules;
    if (line->token_count != 4) {
        nv_text_report(&base->text, line->number, "'place' takes a unit_id, a cell x and a cell row");
        return;
    }
    ++reader->places_seen;
    b32 ok = true;
    UnitIndex unit = find_unit(defs, line->tokens[1]);
    if (!unit.ok) {
        nv_text_report(&base->text, line->number, "unknown unit '%.*s'", nv_text_shown(line->tokens[1]), line->tokens[1].text);
        ok = false;
    }
    NvTextNumber x_number = nv_text_parse_number(&base->text, line->number, line->tokens[2], true);
    if (!x_number.ok)
        return;
    NvTextNumber row_number = nv_text_parse_number(&base->text, line->number, line->tokens[3], true);
    if (!row_number.ok)
        return;
    if (!reader->rules_ok)
        return; // the grid is not known, so a cell cannot be judged (and no cell is reported against a wrong grid)
    // The cell is the corner of the squad's footprint nearest the grid's origin; the whole footprint is in the enemy zone (a
    // footprint larger than the zone was reported with its unit). An unknown unit's cell is judged as a single cell.
    u32 w = unit.ok ? defs->units[unit.index].footprint_w : 1, d = unit.ok ? defs->units[unit.index].footprint_d : 1;
    if (w > rules->grid_width || d > rules->zone_rows)
        return;
    f64 x = x_number.value, row = row_number.value;
    u32 first_row = battle_enemy_first_row(rules);
    u32 last_x = rules->grid_width - w, last_row = rules->grid_length - d;
    if (x < 0 || x > last_x) {
        nv_text_report(&base->text, line->number, "cell x must be 0 to %u", last_x);
        ok = false;
    }
    if (row < first_row || row > last_row) {
        nv_text_report(&base->text, line->number, "cell row must be %u to %u (the enemy zone)", first_row, last_row);
        ok = false;
    }
    if (!ok)
        return;
    const UnitDef* def = &defs->units[unit.index];
    u32 cell_x = (u32)x, cell_row = (u32)row - first_row;
    for (u32 r = cell_row; r < cell_row + d; ++r) {
        for (u32 c = cell_x; c < cell_x + w; ++c) {
            if (reader->taken[r][c]) {
                nv_text_report(&base->text, line->number, "cell (%u, %u) already has a unit", c, r + first_row);
                return;
            }
        }
    }
    if (reader->enemy_units + def->squad_size > BATTLE_MAX_TEAM_UNITS) {
        nv_text_report(&base->text, line->number, "the enemy has more than %d units", BATTLE_MAX_TEAM_UNITS);
        return;
    }
    for (u32 r = cell_row; r < cell_row + d; ++r)
        for (u32 c = cell_x; c < cell_x + w; ++c)
            reader->taken[r][c] = 1;
    reader->enemy_units += def->squad_size;
    reader->place_lines[defs->enemy_count] = line->number;
    defs->enemy[defs->enemy_count++] = (StagePlace){(u8)unit.index, (u8)cell_x, (u8)row};
}

internal void read_project_line(ProjectReader* reader, const NvTextLine* line)
{
    Reader* base = &reader->base;
    while (reader->depth > 1 && (s32)line->indent <= reader->stack[reader->depth - 1].header_indent)
        close_block(reader);
    Block* block = &reader->stack[reader->depth - 1];
    if (block->kind == BLOCK_SKIP)
        return; // the inside of a block that was already reported
    if (block->child_indent < 0)
        block->child_indent = (s32)line->indent;
    if ((s32)line->indent != block->child_indent) {
        nv_text_report(&base->text, line->number, block->kind == BLOCK_ROOT ? "unexpected indentation" : "inconsistent indentation (a block's lines are indented alike)");
        return;
    }

    NvTextToken key = line->tokens[0];
    switch (block->kind) {
    case BLOCK_ROOT:
        read_top_line(reader, line);
        break;
    case BLOCK_ROLES:
        read_role(reader, line);
        break;
    case BLOCK_UNIT:
        read_unit_line(reader, line, block);
        break;
    case BLOCK_RULES: {
        const Field* field = find_field(rules_fields, FIELD_COUNT(rules_fields), key);
        if (!field) {
            nv_text_report(&base->text, line->number, "unknown key '%.*s'", nv_text_shown(key), key.text);
            break;
        }
        if (nv_text_token_is(key, "zone_rows"))
            reader->zone_rows_line = line->number;
        read_field(base, line, field, (u32)(field - rules_fields), &base->defs->rules, &block->seen);
    } break;
    case BLOCK_STAGE: {
        if (nv_text_token_is(key, "place")) {
            read_place(reader, line);
            break;
        }
        const Field* field = find_field(stage_fields, FIELD_COUNT(stage_fields), key);
        if (!field)
            nv_text_report(&base->text, line->number, "unknown key '%.*s'", nv_text_shown(key), key.text);
        else
            read_field(base, line, field, (u32)(field - stage_fields), base->defs, &block->seen);
    } break;
    case BLOCK_SKIP:
        break;
    }
}

b32 defs_read_project(BattleDefs* defs, const char* file_name, const char* text, umm size)
{
    ProjectReader reader = {
        .base = {.text = {.file_name = file_name,
                          .log_source = "battle",
                          .total = &defs->error_count,
                          .first_error = defs->first_error,
                          .first_error_capacity = sizeof(defs->first_error)},
                 .defs = defs},
        .first_statement = true,
    };
    reader.stack[0] = (Block){.kind = BLOCK_ROOT, .header_indent = -1, .child_indent = 0};
    reader.depth = 1;
    defs->seed = 1;

    NvTextLines lines = {text, size, 0, 0};
    for (NvTextLineRange text_line = nv_text_next_line(&lines); text_line.ok && !reader.stop; text_line = nv_text_next_line(&lines)) {
        NvTextTokenized tokenized = nv_text_tokenize(&reader.base.text, text_line.begin, text_line.end, lines.number);
        if (tokenized.ok && tokenized.line.token_count)
            read_project_line(&reader, &tokenized.line);
    }
    while (reader.depth > 1)
        close_block(&reader);
    // What the whole file lacks, once the rest is right: the first of the missing parts, since they come in order.
    if (reader.base.text.errors == 0) {
        if (reader.last < TOP_VERSION)
            nv_text_report(&reader.base.text, 0, "the file must start with 'abproj_version <version>'");
        else if (reader.last < TOP_RULES)
            nv_text_report(&reader.base.text, 0, "missing 'rules'");
        else if (reader.last < TOP_UNIT)
            nv_text_report(&reader.base.text, 0, "no units");
        else if (reader.last < TOP_STAGE)
            nv_text_report(&reader.base.text, 0, "missing 'stage'");
    }
    umm used = 0;
    defs->unfollowed[0] = 0;
    for (u32 i = 0; i < reader.unfollowed_count && used < sizeof(defs->unfollowed); ++i)
        used += (umm)snprintf(defs->unfollowed + used, sizeof(defs->unfollowed) - used, "%s%s", i ? ", " : "", reader.unfollowed[i]);
    return reader.base.text.errors == 0;
}

// Editing the rules (docs/specs/abproj.md, "Editing the rules"). These work on the text of a project that was read without
// errors, so every rule has its one line in the `rules` block.

// Rule `key`'s line, its tokens pointing into the text. Fails (zeroed) when `key` is not a rule or its line is not there.
typedef struct RuleLine {
    b32 ok;
    const Field* field;
    NvTextLine line;
    const char* end; // where the line's text ends (its '\n', or the text's end)
} RuleLine;

internal RuleLine find_rule(const char* text, umm size, const char* key)
{
    const Field* field = NULL;
    for (u32 i = 0; i < FIELD_COUNT(rules_fields); ++i)
        if (strcmp(rules_fields[i].key, key) == 0)
            field = &rules_fields[i];
    if (!field)
        return (RuleLine){0};
    NvTextLines lines = {text, size, 0, 0};
    b32 in_rules = false;
    for (NvTextLineRange text_line = nv_text_next_line(&lines); text_line.ok; text_line = nv_text_next_line(&lines)) {
        NvTextTokenized tokenized = nv_text_tokenize(NULL, text_line.begin, text_line.end, lines.number);
        const NvTextLine* line = &tokenized.line;
        if (!tokenized.ok || !line->token_count)
            continue;
        if (line->indent == 0) // a top-level statement: the rules block starts or ends
            in_rules = nv_text_token_is(line->tokens[0], "rules");
        else if (in_rules && nv_text_token_is(line->tokens[0], key) && line->token_count == 1 + field_values(field))
            return (RuleLine){.ok = true, .field = field, .line = *line, .end = text_line.end};
    }
    return (RuleLine){0};
}

u32 defs_rule_get(const char* text, umm size, const char* key, f64 values[BATTLE_RULE_MAX_VALUES])
{
    RuleLine rule = find_rule(text, size, key);
    if (!rule.ok)
        return 0;
    u32 count = field_values(rule.field);
    NV_ASSERT(count <= BATTLE_RULE_MAX_VALUES);
    for (u32 i = 0; i < count; ++i) {
        NvTextNumber number = nv_text_parse_number(NULL, 0, rule.line.tokens[1 + i], false);
        if (!number.ok)
            return 0;
        values[i] = number.value;
    }
    return count;
}

// A value as the file writes it: an integer, or a number with at most four decimals and no trailing zeros ("9.8", "60").
// Never an exponent, which the format has not. Returns the length, 0 when it does not fit.
internal u32 format_value(char* out, umm capacity, f64 value, b32 integer)
{
    value = integer ? round(value) : round(value * 1e4) / 1e4;
    value += 0.0; // -0 becomes 0
    int length = snprintf(out, capacity, integer ? "%.0f" : "%.4f", value);
    if (length <= 0 || (umm)length >= capacity)
        return 0;
    if (!integer) {
        while (out[length - 1] == '0')
            --length;
        if (out[length - 1] == '.')
            --length;
        out[length] = 0;
    }
    return (u32)length;
}

// Writes the text with the values of `line` (a key line of `field`, ending at `end`) changed to `values`: only the values'
// text changes, and the spaces after it shrink or grow so that a comment keeps its column (one space at the least).
internal umm replace_values(const char* text, umm size, const Field* field, const NvTextLine* line, const char* end,
                            const f64* values, u32 count, char* out, umm capacity)
{
    b32 integer = field->kind == FIELD_INTEGER || field->kind == FIELD_WHOLE || field->kind == FIELD_GRID;
    char written[96];
    u32 used = 0;
    for (u32 i = 0; i < count; ++i) {
        if (i)
            written[used++] = ' ';
        u32 length = format_value(written + used, sizeof(written) - used, values[i], integer);
        if (!length)
            return 0;
        used += length;
    }

    // The values' old text, from the first value to the end of the last, is replaced.
    const char* first = line->tokens[1].text;
    const NvTextToken* last = &line->tokens[count];
    const char* after = last->text + last->length;
    const char* rest = after;
    while (rest < end && *rest == ' ')
        ++rest;
    s64 gap = rest - after;
    if (rest < end && *rest == '#') {
        gap += (s64)(after - first) - (s64)used;
        if (gap < 1)
            gap = 1;
    }
    umm head = (umm)(first - text), tail = (umm)(text + size - rest);
    umm total = head + used + (umm)gap + tail;
    if (total > capacity)
        return 0;
    memmove(out, text, head);
    memcpy(out + head, written, used);
    memset(out + head + used, ' ', (umm)gap);
    memmove(out + head + used + (umm)gap, rest, tail);
    return total;
}

umm defs_rule_set(const char* text, umm size, const char* key, const f64* values, u32 count, char* out, umm capacity)
{
    RuleLine rule = find_rule(text, size, key);
    if (!rule.ok || count != field_values(rule.field))
        return 0;
    return replace_values(text, size, rule.field, &rule.line, rule.end, values, count, out, capacity);
}

// Editing a unit def (docs/specs/unit_lab.md, "Editing the unit's values"), on the text of a project read without errors.

// A number key of a unit: its line when the file gives it, and where a line for it goes when it does not. Fails (zeroed) when the
// unit or the key is not there.
typedef struct UnitKeyLine {
    b32 ok;
    const Field* field;
    b32 given; // the key's line is in the file: `line` and `end`
    NvTextLine line;
    const char* end;
    const char* insert_at; // otherwise: a new line goes here (the start of the line after the unit's last key)
    b32 insert_newline_first; // the unit's last line ends the text with no line break
    u32 indent;               // the unit's keys' indentation
    b32 crlf;                 // the text's lines end in "\r\n"
} UnitKeyLine;

internal b32 is_number_field(const Field* field)
{
    return field->kind == FIELD_INTEGER || field->kind == FIELD_WHOLE || field->kind == FIELD_NUMBER || field->kind == FIELD_ANGLE;
}

internal UnitKeyLine find_unit_key(const char* text, umm size, const char* unit, const char* key)
{
    const Field* field = NULL;
    for (u32 i = 0; i < UNIT_FIELD_COUNT; ++i)
        if (strcmp(unit_fields[i].key, key) == 0 && is_number_field(&unit_fields[i]))
            field = &unit_fields[i];
    if (!field)
        return (UnitKeyLine){0};

    UnitKeyLine found = {.field = field};
    NvTextLines lines = {text, size, 0, 0};
    b32 in_unit = false;
    for (NvTextLineRange text_line = nv_text_next_line(&lines); text_line.ok; text_line = nv_text_next_line(&lines)) {
        NvTextTokenized tokenized = nv_text_tokenize(NULL, text_line.begin, text_line.end, lines.number);
        const NvTextLine* line = &tokenized.line;
        if (!tokenized.ok || !line->token_count)
            continue;
        if (line->indent == 0) {
            if (in_unit)
                break; // the unit's block has ended
            in_unit = line->token_count == 2 && nv_text_token_is(line->tokens[0], "unit") && nv_text_token_is(line->tokens[1], unit);
            if (in_unit) {
                found.ok = true;
                found.insert_at = text_line.end; // after the header, until a key comes
            }
            continue;
        }
        if (!in_unit)
            continue;
        found.insert_at = text_line.end;
        found.indent = line->indent;
        if (nv_text_token_is(line->tokens[0], key) && line->token_count == 2) {
            found.given = true;
            found.line = *line;
            found.end = text_line.end;
        }
    }
    if (!found.ok)
        return (UnitKeyLine){0};
    // insert_at is the end of the unit's last line: the new line goes after its line break.
    const char* stop = text + size;
    found.crlf = found.insert_at > text && found.insert_at[-1] == '\r';
    if (found.insert_at < stop)
        ++found.insert_at; // past the '\n'
    else
        found.insert_newline_first = true;
    if (!found.indent) // a unit with no keys yet
        found.indent = 4;
    return found;
}

u32 defs_unit_get(const char* text, umm size, const char* unit, const char* key, f64* value)
{
    UnitKeyLine found = find_unit_key(text, size, unit, key);
    if (!found.ok || !found.given)
        return 0;
    NvTextNumber number = nv_text_parse_number(NULL, 0, found.line.tokens[1], false);
    if (!number.ok)
        return 0;
    *value = number.value;
    return 1;
}

umm defs_unit_set(const char* text, umm size, const char* unit, const char* key, f64 value, char* out, umm capacity)
{
    UnitKeyLine found = find_unit_key(text, size, unit, key);
    if (!found.ok)
        return 0;
    if (found.given)
        return replace_values(text, size, found.field, &found.line, found.end, &value, 1, out, capacity);

    // A new line, "<indent><key> <value>", after the unit's last key.
    b32 integer = found.field->kind == FIELD_INTEGER || found.field->kind == FIELD_WHOLE;
    char written[160];
    umm used = 0;
    const char* newline = found.crlf ? "\r\n" : "\n";
    if (found.insert_newline_first)
        used += (umm)snprintf(written + used, sizeof(written) - used, "%s", newline);
    if (found.indent + strlen(key) + 2 > sizeof(written) - used)
        return 0;
    memset(written + used, ' ', found.indent);
    used += found.indent;
    used += (umm)snprintf(written + used, sizeof(written) - used, "%s ", key);
    u32 length = format_value(written + used, sizeof(written) - used, value, integer);
    if (!length)
        return 0;
    used += length;
    if (!found.insert_newline_first) {
        if (used + 2 >= sizeof(written))
            return 0;
        used += (umm)snprintf(written + used, sizeof(written) - used, "%s", newline);
    }
    umm head = (umm)(found.insert_at - text), tail = size - head;
    if (head + used + tail > capacity)
        return 0;
    memmove(out, text, head);
    memmove(out + head + used, text + head, tail);
    memcpy(out + head, written, used);
    return head + used + tail;
}
