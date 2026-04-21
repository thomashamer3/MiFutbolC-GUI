#include "torneo.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "equipo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "sqlite3.h"

#ifndef UNIT_TEST
#include "estadisticas_gui_capture.h"
#endif

#define ARRAY_COUNT(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))
#define TOR_ITEM(numero, texto, accion) {(numero), (texto), (accion), MENU_CATEGORY_COMPETENCIA}
#define TOR_BACK_ITEM {0, "Volver", NULL, MENU_CATEGORY_ADMIN}

#define TOR_TEXT_MAX 256

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt);

#ifndef UNIT_TEST
typedef enum
{
    TOR_INPUT_ALNUM = 0,
    TOR_INPUT_NUMERIC
} TorInputMode;

typedef struct
{
    int id;
    char nombre[64];
} TorEquipoRow;

typedef struct
{
    int id;
    char resumen[TOR_TEXT_MAX];
} TorTorneoRow;

static int tor_input_char_valido(unsigned int codepoint, TorInputMode mode)
{
    unsigned char ch = (unsigned char)codepoint;

    if (codepoint < 32 || codepoint > 126)
    {
        return 0;
    }

    if (mode == TOR_INPUT_NUMERIC)
    {
        return isdigit(ch);
    }

    if (isalnum(ch) || isspace(ch))
    {
        return 1;
    }

    return (ch == '_' || ch == '-' || ch == '+' || ch == '.' || ch == ',');
}

static void tor_draw_action_button(Rectangle rect, const char *label, int primary)
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

static int tor_modal_confirmar_gui(const char *titulo,
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

        tor_draw_action_button(btn_ok, accion ? accion : "Confirmar", 1);
        tor_draw_action_button(btn_cancel, "Cancelar", 0);
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

static void tor_mostrar_info_gui(const char *titulo, const char *mensaje)
{
    (void)tor_modal_confirmar_gui(titulo ? titulo : "INFORMACION",
                                  mensaje ? mensaje : "Operacion finalizada.",
                                  "Entendido");
}

static int tor_parse_int(const char *text, int *out)
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

static int tor_modal_input_texto_gui(const char *titulo,
                                     const char *etiqueta,
                                     const char *hint,
                                     char *out,
                                     size_t out_size,
                                     int permitir_vacio,
                                     TorInputMode mode)
{
    char texto[TOR_TEXT_MAX] = {0};
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
            if (tor_input_char_valido((unsigned int)key, mode) &&
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

        tor_draw_action_button(btn_ok, "Guardar", 1);
        tor_draw_action_button(btn_cancel, "Cancelar", 0);
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

static int tor_modal_input_entero_gui(const char *titulo,
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
        if (!tor_modal_input_texto_gui(titulo,
                                       etiqueta,
                                       hint,
                                       texto,
                                       sizeof(texto),
                                       0,
                                       TOR_INPUT_NUMERIC))
        {
            return 0;
        }

        if (tor_parse_int(texto, &valor) && valor >= min_value)
        {
            *valor_out = valor;
            return 1;
        }

        tor_mostrar_info_gui("VALOR INVALIDO",
                             "Ingrese un numero valido dentro del rango permitido.");
    }
}

static int tor_modal_selector_opcion_gui(const char *titulo,
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

static int tor_cargar_equipos_gui_rows(TorEquipoRow **rows_out, int *count_out)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id, nombre FROM equipo ORDER BY id;";
    TorEquipoRow *rows = NULL;
    int count = 0;
    int cap = 0;

    if (!rows_out || !count_out)
    {
        return 0;
    }

    *rows_out = NULL;
    *count_out = 0;

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    cap = 16;
    rows = (TorEquipoRow *)calloc((size_t)cap, sizeof(TorEquipoRow));
    if (!rows)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (count >= cap)
        {
            int new_cap = cap * 2;
            TorEquipoRow *tmp = (TorEquipoRow *)realloc(rows, (size_t)new_cap * sizeof(TorEquipoRow));
            if (!tmp)
            {
                free(rows);
                sqlite3_finalize(stmt);
                return 0;
            }
            rows = tmp;
            cap = new_cap;
        }

        rows[count].id = sqlite3_column_int(stmt, 0);
        strncpy_s(rows[count].nombre,
                  sizeof(rows[count].nombre),
                  sqlite3_column_text(stmt, 1) ? (const char *)sqlite3_column_text(stmt, 1) : "(sin nombre)",
                  _TRUNCATE);
        count++;
    }

    sqlite3_finalize(stmt);

    if (count == 0)
    {
        free(rows);
        rows = NULL;
    }

    *rows_out = rows;
    *count_out = count;
    return 1;
}

static int tor_seleccionar_equipo_gui(const char *titulo, int *id_out)
{
    TorEquipoRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    int selected = 0;
    const int row_h = 34;

    if (!id_out)
    {
        return 0;
    }
    *id_out = 0;

    if (!tor_cargar_equipos_gui_rows(&rows, &count))
    {
        return 0;
    }

    if (count == 0)
    {
        free(rows);
        tor_mostrar_info_gui("SIN EQUIPOS",
                             "No hay equipos registrados. Cree uno en el modulo Equipos.");
        return 0;
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = 60;
        int panel_y = 110;
        int panel_w = sw - 120;
        int panel_h = sh - 180;
        int content_y = panel_y + 32;
        int content_h = panel_h - 32;
        int visible_rows;
        int max_scroll;

        visible_rows = content_h / row_h;
        if (visible_rows < 1)
        {
            visible_rows = 1;
        }
        max_scroll = (count > visible_rows) ? (count - visible_rows) : 0;

        if (GetMouseWheelMove() > 0.01f) scroll -= 3;
        if (GetMouseWheelMove() < -0.01f) scroll += 3;
        if (IsKeyPressed(KEY_UP)) selected--;
        if (IsKeyPressed(KEY_DOWN)) selected++;

        if (selected < 0) selected = 0;
        if (selected >= count) selected = count - 1;

        if (selected < scroll) scroll = selected;
        if (selected >= scroll + visible_rows) scroll = selected - visible_rows + 1;
        if (scroll < 0) scroll = 0;
        if (scroll > max_scroll) scroll = max_scroll;

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header(titulo ? titulo : "SELECCIONAR EQUIPO", sw);

        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                            "ID", 12.0f,
                            "EQUIPO", 80.0f);

        BeginScissorMode(panel_x, content_y, panel_w, content_h);
        for (int i = scroll; i < count; i++)
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
                *id_out = rows[i].id;
                EndScissorMode();
                EndDrawing();
                free(rows);
                return 1;
            }

            gui_text(TextFormat("%3d", rows[i].id), (float)(panel_x + 12), (float)(y + 7), 18.0f, (Color){220, 238, 225, 255});
            gui_text_truncated(rows[i].nombre,
                               (float)(panel_x + 80),
                               (float)(y + 7),
                               18.0f,
                               (float)panel_w - 96.0f,
                               is_selected ? (Color){183, 247, 206, 255} : (Color){233, 247, 236, 255});
        }
        EndScissorMode();

        gui_draw_footer_hint("Click o ENTER para confirmar | Flechas: mover | ESC: cancelar", (float)panel_x, sh);
        EndDrawing();

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        {
            *id_out = rows[selected].id;
            free(rows);
            return 1;
        }
        if (IsKeyPressed(KEY_ESCAPE))
        {
            break;
        }
    }

    free(rows);
    return 0;
}

static void tor_asociar_equipo_guardado_gui(int torneo_id)
{
    sqlite3_stmt *stmt = NULL;
    int equipo_id = 0;

    if (!tor_seleccionar_equipo_gui("ASOCIAR EQUIPO A TORNEO", &equipo_id))
    {
        return;
    }

    const char *sql_check = "SELECT COUNT(*) FROM equipo_torneo WHERE torneo_id = ? AND equipo_id = ?;";
    if (preparar_stmt(sql_check, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, equipo_id);
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0)
        {
            sqlite3_finalize(stmt);
            tor_mostrar_info_gui("EQUIPO YA ASOCIADO",
                                 "Este equipo ya esta asociado al torneo.");
            return;
        }
        sqlite3_finalize(stmt);
    }

    const char *sql_insert = "INSERT INTO equipo_torneo (torneo_id, equipo_id) VALUES (?, ?);";
    if (preparar_stmt(sql_insert, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, equipo_id);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            sqlite3_finalize(stmt);
            tor_mostrar_info_gui("EQUIPO ASOCIADO",
                                 "Equipo asociado al torneo exitosamente.");
            return;
        }

        {
            const char *err = sqlite3_errmsg(db);
            char msg[256];
            snprintf(msg, sizeof(msg), "Error al asociar equipo al torneo: %s", err ? err : "desconocido");
            sqlite3_finalize(stmt);
            tor_mostrar_info_gui("ERROR", msg);
            return;
        }
    }
}

static void tor_agregar_equipos_nombres_gui(int torneo_id)
{
    sqlite3_stmt *stmt = NULL;
    int count = 0;

    while (1)
    {
        char nombre[50] = {0};

        if (!tor_modal_input_texto_gui("AGREGAR EQUIPO",
                                       "Nombre del equipo",
                                       "Letras, numeros y espacios",
                                       nombre,
                                       sizeof(nombre),
                                       0,
                                       TOR_INPUT_ALNUM))
        {
            break;
        }

        const char *sql_check = "SELECT COUNT(*) FROM equipo_torneo_nombre WHERE torneo_id = ? AND nombre = ?;";
        if (preparar_stmt(sql_check, &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0)
            {
                sqlite3_finalize(stmt);
                tor_mostrar_info_gui("EQUIPO DUPLICADO",
                                     "Ya existe un equipo con ese nombre en el torneo.");
                continue;
            }
            sqlite3_finalize(stmt);
        }

        const char *sql_insert = "INSERT INTO equipo_torneo_nombre (torneo_id, nombre) VALUES (?, ?);";
        if (preparar_stmt(sql_insert, &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                count++;
            }
            sqlite3_finalize(stmt);
        }

        if (!tor_modal_confirmar_gui("AGREGAR MAS EQUIPOS",
                                     "Desea agregar otro equipo por nombre?",
                                     "Si"))
        {
            break;
        }
    }

    {
        char msg[128];
        snprintf(msg, sizeof(msg), "%d equipo(s) agregado(s) al torneo.", count);
        tor_mostrar_info_gui("CARGA FINALIZADA", msg);
    }
}

static int tor_cargar_torneos_gui_rows(TorTorneoRow **rows_out, int *count_out)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id, nombre, cantidad_equipos, tipo_torneo, formato_torneo FROM torneo ORDER BY id DESC;";
    TorTorneoRow *rows = NULL;
    int count = 0;
    int cap = 0;

    if (!rows_out || !count_out)
    {
        return 0;
    }

    *rows_out = NULL;
    *count_out = 0;

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    cap = 16;
    rows = (TorTorneoRow *)calloc((size_t)cap, sizeof(TorTorneoRow));
    if (!rows)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id;
        const char *nombre;
        int cantidad;
        int tipo;
        int formato;

        if (count >= cap)
        {
            int new_cap = cap * 2;
            TorTorneoRow *tmp = (TorTorneoRow *)realloc(rows, (size_t)new_cap * sizeof(TorTorneoRow));
            if (!tmp)
            {
                free(rows);
                sqlite3_finalize(stmt);
                return 0;
            }
            rows = tmp;
            cap = new_cap;
        }

        id = sqlite3_column_int(stmt, 0);
        nombre = sqlite3_column_text(stmt, 1) ? (const char *)sqlite3_column_text(stmt, 1) : "(sin nombre)";
        cantidad = sqlite3_column_int(stmt, 2);
        tipo = sqlite3_column_int(stmt, 3);
        formato = sqlite3_column_int(stmt, 4);

        rows[count].id = id;
        snprintf(rows[count].resumen,
                 sizeof(rows[count].resumen),
                 "%s | Equipos:%d | %s | %s",
                 nombre,
                 cantidad,
                 get_nombre_tipo_torneo((TipoTorneos)tipo),
                 get_nombre_formato_torneo((FormatoTorneos)formato));
        count++;
    }

    sqlite3_finalize(stmt);

    if (count == 0)
    {
        free(rows);
        rows = NULL;
    }

    *rows_out = rows;
    *count_out = count;
    return 1;
}

static int tor_seleccionar_torneo_gui(const char *titulo,
                                      const char *ayuda,
                                      int *id_out)
{
    TorTorneoRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    int selected = 0;
    const int row_h = 34;

    if (!id_out)
    {
        return 0;
    }
    *id_out = 0;

    if (!tor_cargar_torneos_gui_rows(&rows, &count))
    {
        return 0;
    }

    if (count == 0)
    {
        free(rows);
        tor_mostrar_info_gui("SIN TORNEOS", "No hay torneos registrados.");
        return 0;
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = 60;
        int panel_y = 110;
        int panel_w = sw - 120;
        int panel_h = sh - 180;
        int content_y = panel_y + 32;
        int content_h = panel_h - 32;
        int visible_rows;
        int max_scroll;

        visible_rows = content_h / row_h;
        if (visible_rows < 1)
        {
            visible_rows = 1;
        }
        max_scroll = (count > visible_rows) ? (count - visible_rows) : 0;

        if (GetMouseWheelMove() > 0.01f) scroll -= 3;
        if (GetMouseWheelMove() < -0.01f) scroll += 3;
        if (IsKeyPressed(KEY_UP)) selected--;
        if (IsKeyPressed(KEY_DOWN)) selected++;

        if (selected < 0) selected = 0;
        if (selected >= count) selected = count - 1;

        if (selected < scroll) scroll = selected;
        if (selected >= scroll + visible_rows) scroll = selected - visible_rows + 1;
        if (scroll < 0) scroll = 0;
        if (scroll > max_scroll) scroll = max_scroll;

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header(titulo ? titulo : "SELECCIONAR TORNEO", sw);

        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                            "ID", 12.0f,
                            "TORNEO", 80.0f);

        BeginScissorMode(panel_x, content_y, panel_w, content_h);
        for (int i = scroll; i < count; i++)
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
                *id_out = rows[i].id;
                EndScissorMode();
                EndDrawing();
                free(rows);
                return 1;
            }

            gui_text(TextFormat("%3d", rows[i].id), (float)(panel_x + 12), (float)(y + 7), 18.0f, (Color){220, 238, 225, 255});
            gui_text_truncated(rows[i].resumen,
                               (float)(panel_x + 80),
                               (float)(y + 7),
                               18.0f,
                               (float)panel_w - 96.0f,
                               is_selected ? (Color){183, 247, 206, 255} : (Color){233, 247, 236, 255});
        }
        EndScissorMode();

        gui_draw_footer_hint(ayuda ? ayuda : "Click o ENTER para confirmar | Flechas: mover | ESC: cancelar", (float)panel_x, sh);
        EndDrawing();

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        {
            *id_out = rows[selected].id;
            free(rows);
            return 1;
        }
        if (IsKeyPressed(KEY_ESCAPE))
        {
            break;
        }
    }

    free(rows);
    return 0;
}

static int tor_obtener_torneo_por_id(int torneo_id, Torneo *torneo)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT nombre, tiene_equipo_fijo, equipo_fijo_id, cantidad_equipos, tipo_torneo, formato_torneo FROM torneo WHERE id = ?;";

    if (!torneo)
    {
        return 0;
    }

    memset(torneo, 0, sizeof(*torneo));
    torneo->id = torneo_id;

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, torneo_id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        strncpy_s(torneo->nombre, sizeof(torneo->nombre),
                  sqlite3_column_text(stmt, 0) ? (const char *)sqlite3_column_text(stmt, 0) : "",
                  _TRUNCATE);
        torneo->tiene_equipo_fijo = sqlite3_column_int(stmt, 1);
        torneo->equipo_fijo_id = sqlite3_column_int(stmt, 2);
        torneo->cantidad_equipos = sqlite3_column_int(stmt, 3);
        torneo->tipo_torneo = sqlite3_column_int(stmt, 4);
        torneo->formato_torneo = sqlite3_column_int(stmt, 5);
        sqlite3_finalize(stmt);
        return 1;
    }

    sqlite3_finalize(stmt);
    return 0;
}
#endif


static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, 0) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

// Forward declaration to fix implicit function declaration
void generar_fixture(int torneo_id);
void gestionar_tablas_goleadores_asistidores();
void listar_tablas_goleadores_asistidores(int torneo_id);
void agregar_registro_goleador_asistidor(int torneo_id);
void eliminar_registro_goleador_asistidor(int torneo_id);
void modificar_registro_goleador_asistidor(int torneo_id);

// Prototipos estaticos para funciones usadas antes de su definicion
static void listar_equipos_asociados(int torneo_id);
static void actualizar_nombre_torneo(int torneo_id);
static void actualizar_equipo_fijo(int torneo_id);
static void actualizar_tipo_formato_torneo(int torneo_id, int cantidad);

// Stubs para funciones no implementadas pero usadas
static void actualizar_cantidad_equipos(int torneo_id)
{
    int nueva_cantidad = 0;
    sqlite3_stmt *stmt = NULL;
    const char *sql_update = "UPDATE torneo SET cantidad_equipos = ? WHERE id = ?;";

#ifdef UNIT_TEST
    nueva_cantidad = input_int("Ingrese la nueva cantidad de equipos: ");
    if (nueva_cantidad <= 0)
    {
        printf("Cantidad invalida.\n");
        return;
    }
#else
    if (!tor_modal_input_entero_gui("MODIFICAR CANTIDAD",
                                    "Nueva cantidad de equipos",
                                    "Ingrese un entero mayor a 0",
                                    1,
                                    &nueva_cantidad))
    {
        return;
    }
#endif

    if (preparar_stmt(sql_update, &stmt))
    {
        sqlite3_bind_int(stmt, 1, nueva_cantidad);
        sqlite3_bind_int(stmt, 2, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
#ifdef UNIT_TEST
            printf("Cantidad de equipos actualizada exitosamente.\n");
#else
            tor_mostrar_info_gui("ACTUALIZACION EXITOSA",
                                 "Cantidad de equipos actualizada exitosamente.");
#endif
        }
        else
        {
#ifdef UNIT_TEST
            printf("Error al actualizar la cantidad de equipos: %s\n", sqlite3_errmsg(db));
#else
            char msg[256];
            snprintf(msg, sizeof(msg), "Error al actualizar la cantidad de equipos: %s", sqlite3_errmsg(db));
            tor_mostrar_info_gui("ERROR", msg);
#endif
        }
        sqlite3_finalize(stmt);
    }
}

static void aplicar_actualizacion_formato(int torneo_id, int tipo, int formato)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql_update = "UPDATE torneo SET tipo_torneo = ?, formato_torneo = ? WHERE id = ?;";

    if (preparar_stmt(sql_update, &stmt))
    {
        sqlite3_bind_int(stmt, 1, tipo);
        sqlite3_bind_int(stmt, 2, formato);
        sqlite3_bind_int(stmt, 3, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
#ifdef UNIT_TEST
            printf("Tipo y formato de torneo actualizados exitosamente.\n");
#else
            tor_mostrar_info_gui("ACTUALIZACION EXITOSA",
                                 "Tipo y formato de torneo actualizados exitosamente.");
#endif
        }
        else
        {
#ifdef UNIT_TEST
            printf("Error al actualizar tipo/formato de torneo: %s\n", sqlite3_errmsg(db));
#else
            char msg[256];
            snprintf(msg, sizeof(msg), "Error al actualizar tipo/formato de torneo: %s", sqlite3_errmsg(db));
            tor_mostrar_info_gui("ERROR", msg);
#endif
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * Traduce valores enumerados de tipos de torneo a nombres legibles para la interfaz de usuario,
 * facilitando la comprension de las opciones disponibles.
 */
const char* get_nombre_tipo_torneo(TipoTorneos tipo)
{
    switch (tipo)
    {
    case IDA_Y_VUELTA:
        return "Ida y Vuelta";
    case SOLO_IDA:
        return "Solo Ida";
    case ELIMINACION_DIRECTA:
        return "Eliminacion Directa";
    case GRUPOS_Y_ELIMINACION:
        return "Grupos y Eliminacion";
    default:
        return "Desconocido";
    }
}

/**
 * @brief Convierte un formato de torneo enumerado a su nombre textual
 *
 * Esta funcion toma un valor del enum FormatoTorneos y devuelve la cadena
 * correspondiente en espanol para mostrar al usuario.
 *
 * @param formato El valor enumerado del formato de torneo
 * @return Cadena constante con el nombre del formato de torneo, o "Desconocido" si no es valido
 */
const char* get_nombre_formato_torneo(FormatoTorneos formato)
{
    switch (formato)
    {
    case ROUND_ROBIN:
        return "Round-robin (sistema liga)";
    case MINI_GRUPO_CON_FINAL:
        return "Mini grupo con final";
    case LIGA_SIMPLE:
        return "Liga simple";
    case LIGA_DOBLE:
        return "Liga doble";
    case GRUPOS_CON_FINAL:
        return "Grupos + final";
    case COPA_SIMPLE:
        return "Copa simple";
    case GRUPOS_ELIMINACION:
        return "Grupos + eliminacion";
    case COPA_REPECHAJE:
        return "Copa + repechaje";
    case LIGA_GRANDE:
        return "Liga grande";
    case MULTIPLES_GRUPOS:
        return "Multiples grupos";
    case ELIMINACION_FASES:
        return "Eliminacion directa por fases";
    default:
        return "Desconocido";
    }
}

/**
 * @brief Funcion generica para listar torneos
 */
static int listar_torneos_generico(const char *no_records_msg)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre FROM torneo ORDER BY id;";

    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error al obtener la lista de torneos: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    printf("=== TORNEOS DISPONIBLES ===\n\n");

    int found = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = 1;
        int id = sqlite3_column_int(stmt, 0);
        const char *nombre = (const char*)sqlite3_column_text(stmt, 1);
        printf("%d. %s\n", id, nombre);
    }

    sqlite3_finalize(stmt);

    if (!found)
    {
        mostrar_no_hay_registros(no_records_msg);
        return 0;
    }

    return 1;
}

/**
 * @brief Funcion generica para obtener formato segun cantidad de equipos y opcion
 */
static void obtener_formato_por_cantidad(int opcion, int cantidad, TipoTorneos *tipo, FormatoTorneos *formato)
{
    *tipo = SOLO_IDA;
    *formato = LIGA_SIMPLE;

    if (cantidad >= 4 && cantidad <= 6)
    {
        if (opcion == 1)
        {
            *formato = ROUND_ROBIN;
            *tipo = IDA_Y_VUELTA;
        }
        else if (opcion == 2)
        {
            *formato = MINI_GRUPO_CON_FINAL;
            *tipo = GRUPOS_Y_ELIMINACION;
        }
    }
    else if (cantidad >= 7 && cantidad <= 12)
    {
        switch (opcion)
        {
        case 2:
            *formato = LIGA_DOBLE;
            *tipo = IDA_Y_VUELTA;
            break;
        case 3:
            *formato = GRUPOS_CON_FINAL;
            *tipo = GRUPOS_Y_ELIMINACION;
            break;
        case 4:
            *formato = COPA_SIMPLE;
            *tipo = ELIMINACION_DIRECTA;
            break;
        default:
            break;
        }
    }
    else if (cantidad >= 13 && cantidad <= 20)
    {
        *tipo = GRUPOS_Y_ELIMINACION;
        *formato = GRUPOS_ELIMINACION;
        switch (opcion)
        {
        case 2:
            *formato = COPA_REPECHAJE;
            *tipo = ELIMINACION_DIRECTA;
            break;
        case 3:
            *formato = LIGA_GRANDE;
            *tipo = IDA_Y_VUELTA;
            break;
        default:
            break;
        }
    }
    else if (cantidad >= 21)
    {
        *tipo = GRUPOS_Y_ELIMINACION;
        *formato = MULTIPLES_GRUPOS;
        if (opcion == 2)
        {
            *formato = ELIMINACION_FASES;
            *tipo = ELIMINACION_DIRECTA;
        }
    }
}

/**
 * Muestra informacion completa de torneo para confirmacion del usuario.
 * Necesario porque la estructura interna no es legible para humanos.
 */
void mostrar_torneo(Torneo *torneo)
{
    printf("=== INFORMACION DEL TORNEO ===\n");
    printf("Nombre: %s\n", torneo->nombre);
    printf("Tiene equipo fijo: %s\n", torneo->tiene_equipo_fijo ? "Si" : "No");
    if (torneo->tiene_equipo_fijo)
    {
        printf("Equipo fijo ID: %d\n", torneo->equipo_fijo_id);
    }
    printf("Cantidad de equipos: %d\n", torneo->cantidad_equipos);
    printf("Tipo de torneo: %s\n", get_nombre_tipo_torneo(torneo->tipo_torneo));
    printf("Formato de torneo: %s\n", get_nombre_formato_torneo(torneo->formato_torneo));
    printf("\n");
}

/**
 * @brief Agrega un equipo al torneo solo por nombre (sin crear equipo completo)
 */
static void agregar_equipo_nombre_torneo(int torneo_id)
{
#ifdef UNIT_TEST
    char nombre[50] = {0};
    input_string("Nombre del equipo: ", nombre, sizeof(nombre));
    if (nombre[0] == '\0')
    {
        printf("El nombre no puede estar vacio.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    const char *sql_check = "SELECT COUNT(*) FROM equipo_torneo_nombre WHERE torneo_id = ? AND nombre = ?;";
    if (preparar_stmt(sql_check, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0)
        {
            printf("Ya existe un equipo con ese nombre en el torneo.\n");
            sqlite3_finalize(stmt);
            pause_console();
            return;
        }
        sqlite3_finalize(stmt);
    }

    const char *sql_insert = "INSERT INTO equipo_torneo_nombre (torneo_id, nombre) VALUES (?, ?);";
    if (preparar_stmt(sql_insert, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE)
            printf("Equipo '%s' agregado al torneo.\n", nombre);
        else
            printf("Error al agregar equipo: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
    }
    pause_console();
#else
    char nombre[50] = {0};
    sqlite3_stmt *stmt = NULL;

    if (!tor_modal_input_texto_gui("AGREGAR EQUIPO",
                                   "Nombre del equipo",
                                   "Letras, numeros y espacios",
                                   nombre,
                                   sizeof(nombre),
                                   0,
                                   TOR_INPUT_ALNUM))
    {
        return;
    }

    const char *sql_check = "SELECT COUNT(*) FROM equipo_torneo_nombre WHERE torneo_id = ? AND nombre = ?;";
    if (preparar_stmt(sql_check, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0)
        {
            sqlite3_finalize(stmt);
            tor_mostrar_info_gui("EQUIPO DUPLICADO",
                                 "Ya existe un equipo con ese nombre en el torneo.");
            return;
        }
        sqlite3_finalize(stmt);
    }

    const char *sql_insert = "INSERT INTO equipo_torneo_nombre (torneo_id, nombre) VALUES (?, ?);";
    if (preparar_stmt(sql_insert, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            tor_mostrar_info_gui("EQUIPO AGREGADO",
                                 "Equipo agregado al torneo exitosamente.");
        }
        else
        {
            char msg[256];
            snprintf(msg, sizeof(msg), "Error al agregar equipo: %s", sqlite3_errmsg(db));
            tor_mostrar_info_gui("ERROR", msg);
        }
        sqlite3_finalize(stmt);
    }
#endif
}

/**
 * @brief Agrega varios equipos por nombre de una vez
 */
static void agregar_equipos_nombres_torneo(int torneo_id)
{
    clear_screen();
    print_header("AGREGAR EQUIPOS POR NOMBRE");
    printf("Escriba los nombres de los equipos (uno por linea, linea vacia para terminar):\n\n");

    int count = 0;
    for (;;)
    {
        char nombre[50] = {0};
        printf("Equipo %d (Enter para terminar): ", count + 1);
        if (fgets(nombre, sizeof(nombre), stdin))
        {
            // trim newline
            size_t len = strlen_s(nombre, sizeof(nombre));
            while (len > 0 && (nombre[len-1] == '\n' || nombre[len-1] == '\r'))
                nombre[--len] = '\0';
        }
        if (nombre[0] == '\0') break;

        sqlite3_stmt *stmt;
        const char *sql = "INSERT INTO equipo_torneo_nombre (torneo_id, nombre) VALUES (?, ?);";
        if (preparar_stmt(sql, &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_DONE)
                count++;
            else
                printf("Error al agregar '%s': %s\n", nombre, sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
        }
    }
    printf("\n%d equipo(s) agregado(s) al torneo.\n", count);
    pause_console();
}

/**
 * @brief Asocia equipos a un torneo
 */
void asociar_equipos_torneo(int torneo_id)
{
    clear_screen();
    print_header("ASOCIAR EQUIPOS A TORNEO");
    sqlite3_stmt *stmt;
    int equipo_id = select_team_id("\nIngrese el ID del equipo a asociar (0 para cancelar): ",
                                   "equipos registrados para asociar", 1);
    if (equipo_id == 0)
    {
        return;
    }

    // Verificar si ya esta asociado
    const char *sql_check = "SELECT COUNT(*) FROM equipo_torneo WHERE torneo_id = ? AND equipo_id = ?;";
    if (preparar_stmt(sql_check, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, equipo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int count = sqlite3_column_int(stmt, 0);
            if (count > 0)
            {
                printf("Este equipo ya esta asociado al torneo.\n");
                sqlite3_finalize(stmt);
                pause_console();
                return;
            }
        }
        sqlite3_finalize(stmt);
    }

    // Asociar equipo a torneo
    const char *sql_insert = "INSERT INTO equipo_torneo (torneo_id, equipo_id) VALUES (?, ?);";
    if (preparar_stmt(sql_insert, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, equipo_id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Equipo asociado al torneo exitosamente.\n");
        }
        else
        {
            printf("Error al asociar equipo al torneo: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

/**
 * @brief Crea un equipo fijo para un torneo
 *
 * Esta funcion crea un nuevo equipo y lo asocia como equipo fijo a un torneo.
 * Si torneo_id es -1, solo crea el equipo sin asociarlo.
 *
 * @param torneo_id ID del torneo al que se asociara el equipo fijo, o -1 para solo crear el equipo
 */
void crear_equipo_fijo_torneo(int torneo_id)
{
    crear_equipo();

    sqlite3_stmt *stmt;
    const char *sql = "SELECT last_insert_rowid();";

    int equipo_id = -1;
    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            equipo_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (equipo_id == -1)
    {
        printf("No se pudo obtener el ID del equipo creado.\n");
        pause_console();
        return;
    }

    if (torneo_id != -1)
    {
        const char *sql_insert = "INSERT INTO equipo_torneo (torneo_id, equipo_id) VALUES (?, ?);";
        if (preparar_stmt(sql_insert, &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, equipo_id);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                printf("Equipo fijo creado y asociado al torneo exitosamente.\n");
            }
            else
            {
                printf("Error al asociar equipo al torneo: %s\n", sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);
        }
    }
    else
    {
        printf("Equipo fijo creado exitosamente. ID: %d\n", equipo_id);
    }

    pause_console();
}

/**
 * Solicita al usuario los datos basicos del torneo (nombre, equipo fijo).
 * Maneja la creacion de equipo fijo si es necesario.
 * Retorna 0 si se debe cancelar la creacion, 1 si continua.
 */
static int input_torneo_data(Torneo *torneo)
{
#ifdef UNIT_TEST
    input_string("Ingrese el nombre del torneo: ", torneo->nombre, sizeof(torneo->nombre));

    torneo->tiene_equipo_fijo = confirmar("El torneo tiene equipo fijo?");

    if (torneo->tiene_equipo_fijo)
    {
        listar_equipos();
        int equipo_id = input_int("\nIngrese el ID del equipo fijo (0 para crear nuevo equipo): ");

        if (equipo_id == 0)
        {
            crear_equipo_fijo_torneo(-1);
            return 0;
        }
        else if (existe_id("equipo", equipo_id))
        {
            torneo->equipo_fijo_id = equipo_id;
        }
        else
        {
            printf("ID de equipo invalido.\n");
            pause_console();
            return 0;
        }
    }

    torneo->cantidad_equipos = input_int("Ingrese la cantidad de equipos en el torneo: ");
    return 1;
#else
    const char *opciones_equipo_fijo[] = {"No", "Si"};
    int seleccion_equipo_fijo = 0;

    if (!torneo)
    {
        return 0;
    }

    if (!tor_modal_input_texto_gui("CREAR TORNEO",
                                   "Nombre del torneo",
                                   "Letras, numeros y espacios",
                                   torneo->nombre,
                                   sizeof(torneo->nombre),
                                   0,
                                   TOR_INPUT_ALNUM))
    {
        return 0;
    }

    if (!tor_modal_selector_opcion_gui("EQUIPO FIJO",
                                       "TIENE EQUIPO FIJO",
                                       opciones_equipo_fijo,
                                       ARRAY_COUNT(opciones_equipo_fijo),
                                       0,
                                       &seleccion_equipo_fijo))
    {
        return 0;
    }

    torneo->tiene_equipo_fijo = (seleccion_equipo_fijo == 1) ? 1 : 0;
    torneo->equipo_fijo_id = -1;

    if (torneo->tiene_equipo_fijo)
    {
        int equipo_id = 0;
        if (!tor_seleccionar_equipo_gui("SELECCIONAR EQUIPO FIJO", &equipo_id))
        {
            return 0;
        }
        torneo->equipo_fijo_id = equipo_id;
    }

    if (!tor_modal_input_entero_gui("CREAR TORNEO",
                                    "Cantidad de equipos",
                                    "Ingrese un entero (4 o mayor recomendado)",
                                    1,
                                    &torneo->cantidad_equipos))
    {
        return 0;
    }

    return 1;
#endif
}

/**
 * Determina el formato y tipo de torneo basado en la cantidad de equipos.
 * Utiliza logica de rangos para simplificar la seleccion automatica.
 */
static void determine_formato_torneo(Torneo *torneo)
{
#ifdef UNIT_TEST
    int cantidad = torneo->cantidad_equipos;
    int opcion = 0;

    if (cantidad >= 4 && cantidad <= 6)
    {
        printf("\nPara 4-6 equipos, seleccione el formato:\n");
        printf("1. Round-robin (sistema liga)\n");
        printf("2. Mini grupo con final\n");
        opcion = input_int(">");
    }
    else if (cantidad >= 7 && cantidad <= 12)
    {
        printf("\nPara 7-12 equipos, seleccione el formato:\n");
        printf("1. Liga simple\n");
        printf("2. Liga doble\n");
        printf("3. Grupos + final\n");
        printf("4. Copa simple\n");
        opcion = input_int(">");
    }
    else if (cantidad >= 13 && cantidad <= 20)
    {
        printf("\nPara 13-20 equipos, seleccione el formato:\n");
        printf("1. Grupos (4-5 grupos) + eliminacion\n");
        printf("2. Copa + repechaje\n");
        printf("3. Liga grande\n");
        opcion = input_int(">");
    }
    else if (cantidad >= 21)
    {
        printf("\nPara 21 o mas equipos, seleccione el formato:\n");
        printf("1. Multiples grupos\n");
        printf("2. Eliminacion directa por fases\n");
        opcion = input_int(">");
    }
    else
    {
        printf("Cantidad de equipos no valida. Se seleccionara formato por defecto.\n");
        torneo->formato_torneo = ROUND_ROBIN;
        torneo->tipo_torneo = IDA_Y_VUELTA;
        return;
    }

    obtener_formato_por_cantidad(opcion, cantidad, &torneo->tipo_torneo, &torneo->formato_torneo);
#else
    int cantidad = torneo->cantidad_equipos;
    int opcion = 1;

    if (!torneo)
    {
        return;
    }

    if (cantidad >= 4 && cantidad <= 6)
    {
        const char *opciones[] =
        {
            "Round-robin (sistema liga)",
            "Mini grupo con final"
        };
        int seleccion = 0;
        if (!tor_modal_selector_opcion_gui("FORMATO DE TORNEO",
                                           "FORMATO",
                                           opciones,
                                           ARRAY_COUNT(opciones),
                                           0,
                                           &seleccion))
        {
            return;
        }
        opcion = seleccion + 1;
    }
    else if (cantidad >= 7 && cantidad <= 12)
    {
        const char *opciones[] =
        {
            "Liga simple",
            "Liga doble",
            "Grupos + final",
            "Copa simple"
        };
        int seleccion = 0;
        if (!tor_modal_selector_opcion_gui("FORMATO DE TORNEO",
                                           "FORMATO",
                                           opciones,
                                           ARRAY_COUNT(opciones),
                                           0,
                                           &seleccion))
        {
            return;
        }
        opcion = seleccion + 1;
    }
    else if (cantidad >= 13 && cantidad <= 20)
    {
        const char *opciones[] =
        {
            "Grupos (4-5 grupos) + eliminacion",
            "Copa + repechaje",
            "Liga grande"
        };
        int seleccion = 0;
        if (!tor_modal_selector_opcion_gui("FORMATO DE TORNEO",
                                           "FORMATO",
                                           opciones,
                                           ARRAY_COUNT(opciones),
                                           0,
                                           &seleccion))
        {
            return;
        }
        opcion = seleccion + 1;
    }
    else if (cantidad >= 21)
    {
        const char *opciones[] =
        {
            "Multiples grupos",
            "Eliminacion directa por fases"
        };
        int seleccion = 0;
        if (!tor_modal_selector_opcion_gui("FORMATO DE TORNEO",
                                           "FORMATO",
                                           opciones,
                                           ARRAY_COUNT(opciones),
                                           0,
                                           &seleccion))
        {
            return;
        }
        opcion = seleccion + 1;
    }
    else
    {
        torneo->formato_torneo = ROUND_ROBIN;
        torneo->tipo_torneo = IDA_Y_VUELTA;
        tor_mostrar_info_gui("CANTIDAD NO VALIDA",
                             "Cantidad de equipos fuera de rango. Se aplicara formato por defecto.");
        return;
    }

    obtener_formato_por_cantidad(opcion, cantidad, &torneo->tipo_torneo, &torneo->formato_torneo);
#endif
}

/**
 * Guarda el torneo en la base de datos y maneja asociaciones iniciales.
 * Retorna el ID del torneo creado o -1 si hay error.
 */
static int save_torneo_to_db(Torneo const *torneo)
{
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO torneo (nombre, tiene_equipo_fijo, equipo_fijo_id, cantidad_equipos, tipo_torneo, formato_torneo) VALUES (?, ?, ?, ?, ?, ?);";

    if (!preparar_stmt(sql, &stmt))
    {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, torneo->nombre, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, torneo->tiene_equipo_fijo);
    sqlite3_bind_int(stmt, 3, torneo->equipo_fijo_id);
    sqlite3_bind_int(stmt, 4, torneo->cantidad_equipos);
    sqlite3_bind_int(stmt, 5, torneo->tipo_torneo);
    sqlite3_bind_int(stmt, 6, torneo->formato_torneo);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        printf("Error al guardar el torneo: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    int torneo_id = (int)sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);

    if (torneo->tiene_equipo_fijo && torneo->equipo_fijo_id != -1)
    {
        const char *sql_asociar = "INSERT INTO equipo_torneo (torneo_id, equipo_id) VALUES (?, ?);";
        if (preparar_stmt(sql_asociar, &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, torneo->equipo_fijo_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    return torneo_id;
}

void crear_torneo()
{
#ifdef UNIT_TEST
    clear_screen();
    print_header("CREAR TORNEO");

    Torneo torneo = {0};
    torneo.tiene_equipo_fijo = 0;
    torneo.equipo_fijo_id = -1;

    if (!input_torneo_data(&torneo))
        return;

    determine_formato_torneo(&torneo);

    clear_screen();
    mostrar_torneo(&torneo);

    int torneo_id = save_torneo_to_db(&torneo);
    if (torneo_id == -1)
        return;

    printf("Torneo guardado exitosamente con ID: %d\n", torneo_id);

    printf("\nAgregar equipos al torneo:\n");
    printf("1. Agregar equipos por nombre (solo nombres para llaves)\n");
    printf("2. Asociar equipos guardados\n");
    printf("0. Continuar sin agregar\n");
    int opc_eq = input_int(">");
    if (opc_eq == 1)
        agregar_equipos_nombres_torneo(torneo_id);
    else if (opc_eq == 2)
        asociar_equipos_torneo(torneo_id);

    pause_console();
#else
    Torneo torneo = {0};
    int torneo_id;
    int opcion_post = 0;
    char resumen[512];
    const char *opciones_post[] =
    {
        "Continuar sin agregar equipos",
        "Agregar equipos por nombre",
        "Asociar equipos guardados"
    };

    torneo.tiene_equipo_fijo = 0;
    torneo.equipo_fijo_id = -1;

    if (!input_torneo_data(&torneo))
    {
        return;
    }

    determine_formato_torneo(&torneo);

    snprintf(resumen,
             sizeof(resumen),
             "Nombre: %s\nTiene equipo fijo: %s\nCantidad de equipos: %d\nTipo: %s\nFormato: %s\n\nDesea guardar este torneo?",
             torneo.nombre,
             torneo.tiene_equipo_fijo ? "Si" : "No",
             torneo.cantidad_equipos,
             get_nombre_tipo_torneo(torneo.tipo_torneo),
             get_nombre_formato_torneo(torneo.formato_torneo));

    if (!tor_modal_confirmar_gui("CONFIRMAR TORNEO", resumen, "Guardar"))
    {
        return;
    }

    torneo_id = save_torneo_to_db(&torneo);
    if (torneo_id == -1)
    {
        tor_mostrar_info_gui("ERROR", "No se pudo guardar el torneo.");
        return;
    }

    {
        char ok_msg[128];
        snprintf(ok_msg, sizeof(ok_msg), "Torneo guardado exitosamente con ID: %d", torneo_id);
        tor_mostrar_info_gui("TORNEO CREADO", ok_msg);
    }

    if (!tor_modal_selector_opcion_gui("POST CREACION",
                                       "ACCION",
                                       opciones_post,
                                       ARRAY_COUNT(opciones_post),
                                       0,
                                       &opcion_post))
    {
        return;
    }

    if (opcion_post == 1)
    {
        tor_agregar_equipos_nombres_gui(torneo_id);
    }
    else if (opcion_post == 2)
    {
        tor_asociar_equipo_guardado_gui(torneo_id);
    }
#endif
}

void listar_torneos()
{
    clear_screen();
    print_header("LISTAR TORNEOS");

    if (!listar_torneos_generico("torneos registrados"))
    {
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre, tiene_equipo_fijo, equipo_fijo_id, cantidad_equipos, tipo_torneo, formato_torneo FROM torneo ORDER BY id;";

    if (preparar_stmt(sql, &stmt))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            Torneo torneo;
            torneo.id = sqlite3_column_int(stmt, 0);
            strncpy_s(torneo.nombre, sizeof(torneo.nombre), (const char*)sqlite3_column_text(stmt, 1), sizeof(torneo.nombre));
            torneo.tiene_equipo_fijo = sqlite3_column_int(stmt, 2);
            torneo.equipo_fijo_id = sqlite3_column_int(stmt, 3);
            torneo.cantidad_equipos = sqlite3_column_int(stmt, 4);
            torneo.tipo_torneo = sqlite3_column_int(stmt, 5);
            torneo.formato_torneo = sqlite3_column_int(stmt, 6);

            mostrar_torneo(&torneo);
            listar_equipos_asociados(torneo.id);

            printf("----------------------------------------\n");
        }
    }
    else
    {
        printf("Error al obtener la lista de torneos: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    pause_console();
}

void modificar_torneo()
{
#ifdef UNIT_TEST
    clear_screen();
    print_header("MODIFICAR TORNEO");

    if (!listar_torneos_generico("torneos registrados para modificar"))
    {
        pause_console();
        return;
    }

    int torneo_id = input_int("\nIngrese el ID del torneo a modificar (0 para cancelar): ");

    if (torneo_id == 0) return;

    if (!existe_id("torneo", torneo_id))
    {
        printf("ID de torneo invalido.\n");
        pause_console();
        return;
    }

    Torneo torneo = {0};
    sqlite3_stmt *stmt;
    const char *sql_torneo = "SELECT nombre, tiene_equipo_fijo, equipo_fijo_id, cantidad_equipos, tipo_torneo, formato_torneo FROM torneo WHERE id = ?;";

    if (preparar_stmt(sql_torneo, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strncpy_s(torneo.nombre, sizeof(torneo.nombre), (const char*)sqlite3_column_text(stmt, 0), sizeof(torneo.nombre));
            torneo.tiene_equipo_fijo = sqlite3_column_int(stmt, 1);
            torneo.equipo_fijo_id = sqlite3_column_int(stmt, 2);
            torneo.cantidad_equipos = sqlite3_column_int(stmt, 3);
            torneo.tipo_torneo = sqlite3_column_int(stmt, 4);
            torneo.formato_torneo = sqlite3_column_int(stmt, 5);
        }
        sqlite3_finalize(stmt);
    }

    printf("\nSeleccione que desea modificar:\n");
    printf("1. Nombre del torneo\n");
    printf("2. Equipo fijo\n");
    printf("3. Cantidad de equipos\n");
    printf("4. Tipo y formato de torneo\n");
    printf("5. Asociar equipos guardados\n");
    printf("6. Agregar equipos por nombre\n");
    printf("7. Agregar un equipo por nombre\n");
    printf("8. Volver\n");

    int opcion = input_int(">");

    switch (opcion)
    {
    case 1:
        actualizar_nombre_torneo(torneo_id);
        break;
    case 2:
        actualizar_equipo_fijo(torneo_id);
        break;
    case 3:
        actualizar_cantidad_equipos(torneo_id);
        break;
    case 4:
        actualizar_tipo_formato_torneo(torneo_id, torneo.cantidad_equipos);
        break;
    case 5:
        asociar_equipos_torneo(torneo_id);
        break;
    case 6:
        agregar_equipos_nombres_torneo(torneo_id);
        break;
    case 7:
        agregar_equipo_nombre_torneo(torneo_id);
        break;
    case 8:
        break;
    default:
        printf("Opcion invalida.\n");
    }

    pause_console();
#else
    int torneo_id = 0;
    int opcion = 0;
    Torneo torneo = {0};
    const char *opciones[] =
    {
        "Nombre del torneo",
        "Equipo fijo",
        "Cantidad de equipos",
        "Tipo y formato de torneo",
        "Asociar equipos guardados",
        "Agregar equipos por nombre",
        "Agregar un equipo por nombre",
        "Volver"
    };

    if (!tor_seleccionar_torneo_gui("MODIFICAR TORNEO",
                                    "Click o ENTER para seleccionar | ESC: cancelar",
                                    &torneo_id))
    {
        return;
    }

    if (!tor_obtener_torneo_por_id(torneo_id, &torneo))
    {
        tor_mostrar_info_gui("ERROR", "No se pudo cargar el torneo seleccionado.");
        return;
    }

    if (!tor_modal_selector_opcion_gui("QUE DESEA MODIFICAR?",
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
        actualizar_nombre_torneo(torneo_id);
        break;
    case 1:
        actualizar_equipo_fijo(torneo_id);
        break;
    case 2:
        actualizar_cantidad_equipos(torneo_id);
        break;
    case 3:
        actualizar_tipo_formato_torneo(torneo_id, torneo.cantidad_equipos);
        break;
    case 4:
        tor_asociar_equipo_guardado_gui(torneo_id);
        break;
    case 5:
        tor_agregar_equipos_nombres_gui(torneo_id);
        break;
    case 6:
        agregar_equipo_nombre_torneo(torneo_id);
        break;
    default:
        return;
    }
#endif
}

void eliminar_torneo()
{
#ifdef UNIT_TEST
    clear_screen();
    print_header("ELIMINAR TORNEO");

    if (!listar_torneos_generico("torneos registrados para eliminar"))
    {
        pause_console();
        return;
    }

    int torneo_id = input_int("\nIngrese el ID del torneo a eliminar (0 para cancelar): ");

    if (torneo_id == 0) return;

    if (!existe_id("torneo", torneo_id))
    {
        printf("ID de torneo invalido.\n");
        pause_console();
        return;
    }

    if (!confirmar("Esta seguro de eliminar este torneo? Se eliminaran datos asociados."))
    {
        printf("Eliminacion cancelada.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    const char *sqls[] =
    {
        "DELETE FROM equipo_fase WHERE torneo_id = ?;",
        "DELETE FROM torneo_fases WHERE torneo_id = ?;",
        "DELETE FROM jugador_estadisticas WHERE torneo_id = ?;",
        "DELETE FROM equipo_torneo_estadisticas WHERE torneo_id = ?;",
        "DELETE FROM partido_torneo WHERE torneo_id = ?;",
        "DELETE FROM equipo_torneo WHERE torneo_id = ?;",
        "DELETE FROM equipo_torneo_nombre WHERE torneo_id = ?;",
        "DELETE FROM equipo_historial WHERE torneo_id = ?;",
        "DELETE FROM torneo_temporada WHERE torneo_id = ?;",
        "DELETE FROM torneo WHERE id = ?;",
        NULL
    };

    for (int i = 0; sqls[i] != NULL; i++)
    {
        if (preparar_stmt(sqls[i], &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    printf("Torneo eliminado exitosamente.\n");
    pause_console();
#else
    int torneo_id = 0;
    sqlite3_stmt *stmt = NULL;
    const char *sqls[] =
    {
        "DELETE FROM equipo_fase WHERE torneo_id = ?;",
        "DELETE FROM torneo_fases WHERE torneo_id = ?;",
        "DELETE FROM jugador_estadisticas WHERE torneo_id = ?;",
        "DELETE FROM equipo_torneo_estadisticas WHERE torneo_id = ?;",
        "DELETE FROM partido_torneo WHERE torneo_id = ?;",
        "DELETE FROM equipo_torneo WHERE torneo_id = ?;",
        "DELETE FROM equipo_torneo_nombre WHERE torneo_id = ?;",
        "DELETE FROM equipo_historial WHERE torneo_id = ?;",
        "DELETE FROM torneo_temporada WHERE torneo_id = ?;",
        "DELETE FROM torneo WHERE id = ?;",
        NULL
    };

    if (!tor_seleccionar_torneo_gui("ELIMINAR TORNEO",
                                    "Click o ENTER para seleccionar | ESC: cancelar",
                                    &torneo_id))
    {
        return;
    }

    if (!tor_modal_confirmar_gui("CONFIRMAR ELIMINACION",
                                 "Esta seguro de eliminar este torneo? Se eliminaran datos asociados.",
                                 "Eliminar"))
    {
        return;
    }

    for (int i = 0; sqls[i] != NULL; i++)
    {
        if (preparar_stmt(sqls[i], &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    tor_mostrar_info_gui("TORNEO ELIMINADO",
                         "Torneo eliminado exitosamente.");
#endif
}

/**
 * @brief Actualiza el nombre del torneo en la base de datos
 */
static void actualizar_nombre_torneo(int torneo_id)
{
#ifdef UNIT_TEST
    char nuevo_nombre[50];
    input_string("Ingrese el nuevo nombre: ", nuevo_nombre, sizeof(nuevo_nombre));

    sqlite3_stmt *stmt;
    const char *sql_update = "UPDATE torneo SET nombre = ? WHERE id = ?;";
    if (preparar_stmt(sql_update, &stmt))
    {
        sqlite3_bind_text(stmt, 1, nuevo_nombre, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Nombre actualizado exitosamente.\n");
        }
        else
        {
            printf("Error al actualizar el nombre: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
#else
    char nuevo_nombre[50] = {0};
    sqlite3_stmt *stmt = NULL;
    const char *sql_update = "UPDATE torneo SET nombre = ? WHERE id = ?;";

    if (!tor_modal_input_texto_gui("MODIFICAR NOMBRE",
                                   "Nuevo nombre del torneo",
                                   "Letras, numeros y espacios",
                                   nuevo_nombre,
                                   sizeof(nuevo_nombre),
                                   0,
                                   TOR_INPUT_ALNUM))
    {
        return;
    }

    if (preparar_stmt(sql_update, &stmt))
    {
        sqlite3_bind_text(stmt, 1, nuevo_nombre, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            tor_mostrar_info_gui("NOMBRE ACTUALIZADO",
                                 "Nombre actualizado exitosamente.");
        }
        else
        {
            char msg[256];
            snprintf(msg, sizeof(msg), "Error al actualizar el nombre: %s", sqlite3_errmsg(db));
            tor_mostrar_info_gui("ERROR", msg);
        }
        sqlite3_finalize(stmt);
    }
#endif
}

/**
 * @brief Muestra lista de equipos disponibles y retorna ID seleccionado
 * @return ID del equipo seleccionado, o 0 si se cancela
 */
static int seleccionar_equipo_disponible(void)
{
#ifdef UNIT_TEST
    return select_team_id("\nIngrese el ID del equipo fijo (0 para cancelar): ",
                          "equipos registrados", 1);
#else
    int equipo_id = 0;
    if (!tor_seleccionar_equipo_gui("SELECCIONAR EQUIPO FIJO", &equipo_id))
    {
        return 0;
    }
    return equipo_id;
#endif
}

/**
 * @brief Actualiza el equipo fijo del torneo
 */
static void actualizar_equipo_fijo(int torneo_id)
{
#ifdef UNIT_TEST
    int nuevo_tiene_equipo_fijo = confirmar("El torneo tiene equipo fijo?");

    if (!nuevo_tiene_equipo_fijo)
    {
        sqlite3_stmt *stmt;
        const char *sql_update = "UPDATE torneo SET tiene_equipo_fijo = 0, equipo_fijo_id = -1 WHERE id = ?;";
        if (preparar_stmt(sql_update, &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                printf("Equipo fijo removido exitosamente.\n");
            }
            else
            {
                printf("Error al remover el equipo fijo: %s\n", sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);
        }
        return;
    }

    int equipo_id = seleccionar_equipo_disponible();
    if (equipo_id == 0) return;

    sqlite3_stmt *stmt;
    const char *sql_update = "UPDATE torneo SET tiene_equipo_fijo = ?, equipo_fijo_id = ? WHERE id = ?;";
    if (preparar_stmt(sql_update, &stmt))
    {
        sqlite3_bind_int(stmt, 1, 1);
        sqlite3_bind_int(stmt, 2, equipo_id);
        sqlite3_bind_int(stmt, 3, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Equipo fijo actualizado exitosamente.\n");
        }
        else
        {
            printf("Error al actualizar el equipo fijo: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
#else
    sqlite3_stmt *stmt = NULL;
    const char *opciones[] = {"No", "Si"};
    int seleccion = 0;

    if (!tor_modal_selector_opcion_gui("EQUIPO FIJO",
                                       "TIENE EQUIPO FIJO",
                                       opciones,
                                       ARRAY_COUNT(opciones),
                                       0,
                                       &seleccion))
    {
        return;
    }

    if (seleccion == 0)
    {
        const char *sql_update = "UPDATE torneo SET tiene_equipo_fijo = 0, equipo_fijo_id = -1 WHERE id = ?;";
        if (preparar_stmt(sql_update, &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                tor_mostrar_info_gui("EQUIPO FIJO",
                                     "Equipo fijo removido exitosamente.");
            }
            else
            {
                char msg[256];
                snprintf(msg, sizeof(msg), "Error al remover el equipo fijo: %s", sqlite3_errmsg(db));
                tor_mostrar_info_gui("ERROR", msg);
            }
            sqlite3_finalize(stmt);
        }
        return;
    }

    {
        int equipo_id = seleccionar_equipo_disponible();
        if (equipo_id == 0)
        {
            return;
        }

        const char *sql_update = "UPDATE torneo SET tiene_equipo_fijo = ?, equipo_fijo_id = ? WHERE id = ?;";
        if (preparar_stmt(sql_update, &stmt))
        {
            sqlite3_bind_int(stmt, 1, 1);
            sqlite3_bind_int(stmt, 2, equipo_id);
            sqlite3_bind_int(stmt, 3, torneo_id);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                tor_mostrar_info_gui("EQUIPO FIJO",
                                     "Equipo fijo actualizado exitosamente.");
            }
            else
            {
                char msg[256];
                snprintf(msg, sizeof(msg), "Error al actualizar el equipo fijo: %s", sqlite3_errmsg(db));
                tor_mostrar_info_gui("ERROR", msg);
            }
            sqlite3_finalize(stmt);
        }
    }
#endif
}

/**
 * @brief Maneja la actualizacion de tipo y formato de torneo
 */
static void actualizar_tipo_formato_torneo(int torneo_id, int cantidad)
{
#ifdef UNIT_TEST
    int opcion = 0;

    if (cantidad >= 4 && cantidad <= 6)
    {
        printf("\nPara 4-6 equipos, seleccione el formato:\n");
        printf("1. Round-robin (sistema liga)\n");
        printf("2. Mini grupo con final\n");
        opcion = input_int(">");
    }
    else if (cantidad >= 7 && cantidad <= 12)
    {
        printf("\nPara 7-12 equipos, seleccione el formato:\n");
        printf("1. Liga simple\n");
        printf("2. Liga doble\n");
        printf("3. Grupos + final\n");
        printf("4. Copa simple\n");
        opcion = input_int(">");
    }
    else if (cantidad >= 13 && cantidad <= 20)
    {
        printf("\nPara 13-20 equipos, seleccione el formato:\n");
        printf("1. Grupos (4-5 grupos) + eliminacion\n");
        printf("2. Copa + repechaje\n");
        printf("3. Liga grande\n");
        opcion = input_int(">");
    }
    else if (cantidad >= 21)
    {
        printf("\nPara 21 o mas equipos, seleccione el formato:\n");
        printf("1. Multiples grupos\n");
        printf("2. Eliminacion directa por fases\n");
        opcion = input_int(">");
    }
    else
    {
        printf("Cantidad de equipos no valida. No se actualizara el formato.\n");
        return;
    }

    TipoTorneos tipo;
    FormatoTorneos formato;
    obtener_formato_por_cantidad(opcion, cantidad, &tipo, &formato);
    aplicar_actualizacion_formato(torneo_id, tipo, formato);
#else
    int opcion = 1;

    if (cantidad >= 4 && cantidad <= 6)
    {
        const char *opciones[] =
        {
            "Round-robin (sistema liga)",
            "Mini grupo con final"
        };
        int seleccion = 0;
        if (!tor_modal_selector_opcion_gui("FORMATO DE TORNEO",
                                           "FORMATO",
                                           opciones,
                                           ARRAY_COUNT(opciones),
                                           0,
                                           &seleccion))
        {
            return;
        }
        opcion = seleccion + 1;
    }
    else if (cantidad >= 7 && cantidad <= 12)
    {
        const char *opciones[] =
        {
            "Liga simple",
            "Liga doble",
            "Grupos + final",
            "Copa simple"
        };
        int seleccion = 0;
        if (!tor_modal_selector_opcion_gui("FORMATO DE TORNEO",
                                           "FORMATO",
                                           opciones,
                                           ARRAY_COUNT(opciones),
                                           0,
                                           &seleccion))
        {
            return;
        }
        opcion = seleccion + 1;
    }
    else if (cantidad >= 13 && cantidad <= 20)
    {
        const char *opciones[] =
        {
            "Grupos (4-5 grupos) + eliminacion",
            "Copa + repechaje",
            "Liga grande"
        };
        int seleccion = 0;
        if (!tor_modal_selector_opcion_gui("FORMATO DE TORNEO",
                                           "FORMATO",
                                           opciones,
                                           ARRAY_COUNT(opciones),
                                           0,
                                           &seleccion))
        {
            return;
        }
        opcion = seleccion + 1;
    }
    else if (cantidad >= 21)
    {
        const char *opciones[] =
        {
            "Multiples grupos",
            "Eliminacion directa por fases"
        };
        int seleccion = 0;
        if (!tor_modal_selector_opcion_gui("FORMATO DE TORNEO",
                                           "FORMATO",
                                           opciones,
                                           ARRAY_COUNT(opciones),
                                           0,
                                           &seleccion))
        {
            return;
        }
        opcion = seleccion + 1;
    }
    else
    {
        tor_mostrar_info_gui("CANTIDAD NO VALIDA",
                             "Cantidad de equipos no valida. No se actualizara el formato.");
        return;
    }

    {
        TipoTorneos tipo;
        FormatoTorneos formato;
        obtener_formato_por_cantidad(opcion, cantidad, &tipo, &formato);
        aplicar_actualizacion_formato(torneo_id, tipo, formato);
    }
#endif
}

/**
 * @brief Muestra los equipos asociados a un torneo
 */
static void listar_equipos_asociados(int torneo_id)
{
    printf("=== EQUIPOS ASOCIADOS ===\n");
    int has_equipos = 0;
    int count = 1;

    // Equipos guardados (de tabla equipo)
    sqlite3_stmt *stmt_equipos;
    const char *sql_equipos = "SELECT e.id, e.nombre FROM equipo e JOIN equipo_torneo et ON e.id = et.equipo_id WHERE et.torneo_id = ? ORDER BY e.id;";
    if (preparar_stmt(sql_equipos, &stmt_equipos))
    {
        sqlite3_bind_int(stmt_equipos, 1, torneo_id);
        while (sqlite3_step(stmt_equipos) == SQLITE_ROW)
        {
            has_equipos = 1;
            const char *equipo_nombre = (const char*)sqlite3_column_text(stmt_equipos, 1);
            printf("%d. %s\n", count++, equipo_nombre);
        }
        sqlite3_finalize(stmt_equipos);
    }

    // Equipos por nombre (solo nombre, sin equipo completo)
    sqlite3_stmt *stmt_nombres;
    const char *sql_nombres = "SELECT nombre FROM equipo_torneo_nombre WHERE torneo_id = ? ORDER BY id;";
    if (preparar_stmt(sql_nombres, &stmt_nombres))
    {
        sqlite3_bind_int(stmt_nombres, 1, torneo_id);
        while (sqlite3_step(stmt_nombres) == SQLITE_ROW)
        {
            has_equipos = 1;
            const char *nombre = (const char*)sqlite3_column_text(stmt_nombres, 0);
            printf("%d. %s\n", count++, nombre);
        }
        sqlite3_finalize(stmt_nombres);
    }

    if (!has_equipos)
    {
        mostrar_no_hay_registros("equipos asociados a este torneo");
    }
}
/**
 * @brief Obtiene el nombre de un equipo por su ID
 * @param equipo_id ID del equipo
 * @return Nombre del equipo o "Desconocido"
 */
const char* get_equipo_nombre(int equipo_id)
{
    static char nombre[50];
    nombre[0] = '\0';

    sqlite3_stmt *stmt;
    const char *sql = "SELECT nombre FROM equipo WHERE id = ?;";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, equipo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const unsigned char *text = sqlite3_column_text(stmt, 0);
            if (text)
            {
                strncpy_s(nombre, sizeof(nombre), (const char *)text, sizeof(nombre) - 1);
            }
            else
            {
                strcpy_s(nombre, sizeof(nombre), "Desconocido");
            }
        }
        else
        {
            strcpy_s(nombre, sizeof(nombre), "Desconocido");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        strcpy_s(nombre, sizeof(nombre), "Desconocido");
    }

    return nombre;
}

/**
 * @brief Menu principal de gestion de torneos
 */
void menu_torneos()
{
    static const MenuItem items[] =
    {
        TOR_ITEM(1, "Crear", crear_torneo),
        TOR_ITEM(2, "Listar", listar_torneos),
        TOR_ITEM(3, "Modificar", modificar_torneo),
        TOR_ITEM(4, "Eliminar", eliminar_torneo),
        TOR_BACK_ITEM
    };

    ejecutar_menu_estandar("TORNEOS", items, ARRAY_COUNT(items));
}
