#pragma once

// The auto-battler executable's state (docs/specs/battle.md, "Executable"): the window, GPU and renderer, the battle, and
// what the view keeps to draw it. Nothing here is the sandbox's (sandbox/): this executable links only the engine.

#include "battle.h"
#include "lab.h"

#include <engine/camera.h>
#include <engine/imgui.h>
#include <engine/local_file.h>
#include <engine/log.h>
#include <engine/renderer.h>
#include <engine/strings.h>
#include <engine/window.h>

// Where the panel goes (CSS pixels, before the UI's scale): beside the viewport on a wide screen, under it on a tall one.
#define PANEL_WIDTH  300.0f
// The UI's own scale, on top of the engine's (NvImgui.ui_scale, larger on a touch screen): what a desktop starts at, and the
// panel's choices.
#define UI_ZOOM_DESKTOP 2.0f
#define UI_ZOOM_CHOICES 6
#define PANEL_HEIGHT_SHARE 0.42f

#define CAMERA_MIN_PITCH    (20.0f * NV_PI / 180.0f)
#define CAMERA_MAX_PITCH    (85.0f * NV_PI / 180.0f)
#define CAMERA_MIN_DISTANCE 15.0f
#define CAMERA_MAX_DISTANCE 140.0f
#define CAMERA_RADIANS_PER_PIXEL 0.008f
// The shadow distance is the camera's distance times this. From the starting camera the field's far corners are about 1.4
// times its distance away, before the fade that starts at 0.9 of the shadow distance, so every unit is shadowed; and the
// map's texels keep about the same size on screen as the camera zooms.
#define SHADOW_DISTANCE_PER_CAMERA_DISTANCE 1.6f

// The camera starts CAMERA_START_DISTANCE away from a field CAMERA_REFERENCE_LENGTH meters long; a longer or shorter field
// (the rules' `grid` and `cell_size`) scales that, and the far limits with it.
#define CAMERA_START_DISTANCE   104.0f
#define CAMERA_REFERENCE_LENGTH 96.0f
#define CAMERA_FAR_Z            400.0f

// The project file (docs/specs/abproj.md, "Loading and saving"): the one packed with the game, and the most a local one may be.
#define PROJECT_BUILT_IN  "default.abproj"
#define PROJECT_EXTENSION ".abproj"
#define PROJECT_MAX_SIZE  NV_KILOBYTES(256)

// What the panel's project buttons do (game_project_action).
typedef enum ProjectAction { PROJECT_OPEN, PROJECT_RELOAD, PROJECT_SAVE, PROJECT_SAVE_AS } ProjectAction;

// What the viewport and the panel show: the battle, or the unit lab (docs/specs/unit_lab.md).
typedef enum GameMode { GAME_MODE_BATTLE, GAME_MODE_LAB } GameMode;

// A number key of a unit def the lab's Unit section edits (docs/specs/unit_lab.md, "Editing the unit's values"): its key in the
// file, where it is in a UnitDef, how its widget drags and when it is shown. `offset` is into the UnitDef. UNIT_VALUE_WHOLE is
// a float the file writes as an integer (hp, armor, shield_hp, damage).
typedef enum UnitValueKind { UNIT_VALUE_INTEGER, UNIT_VALUE_WHOLE, UNIT_VALUE_NUMBER, UNIT_VALUE_ANGLE } UnitValueKind;
// The units a key is shown for: the keys that apply to them (docs/specs/unit_fields.md, "Keys").
typedef enum UnitValueShown {
    UNIT_VALUE_ALWAYS,
    UNIT_VALUE_AIR,        // altitude_m
    UNIT_VALUE_PROJECTILE, // not for Melee
    UNIT_VALUE_GUIDED,     // only for GuidedProjectile
    UNIT_VALUE_ARC,        // only for a ScriptedArc projectile
} UnitValueShown;
typedef struct UnitValueKey {
    const char* key;
    const char* label;
    UnitValueKind kind;
    u32 offset;
    f32 speed, min, max; // the drag's; the project's reader judges
    b32 battle_only;     // the lab's run does not read it (it is for deployment): shown last, under its own heading
    UnitValueShown shown;
} UnitValueKey;

// What the lab draws over its units, each on a box of the panel's View section (game->lab_hidden). A ring and its dimension go
// together.
enum {
    LAB_SHOW_RANGE = 1 << 0,    // the range ring and its dimension
    LAB_SHOW_STOP = 1 << 1,     // the stop distance's ring and its dimension (when the unit walks)
    LAB_SHOW_DISTANCE = 1 << 2, // to the unit's target
    LAB_SHOW_ALTITUDE = 1 << 3, // air units' heights over the ground
    LAB_SHOW_SIZES = 1 << 4,    // every body's height and width
    LAB_SHOW_PATHS = 1 << 5,    // the ways moving targets go
};

// The field's planes.
enum { FIELD_GROUND, FIELD_PLAYER_ZONE, FIELD_ENEMY_ZONE, FIELD_PLANE_COUNT };

typedef struct Layout {
    NvRect viewport; // framebuffer pixels: where the field is drawn and the camera and taps are read
    NvRect panel;
    NvRect bar;      // the playback bar, under the viewport and as wide as it
    NvSceneOutput scene;
} Layout;

// What a tap in the player's zone does there: take away the squad on that cell, or place one of the chosen unit def.
typedef struct DeployTarget {
    b32 remove;
    s32 x, row; // the corner of the footprint nearest the grid's origin
    u32 w, d;   // the footprint's cells along +X and along the rows
} DeployTarget;

typedef struct Effects {
    NvVfxEffectId explosion, shield_hit, muzzle, strike;
    NvVfxLineStyle trail;
    NvVfxDecalStyle scorch;
} Effects;

typedef struct Game {
    NvArena permanent, scratch;
    NvWindow window;
    NvGpu gpu;
    NvRenderer renderer;
    NvImgui imgui;
    NvVfx vfx;

    NvScene* scene;
    NvNodeId camera;
    NvOrbitCamera orbit;
    NvResolution resolution;
    Layout layout;

    BattleDefs defs;
    b32 defs_ok;
    f32 field_width, field_length; // meters, from the rules

    // The project the defs were read from. Its text is kept, so Save writes it back with its comments; the Rules section
    // changes only the values it edits in it.
    char* project_text; // PROJECT_MAX_SIZE bytes of permanent memory and a NUL after the text
    umm project_size;   // 0 while no good project is in place
    b32 project_edited; // the panel's Rules section changed the text since it was read or saved
    char project_name[128];
    b32 kept_is_project; // the local file the browser keeps (NvLocalFile.kept) is this project's, so Save may write it
    NvLocalFile local_file;
    char project_message[320]; // what the last open, reload or save did
    b32 project_message_bad;
    u32 project_loads;          // projects put in place, the packed one included
    Battle battle;
    f32 accumulator; // seconds of game time not yet run as ticks
    f32 speed;       // game time per real second: 0.5, 1, 2 or 4 (docs/specs/battle.md, "Time controls")
    b32 paused;      // the game clock stops; the speed is kept for when it goes on
    s32 step;        // asked by the playback bar for this frame: +1 one tick forward, -1 one tick back (both pause)
    b32 seek_wanted; // the bar's timeline asked for `seek_tick` this frame
    u32 seek_tick;
    b32 scrubbing;   // the timeline is being dragged: the game clock does not run
    u32 bar_rows;    // the rows the playback bar's widgets took last frame, which its height is made for
    f64 last_time;
    char download_text[48];

    // The view (battle_view.c)
    Effects effects;
    NvMeshId unit_meshes[BATTLE_MAX_UNIT_DEFS]; // by unit def; made when first needed, then remade in their slots
    NvNodeId field_nodes[FIELD_PLANE_COUNT];
    NvMaterialId team_materials[2];
    NvMeshId shell_mesh;
    NvMaterialId shell_material;
    NvNodeId unit_nodes[BATTLE_MAX_UNITS];       // by slot; made when first needed
    u32 unit_nodes_made;                         // slots 1 to this have a node
    NvNodeId shell_nodes[BATTLE_MAX_PROJECTILES]; // by index in Battle.projectiles
    u32 shell_nodes_made;
    u32 selected_def; // the unit type the next tap places
    // What a tap under the mouse pointer would do (a touch screen has no pointer), and whether it can.
    DeployTarget hover;
    b32 hover_shown, hover_ok;
    // The role filter (the Battle panel's toggles): a unit type shows when it has every role lit here. Not saved; a new project clears it.
    u32 role_filter;
    u32 unit_buttons_shown; // how many unit buttons the panel drew last frame (debug)
    NvMeshId field_meshes[FIELD_PLANE_COUNT];

    // The unit lab (lab.h, lab_view.c). Its settings stay while the program runs; entering the lab starts a run over.
    GameMode mode;
    Lab lab;
    LabSettings lab_settings;
    b32 lab_settings_set; // lab_settings were made for the project in place
    NvMeshId lab_meshes[LAB_DEF_COUNT];
    f32 lab_mesh_sizes[LAB_DEF_COUNT][2]; // the radius and height each lab mesh was made for
    NvMaterialId target_material, target_out_of_reach_material;
    u32 lab_hidden; // the lab's View section: the LAB_SHOW_* it hides (0 shows all); not saved and not a change of the run

    // The UI's scale (the panel's UI scale combo; not saved). The engine's ui_scale also tells a finger from a mouse, so the
    // game scales on top of it instead: the style and font are the engine's (`base_style`) times `ui_zoom`, made again at the
    // start of a frame when the panel asked for another (`ui_zoom_wanted`), never in the middle of one.
    b32 touch; // the primary pointer is a finger (the engine's ui_scale > 1)
    f32 ui_zoom, ui_zoom_wanted;
    ImGuiStyle base_style;
} Game;

// main.c
void game_layout(Game* game);
f32 game_ui_scale(const Game* game); // CSS pixels per UI pixel: the engine's scale times the game's
// Reads `text` as a project. A good one is put in place (the battle back in deployment, the view remade for it) and its text
// kept; a bad one is reported in the panel and changes nothing, unless no good project is in place yet. True when it was good.
b32 game_load_project(Game* game, const char* name, const char* text, umm size);
// The panel's Rules section (docs/specs/abproj.md, "Editing the rules"): whether the rules can be changed now, and changing
// rule `key` to `values` in the project's text. A good result is put in place at once, keeping the player's deployment where
// it still fits; a bad one changes nothing and says why in the panel. True when the change was made.
b32 game_rules_editable(const Game* game);
b32 game_set_rule(Game* game, const char* key, const f64* values, u32 count);
// The lab's Unit section (docs/specs/unit_lab.md, "Editing the unit's values"): changing number key `key` of the lab's unit
// def to `value`, as game_set_rule does for a rule. The lab takes a good change as any other.
b32 game_set_unit_value(Game* game, const char* key, f64 value);
// Entering (or leaving) the unit lab: only from deployment and with a good project. Entering starts a run.
b32 game_set_mode(Game* game, GameMode mode);
// After the panel changed game->lab_settings: the lab takes the change (lab_change).
void game_lab_settings_changed(Game* game);
// The lab's unit def: another one starts a run, its targets at its own default distance.
void game_lab_choose_unit(Game* game, u32 def);
// The battle the view draws and the frame runs: the round's, or the lab's.
Battle* game_shown_battle(Game* game);
// Whether a project button can be pressed now, and pressing it (it starts a dialog, read or write; false when it cannot).
b32 game_project_action_allowed(const Game* game, ProjectAction action);
b32 game_project_action(Game* game, ProjectAction action);

// battle_view.c
void view_build(Game* game); // materials, the shell mesh, the field's nodes, the camera and the effects, once
void view_apply_project(Game* game); // the field, the unit meshes and the camera for the project in place
void view_apply_rules(Game* game);   // the field after a rule changed; the camera starts over only if the field's size did
void view_apply_units(Game* game);   // the unit meshes after a unit def changed
void view_restart_camera(Game* game); // the battle's (or the lab's) first camera
void view_input(Game* game); // the camera from drags and the wheel; taps place and remove units
void view_on_tick(Game* game); // after each battle_tick: effects for its events and shells; clears the events
void view_update(Game* game, f32 game_dt); // every frame: nodes follow the units and shells, lines, effects' clock
void view_panel(Game* game);  // the Battle panel
void view_bar(Game* game);    // the playback bar under the viewport (docs/specs/battle.md, "Playback bar")
void view_draw_build_label(Game* game);
// Shared with lab_view.c.
void view_set_unit_mesh(Game* game, NvMeshId* mesh, const UnitDef* def); // made, or remade in its slot
void view_draw_ring(NvRenderer* renderer, NvVec3 center, NvVec3 u, NvVec3 v, f32 radius, NvVec3 color);
// The role filter and a button per unit def (with its squad and cost for `deployment`); true when one was pressed.
b32 view_unit_buttons(Game* game, u32* selected, b32 enabled, b32 deployment);

// lab_view.c
void lab_view_start(Game* game);  // the camera on the lab's unit and targets
void lab_view_draw(Game* game);    // every frame in the lab, before its units are drawn: its meshes kept up
void lab_view_overlay(Game* game); // every frame in the lab, after the camera is placed: rings, paths and dimensions
void lab_view_panel(Game* game);  // the panel's lab sections
void lab_view_restart(Game* game); // the bar's Restart: the run starts over
const UnitValueKey* lab_unit_value_key(u32 index); // NULL past the last

// strings.c
void game_strings_init(void);
