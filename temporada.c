#include "temporada.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "equipo.h"
#include "torneo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <ctype.h>

#ifndef UNIT_TEST
#include "estadisticas_gui_capture.h"
#endif

#ifdef _WIN32
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

#define ARRAY_COUNT(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))
#define TEMP_ITEM(numero, texto, accion) {(numero), (texto), (accion), MENU_CATEGORY_COMPETENCIA}
#define TEMP_BACK_ITEM {0, "Volver", NULL, MENU_CATEGORY_ADMIN}

#define TEMP_TEXT_MAX 256


/**
 * @file temporada.c
 * @brief Implementacion del Sistema de Temporadas y Ciclos Deportivos
 */

// ========== FUNCIONES AUXILIARES ==========

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, 0) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

#ifndef UNIT_TEST
typedef enum
{
    TEMP_INPUT_ALNUM = 0,
    TEMP_INPUT_NUMERIC,
    TEMP_INPUT_DATE
} TempInputMode;

typedef struct
{
    int id;
    char resumen[TEMP_TEXT_MAX];
} TempTemporadaRow;

typedef struct
{
    int id;
    char resumen[TEMP_TEXT_MAX];
} TempTorneoRow;

static int temp_parse_int(const char *text, int *out)
{
    char *end = NULL;
    long v;

    if (!text || text[0] == '\0' || !out)
    {
        return 0;
    }

    v = strtol(text, &end, 10);
    if (!end || *end != '\0')
    {
        return 0;
    }

    if (v < INT_MIN || v > INT_MAX)
    {
        return 0;
    }

    *out = (int)v;
    return 1;
}

static int temp_input_char_valido(unsigned int codepoint, TempInputMode mode)
{
    unsigned char ch = (unsigned char)codepoint;

    if (codepoint < 32 || codepoint > 126)
    {
        return 0;
    }

    if (mode == TEMP_INPUT_NUMERIC)
    {
        return isdigit(ch);
    }

    if (mode == TEMP_INPUT_DATE)
    {
        return isdigit(ch) || ch == '/' || ch == '-';
    }

    return isalnum(ch) || isspace(ch) || ch == '_' || ch == '-' || ch == ',' || ch == '.';
}

static void temp_draw_action_button(Rectangle rect, const char *label, int primary)
{
    const GuiTheme *theme = gui_get_active_theme();
    Color fill = primary ? (Color){34, 132, 80, 255} : theme->bg_list;
    Color border = primary ? (Color){57, 178, 110, 255} : theme->card_border;
    Color text = primary ? (Color){244, 255, 248, 255} : theme->text_primary;
    Vector2 tm = gui_text_measure(label, 18.0f);

    DrawRectangleRounded(rect, 0.22f, 8, fill);
    DrawRectangleRoundedLines(rect, 0.22f, 8, border);
    gui_text(label,
             rect.x + (rect.width - tm.x) * 0.5f,
             rect.y + (rect.height - tm.y) * 0.5f,
             18.0f,
             text);
}

static int temp_modal_confirmar_gui(const char *titulo,
                                    const char *mensaje,
                                    const char *accion)
{
    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 760 : sw - 40;
        int panel_h = 240;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle btn_ok = {(float)panel_x + panel_w - 362.0f, (float)panel_y + panel_h - 54.0f, 168.0f, 38.0f};
        Rectangle btn_cancel = {(float)panel_x + panel_w - 184.0f, (float)panel_y + panel_h - 54.0f, 168.0f, 38.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header(titulo ? titulo : "CONFIRMAR", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text_wrapped(mensaje ? mensaje : "Confirma la accion",
                         (Rectangle){(float)panel_x + 24.0f, (float)panel_y + 52.0f, (float)panel_w - 48.0f, 86.0f},
                         20.0f,
                         gui_get_active_theme()->text_primary);

        temp_draw_action_button(btn_ok, accion ? accion : "Confirmar", 1);
        temp_draw_action_button(btn_cancel, "Cancelar", 0);
        gui_draw_footer_hint("ENTER: confirmar | ESC: cancelar", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_ok)) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        {
            return 1;
        }
        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_cancel)) || IsKeyPressed(KEY_ESCAPE))
        {
            return 0;
        }
    }

    return 0;
}

static void temp_mostrar_info_gui(const char *titulo, const char *mensaje)
{
    (void)temp_modal_confirmar_gui(titulo ? titulo : "INFORMACION",
                                   mensaje ? mensaje : "Operacion finalizada.",
                                   "Entendido");
}

static int temp_modal_input_texto_gui(const char *titulo,
                                      const char *etiqueta,
                                      const char *hint,
                                      char *out,
                                      size_t out_size,
                                      int permitir_vacio,
                                      TempInputMode mode)
{
    char texto[TEMP_TEXT_MAX] = {0};
    int cursor = 0;
    int error_vacio = 0;

    if (!out || out_size == 0)
    {
        return 0;
    }

    if (out[0] != '\0')
    {
        strncpy_s(texto, sizeof(texto), out, _TRUNCATE);
        cursor = (int)strlen_s(texto, sizeof(texto));
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 1080 ? 860 : sw - 40;
        int panel_h = 280;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle input_rect = {(float)panel_x + 26.0f, (float)panel_y + 102.0f, (float)panel_w - 52.0f, 44.0f};
        Rectangle btn_ok = {(float)panel_x + panel_w - 362.0f, (float)panel_y + panel_h - 54.0f, 168.0f, 38.0f};
        Rectangle btn_cancel = {(float)panel_x + panel_w - 184.0f, (float)panel_y + panel_h - 54.0f, 168.0f, 38.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        int key = GetCharPressed();

        while (key > 0)
        {
            if (temp_input_char_valido((unsigned int)key, mode) &&
                cursor < (int)(out_size - 1) &&
                cursor < (int)(sizeof(texto) - 1))
            {
                texto[cursor++] = (char)key;
                texto[cursor] = '\0';
            }
            key = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE) && cursor > 0)
        {
            texto[--cursor] = '\0';
        }

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header(titulo ? titulo : "ENTRADA", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text(etiqueta ? etiqueta : "Ingrese un valor:",
                 (float)panel_x + 26.0f,
                 (float)panel_y + 56.0f,
                 20.0f,
                 gui_get_active_theme()->text_primary);

        DrawRectangleRounded(input_rect, 0.18f, 8, gui_get_active_theme()->bg_list);
        DrawRectangleRoundedLines(input_rect, 0.18f, 8, gui_get_active_theme()->card_border);
        gui_text_truncated(texto, input_rect.x + 10.0f, input_rect.y + 11.0f, 18.0f, input_rect.width - 20.0f, gui_get_active_theme()->text_primary);

        if (error_vacio)
        {
            gui_text("El campo no puede estar vacio.",
                     (float)panel_x + 26.0f,
                     (float)panel_y + 154.0f,
                     17.0f,
                     (Color){238, 121, 121, 255});
        }
        else if (hint && hint[0] != '\0')
        {
            gui_text(hint,
                     (float)panel_x + 26.0f,
                     (float)panel_y + 154.0f,
                     16.0f,
                     gui_get_active_theme()->text_muted);
        }

        temp_draw_action_button(btn_ok, "Guardar", 1);
        temp_draw_action_button(btn_cancel, "Cancelar", 0);
        gui_draw_footer_hint("ENTER: confirmar | ESC: cancelar | BACKSPACE: borrar", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_ok)) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        {
            if (!permitir_vacio && texto[0] == '\0')
            {
                error_vacio = 1;
                continue;
            }

            strncpy_s(out, out_size, texto, _TRUNCATE);
            return 1;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_cancel)) || IsKeyPressed(KEY_ESCAPE))
        {
            return 0;
        }
    }

    return 0;
}

static int temp_modal_input_entero_gui(const char *titulo,
                                       const char *etiqueta,
                                       const char *hint,
                                       int min_value,
                                       int *valor_out)
{
    char texto[32] = {0};
    int valor = 0;

    if (!valor_out)
    {
        return 0;
    }

    while (1)
    {
        if (!temp_modal_input_texto_gui(titulo,
                                        etiqueta,
                                        hint,
                                        texto,
                                        sizeof(texto),
                                        0,
                                        TEMP_INPUT_NUMERIC))
        {
            return 0;
        }

        if (temp_parse_int(texto, &valor) && valor >= min_value)
        {
            *valor_out = valor;
            return 1;
        }

        temp_mostrar_info_gui("VALOR INVALIDO",
                              "Ingrese un numero valido dentro del rango permitido.");
    }
}

static int temp_modal_selector_opcion_gui(const char *titulo,
                                          const char *columna,
                                          const char **opciones,
                                          int cantidad,
                                          int seleccion_inicial,
                                          int *seleccion_out)
{
    int selected;
    int scroll = 0;
    const int row_h = 34;

    if (!seleccion_out || !opciones || cantidad <= 0)
    {
        return 0;
    }

    selected = seleccion_inicial;
    if (selected < 0 || selected >= cantidad)
    {
        selected = 0;
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = 72;
        int panel_y = 120;
        int panel_w = sw - 144;
        int panel_h = sh - 190;
        int content_y = panel_y + 32;
        int content_h = panel_h - 32;
        int visible_rows;
        int max_scroll;

        if (panel_w < 560)
        {
            panel_w = 560;
            panel_x = (sw - panel_w) / 2;
        }

        visible_rows = content_h / row_h;
        if (visible_rows < 1)
        {
            visible_rows = 1;
        }
        max_scroll = (cantidad > visible_rows) ? (cantidad - visible_rows) : 0;

        if (GetMouseWheelMove() > 0.01f) scroll -= 3;
        if (GetMouseWheelMove() < -0.01f) scroll += 3;
        if (IsKeyPressed(KEY_UP)) selected--;
        if (IsKeyPressed(KEY_DOWN)) selected++;

        if (selected < 0) selected = 0;
        if (selected >= cantidad) selected = cantidad - 1;

        if (selected < scroll) scroll = selected;
        if (selected >= scroll + visible_rows) scroll = selected - visible_rows + 1;
        if (scroll < 0) scroll = 0;
        if (scroll > max_scroll) scroll = max_scroll;

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header(titulo, sw);

        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                            "ID", 12.0f,
                            columna ? columna : "OPCION", 80.0f);

        BeginScissorMode(panel_x, content_y, panel_w, content_h);
        for (int i = scroll; i < cantidad; i++)
        {
            int row = i - scroll;
            int y = content_y + row * row_h;
            Rectangle fila;
            int hovered;
            int is_selected;

            if (row >= visible_rows)
            {
                break;
            }

            fila = (Rectangle){(float)(panel_x + 6), (float)y, (float)(panel_w - 12), (float)(row_h - 2)};
            hovered = CheckCollisionPointRec(GetMousePosition(), fila) ? 1 : 0;
            is_selected = (i == selected) ? 1 : 0;

            gui_draw_list_row_bg(fila, row, hovered || is_selected);
            if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                *seleccion_out = i;
                EndScissorMode();
                EndDrawing();
                return 1;
            }

            gui_text(TextFormat("%2d", i + 1), (float)(panel_x + 12), (float)(y + 7), 18.0f, (Color){220, 238, 225, 255});
            gui_text(opciones[i], (float)(panel_x + 80), (float)(y + 7), 18.0f,
                     is_selected ? (Color){183, 247, 206, 255} : (Color){233, 247, 236, 255});
        }
        EndScissorMode();

        gui_draw_footer_hint("Click o ENTER para confirmar | Flechas: mover | ESC: cancelar", (float)panel_x, sh);
        EndDrawing();

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        {
            *seleccion_out = selected;
            return 1;
        }

        if (IsKeyPressed(KEY_ESCAPE))
        {
            return 0;
        }
    }

    return 0;
}

static int temp_cargar_temporadas_rows(TempTemporadaRow *rows, int max_rows)
{
    sqlite3_stmt *stmt = NULL;
    int count = 0;
    const char *sql = "SELECT id, nombre, anio, fecha_inicio, fecha_fin, estado "
                      "FROM temporada ORDER BY anio DESC, fecha_inicio DESC, id DESC;";

    if (!rows || max_rows <= 0)
    {
        return 0;
    }

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_rows)
    {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char *nombre = sqlite3_column_text(stmt, 1);
        int anio = sqlite3_column_int(stmt, 2);
        const unsigned char *inicio = sqlite3_column_text(stmt, 3);
        const unsigned char *fin = sqlite3_column_text(stmt, 4);
        const unsigned char *estado = sqlite3_column_text(stmt, 5);

        rows[count].id = id;
        snprintf(rows[count].resumen,
                 sizeof(rows[count].resumen),
                 "%03d | %s | %d | %s a %s | %s",
                 id,
                 nombre ? (const char *)nombre : "Sin nombre",
                 anio,
                 inicio ? (const char *)inicio : "-",
                 fin ? (const char *)fin : "-",
                 estado ? (const char *)estado : "-");
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

static int temp_seleccionar_temporada_gui(const char *titulo, int *temporada_id_out)
{
    TempTemporadaRow rows[256];
    const char *opciones[256];
    int count;
    int idx = 0;

    if (!temporada_id_out)
    {
        return 0;
    }

    count = temp_cargar_temporadas_rows(rows, ARRAY_COUNT(rows));
    if (count <= 0)
    {
        temp_mostrar_info_gui("TEMPORADAS", "No hay temporadas registradas.");
        return 0;
    }

    for (int i = 0; i < count; i++)
    {
        opciones[i] = rows[i].resumen;
    }

    if (!temp_modal_selector_opcion_gui(titulo ? titulo : "SELECCIONAR TEMPORADA",
                                        "TEMPORADA",
                                        opciones,
                                        count,
                                        0,
                                        &idx))
    {
        return 0;
    }

    *temporada_id_out = rows[idx].id;
    return 1;
}

static int temp_cargar_torneos_disponibles_rows(TempTorneoRow *rows, int max_rows)
{
    sqlite3_stmt *stmt = NULL;
    int count = 0;
    const char *sql = "SELECT id, nombre FROM torneo WHERE id NOT IN "
                      "(SELECT torneo_id FROM torneo_temporada) ORDER BY id;";

    if (!rows || max_rows <= 0)
    {
        return 0;
    }

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_rows)
    {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char *nombre = sqlite3_column_text(stmt, 1);

        rows[count].id = id;
        snprintf(rows[count].resumen,
                 sizeof(rows[count].resumen),
                 "%03d | %s",
                 id,
                 nombre ? (const char *)nombre : "Sin nombre");
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

static int temp_seleccionar_torneo_disponible_gui(const char *titulo, int *torneo_id_out)
{
    TempTorneoRow rows[256];
    const char *opciones[256];
    int count;
    int idx = 0;

    if (!torneo_id_out)
    {
        return 0;
    }

    count = temp_cargar_torneos_disponibles_rows(rows, ARRAY_COUNT(rows));
    if (count <= 0)
    {
        temp_mostrar_info_gui("TORNEOS", "No hay torneos disponibles para asociar.");
        return 0;
    }

    for (int i = 0; i < count; i++)
    {
        opciones[i] = rows[i].resumen;
    }

    if (!temp_modal_selector_opcion_gui(titulo ? titulo : "SELECCIONAR TORNEO",
                                        "TORNEO",
                                        opciones,
                                        count,
                                        0,
                                        &idx))
    {
        return 0;
    }

    *torneo_id_out = rows[idx].id;
    return 1;
}

static void temp_fecha_hoy(char *out, size_t out_size)
{
    time_t t = time(NULL);
    struct tm tmv;

    if (!out || out_size == 0)
    {
        return;
    }

    if (out_size < 11)
    {
        out[0] = '\0';
        return;
    }

    localtime_s(&tmv, &t);
    snprintf(out, out_size, "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
}

static int temp_normalizar_fecha_input(const char *input, char *out, size_t out_size)
{
    int d = 0;
    int m = 0;
    int a = 0;

    if (!input || !out || out_size < 11)
    {
        return 0;
    }

    if (input[0] == '\0')
    {
        temp_fecha_hoy(out, out_size);
        return 1;
    }

    if (strchr(input, '/') != NULL)
    {
        if (sscanf(input, "%d/%d/%d", &d, &m, &a) == 3 &&
            d >= 1 && d <= 31 &&
            m >= 1 && m <= 12 &&
            a >= 1900 && a <= 2100)
        {
            snprintf(out, out_size, "%04d-%02d-%02d", a, m, d);
            return 1;
        }
        return 0;
    }

    if (strlen_s(input, 32) == 10 &&
        isdigit((unsigned char)input[0]) && isdigit((unsigned char)input[1]) &&
        isdigit((unsigned char)input[2]) && isdigit((unsigned char)input[3]) &&
        input[4] == '-' &&
        isdigit((unsigned char)input[5]) && isdigit((unsigned char)input[6]) &&
        input[7] == '-' &&
        isdigit((unsigned char)input[8]) && isdigit((unsigned char)input[9]))
    {
        strncpy_s(out, out_size, input, _TRUNCATE);
        return 1;
    }

    return 0;
}

static int temp_validar_mes_anio(const char *mes_anio)
{
    if (!mes_anio)
    {
        return 0;
    }

    if (strlen_s(mes_anio, 16) != 7)
    {
        return 0;
    }

    if (!isdigit((unsigned char)mes_anio[0]) ||
        !isdigit((unsigned char)mes_anio[1]) ||
        !isdigit((unsigned char)mes_anio[2]) ||
        !isdigit((unsigned char)mes_anio[3]) ||
        mes_anio[4] != '-' ||
        !isdigit((unsigned char)mes_anio[5]) ||
        !isdigit((unsigned char)mes_anio[6]))
    {
        return 0;
    }

    {
        int mes = (mes_anio[5] - '0') * 10 + (mes_anio[6] - '0');
        return (mes >= 1 && mes <= 12);
    }
}
#endif

#ifdef UNIT_TEST
static void solicitar_texto_no_vacio(const char *prompt, char *buffer, int size)
{
    while (1)
    {
        input_string(prompt, buffer, size);
        if (safe_strnlen(buffer, (size_t)size) > 0)
            return;
        printf("El campo no puede estar vacio.\n");
    }
}
#endif

static void solicitar_fecha_yyyy_mm_dd(const char *prompt, char *buffer, int size)
{
#ifdef UNIT_TEST
    input_date(prompt, buffer, size);
#else
    char ingresada[32] = {0};
    const char *etiqueta = (prompt && prompt[0] != '\0') ? prompt : "Fecha";

    while (1)
    {
        if (!temp_modal_input_texto_gui("INGRESAR FECHA",
                                        etiqueta,
                                        "Formato DD/MM/AAAA o YYYY-MM-DD. Vacio = hoy",
                                        ingresada,
                                        sizeof(ingresada),
                                        1,
                                        TEMP_INPUT_DATE))
        {
            buffer[0] = '\0';
            return;
        }

        if (temp_normalizar_fecha_input(ingresada, buffer, (size_t)size))
        {
            return;
        }

        temp_mostrar_info_gui("FORMATO INVALIDO",
                              "La fecha debe ser DD/MM/AAAA o YYYY-MM-DD.");
    }
#endif
}

const char* get_nombre_tipo_fase(TipoFaseTemporada tipo)
{
    switch (tipo)
    {
    case PRETEMPORADA:
        return "Pretemporada";
    case TEMPORADA_REGULAR:
        return "Temporada Regular";
    case POSTTEMPORADA:
        return "Postemporada";
    default:
        return "Desconocido";
    }
}

int get_temporada_actual_id()
{
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id FROM temporada WHERE estado = 'Activa' ORDER BY fecha_inicio DESC LIMIT 1;";

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int temporada_id = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
            return temporada_id;
        }
        sqlite3_finalize(stmt);
    }
    return -1;
}

int fecha_en_temporada(const char* fecha, int temporada_id)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT COUNT(*) FROM temporada WHERE id = ? AND ? BETWEEN fecha_inicio AND fecha_fin;";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);
        sqlite3_bind_text(stmt, 2, fecha, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int count = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
            return count;
        }
        sqlite3_finalize(stmt);
    }
    return 0;
}

/**
 * @brief Obtiene el nombre de una temporada por su ID.
 */
static void obtener_nombre_temporada(int temporada_id, char *nombre_buf, size_t buf_len)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT nombre FROM temporada WHERE id = ?;";
    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const unsigned char *text = sqlite3_column_text(stmt, 0);
            if (text)
            {
                snprintf(nombre_buf, buf_len, "%s", (const char *)text);
            }
            else
            {
                snprintf(nombre_buf, buf_len, "%s", "Desconocida");
            }
        }
        else
        {
            snprintf(nombre_buf, buf_len, "%s", "Desconocida");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        snprintf(nombre_buf, buf_len, "%s", "Desconocida");
    }
}

// ========== FUNCIONES DE CREACIoN Y GESTIoN ==========

static void determinar_estado_temporada(const char *fecha_inicio, const char *fecha_fin, char *estado, size_t estado_size)
{
    time_t now = time(NULL);
    struct tm current_time;
    localtime_s(&current_time, &now);
    char current_date[32];
    snprintf(current_date, sizeof(current_date), "%04d-%02d-%02d",
             current_time.tm_year + 1900,
             current_time.tm_mon + 1,
             current_time.tm_mday);

    if (strcmp(current_date, fecha_inicio) >= 0 && strcmp(current_date, fecha_fin) <= 0)
    {
        strcpy_s(estado, estado_size, "Activa");
    }
    else if (strcmp(current_date, fecha_inicio) < 0)
    {
        strcpy_s(estado, estado_size, "Planificada");
    }
    else
    {
        strcpy_s(estado, estado_size, "Finalizada");
    }
}

static int calcular_nuevo_id_temporada(void)
{
    int nuevo_id = 1;
    sqlite3_stmt *stmt_id = NULL;

    // Verificar si ID 1 esta libre
    const char *sql_check1 = "SELECT COUNT(*) FROM temporada WHERE id = 1;";
    if (preparar_stmt(sql_check1, &stmt_id))
    {
        if (sqlite3_step(stmt_id) == SQLITE_ROW && sqlite3_column_int(stmt_id, 0) == 0)
        {
            sqlite3_finalize(stmt_id);
            return 1;
        }
        sqlite3_finalize(stmt_id);
    }

    // Verificar si hay registros
    const char *sql_check = "SELECT COUNT(*) FROM temporada;";
    if (preparar_stmt(sql_check, &stmt_id))
    {
        int count = 0;
        if (sqlite3_step(stmt_id) == SQLITE_ROW)
            count = sqlite3_column_int(stmt_id, 0);
        sqlite3_finalize(stmt_id);

        if (count == 0)
            return 1;
    }

    // Calcular el menor ID libre
    const char *sql_id = "SELECT MIN(t1.id + 1) FROM temporada t1 "
                         "WHERE NOT EXISTS (SELECT 1 FROM temporada t2 WHERE t2.id = t1.id + 1);";
    if (preparar_stmt(sql_id, &stmt_id))
    {
        if (sqlite3_step(stmt_id) == SQLITE_ROW && sqlite3_column_type(stmt_id, 0) != SQLITE_NULL)
            nuevo_id = sqlite3_column_int(stmt_id, 0);
        sqlite3_finalize(stmt_id);
    }

    return nuevo_id;
}

void crear_temporada()
{
    clear_screen();
    print_header("CREAR TEMPORADA");

    Temporada temporada = {0};
    temporada.estado[0] = '\0'; // Inicializar como vacio

#ifdef UNIT_TEST
    // Solicitar datos basicos
    solicitar_texto_no_vacio("Nombre de la temporada: ", temporada.nombre, sizeof(temporada.nombre));
    temporada.anio = input_int("Ano de la temporada: ");
    while (temporada.anio <= 0)
    {
        temporada.anio = input_int("Ano invalido. Ingrese un ano valido: ");
    }

    solicitar_fecha_yyyy_mm_dd("Fecha de inicio (DD/MM/AAAA, Enter=hoy): ", temporada.fecha_inicio, sizeof(temporada.fecha_inicio));

    solicitar_fecha_yyyy_mm_dd("Fecha de fin (DD/MM/AAAA, Enter=hoy): ", temporada.fecha_fin, sizeof(temporada.fecha_fin));
    while (strcmp(temporada.fecha_fin, temporada.fecha_inicio) < 0)
    {
        printf("La fecha de fin no puede ser anterior a la de inicio.\n");
        solicitar_fecha_yyyy_mm_dd("Fecha de fin (DD/MM/AAAA, Enter=hoy): ", temporada.fecha_fin, sizeof(temporada.fecha_fin));
    }

    input_string("Descripcion (opcional): ", temporada.descripcion, sizeof(temporada.descripcion));

    // Determinar estado automaticamente
    determinar_estado_temporada(temporada.fecha_inicio, temporada.fecha_fin,
                                temporada.estado, sizeof(temporada.estado));

    // Guardar en base de datos - reutilizar ID libre
    sqlite3_stmt *stmt = NULL;
    int nuevo_id = calcular_nuevo_id_temporada();

    const char *sql = "INSERT INTO temporada (id, nombre, anio, fecha_inicio, fecha_fin, estado, descripcion) VALUES (?, ?, ?, ?, ?, ?, ?);";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, nuevo_id);
        sqlite3_bind_text(stmt, 2, temporada.nombre, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, temporada.anio);
        sqlite3_bind_text(stmt, 4, temporada.fecha_inicio, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, temporada.fecha_fin, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, temporada.estado, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, temporada.descripcion, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Temporada creada exitosamente con ID: %d\n", nuevo_id);

            // Crear fases por defecto
            crear_fases_temporada_defecto(nuevo_id);
        }
        else
        {
            printf("Error al crear la temporada: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
#else
    if (!temp_modal_input_texto_gui("CREAR TEMPORADA",
                                    "Nombre de la temporada:",
                                    "Solo letras, numeros y espacios",
                                    temporada.nombre,
                                    sizeof(temporada.nombre),
                                    0,
                                    TEMP_INPUT_ALNUM))
    {
        return;
    }

    if (!temp_modal_input_entero_gui("CREAR TEMPORADA",
                                     "Ano de la temporada:",
                                     "Ingrese un ano positivo",
                                     1,
                                     &temporada.anio))
    {
        return;
    }

    solicitar_fecha_yyyy_mm_dd("Fecha de inicio (DD/MM/AAAA, Enter=hoy): ", temporada.fecha_inicio, sizeof(temporada.fecha_inicio));
    if (temporada.fecha_inicio[0] == '\0')
    {
        return;
    }

    while (1)
    {
        solicitar_fecha_yyyy_mm_dd("Fecha de fin (DD/MM/AAAA, Enter=hoy): ", temporada.fecha_fin, sizeof(temporada.fecha_fin));
        if (temporada.fecha_fin[0] == '\0')
        {
            return;
        }

        if (strcmp(temporada.fecha_fin, temporada.fecha_inicio) >= 0)
        {
            break;
        }

        temp_mostrar_info_gui("FECHA INVALIDA", "La fecha de fin no puede ser anterior a la fecha de inicio.");
    }

    (void)temp_modal_input_texto_gui("CREAR TEMPORADA",
                                     "Descripcion (opcional):",
                                     "Puede dejar el campo vacio",
                                     temporada.descripcion,
                                     sizeof(temporada.descripcion),
                                     1,
                                     TEMP_INPUT_ALNUM);

    // Determinar estado automaticamente
    determinar_estado_temporada(temporada.fecha_inicio, temporada.fecha_fin,
                                temporada.estado, sizeof(temporada.estado));

    // Guardar en base de datos - reutilizar ID libre
    sqlite3_stmt *stmt = NULL;
    int nuevo_id = calcular_nuevo_id_temporada();

    const char *sql = "INSERT INTO temporada (id, nombre, anio, fecha_inicio, fecha_fin, estado, descripcion) VALUES (?, ?, ?, ?, ?, ?, ?);";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, nuevo_id);
        sqlite3_bind_text(stmt, 2, temporada.nombre, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, temporada.anio);
        sqlite3_bind_text(stmt, 4, temporada.fecha_inicio, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, temporada.fecha_fin, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, temporada.estado, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, temporada.descripcion, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            char msg[256];
            crear_fases_temporada_defecto(nuevo_id);
            snprintf(msg, sizeof(msg), "Temporada creada exitosamente con ID: %d", nuevo_id);
            temp_mostrar_info_gui("TEMPORADA", msg);
        }
        else
        {
            char msg[256];
            snprintf(msg, sizeof(msg), "Error al crear la temporada: %s", sqlite3_errmsg(db));
            temp_mostrar_info_gui("ERROR", msg);
        }
        sqlite3_finalize(stmt);
    }
#endif
}

void crear_fases_temporada_defecto(int temporada_id)
{
    // Obtener fechas de la temporada
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT fecha_inicio, fecha_fin, anio FROM temporada WHERE id = ?;";

    char fecha_inicio[20] = "";
    char fecha_fin[20] = "";
    int anio = 0;

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *text0 = (const char*)sqlite3_column_text(stmt, 0);
            if (text0) strcpy_s(fecha_inicio, sizeof(fecha_inicio), text0);
            const char *text1 = (const char*)sqlite3_column_text(stmt, 1);
            if (text1) strcpy_s(fecha_fin, sizeof(fecha_fin), text1);
            anio = sqlite3_column_int(stmt, 2);
        }
        else
        {
            sqlite3_finalize(stmt);
            return;
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        return;
    }

    // Calcular fechas de las fases
    char fecha_pretemporada_inicio[20] = {0};
    char fecha_pretemporada_fin[20] = {0};
    char fecha_regular_inicio[20] = {0};
    char fecha_regular_fin[20] = {0};
    char fecha_postemporada_inicio[20] = {0};
    char fecha_postemporada_fin[20] = {0};

    // Pretemporada: primeros 2 meses
    snprintf(fecha_pretemporada_inicio, sizeof(fecha_pretemporada_inicio), "%04d-01-01", anio);
    snprintf(fecha_pretemporada_fin, sizeof(fecha_pretemporada_fin), "%04d-02-28", anio);

    // Temporada regular: marzo a noviembre
    snprintf(fecha_regular_inicio, sizeof(fecha_regular_inicio), "%04d-03-01", anio);
    snprintf(fecha_regular_fin, sizeof(fecha_regular_fin), "%04d-11-30", anio);

    // Postemporada: diciembre
    snprintf(fecha_postemporada_inicio, sizeof(fecha_postemporada_inicio), "%04d-12-01", anio);
    snprintf(fecha_postemporada_fin, sizeof(fecha_postemporada_fin), "%04d-12-31", anio);

    // Insertar fases
    const char *sql_insert = "INSERT INTO temporada_fase (temporada_id, nombre, tipo_fase, fecha_inicio, fecha_fin, descripcion) VALUES (?, ?, ?, ?, ?, ?);";

    // Pretemporada
    if (preparar_stmt(sql_insert, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);
        sqlite3_bind_text(stmt, 2, "Pretemporada", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, PRETEMPORADA);
        sqlite3_bind_text(stmt, 4, fecha_pretemporada_inicio, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, fecha_pretemporada_fin, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, "Fase de preparacion y partidos amistosos", -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Temporada Regular
    if (preparar_stmt(sql_insert, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);
        sqlite3_bind_text(stmt, 2, "Temporada Regular", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, TEMPORADA_REGULAR);
        sqlite3_bind_text(stmt, 4, fecha_regular_inicio, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, fecha_regular_fin, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, "Competencia oficial regular", -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Postemporada
    if (preparar_stmt(sql_insert, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);
        sqlite3_bind_text(stmt, 2, "Postemporada", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, POSTTEMPORADA);
        sqlite3_bind_text(stmt, 4, fecha_postemporada_inicio, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, fecha_postemporada_fin, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, "Fase final y play-offs", -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    printf("Fases de temporada creadas automaticamente.\n");
}

void listar_temporadas()
{
    clear_screen();
    print_header("LISTAR TEMPORADAS");

    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id, nombre, anio, fecha_inicio, fecha_fin, estado FROM temporada ORDER BY anio DESC, fecha_inicio DESC;";

    if (preparar_stmt(sql, &stmt))
    {
        printf("=== TEMPORADAS ===\n\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int id = sqlite3_column_int(stmt, 0);
            const char *nombre = (const char*)sqlite3_column_text(stmt, 1);
            int anio = sqlite3_column_int(stmt, 2);
            const char *fecha_inicio = (const char*)sqlite3_column_text(stmt, 3);
            const char *fecha_fin = (const char*)sqlite3_column_text(stmt, 4);
            const char *estado = (const char*)sqlite3_column_text(stmt, 5);

            printf("ID: %d\n", id);
            printf("Nombre: %s\n", nombre);
            printf("Ano: %d\n", anio);
            printf("Periodo: %s al %s\n", fecha_inicio, fecha_fin);
            printf("Estado: %s\n", estado);
            printf("----------------------------------------\n");
        }

        if (!found)
        {
            mostrar_no_hay_registros("temporadas registradas");
        }
    }
    else
    {
        printf("Error al obtener las temporadas: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    pause_console();
}

void modificar_temporada()
{
    clear_screen();
    print_header("MODIFICAR TEMPORADA");

#ifdef UNIT_TEST

    // Mostrar lista de temporadas
    listar_temporadas();

    int temporada_id = input_int("\nIngrese el ID de la temporada a modificar (0 para cancelar): ");
    if (temporada_id == 0) return;

    if (!existe_id("temporada", temporada_id))
    {
        printf("ID de temporada invalido.\n");
        pause_console();
        return;
    }

    printf("\nSeleccione que modificar:\n");
    printf("1. Nombre\n");
    printf("2. Fechas\n");
    printf("3. Descripcion\n");
    printf("4. Estado\n");

    int opcion = input_int(">");

    sqlite3_stmt *stmt;
    switch (opcion)
    {
    case 1:
    {
        char nuevo_nombre[100];
        solicitar_texto_no_vacio("Nuevo nombre: ", nuevo_nombre, sizeof(nuevo_nombre));

        const char *sql = "UPDATE temporada SET nombre = ? WHERE id = ?;";
        if (preparar_stmt(sql, &stmt))
        {
            sqlite3_bind_text(stmt, 1, nuevo_nombre, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, temporada_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            printf("Nombre actualizado.\n");
        }
        break;
    }
    case 2:
    {
        char nueva_fecha_inicio[20];
        char nueva_fecha_fin[20];
        solicitar_fecha_yyyy_mm_dd("Nueva fecha de inicio (DD/MM/AAAA, Enter=hoy): ", nueva_fecha_inicio, sizeof(nueva_fecha_inicio));
        solicitar_fecha_yyyy_mm_dd("Nueva fecha de fin (DD/MM/AAAA, Enter=hoy): ", nueva_fecha_fin, sizeof(nueva_fecha_fin));
        while (strcmp(nueva_fecha_fin, nueva_fecha_inicio) < 0)
        {
            printf("La fecha de fin no puede ser anterior a la de inicio.\n");
            solicitar_fecha_yyyy_mm_dd("Nueva fecha de fin (DD/MM/AAAA, Enter=hoy): ", nueva_fecha_fin, sizeof(nueva_fecha_fin));
        }

        const char *sql = "UPDATE temporada SET fecha_inicio = ?, fecha_fin = ? WHERE id = ?;";
        if (preparar_stmt(sql, &stmt))
        {
            sqlite3_bind_text(stmt, 1, nueva_fecha_inicio, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, nueva_fecha_fin, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, temporada_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            printf("Fechas actualizadas.\n");
        }
        break;
    }
    case 3:
    {
        char nueva_descripcion[500];
        input_string("Nueva descripcion: ", nueva_descripcion, sizeof(nueva_descripcion));

        const char *sql = "UPDATE temporada SET descripcion = ? WHERE id = ?;";
        if (preparar_stmt(sql, &stmt))
        {
            sqlite3_bind_text(stmt, 1, nueva_descripcion, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, temporada_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            printf("Descripcion actualizada.\n");
        }
        break;
    }
    case 4:
    {
        printf("Nuevo estado:\n");
        printf("1. Planificada\n");
        printf("2. Activa\n");
        printf("3. Finalizada\n");

        int estado_opcion = input_int(">");
        const char *nuevo_estado;

        switch (estado_opcion)
        {
        case 1:
            nuevo_estado = "Planificada";
            break;
        case 2:
            nuevo_estado = "Activa";
            break;
        case 3:
            nuevo_estado = "Finalizada";
            break;
        default:
            printf("Opcion invalida.\n");
            pause_console();
            return;
        }

        const char *sql = "UPDATE temporada SET estado = ? WHERE id = ?;";
        if (preparar_stmt(sql, &stmt))
        {
            sqlite3_bind_text(stmt, 1, nuevo_estado, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, temporada_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            printf("Estado actualizado.\n");
        }
        break;
    }
    default:
        printf("Opcion invalida.\n");
    }

    pause_console();
#else
    int temporada_id = 0;
    const char *opciones[] = {"Nombre", "Fechas", "Descripcion", "Estado"};
    int opcion = 0;
    sqlite3_stmt *stmt = NULL;

    if (!temp_seleccionar_temporada_gui("SELECCIONAR TEMPORADA A MODIFICAR", &temporada_id))
    {
        return;
    }

    if (!temp_modal_selector_opcion_gui("MODIFICAR TEMPORADA",
                                        "CAMPO",
                                        opciones,
                                        ARRAY_COUNT(opciones),
                                        0,
                                        &opcion))
    {
        return;
    }

    switch (opcion)
    {
    case 0:
    {
        char nuevo_nombre[100] = {0};
        if (!temp_modal_input_texto_gui("MODIFICAR TEMPORADA",
                                        "Nuevo nombre:",
                                        "Solo letras, numeros y espacios",
                                        nuevo_nombre,
                                        sizeof(nuevo_nombre),
                                        0,
                                        TEMP_INPUT_ALNUM))
        {
            return;
        }

        if (preparar_stmt("UPDATE temporada SET nombre = ? WHERE id = ?;", &stmt))
        {
            sqlite3_bind_text(stmt, 1, nuevo_nombre, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, temporada_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            temp_mostrar_info_gui("TEMPORADA", "Nombre actualizado.");
        }
        break;
    }
    case 1:
    {
        char nueva_fecha_inicio[20] = {0};
        char nueva_fecha_fin[20] = {0};

        solicitar_fecha_yyyy_mm_dd("Nueva fecha de inicio (DD/MM/AAAA, Enter=hoy): ", nueva_fecha_inicio, sizeof(nueva_fecha_inicio));
        if (nueva_fecha_inicio[0] == '\0')
        {
            return;
        }

        while (1)
        {
            solicitar_fecha_yyyy_mm_dd("Nueva fecha de fin (DD/MM/AAAA, Enter=hoy): ", nueva_fecha_fin, sizeof(nueva_fecha_fin));
            if (nueva_fecha_fin[0] == '\0')
            {
                return;
            }

            if (strcmp(nueva_fecha_fin, nueva_fecha_inicio) >= 0)
            {
                break;
            }

            temp_mostrar_info_gui("FECHA INVALIDA", "La fecha de fin no puede ser anterior a la fecha de inicio.");
        }

        if (preparar_stmt("UPDATE temporada SET fecha_inicio = ?, fecha_fin = ? WHERE id = ?;", &stmt))
        {
            sqlite3_bind_text(stmt, 1, nueva_fecha_inicio, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, nueva_fecha_fin, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, temporada_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            temp_mostrar_info_gui("TEMPORADA", "Fechas actualizadas.");
        }
        break;
    }
    case 2:
    {
        char nueva_descripcion[500] = {0};
        if (!temp_modal_input_texto_gui("MODIFICAR TEMPORADA",
                                        "Nueva descripcion:",
                                        "Puede dejar este campo vacio",
                                        nueva_descripcion,
                                        sizeof(nueva_descripcion),
                                        1,
                                        TEMP_INPUT_ALNUM))
        {
            return;
        }

        if (preparar_stmt("UPDATE temporada SET descripcion = ? WHERE id = ?;", &stmt))
        {
            sqlite3_bind_text(stmt, 1, nueva_descripcion, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, temporada_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            temp_mostrar_info_gui("TEMPORADA", "Descripcion actualizada.");
        }
        break;
    }
    case 3:
    {
        const char *estados[] = {"Planificada", "Activa", "Finalizada"};
        int estado_idx = 0;

        if (!temp_modal_selector_opcion_gui("ESTADO DE TEMPORADA",
                                            "ESTADO",
                                            estados,
                                            ARRAY_COUNT(estados),
                                            0,
                                            &estado_idx))
        {
            return;
        }

        if (preparar_stmt("UPDATE temporada SET estado = ? WHERE id = ?;", &stmt))
        {
            sqlite3_bind_text(stmt, 1, estados[estado_idx], -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, temporada_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            temp_mostrar_info_gui("TEMPORADA", "Estado actualizado.");
        }
        break;
    }
    default:
        break;
    }
#endif
}

void eliminar_temporada()
{
    clear_screen();
    print_header("ELIMINAR TEMPORADA");

#ifdef UNIT_TEST

    listar_temporadas();

    int temporada_id = input_int("\nIngrese el ID de la temporada a eliminar (0 para cancelar): ");
    if (temporada_id == 0) return;

    if (!existe_id("temporada", temporada_id))
    {
        printf("ID de temporada invalido.\n");
        pause_console();
        return;
    }

    if (confirmar("Esta seguro de eliminar esta temporada? Se perderan todos los datos asociados."))
    {
        sqlite3_stmt *stmt = NULL;

        // Eliminar registros relacionados
        const char *sqls[] =
        {
            "DELETE FROM torneo_temporada WHERE temporada_id = ?;",
            "DELETE FROM temporada_fase WHERE temporada_id = ?;",
            "DELETE FROM equipo_temporada_fatiga WHERE temporada_id = ?;",
            "DELETE FROM jugador_temporada_fatiga WHERE temporada_id = ?;",
            "DELETE FROM equipo_temporada_evolucion WHERE temporada_id = ?;",
            "DELETE FROM temporada_resumen WHERE temporada_id = ?;",
            "DELETE FROM temporada WHERE id = ?;",
            NULL
        };

        for (int i = 0; sqls[i] != NULL; i++)
        {
            if (preparar_stmt(sqls[i], &stmt))
            {
                sqlite3_bind_int(stmt, 1, temporada_id);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
        }

        printf("Temporada eliminada exitosamente.\n");
    }
    else
    {
        printf("Eliminacion cancelada.\n");
    }

    pause_console();
#else
    int temporada_id = 0;

    if (!temp_seleccionar_temporada_gui("SELECCIONAR TEMPORADA A ELIMINAR", &temporada_id))
    {
        return;
    }

    if (!temp_modal_confirmar_gui("ELIMINAR TEMPORADA",
                                  "Esta seguro de eliminar esta temporada? Se perderan los datos asociados.",
                                  "Eliminar"))
    {
        temp_mostrar_info_gui("TEMPORADA", "Eliminacion cancelada.");
        return;
    }

    {
        sqlite3_stmt *stmt = NULL;
        const char *sqls[] =
        {
            "DELETE FROM torneo_temporada WHERE temporada_id = ?;",
            "DELETE FROM temporada_fase WHERE temporada_id = ?;",
            "DELETE FROM equipo_temporada_fatiga WHERE temporada_id = ?;",
            "DELETE FROM jugador_temporada_fatiga WHERE temporada_id = ?;",
            "DELETE FROM equipo_temporada_evolucion WHERE temporada_id = ?;",
            "DELETE FROM temporada_resumen WHERE temporada_id = ?;",
            "DELETE FROM temporada WHERE id = ?;",
            NULL
        };

        for (int i = 0; sqls[i] != NULL; i++)
        {
            if (preparar_stmt(sqls[i], &stmt))
            {
                sqlite3_bind_int(stmt, 1, temporada_id);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
        }
    }

    temp_mostrar_info_gui("TEMPORADA", "Temporada eliminada exitosamente.");
#endif
}

// ========== FUNCIONES DE FATIGA Y EVOLUCIoN ==========

float calcular_fatiga_equipo(int equipo_id, int dias_recientes)
{
    // Calcular fatiga basada en partidos jugados en los ultimos dias
    sqlite3_stmt *stmt = NULL;
    char sql_query[500];
    snprintf(sql_query, sizeof(sql_query), "SELECT COUNT(*) FROM partido_torneo pt "
             "JOIN equipo_torneo et ON pt.torneo_id = et.torneo_id "
             "WHERE et.equipo_id = ? AND pt.fecha >= date('now', '-%d days');", dias_recientes);

    float fatiga = 0.0f;
    if (preparar_stmt(sql_query, &stmt))
    {
        sqlite3_bind_int(stmt, 1, equipo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int partidos_recientes = sqlite3_column_int(stmt, 0);
            // Fatiga aumenta con mas partidos recientes
            fatiga = (float)(partidos_recientes * 10); // 10% por partido en periodo reciente
            if (fatiga > 100.0f) fatiga = 100.0f;
        }
        sqlite3_finalize(stmt);
    }

    return fatiga;
}

float calcular_fatiga_jugador(int jugador_id, int dias_recientes)
{
    // Similar al calculo de equipo pero considerando minutos jugados
    sqlite3_stmt *stmt;
    char sql_query[500];
    snprintf(sql_query, sizeof(sql_query), "SELECT COALESCE(SUM(je.minutos_jugados), 0) FROM jugador_estadisticas je "
             "JOIN partido_torneo pt ON je.torneo_id = pt.torneo_id "
             "WHERE je.jugador_id = ? AND pt.fecha >= date('now', '-%d days');", dias_recientes);

    float fatiga = 0.0f;
    if (preparar_stmt(sql_query, &stmt))
    {
        sqlite3_bind_int(stmt, 1, jugador_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int minutos_recientes = sqlite3_column_int(stmt, 0);
            // Fatiga basada en minutos jugados (90 min = partido completo = 15% fatiga)
            fatiga = (float)(minutos_recientes * 15) / 90.0f;
            if (fatiga > 100.0f) fatiga = 100.0f;
        }
        sqlite3_finalize(stmt);
    }

    return fatiga;
}

void actualizar_fatiga_equipo(int equipo_id, int temporada_id, [[maybe_unused]] int partido_id)
{
    // Calcular nueva fatiga
    float fatiga_actual = calcular_fatiga_equipo(equipo_id, 7); // ultima semana

    // Obtener rendimiento promedio del equipo
    float rendimiento = 5.0f; // Valor por defecto, se podria calcular basado en resultados

    // Insertar o actualizar registro de fatiga
    sqlite3_stmt *stmt;
    const char *sql = "INSERT OR REPLACE INTO equipo_temporada_fatiga "
                      "(equipo_id, temporada_id, fecha, fatiga_acumulada, partidos_jugados, rendimiento_promedio) "
                      "VALUES (?, ?, date('now'), ?, "
                      "(SELECT COUNT(*) FROM partido_torneo pt JOIN equipo_torneo et ON pt.torneo_id = et.torneo_id "
                      " WHERE et.equipo_id = ?), ?);";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, equipo_id);
        sqlite3_bind_int(stmt, 2, temporada_id);
        sqlite3_bind_double(stmt, 3, fatiga_actual);
        sqlite3_bind_int(stmt, 4, equipo_id);
        sqlite3_bind_double(stmt, 5, rendimiento);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void actualizar_fatiga_jugador(int jugador_id, int temporada_id, [[maybe_unused]] int minutos_jugados, int lesion_ocurrida)
{
    // Calcular nueva fatiga
    float fatiga_actual = calcular_fatiga_jugador(jugador_id, 7); // ultima semana

    // Aumentar fatiga si hubo lesion
    if (lesion_ocurrida)
    {
        fatiga_actual += 20.0f;
        if (fatiga_actual > 100.0f) fatiga_actual = 100.0f;
    }

    // Obtener rendimiento promedio del jugador
    float rendimiento = 5.0f; // Valor por defecto

    // Contar lesiones acumuladas
    sqlite3_stmt *stmt;
    const char *sql_lesiones = "SELECT COUNT(*) FROM lesion l "
                               "JOIN jugador j ON l.camiseta_id = j.numero " // Simplificacion
                               "WHERE j.id = ?;";

    int lesiones_count = 0;
    if (preparar_stmt(sql_lesiones, &stmt))
    {
        sqlite3_bind_int(stmt, 1, jugador_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            lesiones_count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // Insertar o actualizar registro de fatiga
    const char *sql = "INSERT OR REPLACE INTO jugador_temporada_fatiga "
                      "(jugador_id, temporada_id, fecha, fatiga_acumulada, minutos_jugados_total, rendimiento_promedio, lesiones_acumuladas) "
                      "VALUES (?, ?, date('now'), ?, "
                      "(SELECT COALESCE(SUM(minutos_jugados), 0) FROM jugador_estadisticas WHERE jugador_id = ?), ?, ?);";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, jugador_id);
        sqlite3_bind_int(stmt, 2, temporada_id);
        sqlite3_bind_double(stmt, 3, fatiga_actual);
        sqlite3_bind_int(stmt, 4, jugador_id);
        sqlite3_bind_double(stmt, 5, rendimiento);
        sqlite3_bind_int(stmt, 6, lesiones_count);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void actualizar_evolucion_equipo(int equipo_id, int temporada_id)
{
    // Calcular puntuacion de rendimiento basada en resultados recientes
    sqlite3_stmt *stmt;
    const char *sql = "SELECT COUNT(*) as partidos_total, "
                      "SUM(CASE WHEN ete.puntos > 0 THEN 1 ELSE 0 END) as partidos_ganados "
                      "FROM equipo_torneo_estadisticas ete "
                      "JOIN torneo t ON ete.torneo_id = t.id "
                      "WHERE ete.equipo_id = ? AND EXISTS "
                      "(SELECT 1 FROM torneo_temporada tt WHERE tt.torneo_id = t.id AND tt.temporada_id = ?);";

    int partidos_total = 0;
    int partidos_ganados = 0;
    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, equipo_id);
        sqlite3_bind_int(stmt, 2, temporada_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            partidos_total = sqlite3_column_int(stmt, 0);
            partidos_ganados = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    float puntuacion = partidos_total > 0 ? (float)(partidos_ganados * 100) / (float)partidos_total : 0.0f;

    // Determinar tendencia (simplificada)
    const char *tendencia = "Estable";
    if (puntuacion > 70.0f) tendencia = "Mejorando";
    else if (puntuacion < 40.0f) tendencia = "Decayendo";

    // Insertar registro de evolucion
    const char *sql_insert = "INSERT INTO equipo_temporada_evolucion "
                             "(equipo_id, temporada_id, fecha_medicion, puntuacion_rendimiento, tendencia, partidos_ganados, partidos_totales) "
                             "VALUES (?, ?, date('now'), ?, ?, ?, ?);";

    if (preparar_stmt(sql_insert, &stmt))
    {
        sqlite3_bind_int(stmt, 1, equipo_id);
        sqlite3_bind_int(stmt, 2, temporada_id);
        sqlite3_bind_double(stmt, 3, puntuacion);
        sqlite3_bind_text(stmt, 4, tendencia, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 5, partidos_ganados);
        sqlite3_bind_int(stmt, 6, partidos_total);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

// ========== FUNCIONES DE RESUMEN Y COMPARACIoN ==========

void generar_resumen_temporada(int temporada_id)
{
    sqlite3_stmt *stmt;

    // Contar estadisticas generales
    const char *sql_stats = "SELECT "
                            "COUNT(DISTINCT pt.id) as total_partidos, "
                            "COALESCE(SUM(pt.goles_equipo1 + pt.goles_equipo2), 0) as total_goles, "
                            "COUNT(DISTINCT l.id) as total_lesiones "
                            "FROM torneo_temporada tt "
                            "LEFT JOIN partido_torneo pt ON tt.torneo_id = pt.torneo_id "
                            "LEFT JOIN lesion l ON 1=1 " // Simplificacion
                            "WHERE tt.temporada_id = ?;";

    int total_partidos = 0;
    int total_goles = 0;
    int total_lesiones = 0;
    if (preparar_stmt(sql_stats, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            total_partidos = sqlite3_column_int(stmt, 0);
            total_goles = sqlite3_column_int(stmt, 1);
            total_lesiones = sqlite3_column_int(stmt, 2);
        }
        sqlite3_finalize(stmt);
    }

    float promedio_goles = total_partidos > 0 ? (float)total_goles / (float)total_partidos : 0.0f;

    // Encontrar equipo campeon (simplificado - el que mas puntos tiene)
    int equipo_campeon = -1;
    const char *sql_campeon = "SELECT ete.equipo_id "
                              "FROM equipo_torneo_estadisticas ete "
                              "JOIN torneo_temporada tt ON ete.torneo_id = tt.torneo_id "
                              "WHERE tt.temporada_id = ? "
                              "ORDER BY ete.puntos DESC, ete.goles_favor DESC LIMIT 1;";

    if (preparar_stmt(sql_campeon, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            equipo_campeon = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // Encontrar mejor goleador
    int mejor_goleador_id = -1;
    int mejor_goleador_goles = 0;
    const char *sql_goleador = "SELECT je.jugador_id, SUM(je.goles) as total_goles "
                               "FROM jugador_estadisticas je "
                               "JOIN torneo_temporada tt ON je.torneo_id = tt.torneo_id "
                               "WHERE tt.temporada_id = ? "
                               "GROUP BY je.jugador_id ORDER BY total_goles DESC LIMIT 1;";

    if (preparar_stmt(sql_goleador, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            mejor_goleador_id = sqlite3_column_int(stmt, 0);
            mejor_goleador_goles = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    // Insertar resumen
    const char *sql_insert = "INSERT OR REPLACE INTO temporada_resumen "
                             "(temporada_id, total_partidos, total_goles, promedio_goles_partido, "
                             "equipo_campeon_id, mejor_goleador_jugador_id, mejor_goleador_goles, "
                             "total_lesiones, fecha_generacion) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?, ?, date('now'));";

    if (preparar_stmt(sql_insert, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);
        sqlite3_bind_int(stmt, 2, total_partidos);
        sqlite3_bind_int(stmt, 3, total_goles);
        sqlite3_bind_double(stmt, 4, promedio_goles);
        sqlite3_bind_int(stmt, 5, equipo_campeon);
        sqlite3_bind_int(stmt, 6, mejor_goleador_id);
        sqlite3_bind_int(stmt, 7, mejor_goleador_goles);
        sqlite3_bind_int(stmt, 8, total_lesiones);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    printf("Resumen de temporada generado exitosamente.\n");
}

void comparar_temporadas(int temporada1_id, int temporada2_id)
{
    clear_screen();
    print_header("COMPARACIoN DE TEMPORADAS");

    // Obtener datos de ambas temporadas
    sqlite3_stmt *stmt;
    const char *sql = "SELECT t.nombre, tr.total_partidos, tr.total_goles, tr.promedio_goles_partido, "
                      "tr.total_lesiones, e.nombre as campeon "
                      "FROM temporada t "
                      "LEFT JOIN temporada_resumen tr ON t.id = tr.temporada_id "
                      "LEFT JOIN equipo e ON tr.equipo_campeon_id = e.id "
                      "WHERE t.id = ?;";

    char nombre1[100] = "";
    char nombre2[100] = "";
    char campeon1[50] = "";
    char campeon2[50] = "";
    int partidos1 = 0;
    int partidos2 = 0;
    int goles1 = 0;
    int goles2 = 0;
    int lesiones1 = 0;
    int lesiones2 = 0;
    float promedio1 = 0.0f;
    float promedio2 = 0.0f;

    // Temporada 1
    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada1_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strcpy_s(nombre1, sizeof(nombre1), (const char*)sqlite3_column_text(stmt, 0));
            partidos1 = sqlite3_column_int(stmt, 1);
            goles1 = sqlite3_column_int(stmt, 2);
            promedio1 = (float)sqlite3_column_double(stmt, 3);
            lesiones1 = sqlite3_column_int(stmt, 4);
            const char *campeon = (const char*)sqlite3_column_text(stmt, 5);
            strcpy_s(campeon1, sizeof(campeon1), campeon ? campeon : "N/A");
        }
        sqlite3_finalize(stmt);
    }

    // Temporada 2
    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada2_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strcpy_s(nombre2, sizeof(nombre2), (const char*)sqlite3_column_text(stmt, 0));
            partidos2 = sqlite3_column_int(stmt, 1);
            goles2 = sqlite3_column_int(stmt, 2);
            promedio2 = (float)sqlite3_column_double(stmt, 3);
            lesiones2 = sqlite3_column_int(stmt, 4);
            const char *campeon = (const char*)sqlite3_column_text(stmt, 5);
            strcpy_s(campeon2, sizeof(campeon2), campeon ? campeon : "N/A");
        }
        sqlite3_finalize(stmt);
    }

    // Mostrar comparacion
    printf("COMPARACIoN: %s vs %s\n\n", nombre1, nombre2);

    printf("Partidos jugados: %d vs %d (%+d)\n", partidos1, partidos2, partidos2 - partidos1);
    printf("Goles totales: %d vs %d (%+d)\n", goles1, goles2, goles2 - goles1);
    printf("Promedio de goles por partido: %.2f vs %.2f (%+.2f)\n", promedio1, promedio2, promedio2 - promedio1);
    printf("Lesiones totales: %d vs %d (%+d)\n", lesiones1, lesiones2, lesiones2 - lesiones1);
    printf("Equipo campeon: %s vs %s\n", campeon1, campeon2);

    pause_console();
}

// ========== FUNCIONES DE DASHBOARD Y REPORTES ==========

void mostrar_dashboard_temporada(int temporada_id)
{
    clear_screen();
    print_header("DASHBOARD DE TEMPORADA");

    // Obtener informacion basica de la temporada
    sqlite3_stmt *stmt;
    const char *sql_temporada = "SELECT nombre, estado, fecha_inicio, fecha_fin FROM temporada WHERE id = ?;";

    char nombre[100] = "";
    char estado[20] = "";
    char fecha_inicio[20] = "";
    char fecha_fin[20] = "";
    if (preparar_stmt(sql_temporada, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strcpy_s(nombre, sizeof(nombre), (const char*)sqlite3_column_text(stmt, 0));
            strcpy_s(estado, sizeof(estado), (const char*)sqlite3_column_text(stmt, 1));
            strcpy_s(fecha_inicio, sizeof(fecha_inicio), (const char*)sqlite3_column_text(stmt, 2));
            strcpy_s(fecha_fin, sizeof(fecha_fin), (const char*)sqlite3_column_text(stmt, 3));
        }
        sqlite3_finalize(stmt);
    }

    printf("TEMPORADA: %s\n", nombre);
    printf("Estado: %s | Periodo: %s al %s\n", estado, fecha_inicio, fecha_fin);
    printf("═══════════════════════════════════════════════════════════════\n\n");

    // Estadisticas generales
    const char *sql_resumen = "SELECT total_partidos, total_goles, promedio_goles_partido, total_lesiones "
                              "FROM temporada_resumen WHERE temporada_id = ?;";

    if (preparar_stmt(sql_resumen, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int partidos = sqlite3_column_int(stmt, 0);
            int goles = sqlite3_column_int(stmt, 1);
            float promedio = (float)sqlite3_column_double(stmt, 2);
            int lesiones = sqlite3_column_int(stmt, 3);

            printf("📊 ESTADiSTICAS GENERALES:\n");
            printf("   Partidos jugados: %d\n", partidos);
            printf("   Goles totales: %d\n", goles);
            printf("   Promedio de goles por partido: %.2f\n", promedio);
            printf("   Lesiones registradas: %d\n\n", lesiones);
        }
        else
        {
            mostrar_no_existe("resumen disponible");
            printf("Use 'Generar Resumen' para crear uno.\n\n");
        }
        sqlite3_finalize(stmt);
    }

    // Torneos asociados
    printf("🏆 TORNEOS ASOCIADOS:\n");
    const char *sql_torneos = "SELECT t.nombre, tf.nombre as fase "
                              "FROM torneo_temporada tt "
                              "JOIN torneo t ON tt.torneo_id = t.id "
                              "LEFT JOIN temporada_fase tf ON tt.fase_id = tf.id "
                              "WHERE tt.temporada_id = ? ORDER BY tt.orden_en_temporada;";

    if (preparar_stmt(sql_torneos, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);

        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count++;
            const char *torneo = (const char*)sqlite3_column_text(stmt, 0);
            const char *fase = (const char*)sqlite3_column_text(stmt, 1);
            printf("   %d. %s (%s)\n", count, torneo, fase ? fase : "Sin fase asignada");
        }

        if (count == 0)
        {
            printf("   ");
            mostrar_no_hay_registros("torneos asociados a esta temporada");
        }
        sqlite3_finalize(stmt);
    }

    printf("\n═══════════════════════════════════════════════════════════════\n");
    pause_console();
}

void mostrar_estadisticas_fatiga(int temporada_id)
{
    clear_screen();
    print_header("ESTADiSTICAS DE FATIGA");

    printf("NIVEL DE FATIGA PROMEDIO POR EQUIPO:\n\n");

    sqlite3_stmt *stmt;
    const char *sql_equipos = "SELECT e.nombre, etf.fatiga_acumulada, etf.partidos_jugados "
                              "FROM equipo_temporada_fatiga etf "
                              "JOIN equipo e ON etf.equipo_id = e.id "
                              "WHERE etf.temporada_id = ? "
                              "ORDER BY etf.fatiga_acumulada DESC;";

    if (preparar_stmt(sql_equipos, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            const char *equipo = (const char*)sqlite3_column_text(stmt, 0);
            float fatiga = (float)sqlite3_column_double(stmt, 1);
            int partidos = sqlite3_column_int(stmt, 2);

            printf("🏃 %s: %.1f%% fatiga (%d partidos)\n", equipo, fatiga, partidos);
        }

        if (!found)
        {
            mostrar_no_hay_registros("datos de fatiga disponibles");
        }
        sqlite3_finalize(stmt);
    }

    printf("\n═══════════════════════════════════════════════════════════════\n\n");

    printf("JUGADORES CON MAYOR FATIGA:\n\n");

    const char *sql_jugadores = "SELECT j.nombre, e.nombre as equipo, jtf.fatiga_acumulada, jtf.lesiones_acumuladas "
                                "FROM jugador_temporada_fatiga jtf "
                                "JOIN jugador j ON jtf.jugador_id = j.id "
                                "JOIN equipo e ON j.equipo_id = e.id "
                                "WHERE jtf.temporada_id = ? "
                                "ORDER BY jtf.fatiga_acumulada DESC LIMIT 10;";

    if (preparar_stmt(sql_jugadores, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            const char *jugador = (const char*)sqlite3_column_text(stmt, 0);
            const char *equipo = (const char*)sqlite3_column_text(stmt, 1);
            float fatiga = (float)sqlite3_column_double(stmt, 2);
            int lesiones = sqlite3_column_int(stmt, 3);

            printf("⚠️  %s (%s): %.1f%% fatiga, %d lesiones\n", jugador, equipo, fatiga, lesiones);
        }

        if (!found)
        {
            mostrar_no_hay_registros("datos de fatiga de jugadores disponibles");
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

void exportar_resumen_temporada(int temporada_id)
{
    // Obtener nombre de la temporada
    sqlite3_stmt *stmt;
    const char *sql_temporada = "SELECT nombre FROM temporada WHERE id = ?;";
    char nombre_temporada[100] = "";

    if (preparar_stmt(sql_temporada, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strcpy_s(nombre_temporada, sizeof(nombre_temporada), (const char*)sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }

    // Crear archivo
    char filename[150];
    snprintf(filename, sizeof(filename), "resumen_temporada_%s.txt", nombre_temporada);

    const char *export_dir = get_export_dir();
    char filepath[300];
    snprintf(filepath, sizeof(filepath), "%s%s%s", export_dir, PATH_SEP, filename);

    FILE *file;
    errno_t err = fopen_s(&file, filepath, "w");
    if (err != 0)
    {
        printf("Error al crear archivo de resumen.\n");
        return;
    }
    if (!file)
    {
        printf("Error al crear archivo de resumen.\n");
        return;
    }

    fprintf(file, "RESUMEN DE TEMPORADA: %s\n", nombre_temporada);
    fprintf(file, "Generado: %s\n\n", __DATE__);

    // Copiar contenido del dashboard al archivo
    const char *sql_resumen = "SELECT total_partidos, total_goles, promedio_goles_partido, "
                              "total_lesiones, e.nombre as campeon, j.nombre as goleador, mejor_goleador_goles "
                              "FROM temporada_resumen tr "
                              "LEFT JOIN equipo e ON tr.equipo_campeon_id = e.id "
                              "LEFT JOIN jugador j ON tr.mejor_goleador_jugador_id = j.id "
                              "WHERE tr.temporada_id = ?;";

    if (preparar_stmt(sql_resumen, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int partidos = sqlite3_column_int(stmt, 0);
            int goles = sqlite3_column_int(stmt, 1);
            float promedio = (float)sqlite3_column_double(stmt, 2);
            int lesiones = sqlite3_column_int(stmt, 3);
            const char *campeon = (const char*)sqlite3_column_text(stmt, 4);
            const char *goleador = (const char*)sqlite3_column_text(stmt, 5);
            int goles_goleador = sqlite3_column_int(stmt, 6);

            fprintf(file, "ESTADISTICAS GENERALES:\n");
            fprintf(file, "Partidos jugados: %d\n", partidos);
            fprintf(file, "Goles totales: %d\n", goles);
            fprintf(file, "Promedio de goles por partido: %.2f\n", promedio);
            fprintf(file, "Lesiones registradas: %d\n", lesiones);
            fprintf(file, "Equipo campeon: %s\n", campeon ? campeon : "No determinado");
            fprintf(file, "Mejor goleador: %s (%d goles)\n", goleador ? goleador : "No determinado", goles_goleador);
        }
        sqlite3_finalize(stmt);
    }

    fclose(file);
    printf("Resumen exportado exitosamente a: %s\n", filepath);
}

// ========== MENu PRINCIPAL ==========

void administrar_temporada()
{
    clear_screen();
    print_header("ADMINISTRAR TEMPORADA");

#ifdef UNIT_TEST

    // Mostrar lista de temporadas
    listar_temporadas();

    int temporada_id = input_int("\nIngrese el ID de la temporada a administrar (0 para cancelar): ");
    if (temporada_id == 0) return;

    if (!existe_id("temporada", temporada_id))
    {
        printf("ID de temporada invalido.\n");
        pause_console();
        return;
    }

    while (1)
    {
        clear_screen();
        print_header("ADMINISTRAR TEMPORADA");

        printf("1. Dashboard de Temporada\n");
        printf("2. Ver Estadisticas de Fatiga\n");
        printf("3. Generar Resumen de Temporada\n");
        printf("4. Exportar Resumen de Temporada\n");
        printf("5. Generar Resumen Mensual\n");
        printf("6. Ver Resumen Mensual\n");
        printf("7. Listar Resumenes Mensuales\n");
        printf("8. Exportar Resumen Mensual\n");
        printf("9. Asociar Torneo a Temporada\n");
        printf("10. Gestionar Fases de Temporada\n");
        printf("0. Volver\n");

        int opcion = input_int(">");

        switch (opcion)
        {
        case 1:
            mostrar_dashboard_temporada(temporada_id);
            break;
        case 2:
            mostrar_estadisticas_fatiga(temporada_id);
            break;
        case 3:
            generar_resumen_temporada(temporada_id);
            pause_console();
            break;
        case 4:
            exportar_resumen_temporada(temporada_id);
            pause_console();
            break;
        case 5:
        {
            // Generar resumen mensual
            printf("Ingrese el mes (YYYY-MM): ");
            char mes_anio[8] = {0};
            input_string("", mes_anio, sizeof(mes_anio));
            generar_resumen_mensual(temporada_id, mes_anio);
            pause_console();
            break;
        }
        case 6:
        {
            // Ver resumen mensual
            printf("Ingrese el mes (YYYY-MM): ");
            char mes_anio[8] = {0};
            input_string("", mes_anio, sizeof(mes_anio));
            mostrar_resumen_mensual(temporada_id, mes_anio);
            break;
        }
        case 7:
            listar_resumenes_mensuales(temporada_id);
            break;
        case 8:
        {
            // Exportar resumen mensual
            printf("Ingrese el mes (YYYY-MM): ");
            char mes_anio[8] = {0};
            input_string("", mes_anio, sizeof(mes_anio));
            exportar_resumen_mensual(temporada_id, mes_anio);
            pause_console();
            break;
        }
        case 9:
            asociar_torneo_temporada(0); // Implementar seleccion de torneo
            break;
        case 10:
            // Gestionar fases - por implementar
            printf("Funcion no implementada aun.\n");
            pause_console();
            break;
        case 0:
            return;
        default:
            printf("Opcion invalida.\n");
            pause_console();
        }
    }
#else
    int temporada_id = 0;
    const char *acciones[] = {
        "Dashboard de Temporada",
        "Ver Estadisticas de Fatiga",
        "Generar Resumen de Temporada",
        "Exportar Resumen de Temporada",
        "Generar Resumen Mensual",
        "Ver Resumen Mensual",
        "Listar Resumenes Mensuales",
        "Exportar Resumen Mensual",
        "Asociar Torneo a Temporada",
        "Gestionar Fases de Temporada",
        "Volver"
    };
    int opcion = 0;
    int seleccion = 0;

    if (!temp_seleccionar_temporada_gui("SELECCIONAR TEMPORADA A ADMINISTRAR", &temporada_id))
    {
        return;
    }

    while (1)
    {
        if (!temp_modal_selector_opcion_gui("ADMINISTRAR TEMPORADA",
                                            "ACCION",
                                            acciones,
                                            ARRAY_COUNT(acciones),
                                            seleccion,
                                            &opcion))
        {
            return;
        }

        seleccion = opcion;
        switch (opcion)
        {
        case 0:
            mostrar_dashboard_temporada(temporada_id);
            break;
        case 1:
            mostrar_estadisticas_fatiga(temporada_id);
            break;
        case 2:
            generar_resumen_temporada(temporada_id);
            temp_mostrar_info_gui("TEMPORADA", "Resumen de temporada generado.");
            break;
        case 3:
            exportar_resumen_temporada(temporada_id);
            temp_mostrar_info_gui("TEMPORADA", "Resumen de temporada exportado.");
            break;
        case 4:
        {
            char mes_anio[8] = {0};
            while (1)
            {
                if (!temp_modal_input_texto_gui("RESUMEN MENSUAL",
                                                "Ingrese mes (YYYY-MM):",
                                                "Ejemplo: 2025-08",
                                                mes_anio,
                                                sizeof(mes_anio),
                                                0,
                                                TEMP_INPUT_DATE))
                {
                    break;
                }
                if (temp_validar_mes_anio(mes_anio))
                {
                    generar_resumen_mensual(temporada_id, mes_anio);
                    temp_mostrar_info_gui("TEMPORADA", "Resumen mensual generado.");
                    break;
                }
                temp_mostrar_info_gui("FORMATO INVALIDO", "Ingrese un mes valido con formato YYYY-MM.");
            }
            break;
        }
        case 5:
        {
            char mes_anio[8] = {0};
            while (1)
            {
                if (!temp_modal_input_texto_gui("VER RESUMEN MENSUAL",
                                                "Ingrese mes (YYYY-MM):",
                                                "Ejemplo: 2025-08",
                                                mes_anio,
                                                sizeof(mes_anio),
                                                0,
                                                TEMP_INPUT_DATE))
                {
                    break;
                }
                if (temp_validar_mes_anio(mes_anio))
                {
                    mostrar_resumen_mensual(temporada_id, mes_anio);
                    break;
                }
                temp_mostrar_info_gui("FORMATO INVALIDO", "Ingrese un mes valido con formato YYYY-MM.");
            }
            break;
        }
        case 6:
            listar_resumenes_mensuales(temporada_id);
            break;
        case 7:
        {
            char mes_anio[8] = {0};
            while (1)
            {
                if (!temp_modal_input_texto_gui("EXPORTAR RESUMEN MENSUAL",
                                                "Ingrese mes (YYYY-MM):",
                                                "Ejemplo: 2025-08",
                                                mes_anio,
                                                sizeof(mes_anio),
                                                0,
                                                TEMP_INPUT_DATE))
                {
                    break;
                }
                if (temp_validar_mes_anio(mes_anio))
                {
                    exportar_resumen_mensual(temporada_id, mes_anio);
                    temp_mostrar_info_gui("TEMPORADA", "Resumen mensual exportado.");
                    break;
                }
                temp_mostrar_info_gui("FORMATO INVALIDO", "Ingrese un mes valido con formato YYYY-MM.");
            }
            break;
        }
        case 8:
            asociar_torneo_temporada(0);
            break;
        case 9:
            temp_mostrar_info_gui("TEMPORADA", "Gestion de fases no implementada aun.");
            break;
        case 10:
            return;
        default:
            return;
        }
    }
#endif
}

void asociar_torneo_temporada(int torneo_id)
{
#ifdef UNIT_TEST
    if (torneo_id == 0)
    {
        // Mostrar lista de torneos disponibles
        clear_screen();
        print_header("ASOCIAR TORNO A TEMPORADA");

        sqlite3_stmt *stmt;
        const char *sql = "SELECT id, nombre FROM torneo WHERE id NOT IN "
                          "(SELECT torneo_id FROM torneo_temporada) ORDER BY id;";

        if (preparar_stmt(sql, &stmt))
        {
            printf("\n=== TORNEOS DISPONIBLES ===\n\n");

            int found = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                found = 1;
                int id = sqlite3_column_int(stmt, 0);
                const char *nombre = (const char*)sqlite3_column_text(stmt, 1);
                printf("%d. %s\n", id, nombre);
            }

            if (!found)
            {
                mostrar_no_hay_registros("torneos disponibles para asociar");
                sqlite3_finalize(stmt);
                pause_console();
                return;
            }
            sqlite3_finalize(stmt);

            torneo_id = input_int("\nIngrese el ID del torneo (0 para cancelar): ");
            if (torneo_id == 0) return;
        }
    }

    // Mostrar temporadas disponibles
    listar_temporadas();

    int temporada_id = input_int("\nIngrese el ID de la temporada (0 para cancelar): ");
    if (temporada_id == 0) return;

    if (!existe_id("temporada", temporada_id))
    {
        printf("ID de temporada invalido.\n");
        pause_console();
        return;
    }

    // Asociar torneo a temporada
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO torneo_temporada (torneo_id, temporada_id) VALUES (?, ?);";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, temporada_id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Torneo asociado a temporada exitosamente.\n");
        }
        else
        {
            printf("Error al asociar torneo: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
#else
    if (torneo_id == 0)
    {
        if (!temp_seleccionar_torneo_disponible_gui("SELECCIONAR TORNEO DISPONIBLE", &torneo_id))
        {
            return;
        }
    }

    {
        int temporada_id = 0;
        sqlite3_stmt *stmt = NULL;
        const char *sql = "INSERT INTO torneo_temporada (torneo_id, temporada_id) VALUES (?, ?);";

        if (!temp_seleccionar_temporada_gui("SELECCIONAR TEMPORADA PARA ASOCIAR", &temporada_id))
        {
            return;
        }

        if (preparar_stmt(sql, &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, temporada_id);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                temp_mostrar_info_gui("TEMPORADA", "Torneo asociado a temporada exitosamente.");
            }
            else
            {
                char msg[256];
                snprintf(msg, sizeof(msg), "Error al asociar torneo: %s", sqlite3_errmsg(db));
                temp_mostrar_info_gui("ERROR", msg);
            }
            sqlite3_finalize(stmt);
        }
    }
#endif
}

// ========== FUNCIONES DE RESUMEN MENSUAL ==========

void generar_resumen_mensual(int temporada_id, const char* mes_anio)
{
    sqlite3_stmt *stmt;

    // Contar estadisticas del mes para partidos de torneo
    const char *sql_partidos = "SELECT "
                               "COUNT(pt.id) as total_partidos, "
                               "SUM(pt.goles_equipo1 + pt.goles_equipo2) as total_goles, "
                               "COUNT(CASE WHEN pt.goles_equipo1 > pt.goles_equipo2 THEN 1 END) as partidos_ganados, "
                               "COUNT(CASE WHEN pt.goles_equipo1 = pt.goles_equipo2 THEN 1 END) as partidos_empatados, "
                               "COUNT(CASE WHEN pt.goles_equipo1 < pt.goles_equipo2 THEN 1 END) as partidos_perdidos "
                               "FROM partido_torneo pt "
                               "JOIN torneo_temporada tt ON pt.torneo_id = tt.torneo_id "
                               "WHERE tt.temporada_id = ? AND strftime('%Y-%m', pt.fecha) = ?;";

    int total_partidos = 0;
    int total_goles = 0;
    int partidos_ganados = 0;
    int partidos_empatados = 0;
    int partidos_perdidos = 0;
    if (preparar_stmt(sql_partidos, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);
        sqlite3_bind_text(stmt, 2, mes_anio, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            total_partidos = sqlite3_column_int(stmt, 0);
            total_goles = sqlite3_column_int(stmt, 1);
            partidos_ganados = sqlite3_column_int(stmt, 2);
            partidos_empatados = sqlite3_column_int(stmt, 3);
            partidos_perdidos = sqlite3_column_int(stmt, 4);
        }
        sqlite3_finalize(stmt);
    }

    float promedio_goles = total_partidos > 0 ? (float)total_goles / (float)total_partidos : 0.0f;

    // Contar lesiones del mes
    const char *sql_lesiones = "SELECT COUNT(*) FROM lesion WHERE strftime('%Y-%m', fecha) = ?;";
    int total_lesiones = 0;
    if (preparar_stmt(sql_lesiones, &stmt))
    {
        sqlite3_bind_text(stmt, 1, mes_anio, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            total_lesiones = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // Calcular gastos e ingresos del mes
    const char *sql_finanzas = "SELECT "
                               "SUM(CASE WHEN tipo = 0 THEN monto ELSE 0 END) as gastos, "
                               "SUM(CASE WHEN tipo = 1 THEN monto ELSE 0 END) as ingresos "
                               "FROM financiamiento WHERE strftime('%Y-%m', fecha) = ?;";

    float total_gastos = 0.0f;
    float total_ingresos = 0.0f;
    if (preparar_stmt(sql_finanzas, &stmt))
    {
        sqlite3_bind_text(stmt, 1, mes_anio, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            total_gastos = (float)sqlite3_column_double(stmt, 0);
            total_ingresos = (float)sqlite3_column_double(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    // Encontrar mejor y peor equipo del mes (basado en puntos obtenidos)
    int mejor_equipo_mes = -1;
    int peor_equipo_mes = -1;
    const char *sql_equipos = "SELECT e.id, "
                              "SUM(CASE WHEN pt.goles_equipo1 > pt.goles_equipo2 AND et.equipo_id = pt.equipo1_id THEN 3 "
                              "          WHEN pt.goles_equipo1 < pt.goles_equipo2 AND et.equipo_id = pt.equipo2_id THEN 3 "
                              "          WHEN pt.goles_equipo1 = pt.goles_equipo2 THEN 1 ELSE 0 END) as puntos "
                              "FROM equipo e "
                              "JOIN equipo_torneo et ON e.id = et.equipo_id "
                              "JOIN partido_torneo pt ON et.torneo_id = pt.torneo_id "
                              "JOIN torneo_temporada tt ON pt.torneo_id = tt.torneo_id "
                              "WHERE tt.temporada_id = ? AND strftime('%Y-%m', pt.fecha) = ? "
                              "GROUP BY e.id ORDER BY puntos DESC LIMIT 1;";

    if (preparar_stmt(sql_equipos, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);
        sqlite3_bind_text(stmt, 2, mes_anio, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            mejor_equipo_mes = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // Peor equipo del mes
    const char *sql_peor = "SELECT e.id, "
                           "SUM(CASE WHEN pt.goles_equipo1 > pt.goles_equipo2 AND et.equipo_id = pt.equipo1_id THEN 3 "
                           "          WHEN pt.goles_equipo1 < pt.goles_equipo2 AND et.equipo_id = pt.equipo2_id THEN 3 "
                           "          WHEN pt.goles_equipo1 = pt.goles_equipo2 THEN 1 ELSE 0 END) as puntos "
                           "FROM equipo e "
                           "JOIN equipo_torneo et ON e.id = et.equipo_id "
                           "JOIN partido_torneo pt ON et.torneo_id = pt.torneo_id "
                           "JOIN torneo_temporada tt ON pt.torneo_id = tt.torneo_id "
                           "WHERE tt.temporada_id = ? AND strftime('%Y-%m', pt.fecha) = ? "
                           "GROUP BY e.id ORDER BY puntos ASC LIMIT 1;";

    if (preparar_stmt(sql_peor, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);
        sqlite3_bind_text(stmt, 2, mes_anio, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            peor_equipo_mes = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // Insertar resumen mensual
    const char *sql_insert = "INSERT OR REPLACE INTO mensual_resumen "
                             "(temporada_id, mes_anio, total_partidos, total_goles, promedio_goles_partido, "
                             "partidos_ganados, partidos_empatados, partidos_perdidos, total_lesiones, "
                             "total_gastos, total_ingresos, mejor_equipo_mes, peor_equipo_mes, fecha_generacion) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, date('now'));";

    if (preparar_stmt(sql_insert, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);
        sqlite3_bind_text(stmt, 2, mes_anio, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, total_partidos);
        sqlite3_bind_int(stmt, 4, total_goles);
        sqlite3_bind_double(stmt, 5, promedio_goles);
        sqlite3_bind_int(stmt, 6, partidos_ganados);
        sqlite3_bind_int(stmt, 7, partidos_empatados);
        sqlite3_bind_int(stmt, 8, partidos_perdidos);
        sqlite3_bind_int(stmt, 9, total_lesiones);
        sqlite3_bind_double(stmt, 10, total_gastos);
        sqlite3_bind_double(stmt, 11, total_ingresos);
        sqlite3_bind_int(stmt, 12, mejor_equipo_mes);
        sqlite3_bind_int(stmt, 13, peor_equipo_mes);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    printf("Resumen mensual %s generado exitosamente.\n", mes_anio);
}

void mostrar_resumen_mensual(int temporada_id, const char* mes_anio)
{
    clear_screen();
    print_header("RESUMEN MENSUAL");

    sqlite3_stmt *stmt;
    const char *sql = "SELECT * FROM mensual_resumen WHERE temporada_id = ? AND mes_anio = ?;";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);
        sqlite3_bind_text(stmt, 2, mes_anio, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            // Obtener nombre de la temporada
            char nombre_temporada[100] = "";
            obtener_nombre_temporada(temporada_id, nombre_temporada, sizeof(nombre_temporada));

            printf("TEMPORADA: %s\n", nombre_temporada);
            printf("MES: %s\n", mes_anio);
            printf("═══════════════════════════════════════════════════════════════\n\n");

            int total_partidos = sqlite3_column_int(stmt, 3);
            int total_goles = sqlite3_column_int(stmt, 4);
            float promedio_goles = (float)sqlite3_column_double(stmt, 5);
            int partidos_ganados = sqlite3_column_int(stmt, 6);
            int partidos_empatados = sqlite3_column_int(stmt, 7);
            int partidos_perdidos = sqlite3_column_int(stmt, 8);
            int total_lesiones = sqlite3_column_int(stmt, 9);
            float total_gastos = (float)sqlite3_column_double(stmt, 10);
            float total_ingresos = (float)sqlite3_column_double(stmt, 11);
            int mejor_equipo_mes = sqlite3_column_int(stmt, 12);
            int peor_equipo_mes = sqlite3_column_int(stmt, 13);

            printf("📊 PARTIDOS:\n");
            printf("   Total jugados: %d\n", total_partidos);
            printf("   Ganados: %d\n", partidos_ganados);
            printf("   Empatados: %d\n", partidos_empatados);
            printf("   Perdidos: %d\n", partidos_perdidos);
            printf("   Goles totales: %d\n", total_goles);
            printf("   Promedio de goles por partido: %.2f\n\n", promedio_goles);

            printf("🏥 LESIONES:\n");
            printf("   Total registradas: %d\n\n", total_lesiones);

            printf("💰 FINANZAS:\n");
            printf("   Gastos totales: $%.2f\n", total_gastos);
            printf("   Ingresos totales: $%.2f\n", total_ingresos);
            printf("   Balance: $%.2f\n\n", total_ingresos - total_gastos);

            printf("🏆 EQUIPOS DESTACADOS:\n");
            if (mejor_equipo_mes > 0)
            {
                printf("   Mejor equipo del mes: %s\n", get_equipo_nombre(mejor_equipo_mes));
            }
            if (peor_equipo_mes > 0)
            {
                printf("   Peor equipo del mes: %s\n", get_equipo_nombre(peor_equipo_mes));
            }

        }
        else
        {
            printf("No hay resumen disponible para el mes %s.\n", mes_anio);
            printf("Use 'Generar Resumen Mensual' para crearlo.\n");
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

void listar_resumenes_mensuales(int temporada_id)
{
    clear_screen();
    print_header("RESuMENES MENSUALES");

    // Obtener nombre de la temporada
    sqlite3_stmt *stmt;
    const char *sql_temporada = "SELECT nombre FROM temporada WHERE id = ?;";
    char nombre_temporada[100] = "";

    if (preparar_stmt(sql_temporada, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strcpy_s(nombre_temporada, sizeof(nombre_temporada), (const char*)sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }

    printf("TEMPORADA: %s\n", nombre_temporada);
    printf("═══════════════════════════════════════════════════════════════\n\n");

    const char *sql = "SELECT mes_anio, total_partidos, total_goles, promedio_goles_partido, "
                      "total_lesiones, total_gastos, total_ingresos "
                      "FROM mensual_resumen WHERE temporada_id = ? ORDER BY mes_anio;";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);

        printf("Mes      | Partidos | Goles | Promedio | Lesiones | Gastos   | Ingresos\n");
        printf("---------|----------|-------|----------|----------|----------|----------\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            const char *mes_anio = (const char*)sqlite3_column_text(stmt, 0);
            int total_partidos = sqlite3_column_int(stmt, 1);
            int total_goles = sqlite3_column_int(stmt, 2);
            float promedio = (float)sqlite3_column_double(stmt, 3);
            int lesiones = sqlite3_column_int(stmt, 4);
            float gastos = (float)sqlite3_column_double(stmt, 5);
            float ingresos = (float)sqlite3_column_double(stmt, 6);

            printf("%-8s | %-8d | %-5d | %-8.2f | %-8d | %-8.0f | %-8.0f\n",
                   mes_anio, total_partidos, total_goles, promedio, lesiones, gastos, ingresos);
        }

        if (!found)
        {
            mostrar_no_hay_registros("resumenes mensuales disponibles");
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

void exportar_resumen_mensual(int temporada_id, const char* mes_anio)
{
    // Obtener nombre de la temporada
    sqlite3_stmt *stmt;
    const char *sql_temporada = "SELECT nombre FROM temporada WHERE id = ?;";
    char nombre_temporada[100] = "";

    if (preparar_stmt(sql_temporada, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strcpy_s(nombre_temporada, sizeof(nombre_temporada), (const char*)sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }

    // Crear archivo
    char filename[150];
    snprintf(filename, sizeof(filename), "resumen_mensual_%s_%s.txt", nombre_temporada, mes_anio);

    const char *export_dir = get_export_dir();
    char filepath[300];
    snprintf(filepath, sizeof(filepath), "%s%s%s", export_dir, PATH_SEP, filename);

    FILE *file;
    errno_t err = fopen_s(&file, filepath, "w");
    if (err != 0)
    {
        printf("Error al crear archivo de resumen mensual.\n");
        return;
    }
    if (!file)
    {
        printf("Error al crear archivo de resumen mensual.\n");
        return;
    }

    fprintf(file, "RESUMEN MENSUAL: %s - %s\n", nombre_temporada, mes_anio);
    fprintf(file, "Generado: %s\n\n", __DATE__);

    // Copiar contenido del resumen mensual al archivo
    const char *sql = "SELECT * FROM mensual_resumen WHERE temporada_id = ? AND mes_anio = ?;";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, temporada_id);
        sqlite3_bind_text(stmt, 2, mes_anio, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int total_partidos = sqlite3_column_int(stmt, 3);
            int total_goles = sqlite3_column_int(stmt, 4);
            float promedio_goles = (float)sqlite3_column_double(stmt, 5);
            int partidos_ganados = sqlite3_column_int(stmt, 6);
            int partidos_empatados = sqlite3_column_int(stmt, 7);
            int partidos_perdidos = sqlite3_column_int(stmt, 8);
            int total_lesiones = sqlite3_column_int(stmt, 9);
            float total_gastos = (float)sqlite3_column_double(stmt, 10);
            float total_ingresos = (float)sqlite3_column_double(stmt, 11);
            int mejor_equipo_mes = sqlite3_column_int(stmt, 12);
            int peor_equipo_mes = sqlite3_column_int(stmt, 13);

            fprintf(file, "ESTADISTICAS DE PARTIDOS:\n");
            fprintf(file, "Partidos jugados: %d\n", total_partidos);
            fprintf(file, "Partidos ganados: %d\n", partidos_ganados);
            fprintf(file, "Partidos empatados: %d\n", partidos_empatados);
            fprintf(file, "Partidos perdidos: %d\n", partidos_perdidos);
            fprintf(file, "Goles totales: %d\n", total_goles);
            fprintf(file, "Promedio de goles por partido: %.2f\n\n", promedio_goles);

            fprintf(file, "LESIONES:\n");
            fprintf(file, "Total registradas: %d\n\n", total_lesiones);

            fprintf(file, "FINANZAS:\n");
            fprintf(file, "Gastos totales: $%.2f\n", total_gastos);
            fprintf(file, "Ingresos totales: $%.2f\n", total_ingresos);
            fprintf(file, "Balance: $%.2f\n\n", total_ingresos - total_gastos);

            fprintf(file, "EQUIPOS DESTACADOS:\n");
            if (mejor_equipo_mes > 0)
            {
                fprintf(file, "Mejor equipo del mes: %s\n", get_equipo_nombre(mejor_equipo_mes));
            }
            if (peor_equipo_mes > 0)
            {
                fprintf(file, "Peor equipo del mes: %s\n", get_equipo_nombre(peor_equipo_mes));
            }
        }
        sqlite3_finalize(stmt);
    }

    fclose(file);
    printf("Resumen mensual exportado exitosamente a: %s\n", filepath);
}

// ========== FUNCIoN DE COMPARACIoN DE TEMPORADAS ==========

void seleccionar_comparar_temporadas()
{
    clear_screen();
    print_header("COMPARAR TEMPORADAS");

#ifdef UNIT_TEST

    // Mostrar lista de temporadas disponibles
    listar_temporadas();

    // Solicitar primera temporada
    int temporada1_id = input_int("\nIngrese el ID de la primera temporada a comparar (0 para cancelar): ");
    if (temporada1_id == 0) return;

    if (!existe_id("temporada", temporada1_id))
    {
        printf("ID de temporada invalido.\n");
        pause_console();
        return;
    }

    // Solicitar segunda temporada
    int temporada2_id = input_int("\nIngrese el ID de la segunda temporada a comparar (0 para cancelar): ");
    if (temporada2_id == 0) return;

    if (!existe_id("temporada", temporada2_id))
    {
        printf("ID de temporada invalido.\n");
        pause_console();
        return;
    }

    if (temporada1_id == temporada2_id)
    {
        printf("Debe seleccionar dos temporadas diferentes.\n");
        pause_console();
        return;
    }

    // Llamar a la funcion de comparacion
    comparar_temporadas(temporada1_id, temporada2_id);
#else
    int temporada1_id = 0;
    int temporada2_id = 0;

    if (!temp_seleccionar_temporada_gui("SELECCIONE LA PRIMERA TEMPORADA", &temporada1_id))
    {
        return;
    }

    while (1)
    {
        if (!temp_seleccionar_temporada_gui("SELECCIONE LA SEGUNDA TEMPORADA", &temporada2_id))
        {
            return;
        }

        if (temporada1_id != temporada2_id)
        {
            break;
        }

        temp_mostrar_info_gui("COMPARACION", "Debe seleccionar dos temporadas diferentes.");
    }

    comparar_temporadas(temporada1_id, temporada2_id);
#endif
}

void menu_temporadas()
{
    MenuItem items[] = {
        TEMP_ITEM(1, "Crear Temporada", crear_temporada),
        TEMP_ITEM(2, "Listar Temporadas", listar_temporadas),
        TEMP_ITEM(3, "Modificar Temporada", modificar_temporada),
        TEMP_ITEM(4, "Eliminar Temporada", eliminar_temporada),
        TEMP_ITEM(5, "Administrar Temporada", administrar_temporada),
        TEMP_ITEM(6, "Comparar Temporadas", seleccionar_comparar_temporadas),
        TEMP_BACK_ITEM
    };

    ejecutar_menu_estandar("TEMPORADAS", items, ARRAY_COUNT(items));
}
