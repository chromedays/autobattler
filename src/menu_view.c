// The main menu (docs/specs/main_menu.md): the round that plays behind it, its camera, and the menu's window: the title, Start
// Game and Quit, over a veil that keeps them readable. The battle's own drawing (battle_view.c) draws the round.

#include "game.h"
#include "menu.h"

#include <engine/window.h>

#include <math.h>
#include <string.h>

#define MENU_RESTART_SECONDS 3.0f            // an ended round stands this long, then plays again
#define MENU_TICKS_PER_FRAME 4               // at most; a slow frame falls behind rather than spiral
#define MENU_CAMERA_TURN (4.0f * NV_PI / 180.0f) // radians a second around the field
#define MENU_CAMERA_PITCH (35.0f * NV_PI / 180.0f)
#define MENU_CAMERA_DISTANCE_SHARE 0.75f     // of the battle's starting camera distance: closer, so the field fills the screen
#define MENU_CAMERA_SHIFT 0.15f              // of the field's width: on a wide screen the field sits right of the buttons

// The menu's sizes, CSS pixels before the UI's scale.
#define MENU_MARGIN 48.0f
#define MENU_PHONE_MARGIN 24.0f
#define MENU_BUTTON_GAP 8.0f
#define MENU_BAR_WIDTH 4.0f

void menu_view_start(Game* game)
{
    Battle* battle = &game->menu_battle;
    nv_vfx_clear(&game->vfx);
    game->accumulator = 0.0f;
    game->menu_wait = 0.0f;
    if (!game->defs_ok) {
        // No project to play: the bare ground turns behind the menu.
        memset(battle, 0, sizeof(*battle));
        battle->defs = &game->defs;
        return;
    }
    battle_init(battle, &game->defs);
    menu_place_mirrored(battle);
    if (battle_start(battle))
        ++game->menu_starts;
}

void menu_view_tick(Game* game, f32 dt)
{
    Battle* battle = &game->menu_battle;
    if (battle->phase == BATTLE_FIGHT) {
        // At 1x whatever the game's speed: the same fixed ticks, run from real time.
        game->accumulator += dt;
        for (u32 ticks = 0; game->accumulator >= BATTLE_TICK_SECONDS && battle->phase == BATTLE_FIGHT; ++ticks) {
            if (ticks == MENU_TICKS_PER_FRAME) {
                game->accumulator = 0.0f;
                break;
            }
            battle_tick(battle);
            view_on_tick(game);
            game->accumulator -= BATTLE_TICK_SECONDS;
        }
    } else if (battle->phase == BATTLE_RESULT) {
        game->accumulator = 0.0f;
        game->menu_wait += dt;
        if (game->menu_wait >= MENU_RESTART_SECONDS) {
            // The same round again: the same placement and seed.
            battle_retry(battle);
            nv_vfx_clear(&game->vfx);
            game->menu_wait = 0.0f;
            if (battle_start(battle))
                ++game->menu_starts;
        }
    }

    // The camera turns slowly around the field and takes no input. On a wide screen it looks left of the middle, so the field
    // shows right of the buttons.
    NvOrbitCamera* orbit = &game->orbit;
    game->menu_yaw = fmodf(game->menu_yaw + MENU_CAMERA_TURN * dt, 2.0f * NV_PI);
    f32 start_distance = game->field_length * (CAMERA_START_DISTANCE / CAMERA_REFERENCE_LENGTH);
    orbit->yaw = game->menu_yaw;
    orbit->pitch = MENU_CAMERA_PITCH;
    orbit->distance = start_distance * MENU_CAMERA_DISTANCE_SHARE;
    orbit->target = nv_vec3(game->field_width * 0.5f, 0.0f, game->field_length * 0.5f);
    if (game->layout.viewport.width >= game->layout.viewport.height) {
        NvVec3 right = nv_quat_rotate(nv_scene_get(game->scene, game->camera)->rotation, nv_vec3(1.0f, 0.0f, 0.0f));
        right.y = 0.0f;
        f32 length = sqrtf(nv_vec3_dot(right, right));
        if (length > 1e-4f)
            orbit->target = nv_vec3_sub(orbit->target, nv_vec3_scale(right, game->field_width * MENU_CAMERA_SHIFT / length));
    }
}

internal ImU32 black(f32 alpha)
{
    return igGetColorU32_Vec4((ImVec4_c){0.0f, 0.0f, 0.0f, alpha});
}

// A menu button: left-aligned words on a dark ground, brighter and with a bar of the player's blue on its left edge while the
// pointer is on it or the keyboard has it.
internal b32 menu_button(Game* game, const char* label, ImVec2_c size)
{
    ImGuiStyle* style = igGetStyle();
    igPushStyleColor_Vec4(ImGuiCol_Button, (ImVec4_c){0.0f, 0.0f, 0.0f, 0.35f});
    igPushStyleColor_Vec4(ImGuiCol_ButtonHovered, (ImVec4_c){0.16f, 0.2f, 0.3f, 0.75f});
    igPushStyleColor_Vec4(ImGuiCol_ButtonActive, (ImVec4_c){0.1f, 0.35f, 1.0f, 0.6f});
    igPushStyleVar_Vec2(ImGuiStyleVar_ButtonTextAlign, (ImVec2_c){0.0f, 0.5f});
    igPushStyleVar_Vec2(ImGuiStyleVar_FramePadding, (ImVec2_c){style->FramePadding.x * 2.5f, style->FramePadding.y});
    b32 pressed = igButton(label, size);
    igPopStyleVar(2);
    igPopStyleColor(3);
    b32 lit = igIsItemHovered(0) || (igIsItemFocused() && igGetIO_Nil()->NavVisible);
    if (lit) {
        ImVec2_c min = igGetItemRectMin(), max = igGetItemRectMax();
        ImDrawList_AddRectFilled(igGetWindowDrawList(), min, (ImVec2_c){min.x + MENU_BAR_WIDTH * game_ui_scale(game), max.y},
                                 igGetColorU32_Vec4((ImVec4_c){0.1f, 0.35f, 1.0f, 1.0f}), 0.0f, 0);
    }
    ++game->menu_buttons_shown;
    return pressed;
}

void menu_view_window(Game* game)
{
    ImGuiIO* io = igGetIO_Nil();
    ImGuiStyle* style = igGetStyle();
    f32 width = io->DisplaySize.x, height = io->DisplaySize.y; // CSS pixels
    f32 ui = game_ui_scale(game);
    b32 wide = width >= height;

    // The veil under the words: from the left on a wide screen, from the bottom on a tall one.
    ImDrawList* back = igGetBackgroundDrawList(NULL);
    if (wide)
        ImDrawList_AddRectFilledMultiColor(back, (ImVec2_c){0.0f, 0.0f}, (ImVec2_c){width * 0.45f, height}, black(0.75f), black(0.0f),
                                           black(0.0f), black(0.75f));
    else
        ImDrawList_AddRectFilledMultiColor(back, (ImVec2_c){0.0f, height * 0.5f}, (ImVec2_c){width, height}, black(0.0f), black(0.0f),
                                           black(0.8f), black(0.8f));

    igSetNextWindowPos((ImVec2_c){0.0f, 0.0f}, ImGuiCond_Always, (ImVec2_c){0.0f, 0.0f});
    igSetNextWindowSize((ImVec2_c){width, height}, ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    game->menu_buttons_shown = 0;
    if (igBegin("Main menu", NULL, flags)) {
        f32 margin = MENU_MARGIN * ui;
        // The title, its name for now (open question 1); one word, so no Korean row.
        const char* title = "AUTOBATTLER";
        const char* note = T("A prototype. Units and rules change often.");
        igPushFont(NULL, style->FontSizeBase * 3.0f);
        ImVec2_c title_size = igCalcTextSize(title, NULL, false, -1.0f);
        igSetCursorPos((ImVec2_c){wide ? margin : (width - title_size.x) * 0.5f, margin});
        igTextUnformatted(title, NULL);
        igPopFont();
        ImVec2_c note_size = igCalcTextSize(note, NULL, false, -1.0f);
        igSetCursorPosX(wide ? margin : fmaxf((width - note_size.x) * 0.5f, MENU_PHONE_MARGIN * ui));
        igPushTextWrapPos(width - MENU_PHONE_MARGIN * ui);
        igTextDisabled("%s", note);
        igPopTextWrapPos();

        // The buttons: under the title and halfway down on a wide screen, at the bottom (near the thumb) on a tall one.
        igPushFont(NULL, style->FontSizeBase * 1.3f);
        ImVec2_c size = wide ? (ImVec2_c){280.0f * ui, 44.0f * ui}
                             : (ImVec2_c){fminf(width - 2.0f * MENU_PHONE_MARGIN * ui, 360.0f * ui), 52.0f * ui};
        f32 gap = MENU_BUTTON_GAP * ui;
        f32 total = size.y * 2.0f + gap;
        f32 x = wide ? margin : (width - size.x) * 0.5f;
        f32 y = wide ? fmaxf(height * 0.5f - total * 0.5f, igGetCursorPosY() + margin) : height - margin - total;
        igPushStyleVar_Vec2(ImGuiStyleVar_ItemSpacing, (ImVec2_c){style->ItemSpacing.x, gap});
        igSetCursorPos((ImVec2_c){x, y});
        if (menu_button(game, TL("Start Game"), size))
            game_set_screen(game, GAME_SCREEN_PLAY);
        igSetItemDefaultFocus(); // the keyboard starts on it
        igSetCursorPosX(x);
        if (menu_button(game, TL("Quit"), size))
            nv_quit();
        igPopStyleVar(1);
        igPopFont();
        game->menu_buttons[0] = x;
        game->menu_buttons[1] = y;
        game->menu_buttons[2] = size.x;
        game->menu_buttons[3] = total;
    }
    igEnd();
}
