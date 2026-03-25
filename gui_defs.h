/**
 * @file gui_defs.h
 * @brief Definiciones centrales de tipos para la GUI de MiFutbolC
 *
 * Contiene enums, structs de estado, tema, animación y eventos.
 * No declara funciones — solo tipos y macros de escalado.
 */
#ifndef GUI_DEFS_H
#define GUI_DEFS_H

#include "menu.h"
#include <stddef.h>

/* ── Macros de escalado DPI ────────────────────────────────
 * Requieren una variable `float scale` en scope.
 * Referencia: 1280 px de ancho → scale = 1.0                */
#define GS(px)  ((float)(px) * scale)
#define GSI(px) ((int)((float)(px) * scale))

/* ── Clamp de tamaño de fuente ─────────────────────────── */
#define FONT_CLAMP(sz, lo, hi) ((sz) < (lo) ? (lo) : ((sz) > (hi) ? (hi) : (sz)))

/* ── Jerarquía tipográfica responsive ──────────────────────
 * Requieren `float scale` en scope, como GS/GSI.           */
#define FONT_TITLE  FONT_CLAMP(32.0f * scale, 14.0f, 64.0f)
#define FONT_SUB    FONT_CLAMP(20.0f * scale, 12.0f, 44.0f)
#define FONT_BODY   FONT_CLAMP(16.0f * scale, 11.0f, 36.0f)
#define FONT_SMALL  FONT_CLAMP(14.0f * scale, 10.0f, 32.0f)
#define FONT_TINY   FONT_CLAMP(12.0f * scale,  9.0f, 28.0f)

/* ── Categorías de filtro ──────────────────────────────── */

typedef enum
{
    GUI_FILTER_ALL = 0,
    GUI_FILTER_GESTION,
    GUI_FILTER_COMPETENCIA,
    GUI_FILTER_ANALISIS,
    GUI_FILTER_ADMIN,
    GUI_FILTER_COUNT
} GuiFilter;

/* ── Pantalla activa ───────────────────────────────────── */

typedef enum
{
    GUI_SCREEN_HOME = 0,
    GUI_SCREEN_MODULE = 1,
    GUI_SCREEN_DETAIL = 2
} GuiScreen;

/* ── Foco de teclado ───────────────────────────────────── */

typedef enum
{
    FOCUS_NONE = 0,
    FOCUS_SEARCH,
    FOCUS_LIST,
    FOCUS_BUTTON
} GuiFocus;

/* ── Tipos de evento ───────────────────────────────────── */

typedef enum
{
    GUI_EVT_NONE = 0,
    GUI_EVT_REBUILD_FILTER,
    GUI_EVT_RUN_SELECTED,
    GUI_EVT_OPEN_CLASSIC,
    GUI_EVT_EXIT,
    GUI_EVT_TOGGLE_THEME,
    GUI_EVT_GO_HOME,
    GUI_EVT_GO_MODULE,
    GUI_EVT_FOCUS_SEARCH,
    GUI_EVT_CLEAR_FILTER,
    GUI_EVT_TOGGLE_COMPACT,
    GUI_EVT_TOGGLE_DEBUG,
    GUI_EVT_GO_DETAIL,
    GUI_EVT_GO_BACK
} GuiEventType;

/* ── Tokens de búsqueda ───────────────────────────────── */

typedef struct
{
    char tokens[8][32];
    int count;
} QueryTokens;

/* ── Metadata de módulos ───────────────────────────────── */

typedef struct
{
    const char *title;
    const char *subtitle;
} GuiModuleInfo;

typedef struct
{
    const char *tip_1;
    const char *tip_2;
    const char *tip_3;
} GuiModuleHelp;

/* ═══════════════════════════════════════════════════════════
   Lo que sigue requiere raylib
   ═══════════════════════════════════════════════════════════ */
#ifdef ENABLE_RAYLIB_GUI
#include "raylib.h"

/* ── Tema visual ───────────────────────────────────────── */

typedef struct
{
    Color bg_main;
    Color bg_header;
    Color bg_sidebar;
    Color bg_list;
    Color text_primary;
    Color text_secondary;
    Color text_muted;
    Color text_highlight;
    Color border;
    Color accent;
    Color accent_hover;
    Color accent_primary;   /* CTA button — vivid, high contrast */
    Color accent_primary_hv;/* CTA hover */
    Color row_selected;
    Color row_hover;
    Color card_bg;
    Color card_border;
    Color toast_bg;
    Color toast_border;
    Color search_glow;      /* search box focused glow */
    Color list_focus;       /* list border when focused — subtle */
} GuiTheme;

/* ── Valor animado (lerp exponencial) ──────────────────── */

typedef struct
{
    float value;
    float target;
    float speed; /* tasa de convergencia: mayor = más rápido */
} GuiAnim;

/* ── Input snapshot (una vez por frame) ─────────────────── */

typedef struct
{
    Vector2 mouse;
    int mouse_left_pressed;
    int mouse_left_down;
    float mouse_wheel;
    int key_enter;
    int key_escape;
    int key_up;
    int key_down;
    int key_left;
    int key_right;
    int key_tab;
    int key_shift;
    int key_ctrl;
    int key_backspace;
    int key_home;
    int key_end;
    int key_page_up;
    int key_page_down;
    int key_f1, key_f2, key_f3, key_f4, key_f5, key_f9;
} InputState;

/* ── Cola de eventos (por frame) ───────────────────────── */

#define GUI_EVENT_CAP 32

typedef struct
{
    GuiEventType type;
    int param;
} GuiEvent;

typedef struct
{
    GuiEvent buf[GUI_EVENT_CAP];
    int count;
} GuiEventQueue;

/* ── Resultado de clic en lista ────────────────────────── */

typedef struct
{
    int clicked_global; /* -1 si nada fue clickeado */
} GuiListResult;

/* ── Estado central de la GUI ──────────────────────────── */

/* Layout grouping: agrupa rects principales para pasar layout fácilmente */
typedef struct {
    Rectangle header;
    Rectangle sidebar;
    Rectangle list;
    Rectangle search;
    Rectangle toast;
    Rectangle history;
    Rectangle action_btns[3];
} Layout;

typedef struct
{
    /* Datos externos (no owned) */
    const MenuItem *items;
    int item_count;

    /* Selección */
    int selected_visible;
    int selected_global;

    /* Scroll */
    int scroll_offset;
    float scroll_velocity;
    /* Accumulator for sub-row (sub-pixel) scroll from inertial wheel */
    float scroll_accum;
    GuiAnim scroll_anim;

    /* Filtro / búsqueda */
    GuiFilter active_filter;
    GuiScreen current_screen;
    char search_query[64];
    QueryTokens query_tokens;

    /* Foco */
    GuiFocus focus;
    int focused_button;

    /* Lista visible (índices virtualizados) */
    int *visible_indices;
    int visible_count;
    int visible_capacity;

    /* Toast */
    char toast_text[180];
    float toast_timer;

    /* Barra de estado */
    char status_line[160];
    char last_action[96];

    /* Modal de confirmación */
    int show_confirm;
    int confirm_global;
    int confirm_yes_focused;

    /* Historial de acciones */
    char history[5][96];
    int history_count;

    /* Blink del cursor de búsqueda */
    float cursor_blink;

    /* Factor de escala (referencia: 1280 px) */
    float scale;

    /* Flag de rebuild pendiente */
    int needs_rebuild;

    /* Ejecución pendiente */
    int pending_run;
    int pending_global;

    /* Modo compacto */
    int compact_mode;

    /* Click flash (temporizado) */
    float click_flash_timer;
    int click_flash_row;

    /* Animacion de fila seleccionada */
    GuiAnim row_highlight;

    /* Transicion entre pantallas */
    GuiScreen prev_screen;
    GuiAnim transition;

    /* Input snapshot del frame */
    InputState input;

    /* Debug overlay (F9) */
    int debug_mode;

    /* Indice de tema (0=dark, 1=light, 2=fm) */
    int theme_index;
    /* Indice de variante de color (0 = default) */
    int variant_index;

    /* Cola de eventos del frame actual */
    GuiEventQueue events;

    /* Tema activo */
    const GuiTheme *theme;
} GuiState;

#endif /* ENABLE_RAYLIB_GUI */
#endif /* GUI_DEFS_H */
