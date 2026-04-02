/**
 * @file gui_components.h
 * @brief Componentes reutilizables, layout escalado, filtros y helpers de la GUI
 *
 * Declara funciones para dibujar componentes (botón, lista, card, toast, etc.),
 * sistema de layout con escalado DPI, búsqueda/filtrado, animaciones,
 * cola de eventos y ciclo de vida del estado.
 */
#ifndef GUI_COMPONENTS_H
#define GUI_COMPONENTS_H

#include "gui_defs.h"

/* ── Datos de tema ─────────────────────────────────────── */

const GuiTheme     *gui_get_theme_dark(void);
const GuiTheme     *gui_get_theme_light(void);
const GuiTheme     *gui_get_theme_fm(void);
const GuiTheme     *gui_get_active_theme(void);
const GuiTheme     *gui_get_theme_by_index(int index);
int                 gui_get_theme_count(void);
const char         *gui_get_theme_name(int index);
int                 gui_get_variant_count(void);
const char         *gui_get_variant_name(int index);
void                gui_theme_apply_variant(const GuiTheme *base, int variant_index, GuiTheme *out);
const GuiModuleInfo *gui_get_module_info(int index);
const GuiModuleHelp *gui_get_module_help(int index);
int gui_get_module_icon(int index);

/* ── Escala ────────────────────────────────────────────── */

float gui_compute_scale(int screen_width, int screen_height);

/* ── Animaciones ───────────────────────────────────────── */

void gui_anim_tick(GuiAnim *a, float dt);
void gui_anim_set_target(GuiAnim *a, float target);
void gui_anim_snap(GuiAnim *a, float value);

/* ── Fuente / texto (estado global de fuente) ──────────── */

void    gui_load_font(void);
void    gui_unload_font(void);
Font    gui_get_font(void);
void    gui_text(const char *text, float x, float y, float size, Color color);
Vector2 gui_text_measure(const char *text, float size);

/* ── Helpers de texto responsive ────────────────────────── */

void  gui_text_truncated(const char *text, float x, float y,
                         float size, float max_width, Color color);
void  gui_text_wrapped(const char *text, Rectangle bounds,
                      float font_size, Color color);
float gui_text_fit(const char *text, float font_size,
                   float max_width, float min_size);

/* ── Normalización / búsqueda / filtro ─────────────────── */

void        gui_normalize(const char *src, char *dst, size_t dst_size);
QueryTokens gui_tokenize_query(const char *query);
int         gui_matches_query(const char *label, const QueryTokens *tokens);
int         gui_matches_filter(const MenuItem *item, GuiFilter filter);
int         gui_rebuild_visible(const MenuItem *items, int count,
                                GuiFilter filter, const QueryTokens *tokens,
                                int *indices, int *out_count);

/* ── Componentes de dibujo ─────────────────────────────── */

int  gui_btn(const GuiState *st, Rectangle rect, const char *label);
int  gui_btn_primary(const GuiState *st, Rectangle rect, const char *label);
int  gui_module_btn(const GuiState *st, Rectangle rect, const char *label, int selected, int icon_option);
void gui_draw_option_icon(int option, Rectangle dst);
void gui_card(const GuiState *st, Rectangle rect, const char *title, int value);
int  gui_tab_bar(const GuiState *st, float x, float y,
                 const char *labels[], const int counts[], int n, int active);
void gui_toast_draw(const GuiState *st, Rectangle rect, const char *text, float alpha);
void gui_scrollbar_draw(const GuiState *st, Rectangle area,
                        int total, int visible_rows, float scroll_pos);
void gui_searchbox_draw(const GuiState *st, Rectangle rect,
                        const char *query, int focused, float blink);
void gui_breadcrumb_draw(const GuiState *st, float x, float y,
                         const char *crumbs[], int count);
void gui_help_panel_draw(const GuiState *st, Rectangle rect, const GuiModuleHelp *help);
void gui_history_panel_draw(const GuiState *st, Rectangle rect,
                            const char history[][96], int count);
void gui_detail_panel_draw(const GuiState *st, Rectangle rect,
                           const MenuItem *item,
                           const char history[][96], int hist_count);
void gui_draw_module_header(const char *title, int screen_width);
Rectangle gui_draw_list_shell(Rectangle panel,
                              const char *col1,
                              float col1_x,
                              const char *col2,
                              float col2_x);
void gui_draw_list_row_bg(Rectangle row_rect, int row_index, int hovered);
void gui_draw_footer_hint(const char *text, float x, int screen_height);

/* ── Lista virtualizada ────────────────────────────────── */

typedef struct
{
    const MenuItem *items;
    int item_count;
    const int *visible_indices;
    int visible_count;
    int selected_global;
    float scroll_smooth;
    int row_h;
    const QueryTokens *tokens;
} GuiListDrawContext;

GuiListResult gui_list_draw(const GuiState *st, Rectangle area,
                            const GuiListDrawContext *ctx);

/* ── Layout escalado ───────────────────────────────────── */

Rectangle gui_lay_header(float scale, int sw);
Rectangle gui_lay_sidebar(float scale, int sw, int sh);
Rectangle gui_lay_search(float scale, int sw, Rectangle sidebar);
Rectangle gui_lay_list(float scale, int sw, int sh, Rectangle sidebar);
Rectangle gui_lay_action_btn(float scale, int sh, Rectangle sidebar, int index);
Rectangle gui_lay_toast(float scale, int sw);
Rectangle gui_lay_history(float scale, int sh, Rectangle sidebar);

/* ── Layout helpers (composable) ─────────────────────── */

Rectangle gui_lay_row(Rectangle parent, float y_off, float h);
Rectangle gui_lay_col(Rectangle parent, float x_off, float w);
Rectangle gui_lay_pad(Rectangle rect, float pad);
Rectangle gui_lay_split_h(Rectangle parent, float ratio, Rectangle *right);
Rectangle gui_lay_split_v(Rectangle parent, float ratio, Rectangle *bottom);

/* ── Persistencia de preferencias ────────────────────── */

void gui_prefs_save(const GuiState *st);
void gui_prefs_load(GuiState *st);
void gui_update_active_theme(GuiState *st);

/* ── Audio feedback ──────────────────────────────────── */

void gui_sounds_init(void);
void gui_sounds_cleanup(void);
void gui_sound_click(void);
void gui_sound_hover(void);
void gui_sound_execute(void);

/* ── Debug overlay ───────────────────────────────────── */

void gui_debug_overlay(const GuiState *st, int sw, int sh);

/* ── Pantalla de detalle (full-screen) ───────────────── */

void gui_detail_screen_draw(const GuiState *st, Rectangle area,
                            const MenuItem *item,
                            const char history[][96], int hist_count);

/* ── Cola de eventos ───────────────────────────────────── */

void gui_evt_push(GuiEventQueue *q, GuiEventType type, int param);
int  gui_evt_has(const GuiEventQueue *q, GuiEventType type);

/* ── Ciclo de vida del estado ──────────────────────────── */

void gui_state_init(GuiState *st, const MenuItem *items, int count, int initial_selected);
void gui_state_cleanup(GuiState *st);
void gui_state_push_history(GuiState *st, const char *text);
void gui_state_set_toast(GuiState *st, const char *text, float duration);
void gui_state_change_screen(GuiState *st, GuiScreen new_screen);

/* ── Input de búsqueda ─────────────────────────────────── */

int gui_input_search(char *query, int max_len, int focused);

#endif /* GUI_COMPONENTS_H */
