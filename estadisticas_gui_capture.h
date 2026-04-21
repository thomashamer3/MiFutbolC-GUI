/**
 * @file estadisticas_gui_capture.h
 * @brief Captura salida de estadisticas en GUI para evitar dependencia de consola.
 */
#ifndef ESTADISTICAS_GUI_CAPTURE_H
#define ESTADISTICAS_GUI_CAPTURE_H

#include "gui_components.h"
#include "utils.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifndef ESTADISTICAS_GUI_CAPTURE_TITLE_MAX
#define ESTADISTICAS_GUI_CAPTURE_TITLE_MAX 96
#endif

typedef struct
{
    int active;
    char title[ESTADISTICAS_GUI_CAPTURE_TITLE_MAX];
    char *buffer;
    size_t len;
    size_t cap;
} EstadisticasGuiCaptureState;

static EstadisticasGuiCaptureState g_estadisticas_gui_capture = {0};

static int estadisticas_gui_capture_enabled(void)
{
#ifdef UNIT_TEST
    return 0;
#else
    return IsWindowReady();
#endif
}

static int estadisticas_gui_capture_clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static void estadisticas_gui_capture_set_title(const char *title)
{
    if (!title || title[0] == '\0')
    {
        return;
    }

    const char *title_ascii = remover_tildes(title);
    if (!title_ascii)
    {
        title_ascii = title;
    }

    (void)snprintf(g_estadisticas_gui_capture.title,
                   sizeof(g_estadisticas_gui_capture.title),
                   "%s",
                   title_ascii);
}

static void estadisticas_gui_capture_reset(void)
{
    if (g_estadisticas_gui_capture.buffer)
    {
        free(g_estadisticas_gui_capture.buffer);
    }
    g_estadisticas_gui_capture.buffer = NULL;
    g_estadisticas_gui_capture.len = 0;
    g_estadisticas_gui_capture.cap = 0;
    g_estadisticas_gui_capture.active = 0;
    g_estadisticas_gui_capture.title[0] = '\0';
}

static int estadisticas_gui_capture_reserve(size_t extra)
{
    size_t needed = g_estadisticas_gui_capture.len + extra + 1;
    if (needed <= g_estadisticas_gui_capture.cap)
    {
        return 1;
    }

    size_t new_cap = (g_estadisticas_gui_capture.cap > 0) ? g_estadisticas_gui_capture.cap : 2048;
    while (new_cap < needed)
    {
        new_cap *= 2;
    }

    char *new_buffer = (char *)realloc(g_estadisticas_gui_capture.buffer, new_cap);
    if (!new_buffer)
    {
        return 0;
    }

    g_estadisticas_gui_capture.buffer = new_buffer;
    g_estadisticas_gui_capture.cap = new_cap;
    return 1;
}

static void estadisticas_gui_capture_begin_impl(const char *title)
{
    if (!estadisticas_gui_capture_enabled())
    {
        return;
    }

    if (!g_estadisticas_gui_capture.active)
    {
        g_estadisticas_gui_capture.active = 1;
        g_estadisticas_gui_capture.len = 0;
        g_estadisticas_gui_capture.title[0] = '\0';

        if (!estadisticas_gui_capture_reserve(0))
        {
            g_estadisticas_gui_capture.active = 0;
            return;
        }
        g_estadisticas_gui_capture.buffer[0] = '\0';
    }

    estadisticas_gui_capture_set_title(title);
}

static int estadisticas_gui_capture_vprintf_impl(const char *fmt, va_list ap)
{
    if (!estadisticas_gui_capture_enabled())
    {
        return vprintf(fmt, ap);
    }

    if (!g_estadisticas_gui_capture.active)
    {
        estadisticas_gui_capture_begin_impl("ESTADISTICAS");
    }

    va_list ap_copy;
    va_copy(ap_copy, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);

    if (needed < 0)
    {
        return needed;
    }

    if (!estadisticas_gui_capture_reserve((size_t)needed))
    {
        return -1;
    }

    int written = vsnprintf(g_estadisticas_gui_capture.buffer + g_estadisticas_gui_capture.len,
                            g_estadisticas_gui_capture.cap - g_estadisticas_gui_capture.len,
                            fmt,
                            ap);
    if (written > 0)
    {
        g_estadisticas_gui_capture.len += (size_t)written;
    }
    return written;
}

static void estadisticas_gui_capture_split_lines(const char *text,
                                                 char **storage,
                                                 char ***lines,
                                                 int *line_count)
{
    *storage = NULL;
    *lines = NULL;
    *line_count = 0;

    size_t text_len = strlen(text);
    *storage = (char *)malloc(text_len + 1);
    if (!*storage)
    {
        return;
    }
    memcpy(*storage, text, text_len + 1);

    int count = 1;
    for (size_t i = 0; i < text_len; ++i)
    {
        if ((*storage)[i] == '\n')
        {
            ++count;
        }
    }

    *lines = (char **)malloc((size_t)count * sizeof(char *));
    if (!*lines)
    {
        free(*storage);
        *storage = NULL;
        return;
    }

    int idx = 0;
    char *start = *storage;
    for (size_t i = 0; i <= text_len; ++i)
    {
        if ((*storage)[i] == '\n' || (*storage)[i] == '\0')
        {
            (*storage)[i] = '\0';
            (*lines)[idx++] = start;
            start = &(*storage)[i + 1];
        }
    }

    if (idx == 0)
    {
        (*lines)[idx++] = *storage;
    }

    *line_count = idx;
}

static void estadisticas_gui_capture_show_modal(const char *title, const char *text)
{
    const GuiTheme *theme = gui_get_active_theme();

    const Color bg_main = theme ? theme->bg_main : (Color){18, 23, 32, 255};
    const Color bg_header = theme ? theme->bg_header : (Color){26, 33, 45, 255};
    const Color bg_list = theme ? theme->bg_list : (Color){24, 30, 42, 255};
    const Color text_primary = theme ? theme->text_primary : RAYWHITE;
    const Color text_secondary = theme ? theme->text_secondary : (Color){170, 180, 200, 255};
    const Color border = theme ? theme->border : (Color){86, 104, 140, 255};
    const Color accent = theme ? theme->accent_primary : (Color){37, 126, 255, 255};
    const Color accent_hover = theme ? theme->accent_primary_hv : (Color){54, 146, 255, 255};

    char *storage = NULL;
    char **lines = NULL;
    int owns_lines = 1;
    int total_lines = 0;
    estadisticas_gui_capture_split_lines(text, &storage, &lines, &total_lines);

    if (!lines || total_lines <= 0)
    {
        static char fallback_storage[] = "Sin datos disponibles.";
        static char *fallback_lines[] = {fallback_storage};
        owns_lines = 0;
        storage = NULL;
        lines = fallback_lines;
        total_lines = 1;
    }

    int top_line = 0;

    while (!WindowShouldClose())
    {
        const int sw = GetScreenWidth();
        const int sh = GetScreenHeight();

        const Rectangle panel = {28.0f, 88.0f, (float)sw - 56.0f, (float)sh - 126.0f};
        const Rectangle text_area = {panel.x + 18.0f, panel.y + 58.0f, panel.width - 36.0f, panel.height - 118.0f};
        const Rectangle close_btn = {panel.x + panel.width - 188.0f, panel.y + panel.height - 48.0f, 164.0f, 34.0f};

        const int row_height = 24;
        int visible_rows = (int)(text_area.height / (float)row_height);
        if (visible_rows < 1)
        {
            visible_rows = 1;
        }
        const int max_top_line = (total_lines > visible_rows) ? (total_lines - visible_rows) : 0;

        const float wheel = GetMouseWheelMove();
        if (wheel > 0.01f)
        {
            top_line -= (int)(wheel * 3.0f);
        }
        else if (wheel < -0.01f)
        {
            top_line += (int)(-wheel * 3.0f);
        }

        if (IsKeyPressed(KEY_UP)) top_line -= 1;
        if (IsKeyPressed(KEY_DOWN)) top_line += 1;
        if (IsKeyPressed(KEY_PAGE_UP)) top_line -= visible_rows;
        if (IsKeyPressed(KEY_PAGE_DOWN)) top_line += visible_rows;
        if (IsKeyPressed(KEY_HOME)) top_line = 0;
        if (IsKeyPressed(KEY_END)) top_line = max_top_line;

        top_line = estadisticas_gui_capture_clamp_int(top_line, 0, max_top_line);

        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        {
            break;
        }

        const Vector2 mouse = GetMousePosition();
        const int close_hover = CheckCollisionPointRec(mouse, close_btn);
        if (close_hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            break;
        }

        BeginDrawing();
        ClearBackground(bg_main);
        gui_draw_module_header(title, sw);

        DrawRectangleRec(panel, bg_header);
        DrawRectangleLinesEx(panel, 2.0f, border);

        gui_text("Reporte", panel.x + 18.0f, panel.y + 16.0f, 22.0f, text_primary);

        char lines_info[96];
        (void)snprintf(lines_info, sizeof(lines_info),
                       "Lineas: %d | Vista: %d/%d",
                       total_lines,
                       (max_top_line > 0) ? (top_line + 1) : 1,
                       (max_top_line > 0) ? (max_top_line + 1) : 1);
        gui_text(lines_info, panel.x + 18.0f, panel.y + 40.0f, 14.0f, text_secondary);

        DrawRectangleRec(text_area, bg_list);
        DrawRectangleLinesEx(text_area, 1.0f, border);

        BeginScissorMode((int)text_area.x, (int)text_area.y, (int)text_area.width, (int)text_area.height);
        for (int i = 0; i < visible_rows; ++i)
        {
            const int idx = top_line + i;
            if (idx >= total_lines)
            {
                break;
            }

            const float row_y = text_area.y + (float)(i * row_height);
            if ((idx % 2) != 0)
            {
                DrawRectangle((int)text_area.x,
                              (int)row_y,
                              (int)text_area.width,
                              row_height,
                              ColorAlpha(text_secondary, 0.06f));
            }

            gui_text_truncated(lines[idx],
                               text_area.x + 10.0f,
                               row_y + 4.0f,
                               18.0f,
                               text_area.width - 20.0f,
                               text_primary);
        }
        EndScissorMode();

        DrawRectangleRec(close_btn, close_hover ? accent_hover : accent);
        DrawRectangleLinesEx(close_btn, 1.0f, border);
        gui_text("Cerrar (ESC)", close_btn.x + 20.0f, close_btn.y + 8.0f, 16.0f, text_primary);

        gui_draw_footer_hint("Usa la rueda, las flechas o PgUp/PgDn para desplazarte", 20.0f, sh);

        EndDrawing();
    }

    if (storage)
    {
        free(storage);
    }
    if (owns_lines && lines)
    {
        free(lines);
    }
}

static void estadisticas_gui_capture_end_impl(void)
{
    if (!estadisticas_gui_capture_enabled())
    {
        return;
    }

    if (!g_estadisticas_gui_capture.active)
    {
        return;
    }

    const char *title = g_estadisticas_gui_capture.title[0] ?
                        g_estadisticas_gui_capture.title :
                        "ESTADISTICAS";
    const char *body = (g_estadisticas_gui_capture.buffer && g_estadisticas_gui_capture.buffer[0]) ?
                       g_estadisticas_gui_capture.buffer :
                       "Sin datos disponibles.";

    estadisticas_gui_capture_show_modal(title, body);
    estadisticas_gui_capture_reset();
}

static void stats_clear_screen(void)
{
    if (!estadisticas_gui_capture_enabled())
    {
        clear_screen();
    }
}

static void stats_print_header(const char *title)
{
    if (estadisticas_gui_capture_enabled())
    {
        estadisticas_gui_capture_begin_impl(title);
    }
    else
    {
        print_header(title);
    }
}

static void stats_pause_console(void)
{
    if (estadisticas_gui_capture_enabled())
    {
        estadisticas_gui_capture_end_impl();
    }
    else
    {
        pause_console();
    }
}

static int stats_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int written = estadisticas_gui_capture_vprintf_impl(fmt, ap);
    va_end(ap);
    return written;
}

#define clear_screen stats_clear_screen
#define print_header stats_print_header
#define pause_console stats_pause_console
#define printf stats_printf

#endif /* ESTADISTICAS_GUI_CAPTURE_H */
