// The auto-battler's project file and rules (src/defs.c, battle.c; docs/specs/abproj.md, battle.md and unit_fields.md, "Tests").
// No GPU. The format is tried only with text made here, so that changing a value in the repository's project file breaks no
// test; that file (BATTLE_DATA_DIR/default.abproj) is only checked to read and to play a round. The rules run on small scenes
// made here, where shells are sometimes put into the battle by hand so that one rule at a time is tried.

#include "battle.h"
#include "engine/log.h"
#include "lab.h"
#include "menu.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

global int failures;
#define CHECK(condition)                                                          \
    do {                                                                          \
        if (!(condition)) {                                                       \
            printf("FAILED line %d: %s\n", __LINE__, #condition);                 \
            ++failures;                                                           \
        }                                                                         \
    } while (0)

internal b32 near_value(f32 a, f32 b, f32 tolerance)
{
    return fabsf(a - b) <= tolerance;
}

global BattleDefs defs;
global Battle battle;

// Definition files

internal umm read_file(const char* name, char* buffer, umm capacity)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", BATTLE_DATA_DIR, name);
    FILE* file = fopen(path, "rb");
    if (!file) {
        printf("cannot open %s\n", path);
        ++failures;
        return 0;
    }
    umm size = fread(buffer, 1, capacity, file);
    fclose(file);
    return size;
}

// The parts of a project file. A test that tries one part is wrapped in good parts around it, so the lines before the
// fragment are counted (GOOD_*_LINES) and an expected line is the fragment's own.
#define FILE_NAME "default.abproj"

global const char good_header[] =
    "abproj_version 1\n"
    "rules\n"
    "    cell_size 2\n"
    "    grid 32 48\n"
    "    zone_rows 14\n"
    "    round_time 60\n"
    "    gravity 9.8\n"
    "    retarget_interval 0.25\n"
    "    stop_fraction 0.9\n"
    "    min_damage_fraction 0.25\n";
#define GOOD_HEADER_LINES 10

// A unit with every key that is always required, and no other. Its lines, from 1: the keys are lines 2 to 19 in this order.
global const char good_units[] =
    "unit Crawler\n"                   // 1
    "    name Crawler\n"               // 2
    "    cost 100\n"                   // 3
    "    squad_size 1\n"               // 4
    "    unit_layer Ground\n"          // 5
    "    target_layer Ground\n"        // 6
    "    footprint_w 1\n"              // 7
    "    footprint_d 1\n"              // 8
    "    radius_m 0.5\n"               // 9
    "    height_m 0.8\n"               // 10
    "    hp 120\n"                     // 11
    "    armor 0\n"                    // 12
    "    move_speed 5\n"               // 13
    "    range 20\n"                   // 14
    "    damage 30\n"                  // 15
    "    attack_interval 1.5\n"        // 16
    "    hits_or_shots_per_attack 1\n" // 17
    "    splash_radius 0\n"            // 18
    "    attack_type Melee\n";         // 19
#define GOOD_UNITS_LINES 19

// The keys a projectile attack needs on top of good_units' (whose attack_type line it replaces): lines 19 to 26.
global const char projectile_keys[] =
    "    attack_type UnguidedProjectile\n" // 19
    "    initial_speed_mps 100\n"          // 20
    "    forward_acceleration_mps2 0\n"    // 21
    "    lifetime_s 1\n"                   // 22
    "    max_travel_m 100\n"               // 23
    "    collision_radius_m 0.1\n"         // 24
    "    trajectory_mode Straight\n"       // 25
    "    target_leading true\n";           // 26

// A stage's lines, not yet indented.
global const char good_stage[] =
    "supply 1000\n"
    "place Crawler 12 36\n";

global char project_text[65536];

// Appends `fragment` to project_text, each of its non-empty lines indented by `indent` spaces.
internal void append_indented(const char* fragment, u32 indent)
{
    u32 used = (u32)strlen(project_text);
    b32 line_start = true;
    for (const char* p = fragment; *p; ++p) {
        if (line_start && *p != '\n')
            for (u32 i = 0; i < indent; ++i)
                project_text[used++] = ' ';
        project_text[used++] = *p;
        line_start = *p == '\n';
    }
    project_text[used] = 0;
}

typedef enum Part { PART_RULES, PART_UNITS, PART_STAGE } Part;

// Makes project_text with `fragment` as the part named, the other parts good; returns the number of lines before the
// fragment's first. A units fragment is followed by no stage (a read of it fails, or the test adds `place_unit`'s: with a
// name, a stage that places that unit follows). A rules fragment replaces the `rules` block's lines.
internal u32 make_project(Part part, const char* fragment, const char* place_unit)
{
    project_text[0] = 0;
    u32 before = 0;
    if (part == PART_RULES) {
        append_indented("abproj_version 1\nrules\n", 0);
        before = 2;
        append_indented(fragment, 4);
        append_indented(good_units, 0);
        append_indented("stage\n", 0);
        append_indented(good_stage, 4);
        return before;
    }
    append_indented(good_header, 0);
    before = GOOD_HEADER_LINES;
    if (part == PART_UNITS) {
        append_indented(fragment, 0);
        if (place_unit) {
            append_indented("stage\n    supply 1000\n", 0);
            char place[96];
            snprintf(place, sizeof(place), "    place %s 12 36\n", place_unit);
            append_indented(place, 0);
        }
    } else {
        append_indented(good_units, 0);
        append_indented("stage\n", 0);
        before += GOOD_UNITS_LINES + 1;
        append_indented(fragment, 4);
    }
    return before;
}

// Reads `text` as a whole project file; returns what the read returned.
internal b32 read_project(const char* text)
{
    memset(&defs, 0, sizeof(defs));
    nv_log_clear();
    return defs_read_project(&defs, FILE_NAME, text, strlen(text));
}

// Reads a part wrapped in good ones.
internal b32 read_part(Part part, const char* fragment, const char* place_unit)
{
    make_project(part, fragment, place_unit);
    return read_project(project_text);
}

internal b32 read_units(const char* fragment, const char* place_unit)
{
    return read_part(PART_UNITS, fragment, place_unit);
}

internal b32 read_stage(const char* fragment)
{
    return read_part(PART_STAGE, fragment, NULL);
}

internal b32 first_error_has(const char* text)
{
    return strstr(defs.first_error, text) != NULL;
}

enum { WHOLE_FILE = 0, BLOCK_LINE = 0xFFFF }; // an expected line: none (an error about the whole file), or the part's header

// The read must fail with `count` errors, the first on `line` (of the fragment) with `message` in it. A fragment of a units
// part has no stage after it, so a good unit would not make it succeed: every fragment here fails.
internal void expect_error_in(Part part, const char* fragment, u32 count, u32 line, const char* message)
{
    u32 before = make_project(part, fragment, NULL);
    char prefix[48];
    if (line == WHOLE_FILE)
        snprintf(prefix, sizeof(prefix), "%s: ", FILE_NAME); // an error about the whole file
    else if (line == BLOCK_LINE)
        snprintf(prefix, sizeof(prefix), "%s:%u:", FILE_NAME, before); // the part's own header, the line before the fragment
    else
        snprintf(prefix, sizeof(prefix), "%s:%u:", FILE_NAME, before + line);
    b32 ok = read_project(project_text);
    if (ok || defs.error_count != count || strncmp(defs.first_error, prefix, strlen(prefix)) != 0 || !first_error_has(message)) {
        printf("FAILED expect_error: wanted %u error(s), first \"%s ... %s\"; got ok=%d, %u error(s), first \"%s\"\n", count, prefix,
               message, ok, defs.error_count, defs.first_error);
        ++failures;
    }
}

// The same for a whole file, with the line in the file.
internal void expect_error_file(const char* text, u32 count, u32 line, const char* message)
{
    char prefix[48];
    if (line)
        snprintf(prefix, sizeof(prefix), "%s:%u:", FILE_NAME, line);
    else
        snprintf(prefix, sizeof(prefix), "%s: ", FILE_NAME);
    b32 ok = read_project(text);
    if (ok || defs.error_count != count || strncmp(defs.first_error, prefix, strlen(prefix)) != 0 || !first_error_has(message)) {
        printf("FAILED expect_error_file: wanted %u error(s), first \"%s ... %s\"; got ok=%d, %u error(s), first \"%s\"\n", count,
               prefix, message, ok, defs.error_count, defs.first_error);
        ++failures;
    }
}

// Whether `text` is whole UTF-8 characters: no lead byte without its continuation bytes, and no stray continuation byte.
internal b32 valid_utf8(const char* text)
{
    for (const u8* p = (const u8*)text; *p;) {
        u32 length = *p < 0x80 ? 1 : (*p >> 5) == 0x6 ? 2 : (*p >> 4) == 0xE ? 3 : (*p >> 3) == 0x1E ? 4 : 0;
        if (!length)
            return false;
        for (u32 i = 1; i < length; ++i)
            if ((p[i] & 0xC0) != 0x80)
                return false;
        p += length;
    }
    return true;
}

internal b32 log_has(const char* text)
{
    for (u32 i = 0; i < nv_log_ring.count; ++i) {
        const NvLogMessage* message = nv_log_message(i);
        if (message->level == NV_LOG_ERROR && strncmp(nv_log_text(message), text, strlen(text)) == 0)
            return true;
    }
    return false;
}

// `text` with its first `from` replaced by `to`.
internal const char* replaced(const char* text, const char* from, const char* to)
{
    static char out[2][65536]; // two, so that the result of one may be passed to the next
    static u32 next;
    char* buffer = out[next++ % 2];
    const char* at = strstr(text, from);
    NV_ASSERT(at && strlen(text) - strlen(from) + strlen(to) < sizeof(out[0]));
    snprintf(buffer, sizeof(out[0]), "%.*s%s%s", (int)(at - text), text, to, at + strlen(from));
    return buffer;
}

// good_units with a key line replaced: `from` and `to` are whole lines without their indentation or line end ("cost 100").
internal const char* unit_with(const char* from, const char* to)
{
    char a[128], b[160] = ""; // an empty `to` takes the line away
    snprintf(a, sizeof(a), "    %s\n", from);
    if (to[0])
        snprintf(b, sizeof(b), "    %s\n", to);
    return replaced(good_units, a, b);
}

// good_units as a projectile attack (projectile_keys), with `extra` lines (indented) after its keys.
internal const char* shooter_with(const char* extra)
{
    static char text[4096];
    snprintf(text, sizeof(text), "%s%s", replaced(good_units, "    attack_type Melee\n", projectile_keys), extra);
    return text;
}

// The values of the repository's default.abproj as they were when the units became the wiki's (unit_fields.md); a copy here,
// so that a designer changing the file breaks nothing. The rounds below (and their golden hashes) play on it.
global const char full_project[] =
    "# a project like autobattler/data/default.abproj\n"
    "abproj_version 1\n"
    "\n"
    "rules\n"
    "    cell_size 2              # m\n"
    "    grid 32 68               # cells across, rows long (the wiki's D-17)\n"
    "    zone_rows 14             # each side's deployment rows\n"
    "    round_time 90            # s (the wiki's D-24)\n"
    "    gravity 9.8              # m/s²\n"
    "    retarget_interval 0.25   # s\n"
    "    stop_fraction 0.9        # of the range\n"
    "    min_damage_fraction 0.25 # armor never takes more than 75% of a hit\n"
    "\n"
    "roles\n"
    "    Chaff/Screen\n"
    "    Flanker\n"
    "    Anti-Air\n"
    "    Chaff Clear\n"
    "    Midline DPS\n"
    "    Tank/Anchor\n"
    "    Support(future)\n"
    "    Flanker/Diver\n"
    "    Carry\n"
    "    Control(future)\n"
    "    Anti-Tank\n"
    "    Siege\n"
    "\n"
    "unit U01\n"
    "    name Scout Swarm\n"
    "    cost 50\n"
    "    squad_size 24\n"
    "    roles Chaff/Screen, Flanker\n"
    "    unit_layer Ground\n"
    "    target_layer Ground\n"
    "    footprint_w 5\n"
    "    footprint_d 2\n"
    "    radius_m 0.5\n"
    "    height_m 0.8\n"
    "    hp 45\n"
    "    armor 0\n"
    "    move_speed 15\n"
    "    range 6\n"
    "    damage 7\n"
    "    attack_interval 0.7\n"
    "    hits_or_shots_per_attack 1\n"
    "    splash_radius 0\n"
    "    attack_type Melee\n"
    "\n"
    "unit U02\n"
    "    name Rifle Squad\n"
    "    cost 100\n"
    "    squad_size 12\n"
    "    roles Chaff/Screen, Anti-Air\n"
    "    unit_layer Ground\n"
    "    target_layer Both\n"
    "    target_priority Ground\n"
    "    footprint_w 4\n"
    "    footprint_d 2\n"
    "    radius_m 0.6\n"
    "    height_m 1.8\n"
    "    hp 80\n"
    "    armor 0\n"
    "    move_speed 7\n"
    "    range 60\n"
    "    damage 13\n"
    "    attack_interval 0.65\n"
    "    hits_or_shots_per_attack 1\n"
    "    splash_radius 0\n"
    "    attack_type UnguidedProjectile\n"
    "    initial_speed_mps 180\n"
    "    forward_acceleration_mps2 0\n"
    "    lifetime_s 1.0\n"
    "    max_travel_m 90\n"
    "    collision_radius_m 0.05\n"
    "    gravity_scale 0\n"
    "    aim_point_scatter_m 0\n"
    "    launch_spread_deg 0\n"
    "    piercing_count 0\n"
    "    interceptable false\n"
    "    trajectory_mode Straight\n"
    "    target_leading true\n"
    "\n"
    "unit U03\n"
    "    name Auto Cannon\n"
    "    cost 150\n"
    "    squad_size 2\n"
    "    roles Chaff Clear, Midline DPS\n"
    "    unit_layer Ground\n"
    "    target_layer Ground\n"
    "    footprint_w 4\n"
    "    footprint_d 2\n"
    "    radius_m 1.2\n"
    "    height_m 2.0\n"
    "    hp 450\n"
    "    armor 5\n"
    "    move_speed 7\n"
    "    range 50\n"
    "    damage 22\n"
    "    attack_interval 0.25\n"
    "    hits_or_shots_per_attack 2\n"
    "    splash_radius 2.5\n"
    "    attack_type UnguidedProjectile\n"
    "    initial_speed_mps 140\n"
    "    forward_acceleration_mps2 0\n"
    "    lifetime_s 1.0\n"
    "    max_travel_m 80\n"
    "    collision_radius_m 0.12\n"
    "    gravity_scale 0\n"
    "    aim_point_scatter_m 0\n"
    "    launch_spread_deg 0\n"
    "    piercing_count 0\n"
    "    interceptable false\n"
    "    trajectory_mode Straight\n"
    "    target_leading true\n"
    "\n"
    "unit U04\n"
    "    name Shield Guard\n"
    "    cost 150\n"
    "    squad_size 1\n"
    "    roles Tank/Anchor, Support(future)\n"
    "    unit_layer Ground\n"
    "    target_layer Ground\n"
    "    footprint_w 3\n"
    "    footprint_d 3\n"
    "    radius_m 2.0\n"
    "    height_m 3.5\n"
    "    hp 2000\n"
    "    armor 20\n"
    "    shield_hp 500\n"
    "    move_speed 5\n"
    "    range 12\n"
    "    damage 50\n"
    "    attack_interval 1.5\n"
    "    hits_or_shots_per_attack 1\n"
    "    splash_radius 0\n"
    "    attack_type UnguidedProjectile\n"
    "    initial_speed_mps 80\n"
    "    forward_acceleration_mps2 0\n"
    "    lifetime_s 0.5\n"
    "    max_travel_m 25\n"
    "    collision_radius_m 0.15\n"
    "    gravity_scale 0\n"
    "    aim_point_scatter_m 0\n"
    "    launch_spread_deg 0\n"
    "    piercing_count 0\n"
    "    interceptable false\n"
    "    trajectory_mode Straight\n"
    "    target_leading true\n"
    "\n"
    "unit U05\n"
    "    name Fast Raider\n"
    "    cost 175\n"
    "    squad_size 3\n"
    "    roles Flanker/Diver, Carry\n"
    "    unit_layer Ground\n"
    "    target_layer Ground\n"
    "    footprint_w 3\n"
    "    footprint_d 2\n"
    "    radius_m 0.9\n"
    "    height_m 1.2\n"
    "    hp 260\n"
    "    armor 0\n"
    "    move_speed 18\n"
    "    range 10\n"
    "    damage 75\n"
    "    attack_interval 0.7\n"
    "    hits_or_shots_per_attack 1\n"
    "    splash_radius 0\n"
    "    attack_type UnguidedProjectile\n"
    "    initial_speed_mps 140\n"
    "    forward_acceleration_mps2 0\n"
    "    lifetime_s 0.5\n"
    "    max_travel_m 40\n"
    "    collision_radius_m 0.1\n"
    "    gravity_scale 0\n"
    "    aim_point_scatter_m 0\n"
    "    launch_spread_deg 0\n"
    "    piercing_count 0\n"
    "    interceptable false\n"
    "    trajectory_mode Straight\n"
    "    target_leading true\n"
    "\n"
    "unit U06\n"
    "    name Interceptor\n"
    "    cost 200\n"
    "    squad_size 4\n"
    "    roles Anti-Air, Chaff Clear\n"
    "    unit_layer Air\n"
    "    target_layer Both\n"
    "    target_priority Air\n"
    "    footprint_w 4\n"
    "    footprint_d 2\n"
    "    radius_m 0.9\n"
    "    height_m 0.6\n"
    "    altitude_m 10\n"
    "    hp 230\n"
    "    armor 0\n"
    "    move_speed 20\n"
    "    range 55\n"
    "    damage 32\n"
    "    attack_interval 0.45\n"
    "    hits_or_shots_per_attack 2\n"
    "    splash_radius 0\n"
    "    attack_type GuidedProjectile\n"
    "    initial_speed_mps 50\n"
    "    forward_acceleration_mps2 100\n"
    "    max_speed_mps 120\n"
    "    lifetime_s 2.5\n"
    "    max_travel_m 200\n"
    "    collision_radius_m 0.15\n"
    "    gravity_scale 0\n"
    "    turn_rate_deg_s 240\n"
    "    guidance_delay_s 0.05\n"
    "    guidance_mode PredictiveInterception\n"
    "    aim_point_scatter_m 0\n"
    "    launch_spread_deg 0\n"
    "    piercing_count 0\n"
    "    interceptable false\n"
    "    trajectory_mode Straight\n"
    "    retargeting_allowed false\n"
    "    target_loss_behavior ContinueStraight\n"
    "    target_leading true\n"
    "\n"
    "unit U07\n"
    "    name Field Support\n"
    "    cost 200\n"
    "    squad_size 1\n"
    "    roles Support(future), Control(future)\n"
    "    unit_layer Ground\n"
    "    target_layer Ground\n"
    "    footprint_w 3\n"
    "    footprint_d 2\n"
    "    radius_m 1.5\n"
    "    height_m 2.5\n"
    "    hp 700\n"
    "    armor 5\n"
    "    move_speed 6\n"
    "    range 35\n"
    "    damage 15\n"
    "    attack_interval 1.0\n"
    "    hits_or_shots_per_attack 1\n"
    "    splash_radius 0\n"
    "    attack_type UnguidedProjectile\n"
    "    initial_speed_mps 120\n"
    "    forward_acceleration_mps2 0\n"
    "    lifetime_s 1.0\n"
    "    max_travel_m 60\n"
    "    collision_radius_m 0.1\n"
    "    gravity_scale 0\n"
    "    aim_point_scatter_m 0\n"
    "    launch_spread_deg 0\n"
    "    piercing_count 0\n"
    "    interceptable false\n"
    "    trajectory_mode Straight\n"
    "    target_leading true\n"
    "\n"
    "unit U08\n"
    "    name Railgun\n"
    "    cost 250\n"
    "    squad_size 1\n"
    "    roles Anti-Tank, Carry\n"
    "    unit_layer Ground\n"
    "    target_layer Ground\n"
    "    footprint_w 2\n"
    "    footprint_d 2\n"
    "    radius_m 1.2\n"
    "    height_m 2.5\n"
    "    hp 750\n"
    "    armor 5\n"
    "    move_speed 6\n"
    "    range 90\n"
    "    damage 600\n"
    "    attack_interval 3.0\n"
    "    hits_or_shots_per_attack 1\n"
    "    splash_radius 0\n"
    "    attack_type UnguidedProjectile\n"
    "    initial_speed_mps 400\n"
    "    forward_acceleration_mps2 0\n"
    "    lifetime_s 0.5\n"
    "    max_travel_m 130\n"
    "    collision_radius_m 0.08\n"
    "    gravity_scale 0\n"
    "    aim_point_scatter_m 0\n"
    "    launch_spread_deg 0\n"
    "    piercing_count 0\n"
    "    interceptable false\n"
    "    trajectory_mode Straight\n"
    "    target_leading true\n"
    "\n"
    "unit U09\n"
    "    name Sky Bomber\n"
    "    cost 275\n"
    "    squad_size 1\n"
    "    roles Siege, Chaff Clear\n"
    "    unit_layer Air\n"
    "    target_layer Ground\n"
    "    footprint_w 4\n"
    "    footprint_d 3\n"
    "    radius_m 2.5\n"
    "    height_m 1.5\n"
    "    altitude_m 10\n"
    "    hp 1050\n"
    "    armor 5\n"
    "    move_speed 9\n"
    "    range 85\n"
    "    damage 300\n"
    "    attack_interval 3.5\n"
    "    hits_or_shots_per_attack 1\n"
    "    splash_radius 7\n"
    "    attack_type UnguidedProjectile\n"
    "    initial_speed_mps 45\n"
    "    forward_acceleration_mps2 0\n"
    "    lifetime_s 5.0\n"
    "    max_travel_m 200\n"
    "    collision_radius_m 0.25\n"
    "    gravity_scale 0\n"
    "    aim_point_scatter_m 0\n"
    "    launch_spread_deg 0\n"
    "    piercing_count 0\n"
    "    interceptable false\n"
    "    trajectory_mode ScriptedArc\n"
    "    arc_height_m 20\n"
    "    retargeting_allowed false\n"
    "    target_leading false\n"
    "\n"
    "unit U10\n"
    "    name Artillery Platform\n"
    "    cost 300\n"
    "    squad_size 1\n"
    "    roles Siege, Chaff Clear\n"
    "    unit_layer Ground\n"
    "    target_layer Ground\n"
    "    footprint_w 3\n"
    "    footprint_d 3\n"
    "    radius_m 2.0\n"
    "    height_m 3.0\n"
    "    hp 900\n"
    "    armor 10\n"
    "    move_speed 3\n"
    "    range 130\n"
    "    damage 350\n"
    "    attack_interval 4.0\n"
    "    hits_or_shots_per_attack 1\n"
    "    splash_radius 8\n"
    "    attack_type UnguidedProjectile\n"
    "    initial_speed_mps 55\n"
    "    forward_acceleration_mps2 0\n"
    "    lifetime_s 6.0\n"
    "    max_travel_m 260\n"
    "    collision_radius_m 0.35\n"
    "    gravity_scale 0\n"
    "    aim_point_scatter_m 0\n"
    "    launch_spread_deg 0\n"
    "    piercing_count 0\n"
    "    interceptable false\n"
    "    trajectory_mode ScriptedArc\n"
    "    arc_height_m 30\n"
    "    retargeting_allowed false\n"
    "    target_leading false\n"
    "\n"
    "stage\n"
    "    supply 1000\n"
    "    seed 1\n"
    "    place U01 0 54\n"
    "    place U01 26 54\n"
    "    place U01 8 60\n"
    "    place U02 6 54\n"
    "    place U02 20 54\n"
    "    place U04 14 54\n"
    "    place U03 10 57\n"
    "    place U10 14 60\n";

// Unit defs of full_project, by index.
enum { U01, U02, U03, U04, U05, U06, U07, U08, U09, U10 };

internal void test_project_file(void)
{
    CHECK(read_project(full_project));
    CHECK(defs.error_count == 0);
    const BattleRules* rules = &defs.rules;
    CHECK(rules->cell_size == 2.0f && rules->grid_width == 32 && rules->grid_length == 68 && rules->zone_rows == 14);
    CHECK(rules->round_ticks == 2700 && rules->retarget_ticks == 8); // 90 s and 0.25 s at 30 Hz
    CHECK(rules->gravity == 9.8f && rules->stop_fraction == 0.9f && rules->min_damage_fraction == 0.25f);
    CHECK(battle_enemy_first_row(rules) == 54);
    CHECK(defs.unit_count == 10 && defs.role_count == 12);
    CHECK(strcmp(defs.roles[0], "Chaff/Screen") == 0 && strcmp(defs.roles[3], "Chaff Clear") == 0 && strcmp(defs.roles[6], "Support(future)") == 0);

    // A melee unit: the keys that are always there, and no projectile.
    const UnitDef* swarm = &defs.units[U01];
    CHECK(strcmp(swarm->id, "U01") == 0 && strcmp(swarm->name, "Scout Swarm") == 0);
    CHECK(swarm->cost == 50 && swarm->squad_size == 24 && swarm->footprint_w == 5 && swarm->footprint_d == 2);
    CHECK(swarm->layer == UNIT_LAYER_GROUND && swarm->target_layer == TARGET_LAYER_GROUND && swarm->targets == TARGET_GROUND);
    CHECK(swarm->radius == 0.5f && swarm->height == 0.8f && swarm->altitude == 0.0f);
    CHECK(swarm->hp == 45.0f && swarm->armor == 0.0f && swarm->shield_hp == 0.0f && swarm->move_speed == 15.0f);
    CHECK(swarm->range == 6.0f && swarm->damage == 7.0f && swarm->attack_interval == 0.7f && swarm->hits_per_attack == 1);
    CHECK(swarm->splash_radius == 0.0f && swarm->attack_type == ATTACK_MELEE);
    CHECK(swarm->projectile.initial_speed == 0.0f && swarm->projectile.trajectory == TRAJECTORY_STRAIGHT);
    CHECK(defs.unit_roles[U01] == 3); // Chaff/Screen and Flanker, the first two declared
    // Both layers, with the ground first; a straight shot.
    const UnitDef* rifle = &defs.units[U02];
    CHECK(rifle->target_layer == TARGET_LAYER_BOTH && rifle->targets == (TARGET_GROUND | TARGET_AIR) && rifle->target_priority == UNIT_LAYER_GROUND);
    CHECK(rifle->attack_type == ATTACK_UNGUIDED && rifle->projectile.initial_speed == 180.0f && rifle->projectile.lifetime == 1.0f);
    CHECK(rifle->projectile.max_travel == 90.0f && rifle->projectile.collision_radius == 0.05f && rifle->projectile.target_leading);
    CHECK(!rifle->projectile.interceptable && rifle->projectile.piercing_count == 0 && rifle->projectile.max_speed == 0.0f);
    CHECK(defs.units[U03].hits_per_attack == 2 && defs.units[U03].splash_radius == 2.5f);
    // A shield.
    CHECK(defs.units[U04].shield_hp == 500.0f && defs.units[U04].armor == 20.0f && defs.units[U04].hp == 2000.0f);
    // A guided flier that picks air first.
    const UnitDef* interceptor = &defs.units[U06];
    CHECK(interceptor->layer == UNIT_LAYER_AIR && interceptor->altitude == 10.0f && interceptor->target_priority == UNIT_LAYER_AIR);
    CHECK(interceptor->attack_type == ATTACK_GUIDED && interceptor->projectile.forward_acceleration == 100.0f);
    CHECK(interceptor->projectile.max_speed == 120.0f && near_value(interceptor->projectile.turn_rate, 240.0f * NV_PI / 180.0f, 1e-5f));
    CHECK(interceptor->projectile.guidance_delay == 0.05f && interceptor->projectile.guidance_mode == GUIDANCE_PREDICTIVE_INTERCEPTION);
    CHECK(interceptor->projectile.target_loss == TARGET_LOSS_CONTINUE_STRAIGHT && !interceptor->projectile.retargeting_allowed);
    // A scripted arc, unguided, with no leading.
    const UnitDef* artillery = &defs.units[U10];
    CHECK(artillery->projectile.trajectory == TRAJECTORY_SCRIPTED_ARC && artillery->projectile.arc_height == 30.0f);
    CHECK(!artillery->projectile.target_leading && artillery->splash_radius == 8.0f && artillery->range == 130.0f);
    CHECK(defs.units[U09].layer == UNIT_LAYER_AIR && defs.units[U09].targets == TARGET_GROUND);

    CHECK(defs.supply == 1000 && defs.seed == 1 && defs.enemy_count == 8);
    CHECK(defs.enemy[0].def == U01 && defs.enemy[0].cell_x == 0 && defs.enemy[0].cell_row == 54);
    CHECK(defs.enemy[7].def == U10 && defs.enemy[7].cell_x == 14 && defs.enemy[7].cell_row == 60);
    // What the round does not follow yet, in the order it first came (U01, a melee unit, gives none).
    CHECK(strncmp(defs.unfollowed, "target_priority, attack_type, initial_speed_mps, forward_acceleration_mps2, lifetime_s", 86) == 0);
    CHECK(strstr(defs.unfollowed, "aim_point_scatter_m") == NULL && strstr(defs.unfollowed, "hits_or_shots_per_attack") != NULL);
    CHECK(strstr(defs.unfollowed, "arc_height_m") != NULL && strstr(defs.unfollowed, "turn_rate_deg_s") != NULL);
}

// Editing the rules in a project's text (defs_rule_get, defs_rule_set): the values as the file writes them, a change that
// touches only its values (the comment keeps its column) and reads back, and what is refused.
internal void test_rule_editing(void)
{
    static char edited[sizeof(full_project) + 64];
    umm size = sizeof(full_project) - 1;
    f64 values[BATTLE_RULE_MAX_VALUES] = {0};
    CHECK(defs_rule_get(full_project, size, "grid", values) == 2 && values[0] == 32.0 && values[1] == 68.0);
    CHECK(defs_rule_get(full_project, size, "round_time", values) == 1 && values[0] == 90.0); // seconds, not ticks
    CHECK(defs_rule_get(full_project, size, "retarget_interval", values) == 1 && values[0] == 0.25);
    CHECK(defs_rule_get(full_project, size, "gravity", values) == 1 && values[0] == 9.8);
    CHECK(defs_rule_get(full_project, size, "supply", values) == 0); // the stage's, not a rule
    CHECK(defs_rule_get(full_project, size, "radius_m", values) == 0);
    CHECK(defs_rule_get(full_project, size, "rules", values) == 0);

    // Writing every rule's own values back gives the same text, byte for byte.
    const char* keys[] = {"cell_size", "grid", "zone_rows", "round_time", "gravity", "retarget_interval", "stop_fraction",
                          "min_damage_fraction"};
    for (u32 i = 0; i < NV_ARRAY_COUNT(keys); ++i) {
        u32 count = defs_rule_get(full_project, size, keys[i], values);
        CHECK(count > 0);
        umm written = defs_rule_set(full_project, size, keys[i], values, count, edited, sizeof(edited));
        CHECK(written == size && memcmp(edited, full_project, size) == 0);
    }

    // A longer value takes spaces from before the comment, a shorter one gives them back; other lines stay.
    f64 gravity = 12.3456789;
    umm written = defs_rule_set(full_project, size, "gravity", &gravity, 1, edited, sizeof(edited));
    edited[written] = 0;
    CHECK(written == size && strstr(edited, "\n    gravity 12.3457          # m/s²\n"));
    gravity = 3.0;
    written = defs_rule_set(full_project, size, "gravity", &gravity, 1, edited, sizeof(edited));
    edited[written] = 0;
    CHECK(written == size && strstr(edited, "\n    gravity 3                # m/s²\n"));
    CHECK(memcmp(edited, full_project, (umm)(strstr(full_project, "    gravity") - full_project)) == 0);
    CHECK(read_project(edited) && defs.rules.gravity == 3.0f && defs.rules.cell_size == 2.0f);
    f64 longer = 0.123; // past the comment's column: one space is kept
    written = defs_rule_set(full_project, size, "min_damage_fraction", &longer, 1, edited, sizeof(edited));
    edited[written] = 0;
    CHECK(strstr(edited, "\n    min_damage_fraction 0.123 # armor"));
    f64 negative_zero = -0.00001;
    written = defs_rule_set(full_project, size, "min_damage_fraction", &negative_zero, 1, edited, sizeof(edited));
    edited[written] = 0;
    CHECK(strstr(edited, "\n    min_damage_fraction 0    # armor"));

    // Integers are rounded; the grid takes two values; seconds stay seconds and become ticks when read.
    f64 grid[2] = {40.4, 67.6}; // the length stays 68: the stage's places are in the last 14 rows
    written = defs_rule_set(full_project, size, "grid", grid, 2, edited, sizeof(edited));
    edited[written] = 0;
    CHECK(strstr(edited, "\n    grid 40 68               # cells across"));
    CHECK(read_project(edited) && defs.rules.grid_width == 40 && defs.rules.grid_length == 68);
    f64 round_time = 90.5;
    written = defs_rule_set(full_project, size, "round_time", &round_time, 1, edited, sizeof(edited));
    edited[written] = 0;
    CHECK(read_project(edited) && defs.rules.round_ticks == 2715);
    CHECK(defs_rule_get(edited, written, "round_time", values) == 1 && values[0] == 90.5);

    // A line without a comment, and CRLF line ends, keep what follows the values.
    const char* plain = "abproj_version 1\r\nrules\r\n    zone_rows 14\r\n";
    f64 rows = 9.0;
    written = defs_rule_set(plain, strlen(plain), "zone_rows", &rows, 1, edited, sizeof(edited));
    CHECK(written == strlen(plain) - 1 && memcmp(edited, "abproj_version 1\r\nrules\r\n    zone_rows 9\r\n", written) == 0);

    // A value the rules refuse is still written: the reader judges it.
    f64 rows_too_many = 35.0; // over half the grid's 68 rows
    written = defs_rule_set(full_project, size, "zone_rows", &rows_too_many, 1, edited, sizeof(edited));
    edited[written] = 0;
    CHECK(written > 0 && !read_project(edited));

    // Refused: not a rule, the wrong number of values, no room.
    f64 one = 1.0;
    CHECK(defs_rule_set(full_project, size, "supply", &one, 1, edited, sizeof(edited)) == 0);
    CHECK(defs_rule_set(full_project, size, "grid", &one, 1, edited, sizeof(edited)) == 0);
    CHECK(defs_rule_set(full_project, size, "gravity", grid, 2, edited, sizeof(edited)) == 0);
    gravity = 9.8;
    CHECK(defs_rule_set(full_project, size, "gravity", &gravity, 1, edited, size - 1) == 0);
    CHECK(defs_rule_set(full_project, size, "gravity", &gravity, 1, edited, size) == size);
}

// Editing a unit def in a project's text (defs_unit_get, defs_unit_set; unit_lab.md, "Editing the unit's values"): number keys
// only, found by the unit's unit_id.
internal void test_unit_editing(void)
{
    static char edited[sizeof(full_project) + 256], again[sizeof(full_project) + 256];
    umm size = sizeof(full_project) - 1;
    f64 value = 0.0;
    CHECK(defs_unit_get(full_project, size, "U01", "hp", &value) == 1 && value == 45.0);
    CHECK(defs_unit_get(full_project, size, "U06", "turn_rate_deg_s", &value) == 1 && value == 240.0); // degrees, as written
    CHECK(defs_unit_get(full_project, size, "U06", "lifetime_s", &value) == 1 && value == 2.5);
    CHECK(defs_unit_get(full_project, size, "U01", "shield_hp", &value) == 0);         // left out
    CHECK(defs_unit_get(full_project, size, "U01", "initial_speed_mps", &value) == 0); // a melee unit has none
    CHECK(defs_unit_get(full_project, size, "Nobody", "hp", &value) == 0);
    CHECK(defs_unit_get(full_project, size, "U01", "name", &value) == 0);           // not a number key
    CHECK(defs_unit_get(full_project, size, "U02", "target_leading", &value) == 0); // nor a boolean
    CHECK(defs_unit_get(full_project, size, "U02", "unit_layer", &value) == 0);     // nor a word

    // Writing every given number key's own value back gives the same text, byte for byte (their values are written as the
    // editor writes them).
    const struct { const char* unit; const char* key; } keys[] = {
        {"U01", "cost"}, {"U01", "squad_size"}, {"U01", "footprint_w"}, {"U01", "footprint_d"}, {"U01", "radius_m"},
        {"U01", "height_m"}, {"U01", "hp"}, {"U01", "armor"}, {"U01", "move_speed"}, {"U01", "range"}, {"U01", "damage"},
        {"U01", "attack_interval"}, {"U01", "hits_or_shots_per_attack"}, {"U01", "splash_radius"}, {"U04", "shield_hp"},
        {"U06", "altitude_m"}, {"U06", "initial_speed_mps"}, {"U06", "forward_acceleration_mps2"}, {"U06", "max_speed_mps"},
        {"U06", "max_travel_m"}, {"U06", "collision_radius_m"}, {"U06", "gravity_scale"}, {"U06", "turn_rate_deg_s"},
        {"U06", "guidance_delay_s"}, {"U06", "aim_point_scatter_m"}, {"U06", "launch_spread_deg"}, {"U06", "piercing_count"},
        {"U10", "arc_height_m"},
    };
    for (u32 i = 0; i < NV_ARRAY_COUNT(keys); ++i) {
        CHECK(defs_unit_get(full_project, size, keys[i].unit, keys[i].key, &value) == 1);
        umm written = defs_unit_set(full_project, size, keys[i].unit, keys[i].key, value, edited, sizeof(edited));
        CHECK(written == size && memcmp(edited, full_project, size) == 0);
    }

    // A change touches only that unit's line: U02 has a range of its own.
    f64 range = 7.25;
    umm written = defs_unit_set(full_project, size, "U01", "range", range, edited, sizeof(edited));
    edited[written] = 0;
    CHECK(written == size + 3 && strstr(edited, "    range 7.25\n    damage 7\n") != NULL);
    CHECK(read_project(edited) && defs.units[U01].range == 7.25f && defs.units[U02].range == 60.0f);
    // An integer key is written as an integer.
    written = defs_unit_set(full_project, size, "U01", "damage", 12.4, edited, sizeof(edited));
    edited[written] = 0;
    CHECK(strstr(edited, "    damage 12\n    attack_interval 0.7\n") != NULL && read_project(edited) && defs.units[U01].damage == 12.0f);
    // Degrees are written as degrees.
    written = defs_unit_set(full_project, size, "U06", "turn_rate_deg_s", 180.0, edited, sizeof(edited));
    edited[written] = 0;
    CHECK(read_project(edited) && near_value(defs.units[U06].projectile.turn_rate, NV_PI, 1e-6f));

    // A key the file leaves out gets a line after the unit's last key, before the blank line that ends it.
    written = defs_unit_set(full_project, size, "U01", "shield_hp", 30.0, edited, sizeof(edited));
    edited[written] = 0;
    CHECK(written > size && strstr(edited, "    attack_type Melee\n    shield_hp 30\n\nunit U02\n") != NULL);
    CHECK(read_project(edited) && defs.units[U01].shield_hp == 30.0f);
    CHECK(defs_unit_get(edited, written, "U01", "shield_hp", &value) == 1 && value == 30.0);
    // Written again, it is that line that changes.
    umm rewritten = defs_unit_set(edited, written, "U01", "shield_hp", 40.0, again, sizeof(again));
    again[rewritten] = 0;
    CHECK(rewritten == written && strstr(again, "    shield_hp 40\n") != NULL);
    // A projectile key added to a melee unit is written; the reader refuses it.
    written = defs_unit_set(full_project, size, "U01", "initial_speed_mps", 50.0, edited, sizeof(edited));
    edited[written] = 0;
    CHECK(written > size && !read_project(edited) && first_error_has("'initial_speed_mps' is for projectile attacks; unit 'U01' is Melee"));

    // CRLF lines: the new line ends in CRLF too.
    static char crlf_unit[4096];
    snprintf(crlf_unit, sizeof(crlf_unit), "%s%s%s", good_header, good_units, "stage\n    supply 1000\n    place Crawler 0 34");
    static char crlf[8192];
    umm crlf_size = 0;
    for (const char* p = crlf_unit; *p; ++p) {
        if (*p == '\n')
            crlf[crlf_size++] = '\r';
        crlf[crlf_size++] = *p;
    }
    written = defs_unit_set(crlf, crlf_size, "Crawler", "shield_hp", 20.0, edited, sizeof(edited));
    edited[written] = 0;
    CHECK(strstr(edited, "    attack_type Melee\r\n    shield_hp 20\r\nstage\r\n") != NULL);
    CHECK(read_project(edited) && defs.units[0].shield_hp == 20.0f);

    // What is refused.
    CHECK(defs_unit_set(full_project, size, "Nobody", "hp", 5.0, edited, sizeof(edited)) == 0);
    CHECK(defs_unit_set(full_project, size, "U01", "healt", 5.0, edited, sizeof(edited)) == 0);
    CHECK(defs_unit_set(full_project, size, "U01", "attack_type", 1.0, edited, sizeof(edited)) == 0);
    CHECK(defs_unit_set(full_project, size, "U01", "name", 1.0, edited, sizeof(edited)) == 0);
    CHECK(defs_unit_set(full_project, size, "U01", "hp", 5.0, edited, 32) == 0);
    // A value out of range is written; reading the result refuses it.
    written = defs_unit_set(full_project, size, "U01", "hp", -5.0, edited, sizeof(edited));
    edited[written] = 0;
    CHECK(written > 0 && !read_project(edited) && first_error_has("'hp' must be >= 1"));
}

// The repository's own file: it reads without errors, whatever values a designer has put in it, and the round it plays ends
// within its time limit. (Its values are not checked; test_project_file does that on a copy.)
internal void test_repository_file(void)
{
    static char text[65536];
    umm size = read_file("default.abproj", text, sizeof(text) - 1);
    CHECK(size > 0 && size < sizeof(text) - 1);
    text[size] = 0;
    CHECK(read_project(text));
    CHECK(defs.error_count == 0 && defs.unit_count >= 1 && defs.enemy_count >= 1);

    // Fill the player's zone, from the row nearest the enemy and every other cell, with the first unit while the supply lasts.
    battle_init(&battle, &defs);
    for (s32 row = (s32)defs.rules.zone_rows - 1; row >= 0; --row)
        for (s32 x = 0; x < (s32)defs.rules.grid_width; x += 2)
            battle_place(&battle, 0, x, row);
    CHECK(battle_start(&battle));
    while (battle.phase == BATTLE_FIGHT) {
        battle_tick(&battle);
        battle.event_count = 0;
    }
    CHECK(battle.phase == BATTLE_RESULT && battle.tick <= defs.rules.round_ticks && battle.outcome != OUTCOME_NONE);
}

internal void test_definition_syntax(void)
{
    // Comments, blank lines and CRLF line ends give the same values as plain text.
    CHECK(read_units(good_units, "Crawler"));
    UnitDef plain = defs.units[0];
    const char* messy =
        "# a comment line\r\n"
        "\r\n"
        "unit Crawler   # trailing comment\r\n"
        "    name Crawler   # a comment after a text\r\n"
        "    cost   100\r\n"
        "\r\n"
        "    hp 120\r\n"
        "    armor 0\r\n"
        "    squad_size 1\r\n"
        "    unit_layer Ground\r\n"
        "    target_layer Ground\r\n"
        "    footprint_w 1\r\n"
        "    footprint_d 1\r\n"
        "    radius_m 0.5\r\n"
        "    height_m .8\r\n" // no digit before the point
        "    move_speed 5\r\n"
        "    range +20\r\n" // a sign
        "    damage 30\r\n"
        "    attack_interval 1.5\r\n"
        "    hits_or_shots_per_attack 1\r\n"
        "    splash_radius 0\r\n"
        "    attack_type Melee\r\n";
    CHECK(read_units(messy, "Crawler"));
    CHECK(memcmp(&defs.units[0], &plain, sizeof(plain)) == 0);
    // The last line may lack its line end.
    CHECK(read_project("abproj_version 1\nrules\n cell_size 2\n grid 32 48\n zone_rows 14\n round_time 60\n gravity 9.8\n retarget_interval 0.25\n"
                       " stop_fraction 0.9\n min_damage_fraction 0.25\n"
                       "unit A\n name A\n cost 1\n squad_size 1\n unit_layer Ground\n target_layer Ground\n footprint_w 1\n footprint_d 1\n"
                       " radius_m 1\n height_m 1\n hp 1\n armor 0\n move_speed 0\n range 1\n damage 0\n attack_interval 1\n"
                       " hits_or_shots_per_attack 1\n splash_radius 0\n attack_type Melee\n"
                       "stage\n supply 100\n place A 12 36"));

    // Defaults: no shield, no altitude, the ground first, and a projectile's optional keys at the wiki's defaults (0, false).
    CHECK(read_units(good_units, "Crawler"));
    CHECK(defs.units[0].shield_hp == 0.0f && defs.units[0].altitude == 0.0f && defs.units[0].target_priority == UNIT_LAYER_GROUND);
    CHECK(read_units(shooter_with(""), "Crawler"));
    const ProjectileDef* projectile = &defs.units[0].projectile;
    CHECK(defs.units[0].attack_type == ATTACK_UNGUIDED && projectile->initial_speed == 100.0f && projectile->target_leading);
    CHECK(projectile->gravity_scale == 0.0f && projectile->aim_point_scatter == 0.0f && projectile->launch_spread == 0.0f);
    CHECK(projectile->piercing_count == 0 && !projectile->interceptable && !projectile->retargeting_allowed && projectile->max_speed == 0.0f);
    CHECK(projectile->guidance_mode == GUIDANCE_NONE && projectile->target_loss == TARGET_LOSS_NONE);

    // Two units, and a unit's keys can come in any order.
    char two[4096];
    snprintf(two, sizeof(two), "%s%s", good_units, replaced(replaced(good_units, "unit Crawler\n", "unit B\n    cost 6\n"), "    cost 100\n", ""));
    CHECK(read_units(two, "B"));
    CHECK(defs.unit_count == 2 && defs.units[1].cost == 6 && strcmp(defs.units[1].id, "B") == 0 && strcmp(defs.units[1].name, "Crawler") == 0);

    // The stage: the seed defaults to 1 and may be given; the keys come in any order.
    CHECK(read_stage(good_stage));
    CHECK(defs.seed == 1 && defs.supply == 1000 && defs.enemy_count == 1);
    CHECK(read_stage("seed 4294967295\nplace Crawler 0 47\nplace Crawler 31 34\nsupply 1000\n"));
    CHECK(defs.seed == 4294967295u && defs.enemy_count == 2 && defs.enemy[1].cell_x == 31 && defs.enemy[1].cell_row == 34);
}

// The kinds of value the unit fields brought (unit_fields.md, "Value kinds"): text, words, booleans and lists.
internal void test_value_kinds(void)
{
    // Text: the rest of the line, its spaces kept, up to a comment; 1 to 31 bytes; UTF-8.
    CHECK(read_units(unit_with("name Crawler", "name  Artillery  Platform   # the comment is not the name"), "Crawler"));
    CHECK(strcmp(defs.units[0].name, "Artillery  Platform") == 0);
    CHECK(read_units(unit_with("name Crawler", "name \xEC\xA0\x95\xEC\xB0\xB0\xEB\xB3\x91"), "Crawler")); // 정찰병
    CHECK(strcmp(defs.units[0].name, "\xEC\xA0\x95\xEC\xB0\xB0\xEB\xB3\x91") == 0);
    CHECK(read_units(unit_with("name Crawler", "name 1234567890123456789012345678901"), "Crawler")); // 31 bytes
    expect_error_in(PART_UNITS, unit_with("name Crawler", "name 12345678901234567890123456789012"), 1, 2, "'name' is longer than 31 bytes");
    expect_error_in(PART_UNITS, unit_with("name Crawler", "name"), 1, 2, "'name' takes a text");
    expect_error_in(PART_UNITS, unit_with("name Crawler", "name   # only a comment"), 1, 2, "'name' takes a text");

    // Words: one of the key's, as the wiki writes them (case counts).
    expect_error_in(PART_UNITS, unit_with("attack_type Melee", "attack_type melee"), 1, 19,
                    "unknown attack_type 'melee' (Melee, UnguidedProjectile or GuidedProjectile)");
    expect_error_in(PART_UNITS, unit_with("attack_type Melee", "attack_type Melee Melee"), 1, 19,
                    "'attack_type' takes Melee, UnguidedProjectile or GuidedProjectile");
    expect_error_in(PART_UNITS, unit_with("unit_layer Ground", "unit_layer ground"), 1, 5, "unknown unit_layer 'ground' (Ground or Air)");
    // Booleans: true or false, nothing else.
    char text[4096];
    snprintf(text, sizeof(text), "%s", replaced(shooter_with(""), "target_leading true", "target_leading True"));
    expect_error_in(PART_UNITS, text, 1, 26, "unknown target_leading 'True' (true or false)");
    snprintf(text, sizeof(text), "%s", replaced(shooter_with(""), "target_leading true", "target_leading 1"));
    expect_error_in(PART_UNITS, text, 1, 26, "unknown target_leading '1' (true or false)");
    snprintf(text, sizeof(text), "%s", replaced(shooter_with(""), "target_leading true", "target_leading"));
    expect_error_in(PART_UNITS, text, 1, 26, "'target_leading' takes true or false");
    CHECK(read_units(replaced(shooter_with(""), "target_leading true", "target_leading false"), "Crawler") &&
          !defs.units[0].projectile.target_leading);
}

// The version line, the order of the top-level statements, and the rules block.
internal void test_project_structure(void)
{
    // The version: the first statement, an integer from 1 to ABPROJ_VERSION.
    CHECK(ABPROJ_VERSION == 1);
    char text[8192];
    snprintf(text, sizeof(text), "%s%sstage\n    supply 1000\n    place Crawler 12 36\n", good_header, good_units);
    CHECK(read_project(text));
    // No version line: the first statement is something else. One error, at that statement (the read goes on as if it had one).
    snprintf(text, sizeof(text), "%s%sstage\n    supply 1000\n    place Crawler 12 36\n", strstr(good_header, "rules"), good_units);
    expect_error_file(text, 1, 1, "the file must start with 'abproj_version <version>'");
    expect_error_file("", 1, 0, "the file must start with 'abproj_version <version>'");
    snprintf(text, sizeof(text), "# only a comment\n\nabproj_version 1\n%s%sstage\n    supply 1000\n    place Crawler 12 36\n",
             strstr(good_header, "rules"), good_units);
    CHECK(read_project(text)); // comments and blank lines before it are not statements
    snprintf(text, sizeof(text), "abproj_version 1\nabproj_version 1\n%s%sstage\n    supply 1000\n    place Crawler 12 36\n",
             strstr(good_header, "rules"), good_units);
    expect_error_file(text, 1, 2, "'abproj_version' must be the first statement");
    snprintf(text, sizeof(text), "abproj_version 0\n%s%sstage\n    supply 1000\n    place Crawler 12 36\n", strstr(good_header, "rules"), good_units);
    expect_error_file(text, 1, 1, "'abproj_version' must be 1 to 1");
    snprintf(text, sizeof(text), "abproj_version 1.5\n%s%sstage\n    supply 1000\n    place Crawler 12 36\n", strstr(good_header, "rules"), good_units);
    expect_error_file(text, 1, 1, "not an integer");
    expect_error_file("abproj_version\n", 1, 1, "'abproj_version' takes one integer");
    // A newer format is one error and the rest is not read (it may mean anything by it).
    expect_error_file("abproj_version 2\nrules\n    bogus 1\nunit\n", 1, 1, "format 2 is newer than this build reads (1)");

    // The order: version, rules, units, stage. A statement out of place is one error and its block is skipped.
    const u32 stage_line = GOOD_HEADER_LINES + GOOD_UNITS_LINES + 1;
    char swapped[8192];
    snprintf(swapped, sizeof(swapped), "abproj_version 1\n%s%s%s", good_units, strstr(good_header, "rules"),
             "stage\n    supply 1000\n    place Crawler 12 36\n");
    expect_error_file(swapped, 2, 2, "'unit' must come after 'rules'"); // and the stage, with no unit read before it, is out of place too
    snprintf(swapped, sizeof(swapped), "%s%s%s", good_header, "stage\n    supply 1000\n    place Crawler 12 36\n", good_units);
    expect_error_file(swapped, 1, 11, "'stage' must come after the units"); // the units after it are read all the same
    snprintf(swapped, sizeof(swapped), "%s%s%s%s", good_header, good_units, "stage\n    supply 1000\n    place Crawler 12 36\n", good_units);
    expect_error_file(swapped, 1, stage_line + 3, "'unit' must come before 'stage'");
    snprintf(swapped, sizeof(swapped), "%s%s%s%s", good_header, good_units, "stage\n    supply 1000\n    place Crawler 12 36\n",
             "stage\n    supply 1000\n");
    expect_error_file(swapped, 1, stage_line + 3, "'stage' is given twice");
    snprintf(swapped, sizeof(swapped), "%s%s%s", good_header, strstr(good_header, "rules"), good_units);
    expect_error_file(swapped, 1, 11, "'rules' is given twice");
    snprintf(swapped, sizeof(swapped), "abproj_version 1\n%s", "stage\n    supply 1000\n");
    expect_error_file(swapped, 1, 2, "'stage' must come after 'rules' and the units");
    // What a whole file lacks, once the rest is right: the first missing part.
    expect_error_file("abproj_version 1\n", 1, 0, "missing 'rules'");
    expect_error_file(good_header, 1, 0, "no units");
    snprintf(text, sizeof(text), "%s%s", good_header, good_units);
    expect_error_file(text, 1, 0, "missing 'stage'");
    expect_error_file("abproj_version 1\nbogus\n    child 1\n", 1, 2, "unknown key 'bogus'"); // its inside is skipped
    expect_error_file("abproj_version 1\n unit A\n", 1, 2, "unexpected indentation");

    // The rules: every key is required, with a range; a bad value is one error.
    {
        // clang-format off
        expect_error_in(PART_RULES, "", 8, BLOCK_LINE, "missing 'cell_size'"); // all eight keys are missing, at the block's line
        const char* all =
            "cell_size 2\ngrid 32 48\nzone_rows 14\nround_time 60\ngravity 9.8\nretarget_interval 0.25\nstop_fraction 0.9\nmin_damage_fraction 0.25\n";
        CHECK(read_part(PART_RULES, all, NULL));
        const struct { const char* key; const char* bad; const char* message; } bad_values[] = {
            {"cell_size 2\n", "cell_size 0\n", "'cell_size' must be > 0 and <= 10"},
            {"cell_size 2\n", "cell_size 10.5\n", "<= 10"},
            {"cell_size 2\n", "cell_size 2 3\n", "'cell_size' takes 1 value"},
            {"grid 32 48\n", "grid 0 48\n", "'grid' width must be 1 to 64"},
            {"grid 32 48\n", "grid 65 48\n", "'grid' width must be 1 to 64"},
            {"grid 32 48\n", "grid 32 1\n", "'grid' length must be 2 to 128"},
            {"grid 32 48\n", "grid 32 129\n", "'grid' length must be 2 to 128"},
            {"grid 32 48\n", "grid 32\n", "'grid' takes 2 values"},
            {"grid 32 48\n", "grid 32.5 48\n", "not an integer"},
            {"zone_rows 14\n", "zone_rows 0\n", "'zone_rows' must be >= 1 and <= 32"},
            {"zone_rows 14\n", "zone_rows 33\n", "<= 32"},
            {"round_time 60\n", "round_time 0\n", "'round_time' must be > 0 and <= 600"},
            {"round_time 60\n", "round_time 601\n", "<= 600"},
            {"gravity 9.8\n", "gravity 0\n", "'gravity' must be > 0 and <= 100"},
            {"gravity 9.8\n", "gravity -9.8\n", "> 0"},
            {"retarget_interval 0.25\n", "retarget_interval 0\n", "'retarget_interval' must be > 0 and <= 10"},
            {"retarget_interval 0.25\n", "retarget_interval 11\n", "<= 10"},
            {"stop_fraction 0.9\n", "stop_fraction 0\n", "'stop_fraction' must be > 0 and <= 1"},
            {"stop_fraction 0.9\n", "stop_fraction 1.1\n", "<= 1"},
            {"min_damage_fraction 0.25\n", "min_damage_fraction -0.1\n", "'min_damage_fraction' must be >= 0 and <= 1"},
            {"min_damage_fraction 0.25\n", "min_damage_fraction 1.5\n", "<= 1"},
        };
        // clang-format on
        for (u32 i = 0; i < NV_ARRAY_COUNT(bad_values); ++i) {
            // The bad line replaces the good one at its place in the block; its line is counted in the fragment.
            const char* at = strstr(all, bad_values[i].key);
            u32 line = 1;
            for (const char* p = all; p < at; ++p)
                line += *p == '\n';
            expect_error_in(PART_RULES, replaced(all, bad_values[i].key, bad_values[i].bad), 1, line, bad_values[i].message);
        }
        // A key missing on its own is one error at the block's line; a key twice is one at the second; unknown keys are reported.
        expect_error_in(PART_RULES, "grid 32 48\nzone_rows 14\nround_time 60\ngravity 9.8\nretarget_interval 0.25\nstop_fraction 0.9\nmin_damage_fraction 0.25\n",
                        1, BLOCK_LINE, "block 'rules' is missing 'cell_size'");
        expect_error_in(PART_RULES, "gravity 3\ncell_size 2\ngrid 32 48\nzone_rows 14\nround_time 60\ngravity 9.8\nretarget_interval 0.25\nstop_fraction 0.9\nmin_damage_fraction 0.25\n",
                        1, 6, "'gravity' is given twice in this block");
        expect_error_in(PART_RULES, "wind 3\ncell_size 2\ngrid 32 48\nzone_rows 14\nround_time 60\ngravity 9.8\nretarget_interval 0.25\nstop_fraction 0.9\nmin_damage_fraction 0.25\n",
                        1, 1, "unknown key 'wind'");
    }
    // The zones may not overlap: zone_rows is at most half the grid's length (reported at the zone_rows line).
    CHECK(read_part(PART_RULES, "cell_size 2\ngrid 32 48\nzone_rows 24\nround_time 60\ngravity 9.8\nretarget_interval 0.25\nstop_fraction 0.9\nmin_damage_fraction 0.25\n", NULL));
    expect_error_in(PART_RULES, "cell_size 2\ngrid 32 48\nzone_rows 25\nround_time 60\ngravity 9.8\nretarget_interval 0.25\nstop_fraction 0.9\nmin_damage_fraction 0.25\n",
                    1, 3, "'zone_rows' must be at most 24"); // one error: the stage's cells are not judged against a grid that is wrong

    // Times become whole ticks, rounded up, at least 1.
    CHECK(read_part(PART_RULES, "cell_size 2\ngrid 32 48\nzone_rows 14\nround_time 10\ngravity 9.8\nretarget_interval 1\nstop_fraction 0.9\nmin_damage_fraction 0.25\n", NULL));
    CHECK(defs.rules.round_ticks == 300 && defs.rules.retarget_ticks == 30);
    CHECK(read_part(PART_RULES, "cell_size 2\ngrid 32 48\nzone_rows 14\nround_time 0.01\ngravity 9.8\nretarget_interval 0.05\nstop_fraction 0.9\nmin_damage_fraction 0.25\n", NULL));
    CHECK(defs.rules.round_ticks == 1 && defs.rules.retarget_ticks == 2); // 0.3 and 1.5 ticks
    CHECK(read_project(full_project) && defs.rules.round_ticks == 2700 && defs.rules.retarget_ticks == 8); // 7.5 ticks up to 8

    // The stage's cells follow the rules: another grid and zone move the enemy's rows.
    CHECK(read_part(PART_RULES, "cell_size 3\ngrid 10 20\nzone_rows 4\nround_time 60\ngravity 9.8\nretarget_interval 0.25\nstop_fraction 0.9\nmin_damage_fraction 0.25\n", NULL) == false);
    CHECK(first_error_has("cell x must be 0 to 9") || first_error_has("cell row must be 16 to 19"));
}

internal void test_definition_errors(void)
{
    // Each error: how many errors, the line of the first (of the fragment), and what it says. One mistake in a good unit is one
    // error.
    // clang-format off
    expect_error_in(PART_UNITS, unit_with("hp 120", "healt 120"), 2, 11, "unknown key 'healt'"); // and hp is missing (at the unit's line)
    expect_error_in(PART_UNITS, unit_with("hp 120", ""), 1, 1, "unit 'Crawler' is missing 'hp'");
    expect_error_in(PART_UNITS, "bogus\n", 1, 1, "unknown key 'bogus'"); // and no units
    expect_error_in(PART_UNITS, unit_with("cost 100", "cost 100\n    cost 2"), 1, 4, "'cost' is given twice");
    expect_error_in(PART_UNITS, unit_with("cost 100", "cost 1 2"), 1, 3, "'cost' takes 1 value");
    expect_error_in(PART_UNITS, unit_with("cost 100", "cost 0"), 1, 3, "'cost' must be >= 1 and <= 10000");
    expect_error_in(PART_UNITS, unit_with("cost 100", "cost 10001"), 1, 3, "<= 10000");
    expect_error_in(PART_UNITS, unit_with("cost 100", "cost 1.5"), 1, 3, "not an integer");
    expect_error_in(PART_UNITS, unit_with("hp 120", "hp 0"), 1, 11, "'hp' must be >= 1");
    expect_error_in(PART_UNITS, unit_with("hp 120", "hp 12.5"), 1, 11, "not an integer"); // the wiki's damage units are whole
    expect_error_in(PART_UNITS, unit_with("hp 120", "hp 1e3"), 1, 11, "not a number");    // no exponents
    expect_error_in(PART_UNITS, unit_with("hp 120", "hp abc"), 1, 11, "not a number");
    expect_error_in(PART_UNITS, unit_with("hp 120", "hp"), 1, 11, "'hp' takes 1 value");
    expect_error_in(PART_UNITS, unit_with("armor 0", "armor -1"), 1, 12, "'armor' must be >= 0");
    expect_error_in(PART_UNITS, unit_with("damage 30", "damage 0.5"), 1, 15, "not an integer");
    expect_error_in(PART_UNITS, unit_with("radius_m 0.5", "radius_m 0"), 1, 9, "'radius_m' must be > 0");
    expect_error_in(PART_UNITS, unit_with("move_speed 5", "move_speed -1"), 1, 13, "'move_speed' must be >= 0");
    expect_error_in(PART_UNITS, unit_with("attack_interval 1.5", "attack_interval 0"), 1, 16, "'attack_interval' must be > 0");
    expect_error_in(PART_UNITS, unit_with("hits_or_shots_per_attack 1", "hits_or_shots_per_attack 0"), 1, 17, "'hits_or_shots_per_attack' must be >= 1 and <= 16");
    expect_error_in(PART_UNITS, unit_with("hits_or_shots_per_attack 1", "hits_or_shots_per_attack 17"), 1, 17, "<= 16");
    expect_error_in(PART_UNITS, unit_with("splash_radius 0", "splash_radius -1"), 1, 18, "'splash_radius' must be >= 0");
    expect_error_in(PART_UNITS, unit_with("range 20", "range 0"), 1, 14, "'range' must be > 0");
    expect_error_in(PART_UNITS, replaced(good_units, "    cost", "\tcost"), 2, 3, "tab"); // and cost is missing
    expect_error_in(PART_UNITS, replaced(good_units, "unit Crawler", "unit 1A"), 1, 1, "not a name");
    expect_error_in(PART_UNITS, replaced(good_units, "unit Crawler", "unit A B"), 1, 1, "'unit' takes a unit_id");
    expect_error_in(PART_UNITS, " unit A\n", 1, 1, "inconsistent indentation"); // taken for a line of the rules block before it
    expect_error_in(PART_UNITS, unit_with("hp 120", "      hp 120"), 2, 11, "inconsistent indentation"); // and hp is missing
    expect_error_in(PART_UNITS, unit_with("hp 120", "  hp 120"), 2, 11, "inconsistent indentation");
    expect_error_in(PART_UNITS, "", 1, 0, "no units");
    // Keys of the old format are unknown.
    expect_error_in(PART_UNITS, unit_with("hp 120", "hp 120\n    weapon Lobber"), 1, 12, "unknown key 'weapon'");
    expect_error_in(PART_UNITS, unit_with("hp 120", "hp 120\n    muzzle 0 0.6 0.3"), 1, 12, "unknown key 'muzzle'");
    expect_error_in(PART_UNITS, unit_with("hp 120", "hp 120\n    projectile_hp 5"), 1, 12, "unknown key 'projectile_hp'"); // always empty in the wiki
    // clang-format on

    // The same unit_id twice, at the second one's line.
    char text[8192];
    snprintf(text, sizeof(text), "%s%s", good_units, good_units);
    CHECK(!read_units(text, NULL) && first_error_has(FILE_NAME ":30:") && first_error_has("unit 'Crawler' is defined twice"));

    // Every error is reported, not only the first: three lines, three logged messages.
    CHECK(!read_units(replaced(replaced(unit_with("cost 100", "cost 0"), "    hp 120\n", "    hp 0\n"), "    move_speed 5\n", "    move_speed -1\n"), NULL));
    CHECK(defs.error_count == 3);
    CHECK(log_has(FILE_NAME ":13:") && log_has(FILE_NAME ":21:") && log_has(FILE_NAME ":23:"));

    // More than 20 errors: all counted, 20 logged and a note.
    char many[2048] = "";
    for (u32 i = 0; i < 25; ++i)
        strcat(many, "bogus\n");
    CHECK(!read_units(many, NULL));
    CHECK(defs.error_count == 25);
    CHECK(nv_log_ring.count == 21 && log_has(FILE_NAME ":30:") && !log_has(FILE_NAME ":31:") && log_has("more errors"));

    // At most 16 units.
    static char seventeen[65536];
    seventeen[0] = 0;
    for (u32 i = 0; i < 17; ++i) {
        char header[32];
        snprintf(header, sizeof(header), "unit U%u\n", i);
        strcat(seventeen, replaced(good_units, "unit Crawler\n", header));
    }
    CHECK(!read_units(seventeen, NULL) && defs.error_count == 1 && defs.unit_count == 16);
    CHECK(first_error_has(FILE_NAME ":315:") && first_error_has("too many units")); // 10 + 16 * 19 + 1

    // The stage.
    expect_error_in(PART_STAGE, "supply 1000\nplace Crawler 12 33\n", 1, 2, "cell row must be 34 to 47");
    expect_error_in(PART_STAGE, "supply 1000\nplace Crawler 32 36\n", 1, 2, "cell x must be 0 to 31");
    expect_error_in(PART_STAGE, "supply 1000\nplace Nobody 12 36\n", 1, 2, "unknown unit 'Nobody'");
    expect_error_in(PART_STAGE, "supply 1000\nplace Crawler 12 36\nplace Crawler 12 36\n", 1, 3, "already has a unit");
    expect_error_in(PART_STAGE, "supply 1000\nplace Crawler 12.5 36\n", 1, 2, "not an integer");
    expect_error_in(PART_STAGE, "supply 1000\nplace Crawler 12\n", 1, 2, "'place' takes");
    expect_error_in(PART_STAGE, "place Crawler 12 36\n", 1, BLOCK_LINE, "block 'stage' is missing 'supply'");
    expect_error_in(PART_STAGE, "supply 1000\nsupply 500\nplace Crawler 12 36\n", 1, 2, "'supply' is given twice");
    expect_error_in(PART_STAGE, "supply 0\nplace Crawler 12 36\n", 1, 1, "'supply' must be >= 1 and <= 100000");
    expect_error_in(PART_STAGE, "supply 1000\nseed -1\nplace Crawler 12 36\n", 1, 2, "'seed' must be >= 0 and <= 4294967295");
    expect_error_in(PART_STAGE, "supply 1000\n", 1, BLOCK_LINE, "'stage' has no 'place' lines");
    expect_error_in(PART_STAGE, "supply 1000\nplace Crawler 12 36\n  place Crawler 13 36\n", 1, 3, "inconsistent indentation");
    expect_error_in(PART_STAGE, "supply 1000\nbogus 1\nplace Crawler 12 36\n", 1, 2, "unknown key 'bogus'");
    // Supply: the place that takes the total over it is the one reported, wherever the supply line is.
    expect_error_in(PART_STAGE, "supply 250\nplace Crawler 1 34\nplace Crawler 2 34\nplace Crawler 3 34\nplace Crawler 4 34\n", 1, 4, "over the supply");
    expect_error_in(PART_STAGE, "place Crawler 1 34\nplace Crawler 2 34\nsupply 150\n", 1, 2, "over the supply");
    CHECK(read_stage("supply 200\nplace Crawler 1 34\nplace Crawler 2 34\n")); // exactly the supply is fine

    // A long token in a message is cut before a character, not inside one (Hangul is 3 bytes a syllable; 24 is not a
    // multiple of 3 after the one ASCII byte), in the logged message and in first_error alike.
    CHECK(!read_units(unit_with("hp 120", "hp x\xEC\xB2\xB4\xEB\xA0\xA5\xEC\xB2\xB4\xEB\xA0\xA5\xEC\xB2\xB4\xEB\xA0\xA5"
                                          "\xEC\xB2\xB4\xEB\xA0\xA5\xEC\xB2\xB4\xEB\xA0\xA5"), NULL));
    CHECK(first_error_has("is not a number") && valid_utf8(defs.first_error));
    CHECK(!read_units("x\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80"
                      "\xEA\xB0\x80\xEA\xB0\x80 1\n", NULL));
    CHECK(first_error_has("unknown key") && valid_utf8(defs.first_error));
    CHECK(valid_utf8(nv_log_text(nv_log_message(0))));
}

// What a unit's keys need of each other (unit_fields.md, "Checks when reading"): the wiki's load-time checks.
internal void test_unit_field_checks(void)
{
    char text[8192];
    // A projectile attack needs its required keys; each one missing is an error at the unit's line.
    CHECK(read_units(shooter_with(""), "Crawler"));
    const char* required[] = {"initial_speed_mps 100", "forward_acceleration_mps2 0", "lifetime_s 1", "max_travel_m 100",
                              "collision_radius_m 0.1", "trajectory_mode Straight", "target_leading true"};
    for (u32 i = 0; i < NV_ARRAY_COUNT(required); ++i) {
        char from[64], key[48];
        snprintf(from, sizeof(from), "    %s\n", required[i]);
        snprintf(key, sizeof(key), "unit 'Crawler' is missing '%.*s'", (int)(strchr(required[i], ' ') - required[i]), required[i]);
        expect_error_in(PART_UNITS, replaced(shooter_with(""), from, ""), 1, 1, key);
    }
    // A melee attack has no projectile keys: each is an error at its line.
    expect_error_in(PART_UNITS, unit_with("attack_type Melee", "attack_type Melee\n    initial_speed_mps 50\n    target_leading true"), 2, 20,
                    "'initial_speed_mps' is for projectile attacks; unit 'Crawler' is Melee");
    expect_error_in(PART_UNITS, unit_with("attack_type Melee", "attack_type Melee\n    turn_rate_deg_s 90"), 1, 20, "'turn_rate_deg_s' is for projectile attacks");
    // An attack_type with an error judges none of that: one error.
    expect_error_in(PART_UNITS, unit_with("attack_type Melee", "attack_type Laser\n    initial_speed_mps 50"), 1, 19, "unknown attack_type 'Laser'");
    expect_error_in(PART_UNITS, unit_with("attack_type Melee", ""), 1, 1, "unit 'Crawler' is missing 'attack_type'");

    // Guided: its three keys are required; target_loss_behavior is not (the wiki asks only for the three).
    snprintf(text, sizeof(text), "%s", replaced(shooter_with("    turn_rate_deg_s 240\n    guidance_delay_s 0.05\n    guidance_mode PredictiveInterception\n"),
                                                "UnguidedProjectile", "GuidedProjectile"));
    CHECK(read_units(text, "Crawler") && defs.units[0].attack_type == ATTACK_GUIDED && defs.units[0].projectile.target_loss == TARGET_LOSS_NONE);
    expect_error_in(PART_UNITS, replaced(text, "    guidance_mode PredictiveInterception\n", ""), 1, 1, "unit 'Crawler' is missing 'guidance_mode'");
    expect_error_in(PART_UNITS, replaced(text, "    turn_rate_deg_s 240\n", ""), 1, 1, "unit 'Crawler' is missing 'turn_rate_deg_s'");
    expect_error_in(PART_UNITS, replaced(text, "PredictiveInterception", "Homing"), 1, 29, "unknown guidance_mode 'Homing' (PredictiveInterception)");
    CHECK(read_units(replaced(text, "    guidance_mode PredictiveInterception\n", "    guidance_mode PredictiveInterception\n    target_loss_behavior ContinueStraight\n"),
                     "Crawler") && defs.units[0].projectile.target_loss == TARGET_LOSS_CONTINUE_STRAIGHT);
    // Guidance keys on an unguided projectile are read and kept, not an error (the wiki does not make them one).
    CHECK(read_units(shooter_with("    turn_rate_deg_s 90\n    guidance_mode PredictiveInterception\n"), "Crawler"));
    CHECK(near_value(defs.units[0].projectile.turn_rate, NV_PI / 2.0f, 1e-6f));

    // A scripted arc needs its height; a straight shot has none.
    snprintf(text, sizeof(text), "%s", replaced(shooter_with("    arc_height_m 20\n"), "Straight", "ScriptedArc"));
    CHECK(read_units(text, "Crawler") && defs.units[0].projectile.trajectory == TRAJECTORY_SCRIPTED_ARC && defs.units[0].projectile.arc_height == 20.0f);
    expect_error_in(PART_UNITS, replaced(text, "    arc_height_m 20\n", ""), 1, 1, "unit 'Crawler' is missing 'arc_height_m'");
    expect_error_in(PART_UNITS, shooter_with("    arc_height_m 20\n"), 1, 27, "'arc_height_m' is only for ScriptedArc");
    expect_error_in(PART_UNITS, replaced(text, "    arc_height_m 20\n", "    arc_height_m 0\n"), 1, 27, "'arc_height_m' must be > 0");

    // target_priority only with target_layer Both; Ground when left out.
    CHECK(read_units(unit_with("target_layer Ground", "target_layer Both\n    target_priority Air"), "Crawler"));
    CHECK(defs.units[0].targets == (TARGET_GROUND | TARGET_AIR) && defs.units[0].target_priority == UNIT_LAYER_AIR);
    CHECK(read_units(unit_with("target_layer Ground", "target_layer Both"), "Crawler") && defs.units[0].target_priority == UNIT_LAYER_GROUND);
    expect_error_in(PART_UNITS, unit_with("target_layer Ground", "target_layer Air\n    target_priority Air"), 1, 7, "'target_priority' is only for target_layer Both");
    expect_error_in(PART_UNITS, unit_with("target_layer Ground", "target_layer Both\n    target_priority Sea"), 1, 7, "unknown target_priority 'Sea' (Ground or Air)");

    // The other ranges of the projectile keys.
    expect_error_in(PART_UNITS, replaced(shooter_with(""), "initial_speed_mps 100", "initial_speed_mps 0"), 1, 20, "'initial_speed_mps' must be > 0");
    expect_error_in(PART_UNITS, shooter_with("    launch_spread_deg 90\n"), 1, 27, "'launch_spread_deg' must be >= 0 and < 90");
    expect_error_in(PART_UNITS, shooter_with("    piercing_count 1.5\n"), 1, 27, "not an integer");
    expect_error_in(PART_UNITS, shooter_with("    max_speed_mps 0\n"), 1, 27, "'max_speed_mps' must be > 0");
    expect_error_in(PART_UNITS, shooter_with("    gravity_scale -1\n"), 1, 27, "'gravity_scale' must be >= 0");
    CHECK(read_units(shooter_with("    launch_spread_deg 10\n    interceptable true\n    piercing_count 2\n    aim_point_scatter_m 0.5\n"), "Crawler"));
    CHECK(near_value(defs.units[0].projectile.launch_spread, 10.0f * NV_PI / 180.0f, 1e-6f) && defs.units[0].projectile.interceptable);
    CHECK(defs.units[0].projectile.piercing_count == 2 && defs.units[0].projectile.aim_point_scatter == 0.5f);

    // The keys the round does not follow yet: a melee unit gives none; a projectile one its projectile keys (but the scatter).
    CHECK(read_units(good_units, "Crawler") && defs.unfollowed[0] == 0);
    CHECK(read_units(unit_with("splash_radius 0", "splash_radius 2"), "Crawler") && strcmp(defs.unfollowed, "splash_radius") == 0);
    CHECK(read_units(unit_with("hits_or_shots_per_attack 1", "hits_or_shots_per_attack 3"), "Crawler") && defs.unfollowed[0] == 0); // melee hits are followed
    CHECK(read_units(shooter_with("    aim_point_scatter_m 0.5\n"), "Crawler"));
    CHECK(strcmp(defs.unfollowed, "attack_type, initial_speed_mps, forward_acceleration_mps2, lifetime_s, max_travel_m, collision_radius_m, "
                                  "trajectory_mode, target_leading") == 0);
}
// Scenes. Two kinds of unit: 0 "Shooter" and 1 "Dummy", both 100 hp, radius 0.5, height 0.8, standing still, with a projectile
// attack (the interim shell) that reaches 1 m (so it never fires unless a scene sets the range). A scene sets what it needs in
// `defs` before scene_start; each test begins with scene_defs.

internal void scene_defs(void)
{
    memset(&defs, 0, sizeof(defs));
    defs.rules = (BattleRules){.cell_size = 2.0f, .grid_width = 32, .grid_length = 48, .zone_rows = 14, .round_ticks = 1800,
                               .retarget_ticks = 8, .gravity = 9.8f, .stop_fraction = 0.9f, .min_damage_fraction = 0.25f};
    defs.supply = 1000;
    defs.seed = 1;
    defs.unit_count = 2;
    const char* names[2] = {"Shooter", "Dummy"};
    for (u32 i = 0; i < 2; ++i) {
        UnitDef* def = &defs.units[i];
        snprintf(def->id, sizeof(def->id), "%s", names[i]);
        snprintf(def->name, sizeof(def->name), "%s", names[i]);
        def->cost = 100;
        def->squad_size = 1;
        def->footprint_w = 1;
        def->footprint_d = 1;
        def->hp = 100.0f;
        def->radius = 0.5f;
        def->height = 0.8f;
        def->range = 1.0f;
        def->damage = 30.0f;
        def->attack_interval = 1.5f;
        def->hits_per_attack = 1;
        def->attack_type = ATTACK_UNGUIDED;
        def->target_layer = TARGET_LAYER_GROUND;
        def->targets = TARGET_GROUND;
    }
}

// Unit 2 is a Wasp: a Dummy that flies, at 10 m, with a body 0.6 m high.
internal void scene_air_unit(void)
{
    defs.units[2] = defs.units[1];
    snprintf(defs.units[2].id, sizeof(defs.units[2].id), "Wasp");
    snprintf(defs.units[2].name, sizeof(defs.units[2].name), "Wasp");
    defs.units[2].layer = UNIT_LAYER_AIR;
    defs.units[2].altitude = 10.0f;
    defs.units[2].height = 0.6f;
    defs.unit_count = 3;
}

internal void give_shield(u32 def, f32 shield_hp)
{
    defs.units[def].shield_hp = shield_hp;
}

// Units 1..player_count are the player's (one per cell of row 0), the enemy's follow (one per cell of row 34); `*_defs` are
// their unit defs. Starts the battle. Slots: the player's first.
internal void scene_start(const u8* player_defs, u32 player_count, const u8* enemy_defs, u32 enemy_count)
{
    defs.enemy_count = enemy_count;
    for (u32 i = 0; i < enemy_count; ++i)
        defs.enemy[i] = (StagePlace){enemy_defs[i], (u8)i, (u8)battle_enemy_first_row(&defs.rules)};
    battle_init(&battle, &defs);
    for (u32 i = 0; i < player_count; ++i)
        CHECK(battle_place(&battle, player_defs[i], (s32)i, 0));
    CHECK(battle_start(&battle));
}

internal void put(u32 unit, f32 x, f32 z)
{
    battle.units[unit].position = battle.units[unit].previous_position = nv_vec3(x, 0.0f, z);
}

internal void run(u32 ticks)
{
    for (u32 i = 0; i < ticks; ++i)
        battle_tick(&battle);
}

internal u32 events_of(BattleEventKind kind)
{
    u32 count = 0;
    for (u32 i = 0; i < battle.event_count; ++i)
        count += battle.events[i].kind == kind;
    return count;
}

// A shell put into the battle by hand.
internal void inject_shell(u32 team, NvVec3 position, NvVec3 velocity, f32 damage)
{
    NV_ASSERT(battle.projectile_count < BATTLE_MAX_PROJECTILES);
    battle.projectiles[battle.projectile_count++] = (Projectile){.team = (u8)team, .shooter = {1}, .position = position,
                                                                 .previous_position = position, .velocity = velocity, .damage = damage,
                                                                 .targets = TARGET_GROUND | TARGET_AIR};
}

// A fast player shell that crosses `x`, `z` from 4.5 m before it, level at `y`, in two ticks (3 m per tick).
internal void inject_at(f32 x, f32 z, f32 y, f32 damage)
{
    inject_shell(TEAM_PLAYER, nv_vec3(x, y, z - 4.5f), nv_vec3(0.0f, 0.0f, 90.0f), damage);
}

global const u8 one_shooter[] = {0};
global const u8 one_dummy[] = {1};

internal void test_deployment(void)
{
    scene_defs();
    defs.enemy_count = 1;
    defs.enemy[0] = (StagePlace){0, 5, 40};
    battle_init(&battle, &defs);
    CHECK(battle.phase == BATTLE_DEPLOY && battle.unit_count == 1 && battle.units[1].team == TEAM_ENEMY);
    CHECK(!battle_start(&battle)); // nothing of the player's

    // The supply buys ten units of cost 100; the eleventh does not fit; removing one gives its supply back.
    for (s32 i = 0; i < 10; ++i)
        CHECK(battle_place(&battle, 0, i, 3));
    CHECK(battle_supply_left(&battle) == 0 && battle.unit_count == 11);
    CHECK(!battle_place(&battle, 0, 10, 3));
    CHECK(!battle_place(&battle, 0, 0, 3)); // occupied
    CHECK(battle_remove(&battle, 4, 3) && battle_supply_left(&battle) == 100 && battle.unit_count == 10);
    CHECK(!battle_remove(&battle, 4, 3)); // already empty
    CHECK(!battle_place(&battle, 2, 4, 3)); // no such unit
    CHECK(!battle_place(&battle, 0, 4, 14) && !battle_place(&battle, 0, 32, 3) && !battle_place(&battle, 0, -1, 3)); // outside the zone
    CHECK(battle_place(&battle, 1, 4, 13) && battle_supply_left(&battle) == 0);

    // The units are the placement then the stage: the player's in row order, then the enemy's, at their cell centers.
    CHECK(battle.unit_count == 11);
    CHECK(battle.units[1].team == TEAM_PLAYER && battle.units[1].position.x == 1.0f && battle.units[1].position.z == 7.0f);
    CHECK(battle.units[4].position.x == 7.0f);                     // x = 3
    CHECK(battle.units[5].position.x == 11.0f);                    // x = 5 (4 was removed)
    CHECK(battle.units[10].def == 1 && battle.units[10].position.x == 9.0f && battle.units[10].position.z == 27.0f); // row 13: the last
    CHECK(battle.units[11].team == TEAM_ENEMY && battle.units[11].position.x == 11.0f && battle.units[11].position.z == 81.0f);
    CHECK(battle.units[1].yaw == 0.0f && near_value(battle.units[11].yaw, NV_PI, 1e-6f)); // facing each other
    CHECK(battle.units[1].health == 100.0f && battle.units[1].target.index == 0);

    // Retry goes back to deployment with the same placement and fresh units; Reset clears the placement.
    CHECK(battle_start(&battle) && battle.phase == BATTLE_FIGHT);
    CHECK(!battle_place(&battle, 0, 4, 3) && !battle_remove(&battle, 0, 3)); // not while fighting
    run(3);
    battle.units[1].health = 1.0f;
    battle_retry(&battle);
    CHECK(battle.phase == BATTLE_DEPLOY && battle.tick == 0 && battle.unit_count == 11 && battle.units[1].health == 100.0f);
    CHECK(battle.units[1].position.z == 7.0f);
    battle_clear_placement(&battle);
    CHECK(battle.unit_count == 1 && battle_supply_left(&battle) == 1000 && !battle_start(&battle));
}

internal void test_targets_and_movement(void)
{
    // The nearest enemy is picked; equal distances go to the lower slot.
    scene_defs();
    u8 enemies[3] = {1, 1, 1};
    scene_start(one_shooter, 1, enemies, 3);
    put(1, 10.0f, 10.0f);
    put(2, 10.0f, 30.0f);
    put(3, 10.0f, 20.0f);
    put(4, 14.0f, 30.0f); // none of these move: the units stand still
    run(1);
    CHECK(battle.units[1].target.index == 3);
    // Distances are on the ground: height does not count.
    battle.units[2].position.y = 100.0f;
    put(2, 10.0f, 20.0f);
    battle.units[2].position.y = 100.0f; // as near as slot 3 on the ground, much higher
    battle.tick = 8;                     // a tick that picks again
    run(1);
    CHECK(battle.units[1].target.index == 2); // the tie goes to the lower slot

    // Targets are picked again every 8 ticks, and at once when the target dies.
    scene_start(one_shooter, 1, enemies, 3);
    put(1, 10.0f, 10.0f);
    put(2, 10.0f, 20.0f);
    put(3, 10.0f, 30.0f);
    put(4, 14.0f, 40.0f);
    run(1);
    CHECK(battle.units[1].target.index == 2);
    put(3, 10.0f, 15.0f); // now nearest
    for (u32 i = 1; i < 8; ++i) {
        run(1);
        CHECK(battle.units[1].target.index == 2); // ticks 1 to 7: unchanged
    }
    run(1); // the tick numbered 8
    CHECK(battle.units[1].target.index == 3);
    battle.units[3].flags |= UNIT_DEAD;
    run(1); // not a tick that picks again, but the target is dead
    CHECK(battle.units[1].target.index == 2);

    // Units walk straight at their target and stop at 90% of their range, facing it.
    scene_defs();
    for (u32 i = 0; i < 2; ++i) {
        defs.units[i].move_speed = 5.0f;
        defs.units[i].range = 20.0f;
        defs.units[i].damage = 0.0f;
    }
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 20.0f, 10.0f);
    put(2, 20.0f, 50.0f);
    run(1);
    CHECK(near_value(battle.units[1].position.z, 10.0f + 5.0f / 30.0f, 1e-4f) && battle.units[1].velocity.z > 4.99f);
    CHECK(near_value(battle.units[1].yaw, 0.0f, 1e-6f) && near_value(fabsf(battle.units[2].yaw), NV_PI, 1e-5f)); // faces its way
    run(300);
    f32 gap = battle.units[2].position.z - battle.units[1].position.z;
    CHECK(gap <= 18.001f && gap > 17.6f); // 0.9 of 20 m; the two close in together, so it may end a step short
    CHECK(battle.units[1].velocity.z == 0.0f && battle.units[2].velocity.z == 0.0f);
    CHECK(near_value(battle.units[1].yaw, 0.0f, 1e-5f) && near_value(fabsf(battle.units[2].yaw), NV_PI, 1e-5f));
    CHECK(battle.units[1].position.x == 20.0f && battle.units[2].position.x == 20.0f); // straight

    // Overlapping units push each other apart, half the overlap each; on one spot they part along X.
    scene_defs();
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 20.0f, 20.0f);
    put(2, 20.4f, 20.0f);
    run(1);
    CHECK(near_value(battle.units[1].position.x, 19.7f, 1e-4f) && near_value(battle.units[2].position.x, 20.7f, 1e-4f));
    put(1, 20.0f, 20.0f);
    put(2, 20.0f, 20.0f);
    run(1);
    CHECK(near_value(battle.units[1].position.x, 19.5f, 1e-4f) && near_value(battle.units[2].position.x, 20.5f, 1e-4f));
    // The field's edge holds a unit in, by its radius.
    put(1, 0.1f, 0.2f);
    put(2, 63.9f, 95.9f);
    run(1);
    CHECK(near_value(battle.units[1].position.x, 0.5f, 1e-5f) && near_value(battle.units[1].position.z, 0.5f, 1e-5f));
    CHECK(near_value(battle.units[2].position.x, 63.5f, 1e-5f) && near_value(battle.units[2].position.z, 95.5f, 1e-5f));
}

internal void test_weapon(void)
{
    // The attack interval (1.5 s = 45 ticks), and where the shell starts: a shot at ticks 0, 45 and 90.
    scene_defs();
    defs.units[0].range = 30.0f;
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 33.0f, 9.0f);
    put(2, 33.0f, 24.0f);
    u32 fire_ticks[8] = {0};
    u32 fires = 0;
    for (u32 i = 0; i < 100; ++i) {
        u32 tick = battle.tick;
        battle.event_count = 0;
        run(1);
        if (events_of(BATTLE_EVENT_FIRE) && fires < 8)
            fire_ticks[fires++] = tick;
        if (tick == 0) {
            CHECK(battle.event_count == 1 && battle.events[0].kind == BATTLE_EVENT_FIRE);
            CHECK(near_value(battle.events[0].position.x, 33.0f, 1e-5f) && near_value(battle.events[0].position.y, 0.4f, 1e-5f)
                  && near_value(battle.events[0].position.z, 9.0f, 1e-4f)); // the middle of the unit's body
            CHECK(battle.events[0].team == TEAM_PLAYER && battle.events[0].direction.z > 0.0f && battle.events[0].direction.y > 0.0f);
        }
    }
    CHECK(fires == 3 && fire_ticks[0] == 0 && fire_ticks[1] == 45 && fire_ticks[2] == 90);

    // Out of range it does not fire; in range it does.
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 33.0f, 9.0f);
    put(2, 33.0f, 39.1f);
    run(1);
    CHECK(battle.projectile_count == 0);
    put(2, 33.0f, 38.9f);
    run(1);
    CHECK(battle.projectile_count == 1);

    // A scatter of 0 lands on the target (to the float's accuracy); a scatter of 0.5 lands within 0.5 of it, and not always on
    // it. The target is tiny so that no shell touches it: the landing is where the shell's arc meets the ground.
    for (u32 pass = 0; pass < 2; ++pass) {
        f32 spread = pass ? 0.5f : 0.0f;
        f32 farthest = 0.0f, nearest = 100.0f;
        f32 sum_x = 0.0f;
        for (u32 seed = 1; seed <= 40; ++seed) {
            scene_defs();
            defs.seed = seed;
            defs.units[0].range = 30.0f;
            defs.units[0].attack_interval = 1000.0f; // one shot
            defs.units[0].projectile.aim_point_scatter = spread;
            defs.units[1].radius = 0.001f;
            defs.units[1].height = 0.001f;
            scene_start(one_shooter, 1, one_dummy, 1);
            put(1, 33.0f, 9.0f);
            put(2, 33.0f, 24.0f);
            Projectile last = {0};
            b32 landed = false;
            for (u32 i = 0; i < 200 && !landed; ++i) {
                if (battle.projectile_count == 1)
                    last = battle.projectiles[0];
                run(1);
                landed = last.damage > 0.0f && battle.projectile_count == 0;
            }
            CHECK(landed);
            // The arc from the last position seen: y0 + vy t - g t^2 / 2 = 0.
            f32 t = (last.velocity.y + sqrtf(last.velocity.y * last.velocity.y + 2.0f * defs.rules.gravity * last.position.y)) / defs.rules.gravity;
            f32 x = last.position.x + last.velocity.x * t - 33.0f, z = last.position.z + last.velocity.z * t - 24.0f;
            f32 offset = sqrtf(x * x + z * z);
            farthest = fmaxf(farthest, offset);
            nearest = fminf(nearest, offset);
            sum_x += x;
            if (pass == 1)
                CHECK(battle.units[2].health == 100.0f); // with no spread the shell lands on the target's axis, inside it
        }
        if (pass == 0) {
            CHECK(farthest < 0.02f);
        } else {
            CHECK(farthest <= 0.5f + 0.01f);
            CHECK(farthest > 0.35f); // 40 draws in a disk of radius 0.5 reach near the edge
            CHECK(nearest < 0.25f);
            CHECK(fabsf(sum_x / 40.0f) < 0.15f); // and are not all on one side
        }
    }

    // A shell that lands on the target hurts it: a spread of 0 and a target of size.
    scene_defs();
    defs.units[0].range = 30.0f;
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 33.0f, 9.0f);
    put(2, 33.0f, 24.0f);
    run(70);
    CHECK(battle.units[2].health == 70.0f); // one 30 damage hit, no armor (the second shell, fired at tick 45, is still in the air)
    CHECK(events_of(BATTLE_EVENT_HIT) == 1);
}

// Where a shell starts: the middle of the shooter's body, an air shooter's at its altitude. The shell flies at the interim angle.
internal void test_shell_start(void)
{
    scene_defs();
    scene_air_unit();
    defs.units[2].range = 30.0f;
    u8 wasp_player[1] = {2};
    scene_start(wasp_player, 1, one_dummy, 1);
    put(1, 33.0f, 9.0f);
    battle.units[1].position.y = battle.units[1].previous_position.y = 10.0f;
    put(2, 33.0f, 24.0f);
    run(1);
    CHECK(events_of(BATTLE_EVENT_FIRE) == 1);
    CHECK(near_value(battle.events[0].position.y, 10.3f, 1e-5f) && near_value(battle.events[0].position.z, 9.0f, 1e-5f)); // 10 m + 0.6 / 2
    // Up at 45 degrees on the ground: as much up as along.
    scene_defs();
    defs.units[0].range = 30.0f;
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 33.0f, 9.0f);
    put(2, 33.0f, 24.0f);
    run(1);
    NvVec3 direction = battle.events[0].direction;
    CHECK(near_value(direction.y, direction.z, 0.02f) && direction.z > 0.0f);
}

// Melee (D-13): no shell; a strike at once on a target within range, hits_per_attack hits of the damage, shield first, then armor.
internal void test_melee(void)
{
    scene_defs();
    defs.units[0].attack_type = ATTACK_MELEE;
    defs.units[0].range = 2.0f;
    defs.units[0].hits_per_attack = 2;
    defs.units[0].attack_interval = 0.7f; // 21 ticks
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 20.0f, 20.0f);
    put(2, 20.0f, 21.5f);
    run(1);
    CHECK(battle.projectile_count == 0 && events_of(BATTLE_EVENT_FIRE) == 0);
    CHECK(events_of(BATTLE_EVENT_STRIKE) == 1 && events_of(BATTLE_EVENT_HIT) == 2);
    CHECK(battle.units[2].health == 40.0f); // two hits of 30, applied at the tick's end
    CHECK(battle.events[0].kind == BATTLE_EVENT_STRIKE && battle.events[0].team == TEAM_PLAYER && battle.events[0].direction.z > 0.99f);
    CHECK(near_value(battle.events[0].position.y, 0.4f, 1e-5f) && near_value(battle.events[0].position.z, 21.5f, 1e-5f)); // the target's middle
    // The next strike comes after the attack interval: the target dies on it.
    battle.event_count = 0;
    run(20);
    CHECK(events_of(BATTLE_EVENT_STRIKE) == 0 && battle.units[2].health == 40.0f);
    run(1);
    CHECK(events_of(BATTLE_EVENT_STRIKE) == 1 && (battle.units[2].flags & UNIT_DEAD) && battle.phase == BATTLE_RESULT);

    // Out of range, no strike; armor takes off each hit.
    scene_defs();
    defs.units[0].attack_type = ATTACK_MELEE;
    defs.units[0].range = 2.0f;
    defs.units[1].armor = 5.0f;
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 20.0f, 20.0f);
    put(2, 20.0f, 22.1f);
    run(1);
    CHECK(events_of(BATTLE_EVENT_STRIKE) == 0 && battle.units[2].health == 100.0f);
    put(2, 20.0f, 21.9f);
    battle.event_count = 0;
    run(1);
    CHECK(events_of(BATTLE_EVENT_STRIKE) == 1 && battle.units[2].health == 75.0f);

    // A shield takes each hit whole while it has any: 30 of 40, then the 10 left of the second.
    scene_defs();
    defs.units[0].attack_type = ATTACK_MELEE;
    defs.units[0].range = 2.0f;
    defs.units[0].hits_per_attack = 2;
    give_shield(1, 40.0f);
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 20.0f, 20.0f);
    put(2, 20.0f, 21.5f);
    run(1);
    CHECK(battle.units[2].shield == 0.0f && battle.units[2].health == 100.0f && events_of(BATTLE_EVENT_SHIELD_HIT) == 2);
    CHECK(events_of(BATTLE_EVENT_HIT) == 0);
}

internal void test_lead_aim(void)
{
    // A target walking straight at the shooter: aiming where it is would land the shell ten meters behind it.
    scene_defs();
    defs.units[0].range = 30.0f;
    defs.units[0].attack_interval = 1000.0f; // one shot
    defs.units[1].move_speed = 5.0f;
    defs.units[1].hp = 1000.0f;
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 33.0f, 10.0f);
    put(2, 33.0f, 45.0f);
    u32 ticks = 0;
    while (battle.projectile_count == 0 && ticks++ < 200)
        run(1);
    CHECK(ticks < 200 && battle.units[2].velocity.z < -4.99f);
    while (battle.projectile_count > 0 && ticks++ < 400)
        run(1);
    CHECK(events_of(BATTLE_EVENT_HIT) == 1);
    CHECK(battle.units[2].health == 970.0f);
}

// Shells by hand

internal void test_damage(void)
{
    // Damage is max(damage - armor, damage * 0.25): armor 5 takes 30 to 25; armor 28 takes 30 to 7.5.
    scene_defs();
    defs.units[1].armor = 5.0f;
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 10.0f, 10.0f);
    put(2, 10.0f, 30.0f);
    inject_at(10.0f, 30.0f, 0.4f, 30.0f);
    run(1);
    CHECK(battle.projectile_count == 1 && battle.units[2].health == 100.0f); // not there yet
    run(1);
    CHECK(battle.projectile_count == 0 && battle.units[2].health == 75.0f && events_of(BATTLE_EVENT_HIT) == 1);
    CHECK(battle.events[0].kind == BATTLE_EVENT_HIT && battle.events[0].size == 25.0f && battle.events[0].team == TEAM_ENEMY);

    defs.units[1].armor = 28.0f;
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 10.0f, 10.0f);
    put(2, 10.0f, 30.0f);
    inject_at(10.0f, 30.0f, 0.4f, 30.0f);
    run(2);
    CHECK(battle.units[2].health == 92.5f);

    // A shell on the ground vanishes without damage. A shell hurts the first enemy it touches and then is gone, so a second
    // enemy behind the first is not hurt; a tick's hits add up, and the unit dies at the tick's end.
    scene_defs();
    u8 enemies[2] = {1, 1};
    scene_start(one_shooter, 1, enemies, 2);
    put(1, 10.0f, 10.0f);
    put(2, 20.0f, 30.0f);
    put(3, 20.0f, 31.2f); // behind slot 2 along the shell's way
    inject_shell(TEAM_PLAYER, nv_vec3(20.0f, 0.4f, 25.5f), nv_vec3(0.0f, 0.0f, 90.0f), 40.0f);
    inject_shell(TEAM_PLAYER, nv_vec3(20.0f, 0.4f, 25.5f), nv_vec3(0.0f, 0.0f, 90.0f), 70.0f);
    run(2);
    CHECK(battle.projectile_count == 0 && battle.units[3].health == 100.0f);
    CHECK(battle.units[2].health == 0.0f && (battle.units[2].flags & UNIT_DEAD) && events_of(BATTLE_EVENT_DEATH) == 1);
    inject_shell(TEAM_PLAYER, nv_vec3(40.0f, 0.1f, 40.0f), nv_vec3(0.0f, -20.0f, 0.0f), 30.0f);
    run(1);
    CHECK(battle.projectile_count == 0 && battle.units[3].health == 100.0f);

    // A shell passes allies: a player's shell goes through a player unit that stands in its way.
    scene_defs();
    defs.units[0].height = 2.0f;
    defs.units[0].radius = 0.3f;
    u8 players[2] = {0, 0};
    scene_start(players, 2, one_dummy, 1);
    put(1, 50.0f, 10.0f); // the ally in the way
    put(2, 10.0f, 10.0f);
    put(3, 50.0f, 11.0f);
    inject_shell(TEAM_PLAYER, nv_vec3(50.0f, 0.4f, 5.5f), nv_vec3(0.0f, 0.0f, 90.0f), 30.0f);
    run(3);
    CHECK(battle.units[1].health == 100.0f && battle.units[3].health == 70.0f);

    // A shell that moves fast enough to cross a whole unit between two ticks still hits it (the segment is tested).
    scene_defs();
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 10.0f, 10.0f);
    put(2, 40.0f, 40.0f);
    inject_shell(TEAM_PLAYER, nv_vec3(40.0f, 0.4f, 38.6f), nv_vec3(0.0f, 0.0f, 90.0f), 30.0f); // 38.6 to 41.6 in a tick
    run(1);
    CHECK(battle.units[2].health == 70.0f);
    // ... and a shell passing above or beside a unit does not.
    inject_shell(TEAM_PLAYER, nv_vec3(40.0f, 1.0f, 38.6f), nv_vec3(0.0f, 0.0f, 90.0f), 30.0f);
    inject_shell(TEAM_PLAYER, nv_vec3(40.6f, 0.4f, 38.6f), nv_vec3(0.0f, 0.0f, 90.0f), 30.0f);
    run(1);
    CHECK(battle.units[2].health == 70.0f && battle.projectile_count == 2);
}

// The shield (D-05, D-10): a unit's own shield HP, in front of its health. It takes a whole hit while it has any (no armor,
// nothing spills over), it does not refill in a round, and it guards only its unit.
internal void test_shield(void)
{
    // A shield takes a shell's damage and the shell is gone; the unit starts with its shield full.
    scene_defs();
    give_shield(1, 60.0f);
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 10.0f, 10.0f);
    put(2, 10.0f, 30.0f);
    CHECK(battle.units[2].shield == 60.0f && battle.units[1].shield == 0.0f);
    inject_at(10.0f, 30.0f, 0.4f, 30.0f);
    run(2);
    CHECK(battle.units[2].shield == 30.0f && battle.units[2].health == 100.0f && battle.projectile_count == 0);
    CHECK(events_of(BATTLE_EVENT_SHIELD_HIT) == 1 && events_of(BATTLE_EVENT_HIT) == 0);
    CHECK(battle.events[0].size == 30.0f && battle.events[0].team == TEAM_ENEMY);
    // It is hit where the shell meets the unit's body: its radius before the unit's center on this line.
    CHECK(near_value(battle.events[0].position.z, 30.0f - 0.5f, 1e-3f));

    // A hit larger than what is left: the shield takes it whole and is empty, and nothing goes on to the health.
    defs.units[1].armor = 5.0f;
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 10.0f, 10.0f);
    put(2, 10.0f, 30.0f);
    battle.units[2].shield = 10.0f;
    inject_at(10.0f, 30.0f, 0.4f, 100.0f);
    run(2);
    CHECK(battle.units[2].shield == 0.0f && battle.units[2].health == 100.0f && battle.projectile_count == 0);
    CHECK(events_of(BATTLE_EVENT_SHIELD_HIT) == 1 && events_of(BATTLE_EVENT_HIT) == 0 && battle.events[0].size == 10.0f);
    // Armor does not take off a shield's hit.
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 10.0f, 10.0f);
    put(2, 10.0f, 30.0f);
    inject_at(10.0f, 30.0f, 0.4f, 30.0f);
    run(2);
    CHECK(battle.units[2].shield == 30.0f);

    // At zero the health takes the hit, with armor.
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 10.0f, 10.0f);
    put(2, 10.0f, 30.0f);
    battle.units[2].shield = 0.0f;
    inject_at(10.0f, 30.0f, 0.4f, 30.0f);
    run(2);
    CHECK(battle.units[2].health == 75.0f && events_of(BATTLE_EVENT_SHIELD_HIT) == 0 && events_of(BATTLE_EVENT_HIT) == 1);

    // It does not refill in the round, however long; Retry gives it back full.
    run(600);
    CHECK(battle.units[2].shield == 0.0f);
    battle_retry(&battle);
    CHECK(battle.units[2].shield == 60.0f);

    // It guards only its own unit: a shell that passes beside it hits the unit behind.
    scene_defs();
    give_shield(1, 60.0f);
    u8 enemies[2] = {1, 0}; // a Dummy with a shield, and a Shooter without one, beside it
    scene_start(one_shooter, 1, enemies, 2);
    put(1, 10.0f, 10.0f);
    put(2, 20.0f, 30.0f);
    put(3, 21.0f, 30.0f);
    inject_at(21.0f, 30.0f, 0.4f, 30.0f);
    run(3);
    CHECK(battle.units[3].health == 70.0f && battle.units[2].shield == 60.0f);
}

internal void test_round_end(void)
{
    // A side wiped out ends the round.
    scene_defs();
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 10.0f, 10.0f);
    put(2, 10.0f, 30.0f);
    inject_at(10.0f, 30.0f, 0.4f, 100.0f);
    run(1);
    CHECK(battle.phase == BATTLE_FIGHT);
    run(1);
    CHECK(battle.phase == BATTLE_RESULT && battle.outcome == OUTCOME_PLAYER && battle.tick == 2);
    CHECK(battle_alive_count(&battle, TEAM_ENEMY) == 0 && battle_alive_count(&battle, TEAM_PLAYER) == 1);
    u32 hash = battle_hash(&battle);
    run(10); // nothing runs after the round
    CHECK(battle.tick == 2 && battle_hash(&battle) == hash);

    // The enemy wins by wiping the player out; both sides wiped out on one tick is a draw.
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 10.0f, 10.0f);
    put(2, 10.0f, 30.0f);
    inject_shell(TEAM_ENEMY, nv_vec3(10.0f, 0.4f, 5.5f), nv_vec3(0.0f, 0.0f, 90.0f), 100.0f);
    run(2);
    CHECK(battle.outcome == OUTCOME_ENEMY);
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 10.0f, 10.0f);
    put(2, 10.0f, 30.0f);
    inject_shell(TEAM_ENEMY, nv_vec3(10.0f, 0.4f, 5.5f), nv_vec3(0.0f, 0.0f, 90.0f), 100.0f);
    inject_at(10.0f, 30.0f, 0.4f, 100.0f);
    run(2);
    CHECK(battle.outcome == OUTCOME_DRAW && battle.phase == BATTLE_RESULT);
    CHECK(events_of(BATTLE_EVENT_DEATH) == 2);

    // At 60 s (1800 ticks) the round ends; the side with more remaining value (cost * health / max health) wins.
    scene_defs();
    const u32 round_ticks = defs.rules.round_ticks;
    CHECK(round_ticks == 1800);
    u8 two[2] = {0, 0};
    scene_start(two, 2, one_dummy, 1);
    run(round_ticks - 1);
    CHECK(battle.phase == BATTLE_FIGHT);
    run(1);
    CHECK(battle.phase == BATTLE_RESULT && battle.tick == round_ticks && battle.outcome == OUTCOME_PLAYER); // 200 against 100
    CHECK(battle_remaining_value(&battle, TEAM_PLAYER) == 200.0f && battle_remaining_value(&battle, TEAM_ENEMY) == 100.0f);

    scene_start(two, 1, one_dummy, 1);
    run(round_ticks);
    CHECK(battle.outcome == OUTCOME_DRAW); // 100 against 100

    scene_start(two, 1, one_dummy, 1);
    battle.units[1].health = 50.0f;
    run(round_ticks);
    CHECK(battle.outcome == OUTCOME_ENEMY && battle_remaining_value(&battle, TEAM_PLAYER) == 50.0f);

    // Dead units do not count.
    scene_start(two, 2, one_dummy, 1);
    battle.units[2].flags |= UNIT_DEAD;
    run(round_ticks);
    CHECK(battle.outcome == OUTCOME_DRAW);
}

// The rules in use: each rule of the project file changes what a round does, and not only what the file says.

internal void test_rules_in_use(void)
{
    // The cell size and the grid: another field, its cells and its zones. A 10 x 20 grid of 3 m cells with 4-row zones is
    // 30 m by 60 m; the enemy's rows are 16 to 19.
    scene_defs();
    defs.rules.cell_size = 3.0f;
    defs.rules.grid_width = 10;
    defs.rules.grid_length = 20;
    defs.rules.zone_rows = 4;
    CHECK(battle_enemy_first_row(&defs.rules) == 16);
    defs.enemy_count = 1;
    defs.enemy[0] = (StagePlace){1, 5, 18};
    battle_init(&battle, &defs);
    CHECK(battle_place(&battle, 0, 1, 0) && battle_place(&battle, 0, 9, 3));
    CHECK(!battle_place(&battle, 0, 10, 0) && !battle_place(&battle, 0, 0, 4) && !battle_remove(&battle, 10, 0)); // outside the zone
    CHECK(!battle_can_place(&battle, 0, 0, 14)); // a cell of the old zone, now outside the grid
    CHECK(battle.units[1].position.x == 4.5f && battle.units[1].position.z == 1.5f); // (1 + 0.5) * 3
    CHECK(battle.units[2].position.x == 28.5f && battle.units[2].position.z == 10.5f);
    CHECK(battle.units[3].team == TEAM_ENEMY && battle.units[3].position.x == 16.5f && battle.units[3].position.z == 55.5f);
    // The field's edge holds a unit in: 30 m by 60 m.
    CHECK(battle_start(&battle));
    put(1, -5.0f, -5.0f);
    put(2, 99.0f, 99.0f);
    run(1);
    CHECK(near_value(battle.units[1].position.x, 0.5f, 1e-5f) && near_value(battle.units[1].position.z, 0.5f, 1e-5f));
    CHECK(near_value(battle.units[2].position.x, 29.5f, 1e-5f) && near_value(battle.units[2].position.z, 59.5f, 1e-5f));
    // The smallest field: 1 x 2 cells, with a zone of one row each side.
    scene_defs();
    defs.rules.grid_width = 1;
    defs.rules.grid_length = 2;
    defs.rules.zone_rows = 1;
    defs.enemy_count = 1;
    defs.enemy[0] = (StagePlace){1, 0, 1};
    battle_init(&battle, &defs);
    CHECK(battle_place(&battle, 0, 0, 0) && !battle_place(&battle, 0, 0, 1) && !battle_place(&battle, 0, 1, 0));
    CHECK(battle_start(&battle));
    run(300);
    CHECK(isfinite(battle.units[1].position.z) && battle.units[1].position.z >= 0.5f && battle.units[2].position.z <= 3.5f);

    // The round time: a round of 10 s ends at tick 300 and not before.
    scene_defs();
    defs.rules.round_ticks = 300;
    u8 two[2] = {0, 0};
    scene_start(two, 2, one_dummy, 1);
    run(299);
    CHECK(battle.phase == BATTLE_FIGHT);
    run(1);
    CHECK(battle.phase == BATTLE_RESULT && battle.tick == 300 && battle.outcome == OUTCOME_PLAYER);

    // The retarget interval: targets are picked again every 30 ticks when it is 1 s.
    scene_defs();
    defs.rules.retarget_ticks = 30;
    u8 enemies[2] = {1, 1};
    scene_start(one_shooter, 1, enemies, 2);
    put(1, 10.0f, 10.0f);
    put(2, 10.0f, 20.0f);
    put(3, 10.0f, 30.0f);
    run(1);
    CHECK(battle.units[1].target.index == 2);
    put(3, 10.0f, 15.0f); // now nearest
    run(28);              // ticks 1 to 28: unchanged
    CHECK(battle.units[1].target.index == 2);
    run(2);               // the tick numbered 30
    CHECK(battle.units[1].target.index == 3);

    // The stop fraction: units walking at each other stop at half the range when it is 0.5.
    scene_defs();
    defs.rules.stop_fraction = 0.5f;
    for (u32 i = 0; i < 2; ++i) {
        defs.units[i].move_speed = 5.0f;
        defs.units[i].range = 20.0f;
        defs.units[i].damage = 0.0f;
    }
    scene_start(one_shooter, 1, one_dummy, 1);
    put(1, 20.0f, 10.0f);
    put(2, 20.0f, 50.0f);
    run(300);
    f32 gap = battle.units[2].position.z - battle.units[1].position.z;
    CHECK(gap <= 10.001f && gap > 9.6f);

    // Gravity: a shell is solved to land on its aim point whatever the pull, so it does, and flies longer under less.
    u32 flight[2] = {0, 0};
    for (u32 pass = 0; pass < 2; ++pass) {
        scene_defs();
        defs.rules.gravity = pass ? 4.9f : 19.6f;
        defs.units[0].range = 30.0f;
        defs.units[0].attack_interval = 1000.0f;
        defs.units[1].radius = 0.001f;
        defs.units[1].height = 0.001f;
        scene_start(one_shooter, 1, one_dummy, 1);
        put(1, 33.0f, 9.0f);
        put(2, 33.0f, 24.0f);
        Projectile last = {0};
        for (u32 i = 0; i < 400 && !(last.damage > 0.0f && battle.projectile_count == 0); ++i) {
            if (battle.projectile_count == 1) {
                last = battle.projectiles[0];
                ++flight[pass];
            }
            run(1);
        }
        f32 t = (last.velocity.y + sqrtf(last.velocity.y * last.velocity.y + 2.0f * defs.rules.gravity * last.position.y)) / defs.rules.gravity;
        f32 x = last.position.x + last.velocity.x * t - 33.0f, z = last.position.z + last.velocity.z * t - 24.0f;
        CHECK(sqrtf(x * x + z * z) < 0.02f);
    }
    CHECK(flight[1] > flight[0] * 3 / 2); // a quarter of the pull: twice the time in the air

    // The minimum damage fraction: armor 28 against a 30 shell leaves max(2, 30 * fraction).
    for (u32 pass = 0; pass < 2; ++pass) {
        scene_defs();
        defs.rules.min_damage_fraction = pass ? 1.0f : 0.0f;
        defs.units[1].armor = 28.0f;
        scene_start(one_shooter, 1, one_dummy, 1);
        put(1, 10.0f, 10.0f);
        put(2, 10.0f, 30.0f);
        inject_at(10.0f, 30.0f, 0.4f, 30.0f);
        run(2);
        CHECK(battle.units[2].health == (pass ? 70.0f : 98.0f)); // 1: armor takes nothing off; 0: armor takes its 28
    }
}

// Squads (docs/specs/battle.md, "Squads"; unit_fields.md, "Rectangular footprints"): squad_size, footprint_w and footprint_d, a
// squad placed on its footprint, where its units stand (the wiki's D-07), the limits, and a round of squads.

// A project with squads: `units` as given, then a stage of `stage`'s lines (indented here). The units start on line 11.
internal b32 read_squad_project(const char* units, const char* stage)
{
    project_text[0] = 0;
    append_indented("abproj_version 1\n", 0);
    append_indented(strstr(good_header, "rules"), 0);
    append_indented(units, 0);
    append_indented("stage\n", 0);
    append_indented(stage, 4);
    return read_project(project_text);
}

// good_units as a squad of 4 on 2 x 2 cells. Its lines are those of good_units from 11 on: squad_size is 14, footprint_w 17,
// footprint_d 18, radius_m 19; the stage is 30, its first line 31.
internal const char* squad_unit(void)
{
    static char text[4096];
    snprintf(text, sizeof(text), "%s",
             replaced(replaced(replaced(good_units, "squad_size 1\n", "squad_size 4\n"), "footprint_w 1\n", "footprint_w 2\n"), "footprint_d 1\n",
                      "footprint_d 2\n"));
    return text;
}
#define SQUAD_STAGE "supply 1000\nplace Crawler 12 36\n"

internal b32 first_error_at(u32 line, const char* message)
{
    char prefix[48];
    snprintf(prefix, sizeof(prefix), "%s:%u:", FILE_NAME, line);
    b32 ok = strncmp(defs.first_error, prefix, strlen(prefix)) == 0 && first_error_has(message);
    if (!ok)
        printf("wanted \"%s ... %s\", got \"%s\" (%u error(s))\n", prefix, message, defs.first_error, defs.error_count);
    return ok;
}

internal void test_squad_file(void)
{
    char text[8192];
    CHECK(read_squad_project(squad_unit(), SQUAD_STAGE));
    CHECK(defs.units[0].squad_size == 4 && defs.units[0].footprint_w == 2 && defs.units[0].footprint_d == 2 && defs.units[0].cost == 100);
    CHECK(read_units(squad_unit(), "Crawler"));

    // The grid a squad stands in (D-07): as near square as the footprint's shape lets it. The wiki's worked examples.
    const struct { u32 squad, w, d, columns, rows; } grids[] = {
        {24, 5, 2, 8, 3}, {24, 2, 5, 4, 6}, // U01 Scout Swarm, and turned
        {12, 4, 2, 5, 3}, {12, 2, 4, 3, 4}, // U02 Rifle Squad
        {2, 4, 2, 2, 1},  {2, 2, 4, 1, 2},  // U03 Auto Cannon
        {3, 3, 2, 3, 1},  {3, 2, 3, 2, 2},  // U05 Fast Raider
        {4, 4, 2, 3, 2},  {4, 2, 4, 2, 2},  // U06 Interceptor
        {1, 3, 3, 1, 1},  {4, 2, 2, 2, 2},  {6, 2, 2, 3, 2}, {16, 2, 2, 4, 4}, {16, 1, 1, 4, 4},
    };
    for (u32 i = 0; i < NV_ARRAY_COUNT(grids); ++i) {
        SquadGrid grid = battle_squad_grid(grids[i].squad, grids[i].w, grids[i].d);
        CHECK(grid.columns == grids[i].columns && grid.rows == grids[i].rows);
    }

    // A squad's units must not stand on each other, whichever way its footprint is turned: they are min(W / columns, D / rows)
    // apart, and that is at least their width (2 * radius). Touching is fine: 16 on 2 x 2 cells of 2 m are 1 m apart.
    snprintf(text, sizeof(text), "%s", replaced(squad_unit(), "squad_size 4", "squad_size 16"));
    CHECK(read_squad_project(text, SQUAD_STAGE));
    CHECK(!read_squad_project(replaced(replaced(text, "footprint_w 2", "footprint_w 1"), "footprint_d 2", "footprint_d 1"), SQUAD_STAGE) &&
          defs.error_count == 1);
    CHECK(first_error_at(14, "16 units on 1x1 cells stand 0.5 m apart, under their width (1 m)"));
    CHECK(!read_squad_project(replaced(text, "radius_m 0.5", "radius_m 0.6"), SQUAD_STAGE) && defs.error_count == 1);
    CHECK(first_error_at(14, "stand 1 m apart, under their width (1.2 m)"));
    // A squad of 24 on 5 x 2 cells is 1.25 m apart as it stands and 1 m apart turned: a radius of 0.55 fits only one way.
    snprintf(text, sizeof(text), "%s",
             replaced(replaced(replaced(good_units, "squad_size 1\n", "squad_size 24\n"), "footprint_w 1\n", "footprint_w 5\n"), "footprint_d 1\n",
                      "footprint_d 2\n"));
    CHECK(read_squad_project(text, SQUAD_STAGE));
    CHECK(!read_squad_project(replaced(text, "radius_m 0.5", "radius_m 0.55"), SQUAD_STAGE) && defs.error_count == 1);
    CHECK(first_error_at(14, "24 units on 5x2 cells stand 1 m apart when turned, under their width (1.1 m)"));
    // A squad of one stands on its footprint's middle: its width fits the footprint's shorter side.
    CHECK(read_squad_project(replaced(good_units, "radius_m 0.5", "radius_m 1"), SQUAD_STAGE));
    CHECK(!read_squad_project(replaced(good_units, "radius_m 0.5", "radius_m 1.1"), SQUAD_STAGE));
    CHECK(first_error_at(14, "1 unit on 1x1 cells stands 2 m apart, under their width (2.2 m)"));
    // A unit with another error is not judged on top of it.
    snprintf(text, sizeof(text), "%s", replaced(replaced(squad_unit(), "squad_size 4", "squad_size 16"), "radius_m 0.5", "radius_m 0.6"));
    CHECK(!read_squad_project(replaced(text, "hp 120", "hp 0"), SQUAD_STAGE) && defs.error_count == 1 && first_error_at(21, "'hp'"));
    // The rules editor cannot shrink the cells under a squad either: 4 on 2 x 2 cells of 0.4 m stand 0.4 m apart.
    CHECK(read_squad_project(squad_unit(), SQUAD_STAGE));
    {
        char edited[8192];
        f64 small = 0.4;
        umm size = defs_rule_set(project_text, strlen(project_text), "cell_size", &small, 1, edited, sizeof(edited) - 1);
        edited[size] = 0;
        CHECK(size && !read_project(edited) && defs.error_count == 1 && first_error_has("stand 0.4 m apart, under their width (1 m)"));
    }

    // Their ranges.
    const struct { const char* from; const char* to; u32 line; const char* message; } bad[] = {
        {"squad_size 4", "squad_size 0", 14, "'squad_size' must be >= 1 and <= 64"},
        {"squad_size 4", "squad_size 65", 14, "<= 64"},
        {"squad_size 4", "squad_size 1.5", 14, "not an integer"},
        {"squad_size 4", "squad_size 4\n    squad_size 4", 15, "'squad_size' is given twice"},
        {"footprint_w 2", "footprint_w 0", 17, "'footprint_w' must be >= 1 and <= 8"},
        {"footprint_d 2", "footprint_d 9", 18, "'footprint_d' must be >= 1 and <= 8"},
    };
    for (u32 i = 0; i < NV_ARRAY_COUNT(bad); ++i) {
        CHECK(!read_squad_project(replaced(squad_unit(), bad[i].from, bad[i].to), SQUAD_STAGE) && defs.error_count == 1);
        CHECK(first_error_at(bad[i].line, bad[i].message));
    }
    // A footprint must fit in a zone: 4 rows deep is too many for zones of 3 rows, 4 cells wide for a grid of 3 (its places are
    // then not judged).
    snprintf(text, sizeof(text), "%s", replaced(squad_unit(), "footprint_d 2", "footprint_d 4"));
    CHECK(read_squad_project(text, SQUAD_STAGE)); // 4 fits zones of 14 rows
    snprintf(text, sizeof(text), "%s", replaced(project_text, "zone_rows 14", "zone_rows 3"));
    CHECK(!read_project(text) && defs.error_count == 1 && first_error_at(18, "'footprint_d' must be at most 3 (a zone's rows)"));
    snprintf(text, sizeof(text), "%s", replaced(replaced(project_text, "footprint_w 2", "footprint_w 4"), "grid 32 48", "grid 3 48"));
    CHECK(!read_project(text) && defs.error_count == 1 && first_error_at(17, "'footprint_w' must be at most 3 (the grid's width)"));
    // The rules editor cannot shrink the zone under a footprint either: the edited text does not read.
    CHECK(read_squad_project(squad_unit(), SQUAD_STAGE));
    char edited[8192];
    f64 one = 1;
    umm size = defs_rule_set(project_text, strlen(project_text), "zone_rows", &one, 1, edited, sizeof(edited) - 1);
    edited[size] = 0;
    CHECK(size && !read_project(edited) && defs.error_count == 1 && first_error_has("'footprint_d' must be at most 1"));

    // The stage: a place is the footprint's corner nearest the origin, and the whole footprint is in the enemy zone, free.
    CHECK(read_squad_project(squad_unit(), "supply 1000\nplace Crawler 30 34\nplace Crawler 0 46\nplace Crawler 12 36\nplace Crawler 14 36\n"));
    CHECK(defs.enemy_count == 4 && defs.enemy[1].cell_x == 0 && defs.enemy[1].cell_row == 46);
    CHECK(!read_squad_project(squad_unit(), "supply 1000\nplace Crawler 31 36\n") && first_error_at(32, "cell x must be 0 to 30"));
    CHECK(!read_squad_project(squad_unit(), "supply 1000\nplace Crawler 12 47\n") && first_error_at(32, "cell row must be 34 to 46"));
    CHECK(!read_squad_project(squad_unit(), "supply 1000\nplace Crawler 12 36\nplace Crawler 13 37\n") && defs.error_count == 1 &&
          first_error_at(33, "cell (13, 37) already has a unit"));
    // A footprint wider than deep: its x and its rows each to their own end.
    snprintf(text, sizeof(text), "%s", replaced(good_units, "footprint_w 1", "footprint_w 5"));
    CHECK(read_squad_project(text, "supply 1000\nplace Crawler 27 47\n"));
    CHECK(!read_squad_project(text, "supply 1000\nplace Crawler 28 47\n") && first_error_at(32, "cell x must be 0 to 27"));
    CHECK(!read_squad_project(text, "supply 1000\nplace Crawler 0 34\nplace Crawler 4 34\n") && first_error_at(33, "cell (4, 34) already has a unit"));
    CHECK(read_squad_project(text, "supply 1000\nplace Crawler 0 34\nplace Crawler 5 34\nplace Crawler 0 35\n"));
    // An unknown unit's cell is judged as one cell.
    CHECK(!read_squad_project(squad_unit(), "supply 1000\nplace Nobody 31 47\n") && defs.error_count == 1);
    // The supply buys squads.
    CHECK(!read_squad_project(squad_unit(), "supply 150\nplace Crawler 12 36\nplace Crawler 14 36\n") && first_error_at(33, "over the supply"));
    // At most BATTLE_MAX_TEAM_UNITS enemy units: 32 squads of 64 are 2048, a 33rd is one too many.
    snprintf(text, sizeof(text), "%s", replaced(replaced(good_units, "squad_size 1", "squad_size 64"), "radius_m 0.5", "radius_m 0.1"));
    static char places[4096];
    snprintf(places, sizeof(places), "supply 100000\n");
    for (u32 i = 0; i < 33; ++i) {
        char place[48];
        snprintf(place, sizeof(place), "place Crawler %u %u\n", i % 32, 34 + i / 32);
        strcat(places, place);
    }
    CHECK(!read_squad_project(text, places) && defs.error_count == 1 && first_error_at(31 + 33, "the enemy has more than 2048 units"));
}

internal void test_squad_deployment(void)
{
    // 0 "Shooter": squads of 4 on 2 x 2 cells; 1 "Dummy": squads of 3 on one cell. The enemy has one squad of Dummies.
    scene_defs();
    defs.units[0].squad_size = 4;
    defs.units[0].footprint_w = defs.units[0].footprint_d = 2;
    defs.units[1].squad_size = 3;
    defs.enemy_count = 1;
    defs.enemy[0] = (StagePlace){1, 5, 40};
    battle_init(&battle, &defs);
    CHECK(battle.unit_count == 3 && battle.player_units == 0);

    // The units of a squad of 4 on 2 x 2 cells of 2 m stand on the middles of the four cells, row by row.
    CHECK(battle_place(&battle, 0, 0, 0) && battle.unit_count == 7 && battle.player_units == 4);
    const f32 four[4][2] = {{1.0f, 1.0f}, {3.0f, 1.0f}, {1.0f, 3.0f}, {3.0f, 3.0f}};
    for (u32 i = 0; i < 4; ++i)
        CHECK(battle.units[1 + i].team == TEAM_PLAYER && battle.units[1 + i].position.x == four[i][0] && battle.units[1 + i].position.z == four[i][1]);
    // A squad need not have as many units as cells: 6 on 2 x 2 cells stand in 3 columns of 4/3 m and 2 rows of 2 m.
    defs.units[0].squad_size = 6;
    battle_init(&battle, &defs);
    CHECK(battle_place(&battle, 0, 0, 0) && battle.player_units == 6);
    const f32 six[6][2] = {{2.0f / 3.0f, 1.0f}, {2.0f, 1.0f}, {10.0f / 3.0f, 1.0f}, {2.0f / 3.0f, 3.0f}, {2.0f, 3.0f}, {10.0f / 3.0f, 3.0f}};
    for (u32 i = 0; i < 6; ++i)
        CHECK(near_value(battle.units[1 + i].position.x, six[i][0], 1e-5f) && near_value(battle.units[1 + i].position.z, six[i][1], 1e-5f));
    // A rectangle: 24 on 5 x 2 cells (10 m by 4 m) stand in 8 columns of 1.25 m and 3 rows of 4/3 m (the wiki's U01).
    defs.units[0].squad_size = 24;
    defs.units[0].footprint_w = 5;
    battle_init(&battle, &defs);
    CHECK(battle_place(&battle, 0, 2, 3) && battle.player_units == 24);
    CHECK(near_value(battle.units[1].position.x, 4.0f + 0.625f, 1e-5f) && near_value(battle.units[1].position.z, 6.0f + 2.0f / 3.0f, 1e-5f));
    CHECK(near_value(battle.units[8].position.x, 4.0f + 7.5f * 1.25f, 1e-5f) && near_value(battle.units[9].position.z, 6.0f + 2.0f, 1e-5f));
    CHECK(near_value(battle.units[24].position.x, 4.0f + 7.5f * 1.25f, 1e-5f) && near_value(battle.units[24].position.z, 6.0f + 10.0f / 3.0f, 1e-5f));
    // Its cells: 5 along x, 2 along the rows; any of them finds it, and its edges hold in the zone.
    CHECK(battle_squad_at(&battle, 6, 4).ok && battle_squad_at(&battle, 6, 4).x == 2 && battle_squad_at(&battle, 6, 4).row == 3);
    CHECK(!battle_squad_at(&battle, 7, 3).ok && !battle_squad_at(&battle, 2, 5).ok);
    CHECK(!battle_can_place(&battle, 0, 28, 0) && battle_can_place(&battle, 0, 27, 0) && !battle_can_place(&battle, 0, 0, 13));
    CHECK(!battle_can_place(&battle, 0, 6, 4) && !battle_can_place(&battle, 0, 0, 2) && battle_can_place(&battle, 0, 0, 1));
    defs.units[0].squad_size = 4;
    defs.units[0].footprint_w = 2;
    battle_init(&battle, &defs);
    CHECK(battle_place(&battle, 0, 0, 0));
    // A squad of 3 on one cell: two in the first row and one centered in the second.
    const f32 three[3][2] = {{10.5f, 80.5f}, {11.5f, 80.5f}, {11.0f, 81.5f}};
    for (u32 i = 0; i < 3; ++i)
        CHECK(battle.units[5 + i].team == TEAM_ENEMY && battle.units[5 + i].position.x == three[i][0] && battle.units[5 + i].position.z == three[i][1]);

    // A footprint must be free and inside the zone; any of its cells finds the squad.
    CHECK(!battle_can_place(&battle, 0, 1, 1) && !battle_place(&battle, 1, 1, 1)); // on the first squad
    CHECK(battle_place(&battle, 0, 2, 0));                                          // beside it
    CHECK(!battle_place(&battle, 0, 31, 0) && !battle_place(&battle, 0, 0, 13));   // over the zone's edge
    CHECK(battle_place(&battle, 0, 30, 12));                                        // the last corner that fits
    BattleCell at = battle_squad_at(&battle, 31, 13);
    CHECK(at.ok && at.x == 30 && at.row == 12);
    CHECK(!battle_squad_at(&battle, 4, 0).ok && !battle_squad_at(&battle, -1, 0).ok && !battle_squad_at(&battle, 0, 14).ok);
    CHECK(battle_supply_left(&battle) == 700 && battle.player_units == 12);
    // Removing by any cell of its footprint gives the whole squad's supply back.
    CHECK(battle_remove(&battle, 3, 1) && battle_supply_left(&battle) == 800 && battle.player_units == 8 && battle.unit_count == 11);
    CHECK(!battle_remove(&battle, 3, 1));
    CHECK(battle.units[5].position.x == 61.0f && battle.units[5].position.z == 25.0f); // the squad at (30, 12) is next in row order

    // The remaining value: a unit is worth its squad's cost over the squad's size.
    CHECK(near_value(battle_remaining_value(&battle, TEAM_PLAYER), 200.0f, 1e-3f));
    CHECK(near_value(battle_remaining_value(&battle, TEAM_ENEMY), 100.0f, 1e-3f));
    battle.units[1].health = 50.0f;
    CHECK(near_value(battle_remaining_value(&battle, TEAM_PLAYER), 187.5f, 1e-3f));

    // Each unit of a squad fights on its own: it picks the enemy nearest to it.
    CHECK(battle_start(&battle));
    run(1);
    CHECK(battle.units[1].target.index == 9 && battle.units[2].target.index == 9 && battle.units[5].target.index == 10);

    // At most BATTLE_MAX_TEAM_UNITS on a side: 32 squads of 64 fill it.
    scene_defs();
    defs.units[0].squad_size = 64;
    defs.units[0].cost = 1;
    defs.units[1].cost = 1;
    battle_init(&battle, &defs);
    for (s32 x = 0; x < 32; ++x)
        CHECK(battle_place(&battle, 0, x, 0));
    CHECK(battle.player_units == BATTLE_MAX_TEAM_UNITS && battle.unit_count == BATTLE_MAX_TEAM_UNITS);
    CHECK(!battle_place(&battle, 0, 0, 1) && !battle_place(&battle, 1, 0, 1));
}

// A round of squads plays to its end, the same every time.
internal void test_squad_round(void)
{
    char units[4096];
    snprintf(units, sizeof(units), "%s", replaced(squad_unit(), "    armor 0\n", "    armor 5\n    shield_hp 60\n"));
    CHECK(read_squad_project(units,
                             "supply 1000\nseed 1\nplace Crawler 12 36\nplace Crawler 14 36\nplace Crawler 16 36\nplace Crawler 18 36\n"
                             "place Crawler 13 38\nplace Crawler 15 38\nplace Crawler 17 38\nplace Crawler 14 40\nplace Crawler 16 40\n"
                             "place Crawler 15 42\n"));
    u32 hashes[2] = {0, 0};
    for (u32 pass = 0; pass < 2; ++pass) {
        battle_init(&battle, &defs);
        const s32 corners[10][2] = {{12, 10}, {14, 10}, {16, 10}, {18, 10}, {13, 8}, {15, 8}, {17, 8}, {14, 6}, {16, 6}, {15, 4}};
        for (u32 i = 0; i < 10; ++i)
            CHECK(battle_place(&battle, 0, corners[i][0], corners[i][1]));
        CHECK(battle.unit_count == 80 && battle_supply_left(&battle) == 0);
        CHECK(battle_start(&battle));
        while (battle.phase == BATTLE_FIGHT) {
            battle_tick(&battle);
            battle.event_count = 0;
        }
        CHECK(battle.phase == BATTLE_RESULT && battle.outcome != OUTCOME_NONE && battle.tick <= defs.rules.round_ticks);
        CHECK(battle_alive_count(&battle, TEAM_PLAYER) + battle_alive_count(&battle, TEAM_ENEMY) < 80); // the strikes do hurt
        hashes[pass] = battle_hash(&battle);
    }
    CHECK(hashes[0] == hashes[1]);
}

// Whole rounds on a copy of the repository's project (full_project): the player's army mirrors the stage's.

internal void load_full_project(void)
{
    CHECK(read_project(full_project));
}

// The player's squads: unit def and the corner of its footprint.
global const struct { u8 def; s8 x, row; } player_army[] = {
    {U01, 0, 12}, {U01, 26, 12}, {U01, 8, 6}, {U02, 6, 12}, {U02, 20, 12}, {U04, 14, 11}, {U03, 10, 9}, {U10, 14, 6},
};

internal void place_player_army(u32 count)
{
    for (u32 i = 0; i < count; ++i)
        CHECK(battle_place(&battle, player_army[i].def, player_army[i].x, player_army[i].row));
}

internal u32 play_round(u32 seed, b32* finished)
{
    defs.seed = seed;
    battle_init(&battle, &defs);
    place_player_army(NV_ARRAY_COUNT(player_army));
    CHECK(battle_start(&battle));
    while (battle.phase == BATTLE_FIGHT) {
        battle_tick(&battle);
        battle.event_count = 0;
    }
    *finished = battle.phase == BATTLE_RESULT && battle.tick <= defs.rules.round_ticks;
    return battle_hash(&battle);
}

internal void test_whole_round(void)
{
    load_full_project();
    b32 finished = false;
    u32 first = play_round(1, &finished);
    CHECK(finished && battle.outcome != OUTCOME_NONE);
    CHECK(battle.tick > 100); // they have to walk to each other
    u32 units = battle.unit_count;
    CHECK(units == 2 * (3 * 24 + 2 * 12 + 1 + 2 + 1));
    u32 killed = units - battle_alive_count(&battle, TEAM_PLAYER) - battle_alive_count(&battle, TEAM_ENEMY);
    CHECK(killed > 0); // the shells and strikes do hurt
    f32 width = (f32)defs.rules.grid_width * defs.rules.cell_size, length = (f32)defs.rules.grid_length * defs.rules.cell_size;
    for (u32 i = 1; i <= battle.unit_count; ++i) {
        const Unit* unit = &battle.units[i];
        f32 radius = defs.units[unit->def].radius;
        CHECK(isfinite(unit->position.x) && isfinite(unit->position.z) && isfinite(unit->health) && isfinite(unit->shield));
        CHECK(unit->position.x >= radius && unit->position.x <= width - radius && unit->position.z >= radius && unit->position.z <= length - radius);
    }
    // The same deployment and seed give the same round, however many times.
    b32 finished_again = false;
    CHECK(play_round(1, &finished_again) == first && finished_again);
    CHECK(play_round(1, &finished_again) == first);
    // Another seed: the random numbers are part of the state, so the hash differs, but no unit of the wiki's has a scatter, so
    // the round plays out the same.
    u32 ticks = battle.tick, outcome = battle.outcome, players = battle_alive_count(&battle, TEAM_PLAYER);
    CHECK(play_round(2, &finished_again) != first);
    CHECK(battle.tick == ticks && battle.outcome == outcome && battle_alive_count(&battle, TEAM_PLAYER) == players);
}

// Rounds on full_project came to these when the units became the wiki's (unit_fields.md): the hash at tick 300 and at the end.
// A change to the rules that changes a round changes these on purpose.
// The main menu's round (src/menu.c, docs/specs/main_menu.md, "Background battle"): the player's side is the stage's enemy
// mirrored through the field's middle.
internal void test_menu_round(void)
{
    // 0 "Shooter": squads of 2 on 2 x 1 cells (not square, so a mirror that swapped the sides would show). 1 "Dummy": one cell.
    scene_defs();
    defs.units[0].squad_size = 2;
    defs.units[0].footprint_w = 2;
    defs.enemy_count = 2;
    defs.enemy[0] = (StagePlace){0, 5, 40};
    defs.enemy[1] = (StagePlace){1, 0, 47};
    battle_init(&battle, &defs);
    CHECK(menu_place_mirrored(&battle) == 2);
    // (x, row) goes to (32 - x - w, 48 - row - d): the Shooters' corner (5, 40) to (25, 7), the Dummy's (0, 47) to (31, 0).
    CHECK(battle.placed[7][25] == 1 && battle.placed[0][31] == 2);
    CHECK(battle.player_units == 3 && battle_supply_left(&battle) == defs.supply - 200);

    // Placing again starts from an empty side, from any phase.
    CHECK(battle_start(&battle));
    run(10);
    CHECK(menu_place_mirrored(&battle) == 2 && battle.phase == BATTLE_DEPLOY && battle.player_units == 3);

    // A squad whose mirror cannot be placed (this enemy stands outside its zone, so its mirror is outside the player's) is left
    // out; the others are placed.
    defs.enemy[1] = (StagePlace){1, 3, 20};
    battle_init(&battle, &defs);
    CHECK(menu_place_mirrored(&battle) == 1 && battle.player_units == 2);

    // The same round twice gives the same state.
    defs.enemy[1] = (StagePlace){1, 0, 47};
    defs.units[0].range = 30.0f;
    u32 hashes[2];
    for (u32 i = 0; i < 2; ++i) {
        battle_init(&battle, &defs);
        menu_place_mirrored(&battle);
        CHECK(battle_start(&battle));
        run(300);
        hashes[i] = battle_hash(&battle);
    }
    CHECK(hashes[0] == hashes[1]);
}

internal void test_golden_rounds(void)
{
    load_full_project();
    const struct { u32 seed, ticks, outcome, at_300, at_end, enemies_left; } golden[] = {
        {1, 2060, OUTCOME_ENEMY, 0x9148e3dcu, 0x2267812du, 9},
        {2, 2060, OUTCOME_ENEMY, 0x8ea0d334u, 0xb7dd939cu, 9},
        {3, 2060, OUTCOME_ENEMY, 0x759bbf35u, 0x0ad79170u, 9},
        {4, 2060, OUTCOME_ENEMY, 0x631340b8u, 0x94585b5bu, 9},
    };
    for (u32 g = 0; g < NV_ARRAY_COUNT(golden); ++g) {
        defs.seed = golden[g].seed;
        battle_init(&battle, &defs);
        place_player_army(NV_ARRAY_COUNT(player_army));
        CHECK(battle_start(&battle));
        u32 at_300 = 0;
        while (battle.phase == BATTLE_FIGHT) {
            battle_tick(&battle);
            battle.event_count = 0;
            if (battle.tick == 300)
                at_300 = battle_hash(&battle);
        }
#if defined(BATTLE_PRINT_GOLDEN)
        printf("        {%u, %u, %s, 0x%08xu, 0x%08xu, %u},\n", golden[g].seed, battle.tick,
               battle.outcome == OUTCOME_PLAYER ? "OUTCOME_PLAYER" : battle.outcome == OUTCOME_ENEMY ? "OUTCOME_ENEMY" : "OUTCOME_DRAW", at_300,
               battle_hash(&battle), battle_alive_count(&battle, TEAM_ENEMY));
#endif
        // The round's length, outcome and survivors are the same everywhere. The hashes hold the units' float bits, and the C library's
        // sinf, cosf, tanf and atan2f (the aim) give the last bit differently on each platform, so a native build has its own and
        // only the web's are checked (found 2026-10-12, with MSVC).
#if defined(__EMSCRIPTEN__)
        CHECK(battle.tick == golden[g].ticks && (u32)battle.outcome == golden[g].outcome && at_300 == golden[g].at_300);
        CHECK(battle_hash(&battle) == golden[g].at_end && battle_alive_count(&battle, TEAM_ENEMY) == golden[g].enemies_left);
#else
        (void)at_300;
        CHECK(battle.tick == golden[g].ticks && (u32)battle.outcome == golden[g].outcome);
        CHECK(battle_alive_count(&battle, TEAM_ENEMY) == golden[g].enemies_left);
#endif
    }
}

// Stepping back: a round put back at a tick by battle_seek is the state it had at that tick, from the fight and from the
// result alike; seeking forward runs on to the tick; in deployment it does nothing.
internal void test_seek(void)
{
    load_full_project();
    battle_init(&battle, &defs);
    place_player_army(6);
    u32 hash_before = battle_hash(&battle);
    battle_seek(&battle, 10);
    CHECK(battle.phase == BATTLE_DEPLOY && battle_hash(&battle) == hash_before);

    static u32 hashes[2700 + 1];
    CHECK(defs.rules.round_ticks == 2700);
    CHECK(battle_start(&battle));
    hashes[0] = battle_hash(&battle);
    while (battle.phase == BATTLE_FIGHT) {
        battle_tick(&battle);
        battle.event_count = 0;
        hashes[battle.tick] = battle_hash(&battle);
    }
    u32 last = battle.tick;
    CHECK(battle.phase == BATTLE_RESULT && last > 200);

    const u32 ticks[5] = {last - 1, 200, 57, 1, 0};
    for (u32 i = 0; i < 5; ++i) {
        battle_seek(&battle, ticks[i]);
        CHECK(battle.tick == ticks[i] && battle.phase == BATTLE_FIGHT && battle_hash(&battle) == hashes[ticks[i]]);
        CHECK(battle.event_count == 0);
    }
    battle_seek(&battle, 120); // forward from 0
    CHECK(battle.tick == 120 && battle_hash(&battle) == hashes[120]);
    battle_tick(&battle); // and on from there as if it had never stopped
    CHECK(battle_hash(&battle) == hashes[121]);
    battle_seek(&battle, 200); // forward from the middle runs on, to the same state
    CHECK(battle.tick == 200 && battle.event_count == 0 && battle_hash(&battle) == hashes[200]);
    battle_seek(&battle, 200); // where it is: nothing changes
    CHECK(battle.tick == 200 && battle_hash(&battle) == hashes[200]);
    battle_seek(&battle, last + 50); // past the end: the round ends where it ends
    CHECK(battle.phase == BATTLE_RESULT && battle.tick == last && battle_hash(&battle) == hashes[last]);
}

// Roles (abproj.md, "Roles"; unit_fields.md, "Value kinds"): the names a file declares (text, spaces and all), each unit's
// mask over them (a list with commas), and the errors. A role does not change a round.

// The line of the first match of `needle` in project_text, from 1.
internal u32 line_of(const char* needle)
{
    const char* at = strstr(project_text, needle);
    NV_ASSERT(at);
    u32 line = 1;
    for (const char* p = project_text; p < at; ++p)
        line += *p == '\n';
    return line;
}

// Makes project_text and reads it: the good header, `block` (the roles block, if any), and good_units with `unit_roles` (its
// roles line, or "") after its header.
internal b32 read_roles_project(const char* block, const char* unit_roles)
{
    char header[256];
    snprintf(header, sizeof(header), "unit Crawler\n%s", unit_roles);
    snprintf(project_text, sizeof(project_text), "%s%s%sstage\n    supply 1000\n    place Crawler 12 36\n", good_header, block,
             replaced(good_units, "unit Crawler\n", header));
    return read_project(project_text);
}

internal void test_roles(void)
{
    // Declared roles, and a unit with two of them: its mask is its roles' bits, in the order the file declares them.
    CHECK(read_roles_project("roles\n    Chaff/Screen\n    Chaff Clear\n", "    roles Chaff Clear, Chaff/Screen\n"));
    CHECK(defs.error_count == 0 && defs.role_count == 2 && strcmp(defs.roles[0], "Chaff/Screen") == 0 && strcmp(defs.roles[1], "Chaff Clear") == 0);
    CHECK(defs.unit_roles[0] == 3);
    // Spaces around the commas do not count; one role needs no comma.
    CHECK(read_roles_project("roles\n    Chaff/Screen\n    Chaff Clear\n", "    roles   Chaff Clear  ,Chaff/Screen\n") && defs.unit_roles[0] == 3);
    CHECK(read_roles_project("roles\n    Chaff/Screen\n    Chaff Clear\n", "    roles Chaff Clear\n") && defs.unit_roles[0] == 2);
    // A unit without roles has none; a block with no roles, or no block, is fine.
    CHECK(read_roles_project("roles\n    frontline\n", "") && defs.unit_roles[0] == 0 && defs.role_count == 1);
    CHECK(read_roles_project("roles\n", "") && defs.role_count == 0);
    CHECK(read_roles_project("", "") && defs.role_count == 0 && defs.unit_roles[0] == 0);

    // A role the file does not declare, at the unit's line.
    CHECK(!read_roles_project("roles\n    frontline\n", "    roles shielder\n") && defs.error_count == 1);
    CHECK(first_error_at(line_of("roles shielder"), "unknown role 'shielder' (declare it under 'roles')"));
    CHECK(!read_roles_project("roles\n    Chaff Clear\n", "    roles Chaff\n") && first_error_has("unknown role 'Chaff'")); // a name is whole
    // A unit's roles given twice, a role named twice in a line, an empty one, and a line with no role.
    CHECK(!read_roles_project("roles\n    frontline\n", "    roles frontline\n    roles frontline\n") && defs.error_count == 1);
    CHECK(first_error_at(line_of("roles frontline\n    roles frontline") + 1, "'roles' is given twice in this block"));
    CHECK(!read_roles_project("roles\n    frontline\n", "    roles frontline, frontline\n") && defs.error_count == 1);
    CHECK(first_error_at(line_of("roles frontline, frontline"), "role 'frontline' is given twice"));
    CHECK(!read_roles_project("roles\n    frontline\n", "    roles frontline,\n") && defs.error_count == 1);
    CHECK(first_error_at(line_of("roles frontline,"), "'roles' has an empty role"));
    CHECK(!read_roles_project("roles\n    frontline\n", "    roles , frontline\n") && first_error_has("'roles' has an empty role"));
    CHECK(!read_roles_project("roles\n    frontline\n", "    roles\n") && defs.error_count == 1);
    CHECK(first_error_at(line_of("    roles\n    name"), "'roles' takes at least one role"));

    // The block: each name once, at most 32, no comma in a name, at most 31 bytes.
    CHECK(!read_roles_project("roles\n    frontline\n    frontline\n", "") && defs.error_count == 1);
    CHECK(first_error_at(line_of("    frontline\n    frontline") + 1, "role 'frontline' is declared twice"));
    CHECK(read_roles_project("roles\n    front  line\n", "    roles front  line\n") && strcmp(defs.roles[0], "front  line") == 0 && defs.unit_roles[0] == 1);
    CHECK(!read_roles_project("roles\n    front, line\n", "") && defs.error_count == 1);
    CHECK(first_error_at(line_of("    front, line"), "a role's name cannot hold ','"));
    CHECK(!read_roles_project("roles\n    1234567890123456789012345678901X\n", "") && first_error_has("a role's name is longer than 31 bytes"));
    static char many[4096];
    snprintf(many, sizeof(many), "roles\n");
    for (u32 i = 0; i < 33; ++i) {
        char one[32];
        snprintf(one, sizeof(one), "    r%u\n", i);
        strcat(many, one);
    }
    CHECK(!read_roles_project(many, "") && defs.error_count == 1 && first_error_has("too many roles (at most 32)"));

    // Order: the block comes after the rules and before the units; given once.
    snprintf(project_text, sizeof(project_text), "%s%sroles\n    frontline\nstage\n    supply 1000\n    place Crawler 12 36\n", good_header, good_units);
    CHECK(!read_project(project_text) && defs.error_count == 1);
    CHECK(first_error_at(GOOD_HEADER_LINES + GOOD_UNITS_LINES + 1, "'roles' must come after 'rules' and before the units"));
    static char twice[8192];
    snprintf(twice, sizeof(twice), "%s%s%s%s", good_header, "roles\n    frontline\nroles\n    backline\n", good_units,
             "stage\n    supply 1000\n    place Crawler 12 36\n");
    snprintf(project_text, sizeof(project_text), "%s", twice);
    CHECK(!read_project(project_text) && defs.error_count == 1);
    CHECK(first_error_at(GOOD_HEADER_LINES + 3, "'roles' is given twice"));

    // The rules editor edits a rule in a text with a roles block, and the result reads.
    static char edited[sizeof(full_project) + 64];
    f64 gravity = 5.0;
    umm size = defs_rule_set(full_project, sizeof(full_project) - 1, "gravity", &gravity, 1, edited, sizeof(edited));
    edited[size] = 0;
    CHECK(size && read_project(edited) && defs.rules.gravity == 5.0f && defs.unit_roles[U01] == 3);

    // Roles do not change a round: full_project's round, with other roles on its units, is the same round.
    load_full_project();
    b32 finished = false;
    u32 plain = play_round(1, &finished);
    CHECK(read_project(replaced(replaced(full_project, "    roles Chaff/Screen, Flanker\n", "    roles Siege\n"), "    roles Chaff/Screen, Anti-Air\n", "")));
    CHECK(defs.unit_roles[U01] == 1u << 11 && defs.unit_roles[U02] == 0);
    CHECK(play_round(1, &finished) == plain && finished);
}

// Layers (abproj.md, "Layers"; battle.md, "Layers"): what a unit's unit_layer, altitude_m and target_layer read as, and what
// the layers do in a round.

// good_units named Wasp with `extra` (its lines after its armor line: "    unit_layer Air\n") and target_layer `targets`. The
// unit_layer line is 5, target_layer 6; `extra` starts at 13.
static char wasp[4096];
internal const char* make_wasp(const char* layer, const char* targets, const char* extra)
{
    char armor[512];
    snprintf(armor, sizeof(armor), "    armor 0\n%s", extra);
    snprintf(wasp, sizeof(wasp), "%s",
             replaced(replaced(replaced(replaced(good_units, "unit Crawler", "unit Wasp"), "unit_layer Ground", layer), "target_layer Ground", targets),
                      "    armor 0\n", armor));
    return wasp;
}

internal void test_layers_format(void)
{
    // An air unit with its altitude, and a target_layer of air, or both.
    CHECK(read_units(make_wasp("unit_layer Air", "target_layer Air", "    altitude_m 10\n"), "Wasp"));
    CHECK(defs.units[0].layer == UNIT_LAYER_AIR && defs.units[0].altitude == 10.0f && defs.units[0].targets == TARGET_AIR);
    CHECK(read_units(make_wasp("unit_layer Air", "target_layer Both", "    altitude_m 10\n"), "Wasp") &&
          defs.units[0].targets == (TARGET_GROUND | TARGET_AIR));
    CHECK(read_units(good_units, "Crawler") && defs.units[0].layer == UNIT_LAYER_GROUND && defs.units[0].altitude == 0.0f &&
          defs.units[0].targets == TARGET_GROUND);

    // An air unit needs its altitude, a ground unit has none, and the altitude is above zero.
    expect_error_in(PART_UNITS, make_wasp("unit_layer Air", "target_layer Ground", ""), 1, 1, "unit 'Wasp' flies (unit_layer Air) and needs 'altitude_m'");
    expect_error_in(PART_UNITS, make_wasp("unit_layer Ground", "target_layer Ground", "    altitude_m 5\n"), 1, 13, "'altitude_m' is only for air units");
    expect_error_in(PART_UNITS, make_wasp("unit_layer Air", "target_layer Ground", "    altitude_m 0\n"), 1, 13, "'altitude_m' must be > 0");
    // A layer is Ground or Air, given once, with a word; a bad one judges no altitude.
    expect_error_in(PART_UNITS, make_wasp("unit_layer Sea", "target_layer Ground", "    altitude_m 5\n"), 1, 5, "unknown unit_layer 'Sea' (Ground or Air)");
    expect_error_in(PART_UNITS, make_wasp("unit_layer Air\n    unit_layer Air", "target_layer Ground", "    altitude_m 3\n"), 1, 6,
                    "'unit_layer' is given twice in this block");
    expect_error_in(PART_UNITS, make_wasp("unit_layer", "target_layer Ground", ""), 1, 5, "'unit_layer' takes Ground or Air");
    expect_error_in(PART_UNITS, make_wasp("unit_layer Ground", "target_layer", ""), 1, 6, "'target_layer' takes Ground, Air or Both");
    expect_error_in(PART_UNITS, make_wasp("unit_layer Ground", "target_layer air", ""), 1, 6, "unknown target_layer 'air' (Ground, Air or Both)");
    expect_error_in(PART_UNITS, make_wasp("unit_layer Ground", "target_layer Ground Air", ""), 1, 6, "'target_layer' takes Ground, Air or Both");
}

internal void test_layers_in_use(void)
{
    // An air unit stands at its altitude; a ground unit on the ground.
    scene_defs();
    scene_air_unit();
    u8 wasp_player[1] = {2};
    u8 dummy_enemy[1] = {1};
    scene_start(wasp_player, 1, dummy_enemy, 1);
    CHECK(battle.units[1].position.y == 10.0f && battle.units[2].position.y == 0.0f);

    // A ground unit only picks the enemies it can hit: an air unit is none of them until it can hit air.
    scene_defs();
    scene_air_unit();
    u8 shooter[1] = {0};
    u8 wasp_enemy[1] = {2};
    scene_start(shooter, 1, wasp_enemy, 1);
    put(1, 10.0f, 10.0f);
    put(2, 10.0f, 20.0f);
    battle.units[2].position.y = battle.units[2].previous_position.y = 10.0f;
    run(1);
    CHECK(battle.units[1].target.index == 0 && battle.units[1].position.z == 10.0f); // no target: it stands
    defs.units[0].targets = TARGET_GROUND | TARGET_AIR;
    run(1);
    CHECK(battle.units[1].target.index == 2);

    // A ground-only shell passes an air unit and goes on; an anti-air shell hits it.
    for (u32 pass = 0; pass < 2; ++pass) {
        scene_defs();
        scene_air_unit();
        u8 dummy_player[1] = {1};
        scene_start(dummy_player, 1, wasp_enemy, 1);
        put(1, 5.0f, 2.0f);
        put(2, 10.0f, 15.0f);
        battle.units[2].position.y = battle.units[2].previous_position.y = 10.0f;
        inject_shell(TEAM_PLAYER, nv_vec3(10.0f, 10.3f, 14.0f), nv_vec3(0.0f, 0.0f, 90.0f), 30.0f);
        battle.projectiles[0].targets = pass ? (TARGET_GROUND | TARGET_AIR) : TARGET_GROUND;
        run(1);
        if (pass) {
            CHECK(battle.units[2].health == 70.0f && battle.projectile_count == 0); // hit
        } else {
            CHECK(battle.units[2].health == 100.0f && battle.projectile_count == 1); // passed
        }
    }

    // An anti-air shell is aimed at an air unit's middle, so a stopped flier 20 m away is hit (a shell aimed at the ground would
    // pass over it).
    scene_defs();
    scene_air_unit();
    defs.units[0].range = 30.0f;
    defs.units[0].targets = TARGET_AIR;
    defs.units[0].attack_interval = 1000.0f; // one shot
    u8 aa_shooter[1] = {0};
    scene_start(aa_shooter, 1, wasp_enemy, 1);
    put(1, 10.0f, 5.0f);
    put(2, 10.0f, 25.0f);
    battle.units[2].position.y = battle.units[2].previous_position.y = 10.0f;
    run(120);
    CHECK(battle.units[2].health == 70.0f);

    // Units of different layers do not push each other apart; two air units do.
    scene_defs();
    scene_air_unit();
    u8 two_wasps[2] = {2, 2};
    scene_start(dummy_enemy, 1, two_wasps, 2); // a ground Dummy of the player's, and two Wasps
    put(1, 10.0f, 15.0f);
    put(2, 10.0f, 15.0f);
    put(3, 10.0f, 15.0f);
    battle.units[2].position.y = battle.units[2].previous_position.y = 10.0f;
    battle.units[3].position.y = battle.units[3].previous_position.y = 10.0f;
    run(1);
    CHECK(battle.units[1].position.x == 10.0f && battle.units[2].position.x != battle.units[3].position.x);
}

// The unit lab (docs/specs/unit_lab.md): one unit of the Shooter (a squad of six) against the lab's two targets, on the battle's
// rules. The Shooter reaches 30 m, hits for 30 every 1.5 s with no scatter; the targets have 300 health and no armor.

global Lab lab;

internal void lab_project(void)
{
    scene_defs();
    UnitDef* shooter = &defs.units[0];
    shooter->squad_size = 6;
    shooter->footprint_w = shooter->footprint_d = 2;
    shooter->move_speed = 5.0f;
    shooter->range = 30.0f;
}

// Holding still 20 m from the targets (20.2 m from each, across the gap), the run started.
internal LabSettings lab_hold_settings(void)
{
    LabSettings settings = lab_default_settings(&defs, 0);
    settings.walk = false;
    settings.distance = 20.0f;
    return settings;
}

internal void lab_run(u32 ticks)
{
    for (u32 i = 0; i < ticks; ++i) {
        lab_tick(&lab);
        lab.battle.event_count = 0; // as the view does
    }
}

// Runs until the target has been killed `kills` times (or `limit` ticks); returns the ticks it ran.
internal u32 lab_run_to_kill(u32 target, u32 kills, u32 limit)
{
    u32 ticks = 0;
    while (lab.results.targets[target].kills < kills && ticks < limit) {
        lab_run(1);
        ++ticks;
    }
    return ticks;
}

internal const Unit* lab_unit(u32 slot)
{
    return &lab.battle.units[slot];
}

internal void test_lab_setup(void)
{
    lab_project();
    CHECK(near_value(lab_max_distance(&defs.rules), 96.0f - 2.0f - 24.0f, 1e-4f));
    LabSettings settings = lab_default_settings(&defs, 0);
    CHECK(settings.walk && settings.distance == 45.0f && !settings.targets_move && !settings.restart_on_change);
    CHECK(settings.target_health == 300.0f && settings.respawn_delay == 1.0f && settings.air_altitude == 10.0f);
    lab_start(&lab, &defs, &settings);
    // A squad of six still gives one unit: three in all, the unit first.
    CHECK(lab.battle.unit_count == 3 && lab.battle.phase == BATTLE_FIGHT && lab.battle.endless);
    CHECK(lab_unit(LAB_SLOT_UNIT)->team == TEAM_PLAYER && lab_unit(LAB_SLOT_UNIT)->def == LAB_DEF_UNIT);
    CHECK(lab_unit(LAB_SLOT_UNIT)->flags == 0); // walks
    CHECK(lab_unit(LAB_SLOT_GROUND)->flags == UNIT_DUMMY && lab_unit(LAB_SLOT_AIR)->flags == UNIT_DUMMY);
    CHECK(lab.defs.units[LAB_DEF_GROUND].layer == UNIT_LAYER_GROUND && lab.defs.units[LAB_DEF_AIR].layer == UNIT_LAYER_AIR);
    CHECK(lab.defs.units[LAB_DEF_GROUND].hp == 300.0f && lab.defs.units[LAB_DEF_AIR].radius == 1.0f);
    NvVec3 unit = lab_unit(LAB_SLOT_UNIT)->position, ground = lab_unit(LAB_SLOT_GROUND)->position, air = lab_unit(LAB_SLOT_AIR)->position;
    CHECK(unit.x == 32.0f && unit.y == 0.0f && unit.z == 24.0f); // the middle of the 64 m width, a quarter of the 96 m length
    CHECK(ground.x == 29.0f && ground.y == 0.0f && ground.z == 69.0f);
    CHECK(air.x == 35.0f && air.y == 10.0f && air.z == 69.0f);
    // The targets stay on the field however far they are asked to stand.
    settings.distance = 500.0f;
    lab_start(&lab, &defs, &settings);
    CHECK(near_value(lab_unit(LAB_SLOT_GROUND)->position.z, 94.0f, 1e-4f));

    // Expected values: armor takes off up to the rules' minimum share.
    settings = lab_hold_settings();
    settings.target_armor = 10.0f;
    lab_start(&lab, &defs, &settings);
    LabExpected expected = lab_expected(&lab, LAB_GROUND);
    CHECK(expected.ok && expected.damage_per_hit == 20.0f && expected.shots_to_kill == 15);
    CHECK(near_value(expected.dps, 20.0f / 1.5f, 1e-4f));
    CHECK(!lab_expected(&lab, LAB_AIR).ok); // the Shooter hits only the ground
    settings.target_armor = 28.0f;
    lab_start(&lab, &defs, &settings);
    CHECK(lab_expected(&lab, LAB_GROUND).damage_per_hit == 7.5f); // 30 * 0.25 is more than 30 - 28
}

internal void test_lab_hold(void)
{
    lab_project();
    LabSettings settings = lab_hold_settings();
    lab_start(&lab, &defs, &settings);
    NvVec3 start = lab_unit(LAB_SLOT_UNIT)->position;
    u32 ticks = lab_run_to_kill(LAB_GROUND, 1, 2000);
    const LabTargetResults* ground = &lab.results.targets[LAB_GROUND];
    CHECK(ground->kills == 1 && ground->hits == 10 && ground->damage == 300.0f);
    CHECK(ground->last_kill_ticks == ticks && ground->kill_ticks == ticks);
    // Ten shots from tick 0, one each 45 ticks, the last landing after its flight: as the expected time says.
    LabExpected expected = lab_expected(&lab, LAB_GROUND);
    CHECK(expected.shots_to_kill == 10);
    CHECK(near_value((f32)ticks * BATTLE_TICK_SECONDS, expected.seconds_to_kill, 3.0f * BATTLE_TICK_SECONDS));
    CHECK(lab.results.shots == ground->shots && lab.results.shots >= 10 && lab_misses(&lab) == 0);
    CHECK(lab.results.ticks_with_target == ticks); // the ground target was there the whole time
    const Unit* unit = lab_unit(LAB_SLOT_UNIT);
    CHECK(unit->position.x == start.x && unit->position.z == start.z && (unit->flags & UNIT_HOLD));
    // The air target is out of the Shooter's reach: never aimed at, never hit.
    CHECK(lab.results.targets[LAB_AIR].shots == 0 && lab.results.targets[LAB_AIR].hits == 0);
    CHECK(lab_unit(LAB_SLOT_AIR)->health == 300.0f);

    // Out of range, a unit that holds neither fires nor moves.
    settings.distance = 40.0f;
    lab_start(&lab, &defs, &settings);
    lab_run(300);
    CHECK(lab.results.shots == 0 && lab_unit(LAB_SLOT_UNIT)->position.z == start.z);
    CHECK(lab_unit(LAB_SLOT_UNIT)->target.index == LAB_SLOT_GROUND && lab.results.ticks_with_target == 300);
}

internal void test_lab_walk(void)
{
    lab_project();
    LabSettings settings = lab_hold_settings();
    settings.walk = true;
    settings.distance = 40.0f;
    lab_start(&lab, &defs, &settings);
    // It fires once in range, walking on, and stops at the stop distance: 90% of its 30 m.
    const Unit* unit = lab_unit(LAB_SLOT_UNIT);
    u32 ticks = 0;
    while (lab.results.shots == 0 && ticks++ < 600)
        lab_run(1);
    CHECK(ticks < 600 && unit->velocity.z > 0.0f);
    lab_run(90);
    f32 dx = lab_unit(LAB_SLOT_GROUND)->position.x - unit->position.x, dz = lab_unit(LAB_SLOT_GROUND)->position.z - unit->position.z;
    CHECK(unit->velocity.z == 0.0f && near_value(sqrtf(dx * dx + dz * dz), 27.0f, 0.01f));
}

internal void test_lab_layers(void)
{
    // A weapon that hits both layers takes the ground target first at the same distance (the lower slot), then the air one.
    lab_project();
    defs.units[0].targets = TARGET_GROUND | TARGET_AIR;
    LabSettings settings = lab_hold_settings();
    settings.respawn_delay = 10.0f;
    lab_start(&lab, &defs, &settings);
    lab_run(1);
    CHECK(lab_unit(LAB_SLOT_UNIT)->target.index == LAB_SLOT_GROUND);
    CHECK(lab_run_to_kill(LAB_GROUND, 1, 2000) < 2000);
    lab_run(1);
    CHECK(lab_unit(LAB_SLOT_UNIT)->target.index == LAB_SLOT_AIR);
    CHECK(lab_run_to_kill(LAB_AIR, 1, 2000) < 2000);
    CHECK(lab.results.targets[LAB_AIR].hits == 10 && lab.results.targets[LAB_AIR].damage == 300.0f);
    CHECK(lab_expected(&lab, LAB_AIR).ok);
    // With both down the unit has no target, and that time is not counted for DPS.
    lab_run(1);
    u32 with_target = lab.results.ticks_with_target;
    lab_run(30);
    CHECK(lab_unit(LAB_SLOT_UNIT)->target.index == 0 && lab.results.ticks_with_target == with_target);
}

internal void test_lab_moving_targets(void)
{
    // A target crossing in front of the unit: leading aim lands the one shell on it.
    lab_project();
    defs.units[0].attack_interval = 1000.0f; // one shot
    LabSettings settings = lab_hold_settings();
    settings.targets_move = true;
    settings.move_speed = 4.0f;
    settings.move_width = 40.0f;
    lab_start(&lab, &defs, &settings);
    u32 ticks = 0;
    while ((lab.results.shots == 0 || lab.battle.projectile_count > 0) && ticks++ < 400)
        lab_run(1);
    CHECK(ticks < 400 && lab.results.shots == 1 && lab.results.hits == 1);
    CHECK(lab_unit(LAB_SLOT_GROUND)->velocity.x == 4.0f);

    // Back and forth within the width, turning at each end.
    settings.move_width = 8.0f;
    lab_start(&lab, &defs, &settings);
    f32 low = 1000.0f, high = -1000.0f;
    for (u32 i = 0; i < 600; ++i) {
        lab_run(1);
        low = fminf(low, lab_unit(LAB_SLOT_GROUND)->position.x);
        high = fmaxf(high, lab_unit(LAB_SLOT_GROUND)->position.x);
    }
    CHECK(near_value(low, 29.0f - 4.0f, 0.2f) && near_value(high, 29.0f + 4.0f, 0.2f));
    // The field's edge turns them too.
    settings.move_width = 100.0f;
    lab_start(&lab, &defs, &settings);
    low = 1000.0f;
    for (u32 i = 0; i < 1200; ++i) {
        lab_run(1);
        low = fminf(low, lab_unit(LAB_SLOT_GROUND)->position.x);
        high = fmaxf(high, lab_unit(LAB_SLOT_AIR)->position.x);
    }
    CHECK(low >= 1.0f - 0.2f && low < 1.5f && high <= 64.0f - 1.0f + 0.2f);
}

internal void test_lab_respawn(void)
{
    lab_project();
    LabSettings settings = lab_hold_settings();
    lab_start(&lab, &defs, &settings);
    lab_run_to_kill(LAB_GROUND, 1, 2000);
    u32 died = lab.battle.tick;
    CHECK(lab_unit(LAB_SLOT_GROUND)->flags & UNIT_DEAD);
    while ((lab_unit(LAB_SLOT_GROUND)->flags & UNIT_DEAD) && lab.battle.tick < died + 100)
        lab_run(1);
    // Back a second later, at its home with its health, and the unit takes it as its target again.
    CHECK(lab.battle.tick == died + 30);
    CHECK(lab_unit(LAB_SLOT_GROUND)->health == 300.0f && lab_unit(LAB_SLOT_GROUND)->position.x == 29.0f);
    lab_run(1);
    CHECK(lab_unit(LAB_SLOT_UNIT)->target.index == LAB_SLOT_GROUND);
    // Its time to kill counts from when it came back.
    u32 back = lab.battle.tick - 1;
    lab_run_to_kill(LAB_GROUND, 2, 2000);
    CHECK(lab.results.targets[LAB_GROUND].last_kill_ticks == lab.battle.tick - back);

    // No delay: alive again after the tick it died in.
    settings.respawn_delay = 0.0f;
    lab_start(&lab, &defs, &settings);
    lab_run_to_kill(LAB_GROUND, 1, 2000);
    CHECK(!(lab_unit(LAB_SLOT_GROUND)->flags & UNIT_DEAD) && lab_unit(LAB_SLOT_GROUND)->health == 300.0f);
}

// A melee unit in the lab: its strikes count as shots, each of hits_per_attack hits; the expected time has no flight.
internal void test_lab_melee(void)
{
    lab_project();
    defs.units[0].attack_type = ATTACK_MELEE;
    defs.units[0].range = 6.0f;
    defs.units[0].hits_per_attack = 2;
    LabSettings settings = lab_hold_settings();
    settings.distance = 5.0f; // 5.8 m from each target, across the gap
    lab_start(&lab, &defs, &settings);
    LabExpected expected = lab_expected(&lab, LAB_GROUND);
    CHECK(expected.ok && expected.damage_per_hit == 30.0f && expected.shots_to_kill == 5); // 10 hits, 2 a strike
    CHECK(near_value(expected.seconds_to_kill, 6.0f, 1e-4f) && near_value(expected.dps, 40.0f, 1e-3f));
    u32 ticks = lab_run_to_kill(LAB_GROUND, 1, 2000);
    const LabTargetResults* ground = &lab.results.targets[LAB_GROUND];
    CHECK(ground->kills == 1 && ground->shots == 5 && ground->hits == 10 && ground->damage == 300.0f);
    CHECK(near_value((f32)ticks * BATTLE_TICK_SECONDS, expected.seconds_to_kill, 2.0f * BATTLE_TICK_SECONDS));
    CHECK(lab.battle.projectile_count == 0 && lab_misses(&lab) == 0);
}

internal void test_lab_endless(void)
{
    // Past the round's time and with every target it can hit down, the run goes on.
    lab_project();
    defs.rules.round_ticks = 60;
    LabSettings settings = lab_hold_settings();
    settings.respawn_delay = 10.0f;
    lab_start(&lab, &defs, &settings);
    lab_run_to_kill(LAB_GROUND, 1, 2000);
    lab_run(10);
    CHECK(lab.battle.phase == BATTLE_FIGHT && lab.battle.outcome == OUTCOME_NONE);
    CHECK(battle_alive_count(&lab.battle, TEAM_ENEMY) == 1); // the air target, which the Shooter cannot hit
}

internal void test_lab_changes(void)
{
    lab_project();
    give_shield(0, 60.0f);
    LabSettings settings = lab_hold_settings();
    lab_start(&lab, &defs, &settings);
    lab_run(100);
    u32 shots = lab.results.shots;
    CHECK(shots > 0 && lab_unit(LAB_SLOT_GROUND)->health < 300.0f);

    // A lower maximum cuts the health of what is alive; the run and its results go on.
    f32 health = lab_unit(LAB_SLOT_GROUND)->health;
    settings.target_health = health - 10.0f;
    lab_change(&lab, &defs, &settings);
    CHECK(lab.battle.tick == 100 && lab.results.shots == shots && lab.results.changes == 1 && lab.change_count == 1);
    CHECK(lab_unit(LAB_SLOT_GROUND)->health == health - 10.0f && lab_unit(LAB_SLOT_AIR)->health == health - 10.0f);
    // A higher one leaves it as it is.
    settings.target_health = 1000.0f;
    lab_change(&lab, &defs, &settings);
    CHECK(lab_unit(LAB_SLOT_GROUND)->health == health - 10.0f && lab.results.changes == 2);
    // The unit's def as the project now has it: a smaller shield, more damage.
    defs.units[0].shield_hp = 20.0f;
    defs.units[0].damage = 50.0f;
    lab_change(&lab, &defs, &settings);
    CHECK(lab_unit(LAB_SLOT_UNIT)->shield == 20.0f);
    lab_run(150); // the shells fired before have landed (they keep the damage they were fired with)
    u32 hits = lab.results.hits;
    f32 damage = lab.results.targets[LAB_GROUND].damage;
    u32 limit = lab.battle.tick + 200;
    while (lab.results.hits == hits && lab.battle.tick < limit)
        lab_run(1);
    CHECK(lab.results.hits == hits + 1 && lab.results.targets[LAB_GROUND].damage - damage == 50.0f);
    // A new distance puts the targets at their new homes at once.
    settings.distance = 25.0f;
    lab_change(&lab, &defs, &settings);
    CHECK(lab_unit(LAB_SLOT_GROUND)->position.z == 24.0f + 25.0f && lab_unit(LAB_SLOT_AIR)->position.z == 24.0f + 25.0f);
    // Walking turned on: the unit is free to move.
    settings.walk = true;
    lab_change(&lab, &defs, &settings);
    CHECK(!(lab_unit(LAB_SLOT_UNIT)->flags & UNIT_HOLD));
    CHECK(lab.results.changes == 5 && lab.change_count == 5);
    // Restart on change alone changes nothing in the run.
    settings.restart_on_change = true;
    u32 tick = lab.battle.tick;
    lab_change(&lab, &defs, &settings);
    CHECK(lab.battle.tick == tick && lab.change_count == 5 && lab.settings.restart_on_change);
    // With it on, a change starts over.
    settings.distance = 22.0f;
    lab_change(&lab, &defs, &settings);
    CHECK(lab.battle.tick == 0 && lab.results.shots == 0 && lab.change_count == 0 && lab.results.changes == 0);
    CHECK(lab_unit(LAB_SLOT_GROUND)->position.z == 24.0f + 22.0f);
    // Another unit def starts over even with it off.
    lab_run(50);
    scene_air_unit(); // unit def 2 flies
    settings.restart_on_change = false;
    settings.unit_def = 2;
    lab_change(&lab, &defs, &settings);
    CHECK(lab.battle.tick == 0 && lab.defs.units[LAB_DEF_UNIT].layer == UNIT_LAYER_AIR);
    CHECK(lab_unit(LAB_SLOT_UNIT)->position.y == 10.0f);
    // Reset results clears them and nothing else.
    lab_run(50);
    lab_reset_results(&lab);
    CHECK(lab.battle.tick == 50 && lab.results.shots == 0 && lab.results.changes == 0 && lab.change_count == 1);
}

internal u32 lab_state_hash(void)
{
    return nv_fnv1a(battle_hash(&lab.battle), &lab.results, sizeof(lab.results));
}

internal void test_lab_seek(void)
{
    lab_project();
    LabSettings settings = lab_hold_settings();
    lab_start(&lab, &defs, &settings);
    static u32 hashes[601];
    hashes[0] = lab_state_hash();
    for (u32 tick = 1; tick <= 600; ++tick) {
        // Changes on the way, at the ticks they are made: before the tick runs.
        if (tick == 101) {
            settings.distance = 26.0f;
            lab_change(&lab, &defs, &settings);
        } else if (tick == 201) {
            settings.targets_move = true;
            lab_change(&lab, &defs, &settings);
        } else if (tick == 301) {
            lab_reset_results(&lab);
        } else if (tick == 401) {
            defs.units[0].damage = 45.0f;
            lab_change(&lab, &defs, &settings);
        }
        lab_run(1);
        hashes[tick] = lab_state_hash();
    }
    // Made at tick 100, a change is part of the state at 100: the step back plays it in again.
    const u32 ticks[] = {599, 401, 400, 300, 250, 100, 99, 1, 0};
    for (u32 i = 0; i < NV_ARRAY_COUNT(ticks); ++i) {
        CHECK(lab_seek(&lab, ticks[i]));
        CHECK(lab.battle.tick == ticks[i] && lab.battle.event_count == 0);
        if (ticks[i] == 100 || ticks[i] == 300 || ticks[i] == 400) {
            // The state at that tick before the change was made: the seek put the change in, as it was when the next tick ran.
            lab_run(1);
            CHECK(lab_state_hash() == hashes[ticks[i] + 1]);
        } else {
            CHECK(lab_state_hash() == hashes[ticks[i]]);
        }
    }
    // Forward again from a step back: the recorded changes go in at their ticks.
    CHECK(lab.furthest_tick == 600);
    CHECK(lab_seek(&lab, 150));
    lab_run(450);
    CHECK(lab.battle.tick == 600 && lab_state_hash() == hashes[600]);
    // A seek forward runs on to the same state, the changes on the way included.
    CHECK(lab_seek(&lab, 50) && lab_seek(&lab, 450));
    CHECK(lab.battle.tick == 450 && lab.battle.event_count == 0 && lab_state_hash() == hashes[450]);
    CHECK(lab_seek(&lab, 600) && lab_state_hash() == hashes[600] && lab.furthest_tick == 600);
    // A change after a step back drops the ones that came later.
    CHECK(lab_seek(&lab, 150) && lab.change_count == 4);
    settings.distance = 30.0f;
    lab_change(&lab, &defs, &settings);
    CHECK(lab.change_count == 2 && lab.furthest_tick == 150);
    // Too many changes to play again: no stepping back.
    for (u32 i = 0; i < LAB_MAX_CHANGES; ++i) {
        settings.distance = i % 2 ? 30.0f : 31.0f;
        lab_change(&lab, &defs, &settings);
        lab_run(1);
    }
    CHECK(lab.changes_full && !lab_seek(&lab, 10) && lab.battle.tick == 150 + LAB_MAX_CHANGES);
    CHECK(lab_seek(&lab, 160 + LAB_MAX_CHANGES) && lab.battle.tick == 160 + LAB_MAX_CHANGES); // forward needs no record
    lab_start(&lab, &defs, &settings);
    CHECK(!lab.changes_full && lab_seek(&lab, 0) && lab.furthest_tick == 0);
}

int main(void)
{
    test_project_file();
    test_rule_editing();
    test_unit_editing();
    test_repository_file();
    test_definition_syntax();
    test_value_kinds();
    test_project_structure();
    test_definition_errors();
    test_unit_field_checks();
    test_deployment();
    test_targets_and_movement();
    test_weapon();
    test_shell_start();
    test_melee();
    test_lead_aim();
    test_damage();
    test_shield();
    test_round_end();
    test_rules_in_use();
    test_squad_file();
    test_squad_deployment();
    test_squad_round();
    test_roles();
    test_layers_format();
    test_layers_in_use();
    test_whole_round();
    test_golden_rounds();
    test_menu_round();
    test_seek();
    test_lab_setup();
    test_lab_hold();
    test_lab_walk();
    test_lab_layers();
    test_lab_moving_targets();
    test_lab_respawn();
    test_lab_melee();
    test_lab_endless();
    test_lab_changes();
    test_lab_seek();
    if (failures) {
        printf("battle_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("battle_test: all passed\n");
    return 0;
}
