/**
 * @file gui.c
 * @brief Punto de entrada de la GUI Raylib — loop principal event-driven
 *
 * Usa GuiState centralizado, componentes reutilizables, layout DPI-scaled,
 * lista virtualizada y sistema de animaciones. La logica esta desacoplada
 * del dibujo: input -> eventos -> estado -> draw.
 */
#include "gui.h"
#include "gui_components.h"
#include "db.h"
#include "input.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Estado global que persiste entre llamadas ─────────── */

static int g_last_selected_index = 0;
static int g_gui_window_initialized = 0;
static char g_context_title[96] = "Menu Principal";

#ifdef ENABLE_RAYLIB_GUI

#include "raylib.h"
#include <stdarg.h>

static char g_last_action_text[96] = "Ninguna";
static Texture2D g_app_icon_texture = {0};
static int g_app_icon_texture_loaded = 0;
static Image g_window_icon = {0};
static Texture2D g_user_icon_texture = {0};
static int g_user_icon_texture_loaded = 0;
/* Caches para evitar llamadas repetidas a GetApplicationDirectory / FileExists */
static char g_cached_app_dir[512] = {0};
static int g_cached_app_dir_ready = 0;
/* Nota: se evita declarar bandera extra; la búsqueda de icono se cachea via
    g_cached_app_dir_ready / g_user_icon_texture_loaded */

static void gui_release_icon_resources(void)
{
    if (g_app_icon_texture_loaded && g_app_icon_texture.id != 0)
    {
        UnloadTexture(g_app_icon_texture);
        g_app_icon_texture = (Texture2D){0};
        g_app_icon_texture_loaded = 0;
    }
    if (g_window_icon.data)
    {
        UnloadImage(g_window_icon);
        g_window_icon = (Image){0};
    }
    if (g_user_icon_texture_loaded && g_user_icon_texture.id != 0)
    {
        UnloadTexture(g_user_icon_texture);
        g_user_icon_texture = (Texture2D){0};
        g_user_icon_texture_loaded = 0;
    }
}

/* Centraliza la secuencia de cierre/cleanup de la GUI. Devuelve el codigo de acción proporcionado. */
static int gui_shutdown(GuiState *st, int action_code)
{
    g_last_selected_index = st->selected_global;
    snprintf(g_last_action_text, sizeof(g_last_action_text), "%s", st->last_action);
    gui_prefs_save(st);
    gui_state_cleanup(st);
    gui_sounds_cleanup();
    gui_unload_font();
    gui_release_icon_resources();
    CloseWindow();
    g_gui_window_initialized = 0;
    return action_code;
}

/* Procesa las acciones que ocurren después del EndDrawing y devuelve una acción
   no negativa si hay que salir del run loop con un código concreto. -1 indica
   que no hay acción final pendiente. */
static int gui_handle_post_frame_actions(GuiState *st, const MenuItem *items, int count)
{
    /* Ejecutar directamente al pedir ejecucion (marca pending para confirmación/ejecución) */
    if (gui_evt_has(&st->events, GUI_EVT_RUN_SELECTED) && !st->pending_run)
    {
        if (st->visible_count > 0)
        {
            st->pending_run = 1;
            st->pending_global = st->selected_global;
            snprintf(st->status_line, sizeof(st->status_line), "Ejecutando opcion %d", items[st->selected_global].opcion);
            gui_state_set_toast(st, TextFormat("Ejecutando: %s", items[st->selected_global].texto ? items[st->selected_global].texto : "(sin texto)"), 0.45f);
        }
    }

    /* Salir inmediatamente */
    if (gui_evt_has(&st->events, GUI_EVT_EXIT))
    {
        return gui_shutdown(st, GUI_ACTION_EXIT);
    }

    /* Abrir menu clasico */
    if (gui_evt_has(&st->events, GUI_EVT_OPEN_CLASSIC))
    {
        return gui_shutdown(st, GUI_ACTION_OPEN_CLASSIC_MENU);
    }

    /* Ejecucion pendiente confirmada: desmontar estado y notificar al caller */
    if (st->pending_run && st->pending_global >= 0 && st->pending_global < count)
    {
        g_last_selected_index = st->pending_global;
        if (items[st->pending_global].texto)
        {
            snprintf(st->last_action, sizeof(st->last_action), "%s", items[st->pending_global].texto);
            gui_state_push_history(st, items[st->pending_global].texto);
        }
        snprintf(g_last_action_text, sizeof(g_last_action_text), "%s", st->last_action);
        gui_prefs_save(st);
        gui_state_cleanup(st);
        gui_sound_execute();
        return GUI_ACTION_RUN_SELECTED_OPTION;
    }
    return -1;
}

/* Mark needs_rebuild only when not already set to avoid redundant writes */
static void gui_mark_needs_rebuild(GuiState *st)
{
    if (st && !st->needs_rebuild) st->needs_rebuild = 1;
}

/* Helper para setear status line con formato (reduce repeticion de snprintf) */
static void gui_set_status(GuiState *st, const char *fmt, ...)
{
    if (!st || !fmt) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(st->status_line, sizeof(st->status_line), fmt, ap);
    va_end(ap);
}

static int gui_find_app_icon_path(char *out, size_t out_sz)
{
    if (!out || out_sz == 0)
        return 0;

    {
        const char *rel_candidates[] = {
            "./Icons/MiFutbolC.png",
            "../Icons/MiFutbolC.png",
            "Icons/MiFutbolC.png",
        };
        int nr = (int)(sizeof(rel_candidates) / sizeof(rel_candidates[0]));
        for (int i = 0; i < nr; i++)
        {
            if (FileExists(rel_candidates[i]))
            {
                snprintf(out, out_sz, "%s", rel_candidates[i]);
                return 1;
            }
        }
    }

    {
        if (!g_cached_app_dir_ready) {
            const char *app_dir = GetApplicationDirectory();
            if (app_dir && app_dir[0] != '\0') {
                strncpy(g_cached_app_dir, app_dir, sizeof(g_cached_app_dir)-1);
                g_cached_app_dir[sizeof(g_cached_app_dir)-1] = '\0';
            } else {
                g_cached_app_dir[0] = '\0';
            }
            g_cached_app_dir_ready = 1;
        }
        if (g_cached_app_dir[0] != '\0') {
            {
                const char *sufs[6] = {"../../Icons/MiFutbolC.png",
                                       "../Icons/MiFutbolC.png",
                                       "Icons/MiFutbolC.png",
                                       "../../Icons/MiFutbolC.ico",
                                       "../Icons/MiFutbolC.ico",
                                       "Icons/MiFutbolC.ico"};
                char cand[512];
                for (int i = 0; i < 6; i++) {
                    /* construir ruta segura y comprobar existencia */
                    snprintf(cand, sizeof(cand), "%s%s", g_cached_app_dir, sufs[i]);
                    if (FileExists(cand)) {
                        /* copiar de forma segura al buffer de salida */
                        if (out_sz > 0) {
                            snprintf(out, out_sz, "%s", cand);
                        }
                        return 1;
                    }
                }
            }
        }
    }

    out[0] = '\0';
    return 0;
}

/* Double-click handling moved to input module: use input_register_click() */

/* ═══════════════════════════════════════════════════════════
   Recoleccion de input -> cola de eventos
   Separa completamente la lectura de input de la logica.
   ═══════════════════════════════════════════════════════════ */

/* forward-declare helper so we can call it from keyboard collection */
static void apply_filter_change(GuiState *st, int filter_index);

static void collect_keyboard_events(GuiState *st)
{
    const InputState *in = &st->input;
    GuiEventQueue *q = &st->events;

    /* Ctrl+F -> enfocar busqueda */
    if (in->key_ctrl && input_key_pressed(KEY_F))
        gui_evt_push(q, GUI_EVT_FOCUS_SEARCH, 0);

    /* ESC — context-aware back (BEFORE search to avoid conflict) */
    if (in->key_escape && st->focus != FOCUS_SEARCH && !st->show_confirm)
        gui_evt_push(q, GUI_EVT_GO_BACK, 0);

    /* Tab cycling entre zonas de foco */
    if (in->key_tab)
    {
        if (!in->key_shift)
        {
            if (st->focus == FOCUS_SEARCH)
                st->focus = FOCUS_LIST;
            else if (st->focus == FOCUS_LIST)
                st->focus = FOCUS_BUTTON;
            else
                st->focus = FOCUS_SEARCH;
        }
        else
        {
            if (st->focus == FOCUS_SEARCH)
                st->focus = FOCUS_BUTTON;
            else if (st->focus == FOCUS_BUTTON)
                st->focus = FOCUS_LIST;
            else
                st->focus = FOCUS_SEARCH;
        }
        gui_sound_hover();
    }

    /* Teclas globales */
    if (in->key_f4)
        gui_evt_push(q, GUI_EVT_TOGGLE_THEME, 0);
    if (in->key_f1)
        gui_evt_push(q, GUI_EVT_OPEN_CLASSIC, 0);
    if (in->key_f2)
        gui_evt_push(q, GUI_EVT_GO_HOME, 0);
    if (in->key_f3)
    {
        /* Cycle through modules: GESTION -> COMPETENCIA -> ANALISIS -> ADMIN -> ALL */
        int next = (st->active_filter + 1) % GUI_FILTER_COUNT;
        apply_filter_change(st, next);
    }
    if (in->key_f5)
        gui_evt_push(q, GUI_EVT_TOGGLE_COMPACT, 0);
    if (in->key_f9)
        gui_evt_push(q, GUI_EVT_TOGGLE_DEBUG, 0);

    /* Busqueda: input de texto */
    if (st->focus == FOCUS_SEARCH)
    {
        if (gui_input_search(st->search_query,
                             (int)sizeof(st->search_query), 1))
        {
            st->query_tokens = gui_tokenize_query(st->search_query);
            gui_evt_push(q, GUI_EVT_REBUILD_FILTER, 0);
        }
        if (in->key_enter)
        {
            st->focus = FOCUS_LIST;
            snprintf(st->status_line, sizeof(st->status_line),
                     "Filtro aplicado");
            gui_state_set_toast(st, "Filtro aplicado", 1.2f);
        }
        if (in->key_escape && st->search_query[0] == '\0')
            st->focus = FOCUS_LIST;
    }

    /* Lista: Enter ejecuta opcion seleccionada */
    if (st->focus == FOCUS_LIST && in->key_enter)
        gui_evt_push(q, GUI_EVT_RUN_SELECTED, 0);

    /* Botones de accion: navegacion con flechas + Enter */
    if (st->focus == FOCUS_BUTTON)
    {
        if (in->key_left && st->focused_button > 0)
            st->focused_button--;
        if (in->key_right && st->focused_button < 2)
            st->focused_button++;
        if (in->key_enter)
        {
            if (st->focused_button == 0)
                gui_evt_push(q, GUI_EVT_RUN_SELECTED, 0);
            else if (st->focused_button == 1)
                gui_evt_push(q, GUI_EVT_GO_DETAIL, 0);
            else
                gui_evt_push(q, GUI_EVT_EXIT, 0);
        }
    }
}

static void handle_mouse_focus(GuiState *st,
                               Rectangle search_rect,
                               Rectangle list_rect,
                               const Rectangle action_btns[3])
{
    if (!st->input.mouse_left_pressed)
        return;
    Vector2 mp = st->input.mouse;

    if (CheckCollisionPointRec(mp, search_rect))
    {
        st->focus = FOCUS_SEARCH;
    }
    else if (CheckCollisionPointRec(mp, list_rect))
    {
        st->focus = FOCUS_LIST;
    }
    else
    {
        for (int i = 0; i < 3; i++)
        {
            if (CheckCollisionPointRec(mp, action_btns[i]))
            {
                st->focus = FOCUS_BUTTON;
                st->focused_button = i;
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════
   Procesamiento de eventos -> actualizacion de estado
   ═══════════════════════════════════════════════════════════ */

static void process_state_events(GuiState *st)
{
    const GuiEventQueue *q = &st->events;

    if (gui_evt_has(q, GUI_EVT_FOCUS_SEARCH))
    {
        st->focus = FOCUS_SEARCH;
        gui_set_status(st, "Busqueda activa");
        gui_state_set_toast(st, "Busqueda activada (Ctrl+F)", 1.5f);
    }

    if (gui_evt_has(q, GUI_EVT_TOGGLE_THEME))
    {
        {
            int tc = gui_get_theme_count();
            if (tc <= 0) tc = 1;
            st->theme_index = (st->theme_index + 1) % tc;
            gui_update_active_theme(st);
            const char *tname = gui_get_theme_name(st->theme_index);
            gui_set_status(st, "%s", tname);
            gui_state_set_toast(st, tname, 1.1f);
        }
        gui_sound_click();
    }

    if (gui_evt_has(q, GUI_EVT_GO_HOME))
    {
        gui_state_change_screen(st, GUI_SCREEN_HOME);
        st->active_filter = GUI_FILTER_ALL;
        st->search_query[0] = '\0';
        st->query_tokens = gui_tokenize_query(st->search_query);
        st->focus = FOCUS_LIST;
        gui_mark_needs_rebuild(st);
        gui_set_status(st, "Vista: Inicio");
        gui_state_set_toast(st, "Vista Inicio", 1.2f);
        gui_sound_click();
    }

    if (gui_evt_has(q, GUI_EVT_GO_MODULE))
    {
        gui_state_change_screen(st, GUI_SCREEN_MODULE);
        if (st->active_filter == GUI_FILTER_ALL)
            st->active_filter = GUI_FILTER_GESTION;
        st->search_query[0] = '\0';
        st->query_tokens = gui_tokenize_query(st->search_query);
        st->focus = FOCUS_LIST;
        gui_mark_needs_rebuild(st);
        gui_set_status(st, "Vista: Modulo %s",
                   gui_get_module_info(st->active_filter)->title);
        gui_state_set_toast(st,
                            TextFormat("Modulo %s",
                                       gui_get_module_info(st->active_filter)->title),
                            1.4f);
        gui_sound_click();
    }

        if (gui_evt_has(q, GUI_EVT_REBUILD_FILTER))
            gui_mark_needs_rebuild(st);

    if (gui_evt_has(q, GUI_EVT_CLEAR_FILTER))
    {
        st->search_query[0] = '\0';
        st->query_tokens = gui_tokenize_query(st->search_query);
        st->focus = FOCUS_LIST;
        gui_mark_needs_rebuild(st);
        gui_set_status(st, "Filtro limpiado");
        gui_state_set_toast(st, "Filtro limpiado", 1.0f);
    }

    if (gui_evt_has(q, GUI_EVT_TOGGLE_COMPACT))
    {
        st->compact_mode = !st->compact_mode;
        gui_set_status(st, st->compact_mode ? "Modo compacto" : "Modo detallado");
        gui_state_set_toast(st,
                            st->compact_mode ? "Modo compacto activado"
                                             : "Modo detallado activado",
                            1.2f);
        gui_sound_click();
    }

    if (gui_evt_has(q, GUI_EVT_TOGGLE_DEBUG))
    {
        st->debug_mode = !st->debug_mode;
        gui_state_set_toast(st, st->debug_mode ? "Debug ON (F9)" : "Debug OFF", 0.8f);
    }

    if (gui_evt_has(q, GUI_EVT_GO_DETAIL))
    {
        if (st->visible_count > 0 && st->current_screen != GUI_SCREEN_DETAIL)
        {
            gui_state_change_screen(st, GUI_SCREEN_DETAIL);
            gui_set_status(st, "Vista: Detalle");
            gui_state_set_toast(st, "Vista detalle", 0.8f);
            gui_sound_click();
        }
    }

    if (gui_evt_has(q, GUI_EVT_GO_BACK))
    {
        gui_sound_click();
        if (st->current_screen == GUI_SCREEN_DETAIL) {
            gui_state_change_screen(st, st->prev_screen);
            gui_state_set_toast(st, "Volviendo", 0.8f);
        } else if (st->current_screen == GUI_SCREEN_MODULE) {
            gui_state_change_screen(st, GUI_SCREEN_HOME);
            st->active_filter = GUI_FILTER_ALL;
            gui_mark_needs_rebuild(st);
            gui_set_status(st, "Vista Inicio");
            gui_state_set_toast(st, "Vista Inicio", 0.8f);
        } else if (st->current_screen == GUI_SCREEN_HOME) {
            /* En lugar de salir completamente, devolver control al menu clasico */
            gui_evt_push(&st->events, GUI_EVT_OPEN_CLASSIC, 0);
        }
    }
}

/* ═══════════════════════════════════════════════════════════
   Rebuild de lista visible (solo si cambio filtro/busqueda)
   ═══════════════════════════════════════════════════════════ */

static void rebuild_if_needed(GuiState *st)
{
    if (!st->needs_rebuild)
        return;
    st->needs_rebuild = 0;

    GuiFilter eff = (st->current_screen == GUI_SCREEN_HOME)
                        ? GUI_FILTER_ALL
                        : st->active_filter;

    gui_rebuild_visible(st->items, st->item_count, eff,
                        &st->query_tokens,
                        st->visible_indices, &st->visible_count);

    /* Mantener seleccion dentro del set visible */
    if (st->visible_count > 0)
    {
        int found = 0;
        int vis_index = 0;
        for (int i = 0; i < st->visible_count; i++)
        {
            if (st->visible_indices[i] == st->selected_global)
            {
                found = 1;
                vis_index = i;
                break;
            }
        }
        if (!found)
            st->selected_global = st->visible_indices[0], vis_index = 0;
        st->selected_visible = vis_index;
    }

    gui_anim_snap(&st->scroll_anim, 0.0f);
    st->scroll_offset = 0;
}

/* ═══════════════════════════════════════════════════════════
   Navegacion de lista (teclado + rueda de mouse)
   ═══════════════════════════════════════════════════════════ */

/* Forward declaration for helper used below */
static int gui_find_visible_index(const GuiState *st, int global);

static void update_list_navigation(GuiState *st,
                                   Rectangle list_area, int row_h)
{
    int old_selected = st->selected_global;
    int changed_by_keys = 0;
    int visible_rows = (int)(list_area.height / (float)row_h);
    if (visible_rows < 1)
        visible_rows = 1;
    int max_scroll = (st->visible_count > visible_rows)
                         ? (st->visible_count - visible_rows)
                         : 0;

    /* Usar cached selected_visible si coincide con selected_global, si no re-scanear */
    int sel_vis = 0;
    if (st->visible_count > 0) {
        if (st->selected_visible >= 0 && st->selected_visible < st->visible_count &&
            st->visible_indices[st->selected_visible] == st->selected_global)
        {
            sel_vis = st->selected_visible;
        } else {
            int found = gui_find_visible_index(st, st->selected_global);
            sel_vis = (found >= 0) ? found : 0;
        }
    }

    /* Navegacion por teclado */
    if (st->focus == FOCUS_LIST)
    {
        if (input_key_pressed(KEY_DOWN) && sel_vis < st->visible_count - 1)
        {
            sel_vis++;
            changed_by_keys = 1;
        }
        if (input_key_pressed(KEY_UP) && sel_vis > 0)
        {
            sel_vis--;
            changed_by_keys = 1;
        }
        if (input_key_pressed(KEY_PAGE_DOWN))
        {
            sel_vis = (sel_vis + visible_rows < st->visible_count)
                          ? sel_vis + visible_rows
                          : (st->visible_count > 0
                                 ? st->visible_count - 1
                                 : 0);
            changed_by_keys = 1;
        }
        if (input_key_pressed(KEY_PAGE_UP))
        {
            sel_vis = (sel_vis - visible_rows >= 0)
                          ? sel_vis - visible_rows
                          : 0;
            changed_by_keys = 1;
        }
        if (input_key_pressed(KEY_HOME))
        {
            sel_vis = 0;
            changed_by_keys = 1;
        }
        if (input_key_pressed(KEY_END) && st->visible_count > 0)
        {
            sel_vis = st->visible_count - 1;
            changed_by_keys = 1;
        }
    }

    if (st->visible_count > 0)
        st->selected_global = st->visible_indices[sel_vis];

    /* Cache the visible index for future frames */
    if (st->visible_count > 0)
        st->selected_visible = sel_vis;

    /* Trigger row-highlight animation on selection change */
    if (st->selected_global != old_selected)
    {
        gui_anim_snap(&st->row_highlight, 0.3f);
        gui_anim_set_target(&st->row_highlight, 1.0f);
    }

    /* Rueda de mouse sobre la lista: inercia suave */
    if (CheckCollisionPointRec(st->input.mouse, list_area))
    {
        float wheel = st->input.mouse_wheel;
        if (wheel != 0.0f)
        {
            /* Invertimos para mantener la direccion esperada (wheel>0 => scroll up) */
            const float accel = 0.6f; /* ajuste fino */
            st->scroll_velocity -= wheel * accel;
        }
    }

    /* Aplicar velocidad (inercial) y friccion */
    if (fabsf(st->scroll_velocity) > 0.001f)
    {
        /* Accumular en float para preservar movimiento sub-row (sub-pixel) */
        st->scroll_accum += st->scroll_velocity;
        int delta = (int)roundf(st->scroll_accum);
        if (delta != 0) {
            st->scroll_offset += delta;
            st->scroll_accum -= (float)delta;
        }
        /* friccion */
        st->scroll_velocity *= 0.78f;
        if (fabsf(st->scroll_velocity) < 0.02f) st->scroll_velocity = 0.0f;
    }

    /* Auto-scroll solo cuando cambia la seleccion por teclado */
    if (changed_by_keys)
    {
        if (sel_vis < st->scroll_offset)
            st->scroll_offset = sel_vis;
        if (sel_vis >= st->scroll_offset + visible_rows)
            st->scroll_offset = sel_vis - visible_rows + 1;
    }
    if (st->scroll_offset < 0)
        st->scroll_offset = 0;
    if (st->scroll_offset > max_scroll)
        st->scroll_offset = max_scroll;

    /* Animar scroll suave */
    gui_anim_set_target(&st->scroll_anim, (float)st->scroll_offset);

    st->selected_visible = sel_vis;
}

/* Buscar el índice visible correspondiente a un índice global; -1 si no se encuentra */
static int gui_find_visible_index(const GuiState *st, int global)
{
    if (!st || st->visible_count <= 0) return -1;
    for (int i = 0; i < st->visible_count; i++) {
        if (st->visible_indices[i] == global) return i;
    }
    return -1;
}

/* ═══════════════════════════════════════════════════════════
   Secciones de dibujo (desacopladas del loop principal)
   ═══════════════════════════════════════════════════════════ */

static void draw_header_left(GuiState *st, Rectangle header_rect)
{
    float scale = st->scale;
    /* Icono principal de la app junto al titulo */
    Rectangle app_icon = {GS(16), GS(14), GS(36), GS(36)};
    if (g_app_icon_texture_loaded && g_app_icon_texture.id != 0)
    {
        Rectangle src = {0.0f, 0.0f,
                         (float)g_app_icon_texture.width,
                         (float)g_app_icon_texture.height};
        DrawTexturePro(g_app_icon_texture, src, app_icon, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
    }
    else
    {
        gui_draw_option_icon(15, app_icon);
    }

    gui_text("MiFutbolC", GS(58), GS(20), FONT_TITLE, st->theme->text_primary);
    gui_text_truncated(g_context_title[0] ? g_context_title : "Menu",
             GS(260), GS(34), FONT_SUB, GS(240), st->theme->text_secondary);
}

static void draw_header_right(GuiState *st, Rectangle header_rect)
{
    float scale = st->scale;
    /* Informacion de usuario y fecha a la derecha */
    time_t now = time(NULL);
    /* Cachear la representación de la fecha por segundo para evitar llamadas
       costosas a localtime() cada frame. */
    static time_t g_cached_time_sec = 0;
    static char g_cached_fecha_buf[64] = "";
    if (now != g_cached_time_sec) {
        struct tm *lt = localtime(&now);
        if (lt)
            strftime(g_cached_fecha_buf, sizeof(g_cached_fecha_buf), "%d/%m/%Y %H:%M", lt);
        else
            snprintf(g_cached_fecha_buf, sizeof(g_cached_fecha_buf), "?");
        g_cached_time_sec = now;
    }
    const char *fecha_buf = g_cached_fecha_buf;

    /* Alinear bloque [icono + texto] respecto al margen derecho */
    float rx = header_rect.width - GS(260) - GS(10);
    if (rx < GS(500)) rx = GS(500);

    const char *db_user = db_get_active_user();
    if (!db_user || db_user[0] == '\0') db_user = "Usuario";

    /* Layout del bloque: icono a la izquierda, texto a la derecha */
    float icon_size = GS(36);
    float spacing = GS(8);
    Rectangle user_icon = { rx, header_rect.y + GS(12), icon_size, icon_size };
    float text_x = rx + icon_size + spacing;

    /* User icon is loaded during initialization; just draw if available. */

    if (g_user_icon_texture_loaded && g_user_icon_texture.id != 0)
    {
        Rectangle src = { 0.0f, 0.0f, (float)g_user_icon_texture.width, (float)g_user_icon_texture.height };
        DrawTexturePro(g_user_icon_texture, src, user_icon, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
    }

    gui_text(TextFormat("Usuario: %s", db_user), text_x, GS(16), FONT_BODY, st->theme->text_primary);
    gui_text(TextFormat("Fecha: %s", fecha_buf), text_x, GS(40), FONT_BODY, st->theme->text_secondary);
}

static void draw_header(GuiState *st, Rectangle header_rect)
{
    DrawRectangleRec(header_rect, st->theme->bg_header);
    draw_header_left(st, header_rect);
    draw_header_right(st, header_rect);
}

static void apply_filter_change(GuiState *st, int filter_index)
{
    st->active_filter = (GuiFilter)filter_index;
    GuiScreen target = (filter_index == 0) ? GUI_SCREEN_HOME : GUI_SCREEN_MODULE;
    gui_state_change_screen(st, target);
    st->search_query[0] = '\0';
        st->query_tokens = gui_tokenize_query(st->search_query);
        st->focus = FOCUS_LIST;
        gui_mark_needs_rebuild(st);
    snprintf(st->status_line, sizeof(st->status_line),
             "Modulo activo: %s",
             gui_get_module_info(st->active_filter)->title);
    gui_state_set_toast(st,
                        TextFormat("Modulo activo: %s",
                                   gui_get_module_info(st->active_filter)->title),
                        1.2f);
    gui_sound_click();
}

static void draw_sidebar(GuiState *st, Rectangle sidebar_rect)
{
    float scale = st->scale;

    DrawRectangleRec(sidebar_rect, st->theme->bg_sidebar);

    /* Titulo dinamico: item seleccionado o modulo activo, con icono */
    const char *sidebar_title = gui_get_module_info(st->active_filter)->title;
    int sidebar_title_icon = -1;
    if (st->selected_global >= 0 && st->selected_global < st->item_count)
    {
        const char *sel_txt = st->items[st->selected_global].texto;
        if (sel_txt && sel_txt[0] != '\0') {
            sidebar_title = sel_txt;
            sidebar_title_icon = st->items[st->selected_global].opcion;
        }
    }
    if (sidebar_title_icon >= 0) {
        Rectangle title_icon = {GS(14), sidebar_rect.y + GS(16), GS(28), GS(28)};
        gui_draw_option_icon(sidebar_title_icon, title_icon);
        gui_text_truncated(sidebar_title, GS(48), sidebar_rect.y + GS(22), FONT_SUB,
                 sidebar_rect.width - GS(58), st->theme->text_primary);
    } else {
        gui_text(sidebar_title, GS(24), sidebar_rect.y + GS(24), FONT_SUB,
                 st->theme->text_primary);
    }

    /* Botones de modulo con iconos representativos (centralizado) */
    for (int i = 0; i < GUI_FILTER_COUNT; i++)
    {
        const char *label = gui_get_module_info(i)->title;
        int icon = gui_get_module_icon(i);
        Rectangle mr = {
            GS(20),
            sidebar_rect.y + GS(66) + (float)i * GS(48),
            sidebar_rect.width - GS(40),
            GS(40)};
        if (gui_module_btn(st, mr, label,
                           (int)st->active_filter == i,
                           icon))
        {
            apply_filter_change(st, i);
        }
    }

    /* Panel de ayuda */
    Rectangle help_rect = {
        GS(20), sidebar_rect.y + GS(316),
        sidebar_rect.width - GS(40), GS(160)};
    gui_help_panel_draw(st, help_rect,
                        gui_get_module_help(st->active_filter));
}

static void draw_stat_cards(GuiState *st, float list_x)
{
    /* Barra superior de tabs deshabilitada por solicitud de UX. */
    (void)st;
    (void)list_x;
}

/* ═══════════════════════════════════════════════════════════
   Punto de entrada principal de la GUI
   ═══════════════════════════════════════════════════════════ */

int run_raylib_gui(const MenuItem *items, int count)
{
    if (!items || count <= 0)
        return GUI_ACTION_OPEN_CLASSIC_MENU;

    /* ── Inicializar estado ─────────────────── */
    GuiState st;
    gui_state_init(&st, items, count, g_last_selected_index);
    if (!st.visible_indices)
        return GUI_ACTION_OPEN_CLASSIC_MENU;

    /* Copiar ultima accion persistente */
    snprintf(st.last_action, sizeof(st.last_action), "%s",
             g_last_action_text);

    /* Cargar preferencias guardadas */
    gui_prefs_load(&st);

    /* Build inicial de lista visible */
    {
        GuiFilter eff = (st.current_screen == GUI_SCREEN_HOME)
                            ? GUI_FILTER_ALL : st.active_filter;
        gui_rebuild_visible(items, count, eff,
                            &st.query_tokens,
                            st.visible_indices, &st.visible_count);
    }

    /* ── Inicializar ventana (solo la primera vez) ── */
    if (!g_gui_window_initialized)
    {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
        InitWindow(1280, 720, "MiFutbolC - GUI");

        {
            char icon_path[512];
            if (gui_find_app_icon_path(icon_path, sizeof(icon_path)))
            {
                Image app_img = LoadImage(icon_path);
                if (app_img.data)
                {
                    /* SetWindowIcon requiere RGBA de 32 bits. */
                    if (app_img.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
                        ImageFormat(&app_img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

                    if (g_window_icon.data)
                        UnloadImage(g_window_icon);

                    g_window_icon = ImageCopy(app_img);
                    if (g_window_icon.data)
                    {
                        ImageResize(&g_window_icon, 64, 64);
                        SetWindowIcon(g_window_icon);
                    }
                    else
                    {
                        SetWindowIcon(app_img);
                        g_window_icon = app_img;
                        app_img.data = NULL;
                    }
                    g_app_icon_texture = LoadTextureFromImage(app_img);
                    if (g_app_icon_texture.id != 0)
                        g_app_icon_texture_loaded = 1;

                    if (app_img.data)
                        UnloadImage(app_img);
                }
            }
        }

        SetExitKey(KEY_NULL);
        MaximizeWindow();
        gui_load_font();   /* carga fuente + iconos (resuelve base path) */
        gui_sounds_init();
        /* Cargar icono de usuario de forma eager durante la inicialización
           en lugar de intentar hacerlo cada frame en la función de dibujo. */
        if (!g_user_icon_texture_loaded)
        {
            const char *rel_candidates[] = {"./Icons/Usuario.png","../Icons/Usuario.png","../../Icons/Usuario.png","Icons/Usuario.png"};
            for (int ci = 0; ci < (int)(sizeof(rel_candidates)/sizeof(rel_candidates[0])); ci++) {
                if (FileExists(rel_candidates[ci])) {
                    g_user_icon_texture = LoadTexture(rel_candidates[ci]);
                    if (g_user_icon_texture.id != 0) {
                        SetTextureFilter(g_user_icon_texture, TEXTURE_FILTER_BILINEAR);
                        g_user_icon_texture_loaded = 1;
                        break;
                    }
                }
            }
            if (!g_user_icon_texture_loaded && !g_cached_app_dir_ready) {
                const char *tmp = GetApplicationDirectory();
                if (tmp && tmp[0] != '\0') strncpy(g_cached_app_dir, tmp, sizeof(g_cached_app_dir)-1);
                g_cached_app_dir_ready = 1;
            }
            if (!g_user_icon_texture_loaded && g_cached_app_dir[0] != '\0')
            {
                {
                    const char *sufs[3] = {"../../Icons/Usuario.png",
                                           "../Icons/Usuario.png",
                                           "Icons/Usuario.png"};
                    char cand[512];
                    for (int ai = 0; ai < 3; ai++) {
                        snprintf(cand, sizeof(cand), "%s%s", g_cached_app_dir, sufs[ai]);
                        if (FileExists(cand)) {
                            g_user_icon_texture = LoadTexture(cand);
                            if (g_user_icon_texture.id != 0) {
                                SetTextureFilter(g_user_icon_texture, TEXTURE_FILTER_BILINEAR);
                                g_user_icon_texture_loaded = 1;
                                break;
                            }
                        }
                    }
                }
            }
        }
        SetTargetFPS(60);
        input_init();
        g_gui_window_initialized = 1;
    }

    /* ═══════════════════════════════════════════
       Loop principal: Input -> Eventos -> Estado -> Draw
       ═══════════════════════════════════════════ */
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        st.scale = gui_compute_scale(sw, sh);
        float scale = st.scale;
        int row_h = st.compact_mode ? GSI(26) : GSI(34);

        /* ── Snapshot de input (centralizado una vez por frame) ── */
        input_update();
        const InputState *inp = input_get_state();
        st.input = *inp;

        /* ── Tick de animaciones ──────────────── */
        gui_anim_tick(&st.scroll_anim, dt);
        if (st.toast_timer > 0.0f)
            st.toast_timer -= dt;
        st.cursor_blink += dt;
        if (st.cursor_blink >= 1.0f)
            st.cursor_blink = 0.0f;
        if (st.click_flash_timer > 0.0f)
            st.click_flash_timer -= dt;
        gui_anim_tick(&st.row_highlight, dt);
        gui_anim_tick(&st.transition, dt);

        /* ── Layout (todo escalado por DPI) ───── */
        Rectangle header = gui_lay_header(scale, sw);
        Rectangle sidebar = gui_lay_sidebar(scale, sw, sh);
        Rectangle list = gui_lay_list(scale, sw, sh, sidebar);
        Rectangle search = gui_lay_search(scale, sw, sidebar);
        Rectangle toast_r = gui_lay_toast(scale, sw);
        Rectangle hist_r = gui_lay_history(scale, sh, sidebar);
        Rectangle action_btns[3];
        for (int i = 0; i < 3; i++)
            action_btns[i] = gui_lay_action_btn(scale, sh, sidebar, i);

        /* ── Fase 1: recolectar input ─────────── */
        st.events.count = 0;
        collect_keyboard_events(&st);
        handle_mouse_focus(&st, search, list, action_btns);

        /* ── Fase 2: procesar eventos keyboard ── */
        process_state_events(&st);
        rebuild_if_needed(&st);

        /* ── Fase 3: navegacion de lista ──────── */
        update_list_navigation(&st, list, row_h);

        /* ── Fase 4: dibujo ───────────────────── */
        BeginDrawing();
        ClearBackground(st.theme->bg_main);

        /* Header */
        draw_header(&st, header);

        /* Sidebar */
        draw_sidebar(&st, sidebar);

        /* Tab bar (filtros clickeables) */
        /* Aplicar desplazamiento de 'slide' durante transiciones entre pantallas */
        float slide_offset = 0.0f;
        if (st.transition.value < 0.999f) {
            int dir = 0; /* -1 = entra desde derecha (slide left), +1 = vuelve desde la izquierda */
            if (st.current_screen == GUI_SCREEN_DETAIL) dir = -1;
            else if (st.prev_screen == GUI_SCREEN_DETAIL) dir = 1;
            /* suavizado (ease) para el movimiento */
            float s = 1.0f - st.transition.value; /* 1 -> al inicio de la transicion */
            float eased = s * s * (3.0f - 2.0f * s); /* smoothstep */
            slide_offset = eased * (float)sw * (float)dir;
        }
        float content_x = sidebar.width + GS(30) + slide_offset;
        draw_stat_cards(&st, content_x);

        /* Breadcrumb */
        {
            const char *crumbs[3];
            int nc = 0;
            crumbs[nc++] = "Inicio";
            if (st.current_screen != GUI_SCREEN_HOME)
                crumbs[nc++] = gui_get_module_info(st.active_filter)->title;
            gui_breadcrumb_draw(&st, content_x, GS(122), crumbs, nc);
        }
        /* ── Contenido principal: Detalle / Lista ── */
        if (st.current_screen == GUI_SCREEN_DETAIL) {
            const MenuItem *sel_item = NULL;
            if (st.selected_global >= 0 && st.selected_global < count)
                sel_item = &items[st.selected_global];
            Rectangle detail_area = {search.x, search.y,
                                     list.width,
                                     (list.y + list.height) - search.y};
            gui_detail_screen_draw(&st, detail_area, sel_item,
                                   (const char (*)[96])st.history,
                                   st.history_count);
            if (gui_btn_primary(&st, action_btns[0], "Ejecutar"))
                gui_evt_push(&st.events, GUI_EVT_RUN_SELECTED, 0);
            if (gui_btn(&st, action_btns[1], "Volver (ESC)"))
                gui_evt_push(&st.events, GUI_EVT_GO_BACK, 0);
        } else {
        /* Barra de busqueda */
        gui_searchbox_draw(&st, search, st.search_query,
                           st.focus == FOCUS_SEARCH, st.cursor_blink);

        /* Boton limpiar filtro */
        if (gui_btn(&st,
                    (Rectangle){search.x + search.width + GS(8), search.y,
                                GS(110), GS(30)},
                    "Limpiar"))
        {
            gui_evt_push(&st.events, GUI_EVT_CLEAR_FILTER, 0);
        }

        /* Acciones rapidas (arriba de la lista) */
        gui_text("Acciones rapidas:",
                 content_x, GS(184), FONT_SMALL,
                 st.theme->text_secondary);
        {
            int qn = st.visible_count < 3 ? st.visible_count : 3;
            for (int i = 0; i < qn; i++)
            {
                int idx = st.visible_indices[i];
                char qlabel[64];
                snprintf(qlabel, sizeof(qlabel), "(%d) %s",
                         i + 1,
                         items[idx].texto ? items[idx].texto : "(sin texto)");
                Rectangle quick_btn_rect = {
                    content_x + GS(130) + (float)i * GS(200),
                    GS(180), GS(190), GS(26)};
                if (gui_btn(&st, quick_btn_rect, qlabel))
                {
                    st.selected_global = idx;
                    gui_evt_push(&st.events, GUI_EVT_RUN_SELECTED, 0);
                }

                if (input_register_click(200 + i,
                                         CheckCollisionPointRec(st.input.mouse, quick_btn_rect)))
                {
                    st.selected_global = idx;
                    gui_evt_push(&st.events, GUI_EVT_RUN_SELECTED, 0);
                    gui_sound_click();
                }
            }
        }

        /* Area de lista */
        DrawRectangleRec(list, st.theme->bg_list);
        if (st.focus == FOCUS_LIST)
            DrawRectangleLinesEx(list, 1.5f, st.theme->list_focus);

        /* Lista virtualizada */
        GuiListResult lr = gui_list_draw(
            &st, list, items, count,
            st.visible_indices, st.visible_count,
            st.selected_global, st.scroll_anim.value,
            row_h, &st.query_tokens);

        if (lr.clicked_global >= 0)
        {
            if (lr.clicked_global == st.selected_global)
                gui_evt_push(&st.events, GUI_EVT_RUN_SELECTED, 0);
            st.selected_global = lr.clicked_global;
            st.click_flash_timer = 0.35f;
            st.click_flash_row = lr.clicked_global;
            gui_anim_snap(&st.row_highlight, 0.3f);
            gui_anim_set_target(&st.row_highlight, 1.0f);
            gui_sound_click();
        }

        /* Scrollbar */
        int vis_rows = (int)(list.height / (float)row_h);
        if (vis_rows < 1)
            vis_rows = 1;
        gui_scrollbar_draw(&st, list, st.visible_count,
                           vis_rows, st.scroll_anim.value);

        /* Botones de accion principales */
        if (gui_btn_primary(&st, action_btns[0], "Ejecutar (Enter)"))
        {
            st.focus = FOCUS_BUTTON;
            st.focused_button = 0;
            gui_evt_push(&st.events, GUI_EVT_RUN_SELECTED, 0);
        }
        if (input_register_click(100,
                     CheckCollisionPointRec(st.input.mouse, action_btns[0])))
            gui_evt_push(&st.events, GUI_EVT_RUN_SELECTED, 0);

        if (gui_btn(&st, action_btns[1], "Ver detalle"))
        {
            st.focus = FOCUS_BUTTON;
            st.focused_button = 1;
            gui_evt_push(&st.events, GUI_EVT_GO_DETAIL, 0);
        }
        if (input_register_click(101,
                     CheckCollisionPointRec(st.input.mouse, action_btns[1])))
            gui_evt_push(&st.events, GUI_EVT_GO_DETAIL, 0);

        if (gui_btn(&st, action_btns[2], "Salir"))
        {
            st.focus = FOCUS_BUTTON;
            st.focused_button = 2;
            gui_evt_push(&st.events, GUI_EVT_EXIT, 0);
        }
        if (input_register_click(102,
                     CheckCollisionPointRec(st.input.mouse, action_btns[2])))
            gui_evt_push(&st.events, GUI_EVT_EXIT, 0);

        if (st.focus == FOCUS_BUTTON)
            DrawRectangleLinesEx(action_btns[st.focused_button],
                                 2.0f, st.theme->text_highlight);
        } /* end else (list mode) */

        /* Barra de estado */
        DrawRectangle(0, sh - GSI(28), sw, GSI(28),
                      (Color){10, 20, 15, 255});
        {
            const char *mode_txt = st.compact_mode ? "[Compacto]" : "[Detallado]";
            gui_text_truncated(
                TextFormat("%s | %s %s | Ctrl+F buscar | TAB foco | F4 tema | F5 modo | F9 debug",
                           st.status_line, st.last_action, mode_txt),
                GS(14), (float)sh - GS(22), FONT_SMALL,
                (float)sw - GS(28), st.theme->text_muted);
        }

        /* Toast */
        if (st.toast_timer > 0.0f)
        {
            float alpha = 1.0f;
            if (st.toast_timer < 0.25f)
                alpha = st.toast_timer / 0.25f;
            /* añadir leve animacion de entrada: slide vertical basado en alpha */
            float eased = 1.0f - powf(1.0f - (alpha > 1.0f ? 1.0f : alpha), 2.0f);
            Rectangle toast_r2 = toast_r;
            toast_r2.y += GS(12) * (1.0f - eased);
            gui_toast_draw(&st, toast_r2, st.toast_text, alpha);
        }

        /* Panel de detalle del item seleccionado (sidebar) */
        if (st.current_screen != GUI_SCREEN_DETAIL)
        {
            const MenuItem *sel_item = NULL;
            if (st.selected_global >= 0 && st.selected_global < count)
                sel_item = &items[st.selected_global];
            gui_detail_panel_draw(&st, hist_r, sel_item,
                                  (const char (*)[96])st.history, st.history_count);
        }

        /* Transition fade overlay */
        if (st.transition.value < 0.97f) {
            float fade = 1.0f - st.transition.value;
            unsigned char alpha = (unsigned char)(fade * 200.0f);
            DrawRectangle(0, GSI(84), sw, sh - GSI(84),
                          (Color){st.theme->bg_main.r, st.theme->bg_main.g,
                                  st.theme->bg_main.b, alpha});
        }

        /* Debug overlay */
        if (st.debug_mode)
            gui_debug_overlay(&st, sw, sh);

        /* Sin confirmacion modal: ejecucion directa desde Enter/Boton */

        /* Mouse cursor feedback */
        {
            Vector2 mp = GetMousePosition();
            int any_hover = 0;
            if (CheckCollisionPointRec(mp, list)) any_hover = 1;
            for (int bi = 0; bi < 3; bi++)
                if (CheckCollisionPointRec(mp, action_btns[bi])) any_hover = 1;
            if (CheckCollisionPointRec(mp, search)) any_hover = 1;
            SetMouseCursor(any_hover ? MOUSE_CURSOR_POINTING_HAND
                                     : MOUSE_CURSOR_DEFAULT);
        }

        EndDrawing();
        /* ── Fase 5: post-draw (eventos de accion ya en cola) ── */
        int post_action = gui_handle_post_frame_actions(&st, items, count);
        if (post_action >= 0)
            return post_action;
    }

    /* Cierre por WindowShouldClose */
    g_last_selected_index = st.selected_global;
    snprintf(g_last_action_text, sizeof(g_last_action_text),
             "%s", st.last_action);
    gui_prefs_save(&st);
    gui_state_cleanup(&st);
    gui_sounds_cleanup();
    gui_unload_font();
    gui_release_icon_resources();
    CloseWindow();
    g_gui_window_initialized = 0;
    return GUI_ACTION_EXIT;
}

#else /* !ENABLE_RAYLIB_GUI */

int run_raylib_gui(const MenuItem *items, int count)
{
    (void)items;
    (void)count;
    fprintf(stderr,
            "GUI no disponible: compila con -DENABLE_RAYLIB_GUI"
            " y enlaza raylib.\n");
    return GUI_ACTION_OPEN_CLASSIC_MENU;
}

#endif /* ENABLE_RAYLIB_GUI */

/* ═══════════════════════════════════════════════════════════
   API publica (siempre disponible)
   ═══════════════════════════════════════════════════════════ */

int gui_get_last_selected_index(void)
{
    return g_last_selected_index;
}

int gui_is_compiled(void)
{
#ifdef ENABLE_RAYLIB_GUI
    return 1;
#else
    return 0;
#endif
}

void gui_set_context_title(const char *title)
{
    if (!title || title[0] == '\0')
    {
        snprintf(g_context_title, sizeof(g_context_title), "%s", "Menu");
        return;
    }
    snprintf(g_context_title, sizeof(g_context_title), "%s", title);
}
