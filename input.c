#include "input.h"
#include "raylib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Internal snapshot — compatible con InputState de gui_defs.h */
static InputState s_in = {0};

/* Key arrays sized by KEY_LAST (raylib) */
static unsigned char *s_key_down = NULL;
static unsigned char *s_key_pressed = NULL;
static unsigned char *s_key_released = NULL;
static int s_key_array_size = 0;

void input_init(void)
{
    /* Allocate arrays based on KEY_LAST (raylib) when available, else fallback */
#ifdef KEY_LAST
    int maxk = KEY_LAST + 1;
#else
    int maxk = 512;
#endif
    s_key_down = (unsigned char*)malloc(maxk);
    s_key_pressed = (unsigned char*)malloc(maxk);
    s_key_released = (unsigned char*)malloc(maxk);
    if (s_key_down && s_key_pressed && s_key_released) {
        memset(s_key_down, 0, maxk);
        memset(s_key_pressed, 0, maxk);
        memset(s_key_released, 0, maxk);
        s_key_array_size = maxk;
        /* Ensure click table is in a known empty state */
        input_clear_clicks();
    }
}

void input_update(void)
{
    if (s_key_array_size > 0) {
        for (int k = 0; k < s_key_array_size; k++) {
            s_key_pressed[k] = IsKeyPressed(k);
            s_key_down[k] = IsKeyDown(k);
            s_key_released[k] = IsKeyReleased(k);
        }
    }

    s_in.mouse = GetMousePosition();
    s_in.mouse_left_pressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    s_in.mouse_left_down = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    s_in.mouse_wheel = GetMouseWheelMove();

    /* Populate commonly used flags so gui code can copy them */
    s_in.key_enter = s_key_pressed[KEY_ENTER];
    s_in.key_escape = s_key_pressed[KEY_ESCAPE];
    s_in.key_up = s_key_pressed[KEY_UP];
    s_in.key_down = s_key_pressed[KEY_DOWN];
    s_in.key_left = s_key_pressed[KEY_LEFT];
    s_in.key_right = s_key_pressed[KEY_RIGHT];
    s_in.key_tab = s_key_pressed[KEY_TAB];
    s_in.key_shift = (s_key_down[KEY_LEFT_SHIFT] || s_key_down[KEY_RIGHT_SHIFT]);
    s_in.key_ctrl = (s_key_down[KEY_LEFT_CONTROL] || s_key_down[KEY_RIGHT_CONTROL]);
    s_in.key_backspace = s_key_pressed[KEY_BACKSPACE];
    s_in.key_home = s_key_pressed[KEY_HOME];
    s_in.key_end = s_key_pressed[KEY_END];
    s_in.key_page_up = s_key_pressed[KEY_PAGE_UP];
    s_in.key_page_down = s_key_pressed[KEY_PAGE_DOWN];
    s_in.key_f1 = s_key_pressed[KEY_F1];
    s_in.key_f2 = s_key_pressed[KEY_F2];
    s_in.key_f3 = s_key_pressed[KEY_F3];
    s_in.key_f4 = s_key_pressed[KEY_F4];
    s_in.key_f5 = s_key_pressed[KEY_F5];
    s_in.key_f9 = s_key_pressed[KEY_F9];
}

const InputState *input_get_state(void)
{
    return &s_in;
}

int input_key_pressed(int key)
{
    if (key < 0 || key >= s_key_array_size) return 0;
    return s_key_pressed[key];
}

int input_key_down(int key)
{
    if (key < 0 || key >= s_key_array_size) return 0;
    return s_key_down[key];
}

int input_key_released(int key)
{
    if (key < 0 || key >= s_key_array_size) return 0;
    return s_key_released[key];
}

/* Double-click table (per-target) */
typedef struct { int id; double t; } ClickEntry;
#define INPUT_CLICK_TABLE_SIZE 64
static ClickEntry s_click_table[INPUT_CLICK_TABLE_SIZE] = {{-1, 0.0}};

int input_register_click(int target_id, int hit)
{
    const double max_dt = 0.35;
    if (!hit || !s_in.mouse_left_pressed)
        return 0;

    double now = GetTime();

    /* find existing */
    int found = -1;
    for (int i = 0; i < INPUT_CLICK_TABLE_SIZE; i++) {
        if (s_click_table[i].id == target_id) { found = i; break; }
    }
    if (found >= 0) {
        if ((now - s_click_table[found].t) <= max_dt) {
            /* consume: restart timing for this target so subsequent clicks start a new cycle */
            s_click_table[found].t = now;
            return 1;
        }
        s_click_table[found].t = now;
        return 0;
    }

    /* insert: prefer empty slot */
    int empty = -1; double oldest = now; int oldest_i = 0;
    for (int i = 0; i < INPUT_CLICK_TABLE_SIZE; i++) {
        if (s_click_table[i].id == -1) { empty = i; break; }
        if (s_click_table[i].t < oldest) { oldest = s_click_table[i].t; oldest_i = i; }
    }
    if (empty >= 0) {
        s_click_table[empty].id = target_id;
        s_click_table[empty].t = now;
        return 0;
    }
    s_click_table[oldest_i].id = target_id;
    s_click_table[oldest_i].t = now;
    return 0;
}

void input_clear_clicks(void)
{
    for (int i = 0; i < INPUT_CLICK_TABLE_SIZE; i++) s_click_table[i].id = -1, s_click_table[i].t = 0.0;
}

void input_consume_key(int key)
{
    if (key < 0 || key >= s_key_array_size) return;
    s_key_pressed[key] = 0;
    s_key_released[key] = 0;
    s_key_down[key] = 0;

    /* update snapshot flags for commonly used keys */
    if (key == KEY_ESCAPE) s_in.key_escape = 0;
    if (key == KEY_ENTER) s_in.key_enter = 0;
    if (key == KEY_BACKSPACE) s_in.key_backspace = 0;
    if (key == KEY_UP) s_in.key_up = 0;
    if (key == KEY_DOWN) s_in.key_down = 0;
    if (key == KEY_LEFT) s_in.key_left = 0;
    if (key == KEY_RIGHT) s_in.key_right = 0;
}
