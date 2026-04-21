#include "lesion.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "gui_components.h"
#include "input.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

typedef struct
{
    int camiseta_id;
    int partido_id;
    char tipo[100];
    char descripcion[200];
    char fecha[20];
    char estado[50];
} LesionDatos;

typedef struct
{
    int id;
    int partido_id;
    char jugador[96];
    char tipo[80];
    char descripcion[220];
    char fecha[24];
    char camiseta[96];
    char estado[32];
    char cancha[96];
} LesionGuiRow;

typedef struct
{
    int id;
    char tipo[80];
    char fecha[24];
    int primera;
    int diferencia_dias;
} LesionDiffRow;

typedef struct
{
    int total;
    int activas;
    int recuperadas;
    int con_partido;
    int top_tipo_count;
    char top_tipo[80];
} LesionStatsSnapshot;

typedef struct
{
    char tipo[100];
    char descripcion[200];
    char fecha[20];
    char camiseta_id[16];
    char estado_opcion[16];
    char partido_id[16];
    int active_field;
    char error_msg[192];
} LesionGuiForm;

typedef struct
{
    int sw;
    int sh;
    int panel_x;
    int panel_y;
    int panel_w;
    int panel_h;
    Rectangle btn_save;
    Rectangle btn_cancel;
} LesionGuiFormLayout;

typedef struct
{
    int value;
    char label[128];
} LesionGuiPartidoOption;

typedef struct
{
    const LesionGuiPartidoOption *options;
    int count;
    int open;
    int scroll;
    int selected;
} LesionGuiPartidoDropdown;

typedef struct
{
    const LesionGuiPartidoOption *options;
    int count;
    int open;
    int scroll;
    int selected;
} LesionGuiCamisetaDropdown;

enum
{
    LESION_GUI_FIELD_TIPO = 0,
    LESION_GUI_FIELD_DESCRIPCION,
    LESION_GUI_FIELD_FECHA,
    LESION_GUI_FIELD_CAMISETA_ID,
    LESION_GUI_FIELD_ESTADO,
    LESION_GUI_FIELD_PARTIDO_ID,
    LESION_GUI_FIELD_COUNT
};

static int insertar_lesion_registro(const LesionDatos *datos, long long *id_out);
static int actualizar_lesion_registro(int lesion_id, const LesionDatos *datos);
static int eliminar_lesion_por_id(int lesion_id);
static int listar_lesiones_gui(void);
static int crear_lesion_gui(void);
static int modificar_lesion_gui(void);
static int eliminar_lesion_gui(void);
static int mostrar_diferencias_lesiones_gui(void);
static int actualizar_estados_lesiones_gui(void);
static void mostrar_estadisticas_lesiones_menu_adapter(void);
static void mostrar_estadisticas_lesiones_gui(void);
static int calcular_diferencia_dias(const char *fecha1, const char *fecha2);
static int lesion_gui_estado_clicked(const Rectangle *botones, int total, Vector2 mouse);
static void lesion_gui_dibujar_diferencias_rows(const LesionDiffRow *rows_in, int count_in, int scroll_in, int row_h_in, Rectangle area_in);

/**
 * @brief Preparar statement y reportar errores
 */
static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    return 1;
}

/**
 * @brief Obtener estado por opcion
 */
static const char *estado_por_opcion(int opcion)
{
    switch (opcion)
    {
    case 1:
        return "ACTIVA";
    case 2:
        return "EN TRATAMIENTO";
    case 3:
        return "MEJORANDO";
    case 4:
        return "RECUPERADO";
    case 5:
        return "RECAIDA";
    default:
        return NULL;
    }
}

static int opcion_por_estado(const char *estado)
{
    if (!estado)
    {
        return 1;
    }
    if (strcmp(estado, "ACTIVA") == 0)
    {
        return 1;
    }
    if (strcmp(estado, "EN TRATAMIENTO") == 0)
    {
        return 2;
    }
    if (strcmp(estado, "MEJORANDO") == 0)
    {
        return 3;
    }
    if (strcmp(estado, "RECUPERADO") == 0)
    {
        return 4;
    }
    if (strcmp(estado, "RECAIDA") == 0)
    {
        return 5;
    }
    return 1;
}

static int parse_int_texto(const char *text, int *out)
{
    char *end = NULL;
    long value;

    if (!text || !out)
    {
        return 0;
    }

    while (*text == ' ' || *text == '\t')
    {
        text++;
    }

    if (*text == '\0')
    {
        return 0;
    }

    value = strtol(text, &end, 10);
    if (!end || *end != '\0')
    {
        return 0;
    }
    if (value < INT_MIN || value > INT_MAX)
    {
        return 0;
    }

    *out = (int)value;
    return 1;
}

static void copy_texto_seguro(char *dst, size_t dst_sz, const char *src, const char *fallback)
{
    const char *safe_src = src;
    if (!dst || dst_sz == 0)
    {
        return;
    }
    if (!safe_src || safe_src[0] == '\0')
    {
        safe_src = fallback ? fallback : "";
    }
    snprintf(dst, dst_sz, "%s", safe_src);
}

static int insertar_lesion_registro(const LesionDatos *datos, long long *id_out)
{
    sqlite3_stmt *stmt = NULL;
    char *jugador = NULL;
    const char *jugador_fallback = "Usuario Desconocido";
    long long id;

    if (!datos)
    {
        return 0;
    }

    id = obtener_siguiente_id("lesion");
    jugador = get_user_name();

    if (!preparar_stmt(
                "INSERT INTO lesion(id, jugador, tipo, descripcion, fecha, camiseta_id, estado, partido_id) VALUES(?,?,?,?,?,?,?,?)",
                &stmt))
    {
        if (jugador)
        {
            free(jugador);
        }
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, jugador ? jugador : jugador_fallback, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, datos->tipo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, datos->descripcion, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, datos->fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, datos->camiseta_id);
    sqlite3_bind_text(stmt, 7, datos->estado, -1, SQLITE_TRANSIENT);

    if (datos->partido_id > 0)
    {
        sqlite3_bind_int(stmt, 8, datos->partido_id);
    }
    else
    {
        sqlite3_bind_null(stmt, 8);
    }

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        if (jugador)
        {
            free(jugador);
        }
        return 0;
    }

    sqlite3_finalize(stmt);
    if (jugador)
    {
        free(jugador);
    }
    if (id_out)
    {
        *id_out = id;
    }
    return 1;
}

static int actualizar_lesion_registro(int lesion_id, const LesionDatos *datos)
{
    sqlite3_stmt *stmt = NULL;

    if (!datos)
    {
        return 0;
    }

    if (!preparar_stmt(
                "UPDATE lesion SET tipo=?, descripcion=?, fecha=?, camiseta_id=?, estado=?, partido_id=? WHERE id=?",
                &stmt))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, datos->tipo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, datos->descripcion, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, datos->fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, datos->camiseta_id);
    sqlite3_bind_text(stmt, 5, datos->estado, -1, SQLITE_TRANSIENT);
    if (datos->partido_id > 0)
    {
        sqlite3_bind_int(stmt, 6, datos->partido_id);
    }
    else
    {
        sqlite3_bind_null(stmt, 6);
    }
    sqlite3_bind_int(stmt, 7, lesion_id);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    return 1;
}

static int eliminar_lesion_por_id(int lesion_id)
{
    sqlite3_stmt *stmt = NULL;

    if (!preparar_stmt("DELETE FROM lesion WHERE id=?", &stmt))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, lesion_id);
    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    return 1;
}

static int lesion_gui_ampliar_capacidad_rows(LesionGuiRow **rows, int *cap)
{
    LesionGuiRow *tmp;
    int new_cap;

    if (!rows || !cap)
    {
        return 0;
    }

    new_cap = (*cap == 0) ? 32 : (*cap * 2);
    tmp = (LesionGuiRow *)realloc(*rows, (size_t)new_cap * sizeof(LesionGuiRow));
    if (!tmp)
    {
        return 0;
    }

    *rows = tmp;
    *cap = new_cap;
    return 1;
}

static int cargar_lesiones_gui_rows(LesionGuiRow **rows_out, int *count_out, int solo_activas)
{
    const char *sql_base =
        "SELECT l.id, l.jugador, l.tipo, l.descripcion, l.fecha, "
        "COALESCE(c.nombre, 'Sin camiseta'), COALESCE(l.estado, 'ACTIVA'), "
        "COALESCE(can.nombre, 'Sin cancha'), COALESCE(l.partido_id, 0) "
        "FROM lesion l "
        "LEFT JOIN camiseta c ON l.camiseta_id = c.id "
        "LEFT JOIN partido p ON l.partido_id = p.id "
        "LEFT JOIN cancha can ON p.cancha_id = can.id ";
    const char *sql_activo =
        "WHERE COALESCE(l.estado, 'ACTIVA') != 'RECUPERADO' "
        "ORDER BY l.id ASC";
    const char *sql_all = "ORDER BY l.id ASC";
    sqlite3_stmt *stmt = NULL;
    LesionGuiRow *rows = NULL;
    int count = 0;
    int cap = 0;
    char sql[640];

    if (!rows_out || !count_out)
    {
        return 0;
    }

    snprintf(sql, sizeof(sql), "%s%s", sql_base, solo_activas ? sql_activo : sql_all);
    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *jugador;
        const char *tipo;
        const char *descripcion;
        const char *fecha;
        const char *camiseta;
        const char *estado;
        const char *cancha;

        if (count >= cap && !lesion_gui_ampliar_capacidad_rows(&rows, &cap))
        {
            sqlite3_finalize(stmt);
            free(rows);
            return 0;
        }

        jugador = (const char *)sqlite3_column_text(stmt, 1);
        tipo = (const char *)sqlite3_column_text(stmt, 2);
        descripcion = (const char *)sqlite3_column_text(stmt, 3);
        fecha = (const char *)sqlite3_column_text(stmt, 4);
        camiseta = (const char *)sqlite3_column_text(stmt, 5);
        estado = (const char *)sqlite3_column_text(stmt, 6);
        cancha = (const char *)sqlite3_column_text(stmt, 7);

        rows[count].id = sqlite3_column_int(stmt, 0);
        rows[count].partido_id = sqlite3_column_int(stmt, 8);
        copy_texto_seguro(rows[count].jugador, sizeof(rows[count].jugador), jugador, "Sin jugador");
        copy_texto_seguro(rows[count].tipo, sizeof(rows[count].tipo), tipo, "Sin tipo");
        copy_texto_seguro(rows[count].descripcion, sizeof(rows[count].descripcion), descripcion, "Sin descripcion");
        copy_texto_seguro(rows[count].fecha, sizeof(rows[count].fecha), fecha, "Sin fecha");
        copy_texto_seguro(rows[count].camiseta, sizeof(rows[count].camiseta), camiseta, "Sin camiseta");
        copy_texto_seguro(rows[count].estado, sizeof(rows[count].estado), estado, "ACTIVA");
        copy_texto_seguro(rows[count].cancha, sizeof(rows[count].cancha), cancha, "Sin cancha");

        count++;
    }

    sqlite3_finalize(stmt);
    *rows_out = rows;
    *count_out = count;
    return 1;
}

static int lesion_gui_clamp_scroll(int scroll, int count, int visible_rows)
{
    int max_scroll;

    if (visible_rows < 1)
    {
        visible_rows = 1;
    }
    if (count <= visible_rows)
    {
        return 0;
    }

    max_scroll = count - visible_rows;
    if (scroll < 0)
    {
        return 0;
    }
    if (scroll > max_scroll)
    {
        return max_scroll;
    }
    return scroll;
}

static int lesion_gui_actualizar_scroll(int scroll, int count, int visible_rows, Rectangle area)
{
    int wheel = (int)GetMouseWheelMove();

    if (wheel != 0 && CheckCollisionPointRec(GetMousePosition(), area))
    {
        scroll -= wheel;
    }

    return lesion_gui_clamp_scroll(scroll, count, visible_rows);
}

static void lesion_gui_calcular_panel(int sw, int sh, int *panel_x, int *panel_w, int *panel_h)
{
    int margin_x = (sw > 1100) ? 60 : 20;
    int h = sh - 180;

    if (h < 300)
    {
        h = 300;
    }

    *panel_x = margin_x;
    *panel_w = sw - margin_x * 2;
    *panel_h = h;
}

static void lesion_gui_dibujar_filas(const LesionGuiRow *rows, int count, int scroll, int row_h, Rectangle area)
{
    int panel_x = (int)area.x;
    int panel_y = (int)area.y;
    int panel_w = (int)area.width;
    int panel_h = (int)area.height;

    if (!rows || count == 0)
    {
        gui_text("No hay lesiones cargadas.", area.x + 24.0f, area.y + 24.0f, 24.0f, gui_get_active_theme()->text_primary);
        return;
    }

    BeginScissorMode(panel_x, panel_y, panel_w, panel_h);
    for (int i = scroll; i < count; i++)
    {
        int row = i - scroll;
        int y = panel_y + row * row_h;
        Rectangle fr;

        if (y + row_h > panel_y + panel_h)
        {
            break;
        }

        fr = (Rectangle){(float)(panel_x + 2), (float)y, (float)(panel_w - 4), (float)(row_h - 1)};
        gui_draw_list_row_bg(fr, row, CheckCollisionPointRec(GetMousePosition(), fr));

        gui_text(TextFormat("%3d", rows[i].id), (float)(panel_x + 10), (float)(y + 7), 18.0f, gui_get_active_theme()->text_primary);
        gui_text(rows[i].fecha, (float)(panel_x + 70), (float)(y + 7), 18.0f, gui_get_active_theme()->text_secondary);
        gui_text(rows[i].jugador, (float)(panel_x + 260), (float)(y + 7), 18.0f, gui_get_active_theme()->text_primary);
        gui_text_truncated(rows[i].tipo, (float)(panel_x + 470), (float)(y + 7), 18.0f,
                           (float)(panel_w - 610), gui_get_active_theme()->text_primary);
        gui_text(rows[i].estado, (float)(panel_x + panel_w - 130), (float)(y + 7), 17.0f, gui_get_active_theme()->text_secondary);
    }
    EndScissorMode();
}

static int lesion_gui_detectar_click_fila(const LesionGuiRow *rows, int count, int scroll, int row_h, Rectangle panel)
{
    int panel_x;
    int panel_y;
    int panel_w;
    int panel_h;

    if (!rows || count <= 0 || !IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        return 0;
    }

    panel_x = (int)panel.x;
    panel_y = (int)panel.y;
    panel_w = (int)panel.width;
    panel_h = (int)panel.height;

    for (int i = scroll; i < count; i++)
    {
        int row = i - scroll;
        int y = panel_y + row * row_h;
        Rectangle fr;

        if (y + row_h > panel_y + panel_h)
        {
            break;
        }

        fr = (Rectangle){(float)(panel_x + 2), (float)y, (float)(panel_w - 4), (float)(row_h - 1)};
        if (CheckCollisionPointRec(GetMousePosition(), fr))
        {
            return rows[i].id;
        }
    }

    return 0;
}

static const LesionGuiRow *lesion_gui_buscar_row_por_id(const LesionGuiRow *rows, int count, int id)
{
    if (!rows || count <= 0 || id <= 0)
    {
        return NULL;
    }

    for (int i = 0; i < count; i++)
    {
        if (rows[i].id == id)
        {
            return &rows[i];
        }
    }

    return NULL;
}

static int lesion_gui_should_close(void)
{
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
    {
        input_consume_key(KEY_ESCAPE);
        input_consume_key(KEY_ENTER);
        return 1;
    }
    return 0;
}

static void lesion_gui_feedback(const char *title, const char *message, int ok)
{
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = (sw > 900) ? 700 : sw - 40;
        int panel_h = 180;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle btn_ok = {(float)(panel_x + panel_w - 184), (float)(panel_y + panel_h - 54), 160.0f, 36.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header(title, sw);
        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h,
                           ok ? (Color){86,170,120,255} : (Color){170,86,86,255});
        gui_text(message, (float)(panel_x + 24), (float)(panel_y + 62), 22.0f,
                 gui_get_active_theme()->text_primary);
        DrawRectangleRec(btn_ok, gui_get_active_theme()->accent_primary);
        DrawRectangleLines((int)btn_ok.x, (int)btn_ok.y, (int)btn_ok.width, (int)btn_ok.height,
                           gui_get_active_theme()->card_border);
        gui_text("Aceptar", btn_ok.x + 40.0f, btn_ok.y + 8.0f, 18.0f,
                 gui_get_active_theme()->text_primary);
        gui_draw_footer_hint("Enter/Esc: volver", (float)panel_x, sh);
        EndDrawing();

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) ||
                (click && CheckCollisionPointRec(GetMousePosition(), btn_ok)))
        {
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            return;
        }
    }
}

static int lesion_gui_confirmar_eliminar(int lesion_id)
{
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = (sw > 900) ? 760 : sw - 40;
        int panel_h = 200;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle btn_yes = {(float)(panel_x + panel_w - 360), (float)(panel_y + panel_h - 56), 160.0f, 38.0f};
        Rectangle btn_no = {(float)(panel_x + panel_w - 184), (float)(panel_y + panel_h - 56), 160.0f, 38.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("ELIMINAR LESION", sw);
        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);
        gui_text(TextFormat("Desea eliminar la lesion ID %d?", lesion_id),
                 (float)(panel_x + 24), (float)(panel_y + 66), 22.0f, gui_get_active_theme()->text_primary);

        DrawRectangleRec(btn_yes, (Color){168,70,70,255});
        DrawRectangleRec(btn_no, gui_get_active_theme()->accent_primary);
        DrawRectangleLines((int)btn_yes.x, (int)btn_yes.y, (int)btn_yes.width, (int)btn_yes.height,
                           gui_get_active_theme()->card_border);
        DrawRectangleLines((int)btn_no.x, (int)btn_no.y, (int)btn_no.width, (int)btn_no.height,
                           gui_get_active_theme()->card_border);
        gui_text("Eliminar", btn_yes.x + 34.0f, btn_yes.y + 9.0f, 18.0f, gui_get_active_theme()->text_primary);
        gui_text("Cancelar", btn_no.x + 34.0f, btn_no.y + 9.0f, 18.0f, gui_get_active_theme()->text_primary);
        gui_draw_footer_hint("Enter: eliminar | Esc: cancelar", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_yes)) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ENTER);
            return 1;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_no)) || IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static void lesion_gui_mostrar_detalle(const LesionGuiRow *row)
{
    if (!row)
    {
        return;
    }

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = (sw > 1000) ? 900 : sw - 40;
        int panel_h = (sh > 700) ? 460 : sh - 120;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle btn_ok = {(float)(panel_x + panel_w - 184), (float)(panel_y + panel_h - 54), 160.0f, 36.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("DETALLE DE LESION", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text(TextFormat("ID: %d", row->id), (float)(panel_x + 24), (float)(panel_y + 32), 22.0f, gui_get_active_theme()->text_primary);
        gui_text(TextFormat("Jugador: %s", row->jugador), (float)(panel_x + 24), (float)(panel_y + 70), 20.0f, gui_get_active_theme()->text_primary);
        gui_text(TextFormat("Tipo: %s", row->tipo), (float)(panel_x + 24), (float)(panel_y + 106), 20.0f, gui_get_active_theme()->text_primary);
        gui_text(TextFormat("Estado: %s", row->estado), (float)(panel_x + 24), (float)(panel_y + 142), 20.0f, gui_get_active_theme()->text_primary);
        gui_text(TextFormat("Fecha: %s", row->fecha), (float)(panel_x + 24), (float)(panel_y + 178), 20.0f, gui_get_active_theme()->text_primary);
        gui_text(TextFormat("Camiseta: %s", row->camiseta), (float)(panel_x + 24), (float)(panel_y + 214), 20.0f, gui_get_active_theme()->text_primary);
        gui_text(TextFormat("Cancha: %s", row->cancha), (float)(panel_x + 24), (float)(panel_y + 250), 20.0f, gui_get_active_theme()->text_primary);
        if (row->partido_id > 0)
        {
            gui_text(TextFormat("Partido ID: %d", row->partido_id), (float)(panel_x + 24), (float)(panel_y + 286), 20.0f,
                     gui_get_active_theme()->text_primary);
        }
        else
        {
            gui_text("Partido ID: Sin asociar", (float)(panel_x + 24), (float)(panel_y + 286), 20.0f,
                     gui_get_active_theme()->text_primary);
        }

        gui_text("Descripcion:", (float)(panel_x + 24), (float)(panel_y + 322), 20.0f, gui_get_active_theme()->text_secondary);
        gui_text_wrapped(row->descripcion,
                         (Rectangle){(float)(panel_x + 24), (float)(panel_y + 352), (float)(panel_w - 48), 70.0f},
                         18.0f, gui_get_active_theme()->text_primary);

        DrawRectangleRec(btn_ok, gui_get_active_theme()->accent_primary);
        DrawRectangleLines((int)btn_ok.x, (int)btn_ok.y, (int)btn_ok.width, (int)btn_ok.height,
                           gui_get_active_theme()->card_border);
        gui_text("Volver", btn_ok.x + 44.0f, btn_ok.y + 8.0f, 18.0f, gui_get_active_theme()->text_primary);

        gui_draw_footer_hint("Esc/Enter/click: volver", (float)panel_x, sh);
        EndDrawing();

        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) ||
                (click && CheckCollisionPointRec(GetMousePosition(), btn_ok)))
        {
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            return;
        }
    }
}

static int lesion_gui_seleccionar_lesion_id(const char *title, const char *hint, int solo_activas, int *id_out)
{
    LesionGuiRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 34;
    const int panel_y = 130;

    if (!id_out)
    {
        return 0;
    }

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    if (!cargar_lesiones_gui_rows(&rows, &count, solo_activas))
    {
        return 0;
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = 0;
        int panel_w = 0;
        int panel_h = 0;
        int content_h = 0;
        int visible_rows = 1;
        int clicked_id = 0;
        Rectangle area;

        lesion_gui_calcular_panel(sw, sh, &panel_x, &panel_w, &panel_h);
        content_h = panel_h - 32;
        if (content_h < row_h)
        {
            content_h = row_h;
        }

        visible_rows = content_h / row_h;
        if (visible_rows < 1)
        {
            visible_rows = 1;
        }

        area = (Rectangle){(float)panel_x, (float)(panel_y + 32), (float)panel_w, (float)content_h};
        scroll = lesion_gui_actualizar_scroll(scroll, count, visible_rows, area);
        clicked_id = lesion_gui_detectar_click_fila(rows, count, scroll, row_h, area);

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header(title, sw);
        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                            "ID/FECHA", 10.0f,
                            "JUGADOR / TIPO / ESTADO", 260.0f);
        lesion_gui_dibujar_filas(rows, count, scroll, row_h, area);
        gui_draw_footer_hint(hint, (float)panel_x, sh);
        EndDrawing();

        if (clicked_id > 0)
        {
            *id_out = clicked_id;
            free(rows);
            return 1;
        }

        if (lesion_gui_should_close())
        {
            break;
        }
    }

    free(rows);
    return 0;
}

static int listar_lesiones_gui(void)
{
    LesionGuiRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 34;
    const int panel_y = 130;

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    if (!cargar_lesiones_gui_rows(&rows, &count, 0))
    {
        return 0;
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = 0;
        int panel_w = 0;
        int panel_h = 0;
        int content_h = 0;
        int visible_rows = 1;
        int clicked_id = 0;
        Rectangle area;

        lesion_gui_calcular_panel(sw, sh, &panel_x, &panel_w, &panel_h);
        content_h = panel_h - 32;
        if (content_h < row_h)
        {
            content_h = row_h;
        }

        visible_rows = content_h / row_h;
        if (visible_rows < 1)
        {
            visible_rows = 1;
        }

        area = (Rectangle){(float)panel_x, (float)(panel_y + 32), (float)panel_w, (float)content_h};
        scroll = lesion_gui_actualizar_scroll(scroll, count, visible_rows, area);
        clicked_id = lesion_gui_detectar_click_fila(rows, count, scroll, row_h, area);

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("LISTADO DE LESIONES", sw);
        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                            "ID/FECHA", 10.0f,
                            "JUGADOR / TIPO / ESTADO", 260.0f);
        lesion_gui_dibujar_filas(rows, count, scroll, row_h, area);
        gui_draw_footer_hint("Click: ver detalle | Rueda: scroll | Esc/Enter: volver", (float)panel_x, sh);
        EndDrawing();

        if (clicked_id > 0)
        {
            const LesionGuiRow *row = lesion_gui_buscar_row_por_id(rows, count, clicked_id);
            lesion_gui_mostrar_detalle(row);
            continue;
        }

        if (lesion_gui_should_close())
        {
            break;
        }
    }

    free(rows);
    return 1;
}

static const char *lesion_gui_field_label(int field)
{
    switch (field)
    {
    case LESION_GUI_FIELD_TIPO:
        return "Tipo";
    case LESION_GUI_FIELD_DESCRIPCION:
        return "Descripcion";
    case LESION_GUI_FIELD_FECHA:
        return "Fecha (DD/MM/AAAA HH:MM)";
    case LESION_GUI_FIELD_CAMISETA_ID:
        return "Camiseta ID";
    case LESION_GUI_FIELD_ESTADO:
        return "Estado (1-5)";
    case LESION_GUI_FIELD_PARTIDO_ID:
        return "Partido ID (0 sin asociar)";
    default:
        return "Campo";
    }
}

static int lesion_gui_field_is_numeric(int field)
{
    return (field == LESION_GUI_FIELD_CAMISETA_ID || field == LESION_GUI_FIELD_ESTADO || field == LESION_GUI_FIELD_PARTIDO_ID);
}

static char *lesion_gui_field_value_mut(LesionGuiForm *form, int field)
{
    if (!form)
    {
        return NULL;
    }

    switch (field)
    {
    case LESION_GUI_FIELD_TIPO:
        return form->tipo;
    case LESION_GUI_FIELD_DESCRIPCION:
        return form->descripcion;
    case LESION_GUI_FIELD_FECHA:
        return form->fecha;
    case LESION_GUI_FIELD_CAMISETA_ID:
        return form->camiseta_id;
    case LESION_GUI_FIELD_ESTADO:
        return form->estado_opcion;
    case LESION_GUI_FIELD_PARTIDO_ID:
        return form->partido_id;
    default:
        return NULL;
    }
}

static const char *lesion_gui_field_value(const LesionGuiForm *form, int field)
{
    if (!form)
    {
        return "";
    }

    switch (field)
    {
    case LESION_GUI_FIELD_TIPO:
        return form->tipo;
    case LESION_GUI_FIELD_DESCRIPCION:
        return form->descripcion;
    case LESION_GUI_FIELD_FECHA:
        return form->fecha;
    case LESION_GUI_FIELD_CAMISETA_ID:
        return form->camiseta_id;
    case LESION_GUI_FIELD_ESTADO:
        return form->estado_opcion;
    case LESION_GUI_FIELD_PARTIDO_ID:
        return form->partido_id;
    default:
        return "";
    }
}

static size_t lesion_gui_field_capacity(int field)
{
    switch (field)
    {
    case LESION_GUI_FIELD_TIPO:
        return sizeof(((LesionGuiForm *)0)->tipo);
    case LESION_GUI_FIELD_DESCRIPCION:
        return sizeof(((LesionGuiForm *)0)->descripcion);
    case LESION_GUI_FIELD_FECHA:
        return sizeof(((LesionGuiForm *)0)->fecha);
    case LESION_GUI_FIELD_CAMISETA_ID:
        return sizeof(((LesionGuiForm *)0)->camiseta_id);
    case LESION_GUI_FIELD_ESTADO:
        return sizeof(((LesionGuiForm *)0)->estado_opcion);
    case LESION_GUI_FIELD_PARTIDO_ID:
        return sizeof(((LesionGuiForm *)0)->partido_id);
    default:
        return 0;
    }
}

static Rectangle lesion_gui_form_field_rect(int panel_x, int panel_y, int panel_w, int field)
{
    float x = (float)(panel_x + 24);
    float y = (float)(panel_y + 80 + field * 62);
    float w = (float)(panel_w - 48);
    return (Rectangle){x, y, w, 34.0f};
}

static int lesion_gui_cargar_partidos_recientes(LesionGuiPartidoOption **rows_out, int *count_out)
{
    const char *sql =
        "SELECT p.id, COALESCE(p.fecha_hora, 'Sin fecha'), COALESCE(c.nombre, 'Sin cancha') "
        "FROM partido p "
        "LEFT JOIN cancha c ON p.cancha_id = c.id "
        "ORDER BY p.fecha_hora DESC, p.id DESC "
        "LIMIT 12";
    sqlite3_stmt *stmt = NULL;
    LesionGuiPartidoOption *rows;
    int cap = 13;
    int count = 1;

    if (!rows_out || !count_out)
    {
        return 0;
    }

    rows = (LesionGuiPartidoOption *)calloc((size_t)cap, sizeof(LesionGuiPartidoOption));
    if (!rows)
    {
        return 0;
    }

    rows[0].value = 0;
    snprintf(rows[0].label, sizeof(rows[0].label), "0 - Sin asociar");

    if (!preparar_stmt(sql, &stmt))
    {
        *rows_out = rows;
        *count_out = count;
        return 1;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW && count < cap)
    {
        const char *fecha = (const char *)sqlite3_column_text(stmt, 1);
        const char *cancha = (const char *)sqlite3_column_text(stmt, 2);

        rows[count].value = sqlite3_column_int(stmt, 0);
        snprintf(rows[count].label, sizeof(rows[count].label), "%d - %s | %s",
                 rows[count].value,
                 fecha ? fecha : "Sin fecha",
                 cancha ? cancha : "Sin cancha");
        count++;
    }

    sqlite3_finalize(stmt);

    *rows_out = rows;
    *count_out = count;
    return 1;
}

static int lesion_gui_cargar_camisetas_disponibles(LesionGuiPartidoOption **rows_out, int *count_out)
{
    const char *sql =
        "SELECT id, COALESCE(nombre, 'Sin nombre') "
        "FROM camiseta "
        "ORDER BY id DESC "
        "LIMIT 20";
    sqlite3_stmt *stmt = NULL;
    LesionGuiPartidoOption *rows = NULL;
    int cap = 32;
    int count = 0;

    if (!rows_out || !count_out)
    {
        return 0;
    }

    rows = (LesionGuiPartidoOption *)calloc((size_t)cap, sizeof(LesionGuiPartidoOption));
    if (!rows)
    {
        return 0;
    }

    if (!preparar_stmt(sql, &stmt))
    {
        free(rows);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *nombre = (const char *)sqlite3_column_text(stmt, 1);

        if (count >= cap)
        {
            int new_cap = cap * 2;
            LesionGuiPartidoOption *tmp = (LesionGuiPartidoOption *)realloc(rows, (size_t)new_cap * sizeof(LesionGuiPartidoOption));
            if (!tmp)
            {
                sqlite3_finalize(stmt);
                free(rows);
                return 0;
            }
            rows = tmp;
            cap = new_cap;
        }

        rows[count].value = sqlite3_column_int(stmt, 0);
        snprintf(rows[count].label, sizeof(rows[count].label), "%d - %s",
                 rows[count].value,
                 nombre ? nombre : "Sin nombre");
        count++;
    }

    sqlite3_finalize(stmt);
    *rows_out = rows;
    *count_out = count;
    return 1;
}

static int lesion_gui_partido_option_index_by_value(const LesionGuiPartidoOption *opts, int count, int value)
{
    if (!opts || count <= 0)
    {
        return 0;
    }

    for (int i = 0; i < count; i++)
    {
        if (opts[i].value == value)
        {
            return i;
        }
    }

    return 0;
}

static int lesion_gui_camiseta_option_index_by_value(const LesionGuiPartidoOption *opts, int count, int value)
{
    if (!opts || count <= 0)
    {
        return 0;
    }

    for (int i = 0; i < count; i++)
    {
        if (opts[i].value == value)
        {
            return i;
        }
    }

    return 0;
}

static Rectangle lesion_gui_partido_dropdown_list_rect(Rectangle base, int options_count)
{
    int visible = options_count;
    if (visible > 6)
    {
        visible = 6;
    }
    if (visible < 1)
    {
        visible = 1;
    }

    return (Rectangle){base.x, base.y + base.height + 4.0f, base.width, (float)(visible * 30)};
}

static void lesion_gui_partido_dropdown_draw_popup(Rectangle base,
        const LesionGuiPartidoOption *opts,
        int count,
        int selected,
        int scroll)
{
    int visible = count;
    Rectangle list_rect;

    if (!opts || count <= 0)
    {
        return;
    }

    if (visible > 6)
    {
        visible = 6;
    }
    if (visible < 1)
    {
        visible = 1;
    }

    list_rect = lesion_gui_partido_dropdown_list_rect(base, count);
    DrawRectangleRec(list_rect, (Color){18,36,28,255});
    DrawRectangleLines((int)list_rect.x, (int)list_rect.y, (int)list_rect.width, (int)list_rect.height,
                       gui_get_active_theme()->card_border);

    for (int i = 0; i < visible; i++)
    {
        int idx = scroll + i;
        Rectangle row_rect;
        int hovered;
        Color row_bg;

        if (idx >= count)
        {
            break;
        }

        row_rect = (Rectangle){list_rect.x, list_rect.y + (float)(i * 30), list_rect.width, 30.0f};
        hovered = CheckCollisionPointRec(GetMousePosition(), row_rect);
        row_bg = (idx == selected) ? (Color){46,90,62,255} : (hovered ? (Color){34,64,47,255} : (Color){18,36,28,255});
        DrawRectangleRec(row_rect, row_bg);
        gui_text_truncated(opts[idx].label, row_rect.x + 8.0f, row_rect.y + 7.0f, 15.0f,
                           row_rect.width - 12.0f, gui_get_active_theme()->text_primary);
    }
}

static void lesion_gui_partido_dropdown_scroll(LesionGuiPartidoDropdown *dropdown, Rectangle list_rect, Vector2 mouse)
{
    float wheel;
    int visible;
    int max_scroll;

    if (!dropdown || !dropdown->open || dropdown->count <= 0 || !CheckCollisionPointRec(mouse, list_rect))
    {
        return;
    }

    wheel = GetMouseWheelMove();
    if (wheel == 0.0f)
    {
        return;
    }

    visible = dropdown->count;
    if (visible > 6)
    {
        visible = 6;
    }
    if (visible < 1)
    {
        visible = 1;
    }

    max_scroll = dropdown->count - visible;
    if (max_scroll < 0)
    {
        max_scroll = 0;
    }

    if (wheel < 0.0f && dropdown->scroll < max_scroll)
    {
        dropdown->scroll++;
    }
    else if (wheel > 0.0f && dropdown->scroll > 0)
    {
        dropdown->scroll--;
    }
}

static void lesion_gui_camiseta_dropdown_scroll(LesionGuiCamisetaDropdown *dropdown, Rectangle list_rect, Vector2 mouse)
{
    float wheel;
    int visible;
    int max_scroll;

    if (!dropdown || !dropdown->open || dropdown->count <= 0 || !CheckCollisionPointRec(mouse, list_rect))
    {
        return;
    }

    wheel = GetMouseWheelMove();
    if (wheel == 0.0f)
    {
        return;
    }

    visible = dropdown->count;
    if (visible > 6)
    {
        visible = 6;
    }
    if (visible < 1)
    {
        visible = 1;
    }

    max_scroll = dropdown->count - visible;
    if (max_scroll < 0)
    {
        max_scroll = 0;
    }

    if (wheel < 0.0f && dropdown->scroll < max_scroll)
    {
        dropdown->scroll++;
    }
    else if (wheel > 0.0f && dropdown->scroll > 0)
    {
        dropdown->scroll--;
    }
}

static void lesion_gui_partido_dropdown_handle_click(LesionGuiPartidoDropdown *dropdown,
        LesionGuiForm *form,
        int click,
        Vector2 mouse,
        Rectangle field_rect,
        Rectangle list_rect)
{
    if (!dropdown || !form || !click)
    {
        return;
    }

    if (CheckCollisionPointRec(mouse, field_rect))
    {
        dropdown->open = !dropdown->open;
        return;
    }

    if (dropdown->open && CheckCollisionPointRec(mouse, list_rect))
    {
        int row = (int)((mouse.y - list_rect.y) / 30.0f);
        int idx = dropdown->scroll + row;

        if (idx >= 0 && idx < dropdown->count)
        {
            snprintf(form->partido_id, sizeof(form->partido_id), "%d", dropdown->options[idx].value);
        }
        dropdown->open = 0;
        return;
    }

    if (dropdown->open)
    {
        dropdown->open = 0;
    }
}

static void lesion_gui_camiseta_dropdown_handle_click(LesionGuiCamisetaDropdown *dropdown,
        LesionGuiForm *form,
        int click,
        Vector2 mouse,
        Rectangle field_rect,
        Rectangle list_rect)
{
    if (!dropdown || !form || !click)
    {
        return;
    }

    if (CheckCollisionPointRec(mouse, field_rect))
    {
        dropdown->open = !dropdown->open;
        return;
    }

    if (dropdown->open && CheckCollisionPointRec(mouse, list_rect))
    {
        int row = (int)((mouse.y - list_rect.y) / 30.0f);
        int idx = dropdown->scroll + row;

        if (idx >= 0 && idx < dropdown->count)
        {
            snprintf(form->camiseta_id, sizeof(form->camiseta_id), "%d", dropdown->options[idx].value);
        }
        dropdown->open = 0;
        return;
    }

    if (dropdown->open)
    {
        dropdown->open = 0;
    }
}

static void lesion_gui_partido_dropdown_sync_selected(LesionGuiPartidoDropdown *dropdown, const LesionGuiForm *form)
{
    int partido_id_actual = 0;

    if (!dropdown || !form)
    {
        return;
    }

    if (parse_int_texto(form->partido_id, &partido_id_actual))
    {
        dropdown->selected = lesion_gui_partido_option_index_by_value(dropdown->options,
                             dropdown->count,
                             partido_id_actual);
    }
    else
    {
        dropdown->selected = lesion_gui_partido_option_index_by_value(dropdown->options,
                             dropdown->count,
                             0);
    }
}

static void lesion_gui_camiseta_dropdown_sync_selected(LesionGuiCamisetaDropdown *dropdown, const LesionGuiForm *form)
{
    int camiseta_id_actual = 0;

    if (!dropdown || !form)
    {
        return;
    }

    if (parse_int_texto(form->camiseta_id, &camiseta_id_actual))
    {
        dropdown->selected = lesion_gui_camiseta_option_index_by_value(dropdown->options,
                             dropdown->count,
                             camiseta_id_actual);
    }
    else
    {
        dropdown->selected = 0;
    }
}

static void lesion_gui_form_handle_input(LesionGuiForm *form)
{
    char *value;
    size_t cap;
    int key;
    size_t len;

    if (!form)
    {
        return;
    }

    value = lesion_gui_field_value_mut(form, form->active_field);
    if (!value)
    {
        return;
    }

    cap = lesion_gui_field_capacity(form->active_field);
    key = GetCharPressed();
    while (key > 0)
    {
        if (key >= 32 && key <= 126 &&
                (!lesion_gui_field_is_numeric(form->active_field) || isdigit((unsigned char)key)))
        {
            len = strlen(value);
            if (len + 1 < cap)
            {
                value[len] = (char)key;
                value[len + 1] = '\0';
            }
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE))
    {
        len = strlen(value);
        if (len > 0)
        {
            value[len - 1] = '\0';
        }
    }
}

static void lesion_gui_form_init_default(LesionGuiForm *form)
{
    if (!form)
    {
        return;
    }

    memset(form, 0, sizeof(*form));
    get_datetime(form->fecha, (int)sizeof(form->fecha));
    snprintf(form->estado_opcion, sizeof(form->estado_opcion), "%d", 1);
    snprintf(form->partido_id, sizeof(form->partido_id), "%d", 0);
    form->active_field = LESION_GUI_FIELD_TIPO;
}

static int lesion_gui_form_load_by_id(int lesion_id, LesionGuiForm *form)
{
    sqlite3_stmt *stmt = NULL;
    const char *estado;

    if (!form)
    {
        return 0;
    }

    if (!preparar_stmt(
                "SELECT tipo, descripcion, fecha, camiseta_id, COALESCE(estado, 'ACTIVA'), COALESCE(partido_id, 0) "
                "FROM lesion WHERE id=?",
                &stmt))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, lesion_id);
    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    memset(form, 0, sizeof(*form));
    copy_texto_seguro(form->tipo, sizeof(form->tipo), (const char *)sqlite3_column_text(stmt, 0), "");
    copy_texto_seguro(form->descripcion, sizeof(form->descripcion), (const char *)sqlite3_column_text(stmt, 1), "");
    copy_texto_seguro(form->fecha, sizeof(form->fecha), (const char *)sqlite3_column_text(stmt, 2), "");
    snprintf(form->camiseta_id, sizeof(form->camiseta_id), "%d", sqlite3_column_int(stmt, 3));
    estado = (const char *)sqlite3_column_text(stmt, 4);
    snprintf(form->estado_opcion, sizeof(form->estado_opcion), "%d", opcion_por_estado(estado));
    snprintf(form->partido_id, sizeof(form->partido_id), "%d", sqlite3_column_int(stmt, 5));
    form->active_field = LESION_GUI_FIELD_TIPO;
    form->error_msg[0] = '\0';

    sqlite3_finalize(stmt);
    return 1;
}

static int lesion_gui_form_validate(const LesionGuiForm *form, LesionDatos *datos, char *err, size_t err_sz)
{
    int camiseta_id;
    int estado_opcion;
    int partido_id = 0;
    const char *estado_texto;

    if (!form || !datos || !err || err_sz == 0)
    {
        return 0;
    }

    if (safe_strnlen(form->tipo, sizeof(form->tipo)) == 0)
    {
        snprintf(err, err_sz, "El tipo es obligatorio");
        return 0;
    }

    if (safe_strnlen(form->descripcion, sizeof(form->descripcion)) == 0)
    {
        snprintf(err, err_sz, "La descripcion es obligatoria");
        return 0;
    }

    if (!parse_int_texto(form->camiseta_id, &camiseta_id) || camiseta_id <= 0 || !existe_id("camiseta", camiseta_id))
    {
        snprintf(err, err_sz, "Camiseta ID invalida");
        return 0;
    }

    if (!parse_int_texto(form->estado_opcion, &estado_opcion) || !estado_por_opcion(estado_opcion))
    {
        snprintf(err, err_sz, "Estado invalido (1-5)");
        return 0;
    }

    if (safe_strnlen(form->partido_id, sizeof(form->partido_id)) > 0)
    {
        if (!parse_int_texto(form->partido_id, &partido_id) || partido_id < 0)
        {
            snprintf(err, err_sz, "Partido ID invalido");
            return 0;
        }
        if (partido_id > 0 && !existe_id("partido", partido_id))
        {
            snprintf(err, err_sz, "El partido no existe");
            return 0;
        }
    }

    estado_texto = estado_por_opcion(estado_opcion);

    memset(datos, 0, sizeof(*datos));
    datos->camiseta_id = camiseta_id;
    datos->partido_id = partido_id;
    copy_texto_seguro(datos->tipo, sizeof(datos->tipo), form->tipo, "");
    copy_texto_seguro(datos->descripcion, sizeof(datos->descripcion), form->descripcion, "");
    if (safe_strnlen(form->fecha, sizeof(form->fecha)) > 0)
    {
        copy_texto_seguro(datos->fecha, sizeof(datos->fecha), form->fecha, "");
    }
    else
    {
        get_datetime(datos->fecha, (int)sizeof(datos->fecha));
    }
    copy_texto_seguro(datos->estado, sizeof(datos->estado), estado_texto, "ACTIVA");

    return 1;
}

static int lesion_gui_run_form_guardar(LesionGuiForm *form, int is_create, int lesion_id)
{
    LesionDatos datos;
    char err[160] = {0};
    int ok;

    if (!form)
    {
        return 0;
    }

    if (!lesion_gui_form_validate(form, &datos, err, sizeof(err)))
    {
        copy_texto_seguro(form->error_msg, sizeof(form->error_msg), err, "Error de validacion");
        return 0;
    }

    ok = is_create ? insertar_lesion_registro(&datos, NULL)
         : actualizar_lesion_registro(lesion_id, &datos);

    if (!ok)
    {
        copy_texto_seguro(form->error_msg, sizeof(form->error_msg), "No se pudo guardar la lesion", "Error");
        return 0;
    }

    form->error_msg[0] = '\0';
    return 1;
}

static void lesion_gui_form_update_active_field(LesionGuiForm *form, int panel_x, int panel_y, int panel_w, int click, Vector2 mouse)
{
    if (!form)
    {
        return;
    }

    for (int i = 0; i < LESION_GUI_FIELD_COUNT; i++)
    {
        Rectangle r = lesion_gui_form_field_rect(panel_x, panel_y, panel_w, i);
        if (click && CheckCollisionPointRec(mouse, r))
        {
            form->active_field = i;
        }
    }

    if (IsKeyPressed(KEY_TAB))
    {
        form->active_field = (form->active_field + 1) % LESION_GUI_FIELD_COUNT;
    }
}

static void lesion_gui_form_draw_field_value(const LesionGuiForm *form,
        int field,
        Rectangle r,
        const LesionGuiCamisetaDropdown *camiseta_dd,
        const LesionGuiPartidoDropdown *partido_dd)
{
    if (field == LESION_GUI_FIELD_CAMISETA_ID && camiseta_dd && camiseta_dd->options && camiseta_dd->count > 0)
    {
        const char *camiseta_label = lesion_gui_field_value(form, field);
        if (camiseta_dd->selected >= 0 && camiseta_dd->selected < camiseta_dd->count)
        {
            camiseta_label = camiseta_dd->options[camiseta_dd->selected].label;
        }
        gui_text_truncated(camiseta_label, r.x + 8.0f, r.y + 8.0f, 16.0f,
                           r.width - 32.0f, gui_get_active_theme()->text_primary);
        gui_text(camiseta_dd->open ? "^" : "v", r.x + r.width - 16.0f, r.y + 8.0f, 16.0f,
                 gui_get_active_theme()->text_secondary);
        return;
    }

    if (field == LESION_GUI_FIELD_PARTIDO_ID && partido_dd && partido_dd->options && partido_dd->count > 0)
    {
        const char *partido_label = lesion_gui_field_value(form, field);
        if (partido_dd->selected >= 0 && partido_dd->selected < partido_dd->count)
        {
            partido_label = partido_dd->options[partido_dd->selected].label;
        }
        gui_text_truncated(partido_label, r.x + 8.0f, r.y + 8.0f, 16.0f,
                           r.width - 32.0f, gui_get_active_theme()->text_primary);
        gui_text(partido_dd->open ? "^" : "v", r.x + r.width - 16.0f, r.y + 8.0f, 16.0f,
                 gui_get_active_theme()->text_secondary);
        return;
    }

    gui_text_truncated(lesion_gui_field_value(form, field), r.x + 8.0f, r.y + 8.0f, 16.0f,
                       r.width - 16.0f, gui_get_active_theme()->text_primary);
}

static void lesion_gui_form_draw_open_dropdowns(int panel_x,
        int panel_y,
        int panel_w,
        const LesionGuiCamisetaDropdown *camiseta_dd,
        const LesionGuiPartidoDropdown *partido_dd)
{
    if (camiseta_dd && camiseta_dd->open)
    {
        Rectangle camiseta_rect = lesion_gui_form_field_rect(panel_x, panel_y, panel_w, LESION_GUI_FIELD_CAMISETA_ID);
        lesion_gui_partido_dropdown_draw_popup(camiseta_rect,
                                               camiseta_dd->options,
                                               camiseta_dd->count,
                                               camiseta_dd->selected,
                                               camiseta_dd->scroll);
    }

    if (partido_dd && partido_dd->open)
    {
        Rectangle partido_rect = lesion_gui_form_field_rect(panel_x, panel_y, panel_w, LESION_GUI_FIELD_PARTIDO_ID);
        lesion_gui_partido_dropdown_draw_popup(partido_rect,
                                               partido_dd->options,
                                               partido_dd->count,
                                               partido_dd->selected,
                                               partido_dd->scroll);
    }
}

static void lesion_gui_form_draw(const char *title,
                                 LesionGuiForm *form,
                                 const LesionGuiFormLayout *layout,
                                 const LesionGuiCamisetaDropdown *camiseta_dd,
                                 const LesionGuiPartidoDropdown *partido_dd)
{
    int sw;
    int sh;
    int panel_x;
    int panel_y;
    int panel_w;
    int panel_h;
    Rectangle btn_save;
    Rectangle btn_cancel;

    if (!layout)
    {
        return;
    }

    sw = layout->sw;
    sh = layout->sh;
    panel_x = layout->panel_x;
    panel_y = layout->panel_y;
    panel_w = layout->panel_w;
    panel_h = layout->panel_h;
    btn_save = layout->btn_save;
    btn_cancel = layout->btn_cancel;

    BeginDrawing();
    ClearBackground((Color){14,27,20,255});
    gui_draw_module_header(title, sw);

    DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
    DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);
    gui_text("TAB/click para cambiar campo | Enter para guardar", (float)(panel_x + 24), (float)(panel_y + 24),
             18.0f, gui_get_active_theme()->text_secondary);

    for (int i = 0; i < LESION_GUI_FIELD_COUNT; i++)
    {
        Rectangle r = lesion_gui_form_field_rect(panel_x, panel_y, panel_w, i);
        int active = (i == form->active_field);

        gui_text(lesion_gui_field_label(i), r.x, r.y - 20.0f, 16.0f, gui_get_active_theme()->text_secondary);
        DrawRectangleRec(r, active ? (Color){28,56,40,255} : (Color){18,36,28,255});
        DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height,
                           active ? gui_get_active_theme()->accent_primary : gui_get_active_theme()->card_border);
        lesion_gui_form_draw_field_value(form, i, r, camiseta_dd, partido_dd);
    }

    gui_text("Estados: 1 ACTIVA | 2 EN TRATAMIENTO | 3 MEJORANDO | 4 RECUPERADO | 5 RECAIDA",
             (float)(panel_x + 24), (float)(panel_y + panel_h - 92), 16.0f, gui_get_active_theme()->text_secondary);

    DrawRectangleRec(btn_save, gui_get_active_theme()->accent_primary);
    DrawRectangleRec(btn_cancel, (Color){77,92,80,255});
    DrawRectangleLines((int)btn_save.x, (int)btn_save.y, (int)btn_save.width, (int)btn_save.height,
                       gui_get_active_theme()->card_border);
    DrawRectangleLines((int)btn_cancel.x, (int)btn_cancel.y, (int)btn_cancel.width, (int)btn_cancel.height,
                       gui_get_active_theme()->card_border);
    gui_text("Guardar", btn_save.x + 42.0f, btn_save.y + 8.0f, 18.0f, gui_get_active_theme()->text_primary);
    gui_text("Cancelar", btn_cancel.x + 38.0f, btn_cancel.y + 8.0f, 18.0f, gui_get_active_theme()->text_primary);

    if (form->error_msg[0] != '\0')
    {
        gui_text(form->error_msg, (float)(panel_x + 24), (float)(panel_y + panel_h - 120),
                 18.0f, (Color){232,120,120,255});
    }

    lesion_gui_form_draw_open_dropdowns(panel_x, panel_y, panel_w, camiseta_dd, partido_dd);

    gui_draw_footer_hint("Enter: guardar | Esc: volver", (float)panel_x, sh);
    EndDrawing();
}

static int lesion_gui_run_form(const char *title, LesionGuiForm *form, int is_create, int lesion_id)
{
    LesionGuiPartidoOption *camiseta_opts = NULL;
    int camiseta_count = 0;
    LesionGuiPartidoOption *partido_opts = NULL;
    int partido_count = 0;
    LesionGuiCamisetaDropdown camiseta_dd;
    LesionGuiPartidoDropdown partido_dd;

    if (!lesion_gui_cargar_camisetas_disponibles(&camiseta_opts, &camiseta_count))
    {
        return 0;
    }

    if (!lesion_gui_cargar_partidos_recientes(&partido_opts, &partido_count))
    {
        free(camiseta_opts);
        return 0;
    }

    camiseta_dd.options = camiseta_opts;
    camiseta_dd.count = camiseta_count;
    camiseta_dd.open = 0;
    camiseta_dd.scroll = 0;
    camiseta_dd.selected = 0;

    partido_dd.options = partido_opts;
    partido_dd.count = partido_count;
    partido_dd.open = 0;
    partido_dd.scroll = 0;
    partido_dd.selected = 0;

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);
    input_consume_key(KEY_TAB);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = (sw > 980) ? 920 : sw - 40;
        int panel_h = sh - 170;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = 98;
        LesionGuiFormLayout layout;
        Rectangle btn_save;
        Rectangle btn_cancel;
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        int save_trigger = 0;
        int cancel_trigger = 0;
        Vector2 mouse = GetMousePosition();
        Rectangle camiseta_rect;
        Rectangle camiseta_list_rect;
        Rectangle partido_rect;
        Rectangle partido_list_rect;

        if (panel_h < 540)
        {
            panel_h = 540;
        }

        btn_save = (Rectangle){(float)(panel_x + panel_w - 360), (float)(panel_y + panel_h - 52), 160.0f, 36.0f};
        btn_cancel = (Rectangle){(float)(panel_x + panel_w - 184), (float)(panel_y + panel_h - 52), 160.0f, 36.0f};
        camiseta_rect = lesion_gui_form_field_rect(panel_x, panel_y, panel_w, LESION_GUI_FIELD_CAMISETA_ID);
        camiseta_list_rect = lesion_gui_partido_dropdown_list_rect(camiseta_rect, camiseta_count);
        partido_rect = lesion_gui_form_field_rect(panel_x, panel_y, panel_w, LESION_GUI_FIELD_PARTIDO_ID);
        partido_list_rect = lesion_gui_partido_dropdown_list_rect(partido_rect, partido_count);

        lesion_gui_form_update_active_field(form, panel_x, panel_y, panel_w, click, mouse);

        lesion_gui_camiseta_dropdown_scroll(&camiseta_dd, camiseta_list_rect, mouse);
        lesion_gui_partido_dropdown_scroll(&partido_dd, partido_list_rect, mouse);

        lesion_gui_camiseta_dropdown_handle_click(&camiseta_dd, form, click, mouse, camiseta_rect, camiseta_list_rect);
        lesion_gui_partido_dropdown_handle_click(&partido_dd, form, click, mouse, partido_rect, partido_list_rect);

        if (camiseta_dd.open)
        {
            partido_dd.open = 0;
        }
        if (partido_dd.open)
        {
            camiseta_dd.open = 0;
        }

        if (click && CheckCollisionPointRec(mouse, btn_save))
        {
            save_trigger = 1;
        }
        else if (click && CheckCollisionPointRec(mouse, btn_cancel))
        {
            cancel_trigger = 1;
        }

        lesion_gui_form_handle_input(form);

        if (IsKeyPressed(KEY_ENTER))
        {
            save_trigger = 1;
        }
        if (IsKeyPressed(KEY_ESCAPE))
        {
            cancel_trigger = 1;
        }

        if (save_trigger && lesion_gui_run_form_guardar(form, is_create, lesion_id))
        {
            free(camiseta_opts);
            free(partido_opts);
            return 1;
        }

        if (cancel_trigger)
        {
            input_consume_key(KEY_ESCAPE);
            free(camiseta_opts);
            free(partido_opts);
            return 0;
        }

        lesion_gui_camiseta_dropdown_sync_selected(&camiseta_dd, form);
        lesion_gui_partido_dropdown_sync_selected(&partido_dd, form);

        layout.sw = sw;
        layout.sh = sh;
        layout.panel_x = panel_x;
        layout.panel_y = panel_y;
        layout.panel_w = panel_w;
        layout.panel_h = panel_h;
        layout.btn_save = btn_save;
        layout.btn_cancel = btn_cancel;

        lesion_gui_form_draw(title,
                             form,
                             &layout,
                             &camiseta_dd,
                             &partido_dd);
    }

    free(camiseta_opts);
    free(partido_opts);
    return 0;
}

static int crear_lesion_gui(void)
{
    LesionGuiForm form;

    if (!hay_registros("camiseta"))
    {
        lesion_gui_feedback("CREAR LESION", "Debe existir al menos una camiseta", 0);
        return 0;
    }

    lesion_gui_form_init_default(&form);
    if (!lesion_gui_run_form("CREAR LESION", &form, 1, 0))
    {
        return 0;
    }

    lesion_gui_feedback("CREAR LESION", "Lesion creada correctamente", 1);
    return 1;
}

static int modificar_lesion_gui(void)
{
    LesionGuiForm form;
    int lesion_id = 0;

    if (!hay_registros("lesion"))
    {
        lesion_gui_feedback("MODIFICAR LESION", "No hay lesiones registradas", 0);
        return 0;
    }

    if (!lesion_gui_seleccionar_lesion_id("MODIFICAR LESION", "Click en una fila para editar | Esc/Enter: volver", 0, &lesion_id))
    {
        return 0;
    }

    if (!lesion_gui_form_load_by_id(lesion_id, &form))
    {
        lesion_gui_feedback("MODIFICAR LESION", "No se pudo cargar la lesion", 0);
        return 0;
    }

    if (!lesion_gui_run_form("MODIFICAR LESION", &form, 0, lesion_id))
    {
        return 0;
    }

    lesion_gui_feedback("MODIFICAR LESION", "Lesion modificada correctamente", 1);
    return 1;
}

static int eliminar_lesion_gui(void)
{
    int lesion_id = 0;

    if (!hay_registros("lesion"))
    {
        lesion_gui_feedback("ELIMINAR LESION", "No hay lesiones registradas", 0);
        return 0;
    }

    if (!lesion_gui_seleccionar_lesion_id("ELIMINAR LESION", "Click en una fila para eliminar | Esc/Enter: volver", 0, &lesion_id))
    {
        return 0;
    }

    if (!lesion_gui_confirmar_eliminar(lesion_id))
    {
        return 0;
    }

    if (!eliminar_lesion_por_id(lesion_id))
    {
        lesion_gui_feedback("ELIMINAR LESION", "No se pudo eliminar la lesion", 0);
        return 0;
    }

    lesion_gui_feedback("ELIMINAR LESION", "Lesion eliminada correctamente", 1);
    return 1;
}

static int lesion_gui_ampliar_capacidad_diff(LesionDiffRow **rows, int *cap)
{
    LesionDiffRow *tmp;
    int new_cap;

    if (!rows || !cap)
    {
        return 0;
    }

    new_cap = (*cap == 0) ? 32 : (*cap * 2);
    tmp = (LesionDiffRow *)realloc(*rows, (size_t)new_cap * sizeof(LesionDiffRow));
    if (!tmp)
    {
        return 0;
    }

    *rows = tmp;
    *cap = new_cap;
    return 1;
}

static int cargar_diferencias_gui_rows(LesionDiffRow **rows_out, int *count_out)
{
    sqlite3_stmt *stmt = NULL;
    LesionDiffRow *rows = NULL;
    int count = 0;
    int cap = 0;
    char fecha_anterior[32] = "";
    int primera = 1;

    if (!rows_out || !count_out)
    {
        return 0;
    }

    if (!preparar_stmt("SELECT id, fecha, tipo FROM lesion ORDER BY fecha ASC", &stmt))
    {
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha = (const char *)sqlite3_column_text(stmt, 1);
        const char *tipo = (const char *)sqlite3_column_text(stmt, 2);

        if (count >= cap && !lesion_gui_ampliar_capacidad_diff(&rows, &cap))
        {
            sqlite3_finalize(stmt);
            free(rows);
            return 0;
        }

        rows[count].id = sqlite3_column_int(stmt, 0);
        copy_texto_seguro(rows[count].fecha, sizeof(rows[count].fecha), fecha, "Sin fecha");
        copy_texto_seguro(rows[count].tipo, sizeof(rows[count].tipo), tipo, "Sin tipo");
        rows[count].primera = primera;
        rows[count].diferencia_dias = primera ? 0 : calcular_diferencia_dias(fecha_anterior, rows[count].fecha);

        copy_texto_seguro(fecha_anterior, sizeof(fecha_anterior), rows[count].fecha, "");
        primera = 0;
        count++;
    }

    sqlite3_finalize(stmt);
    *rows_out = rows;
    *count_out = count;
    return 1;
}

static int lesion_gui_actualizar_estado_por_id(int lesion_id, const char *estado)
{
    sqlite3_stmt *stmt = NULL;

    if (!estado)
    {
        return 0;
    }

    if (!preparar_stmt("UPDATE lesion SET estado=? WHERE id=?", &stmt))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, estado, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, lesion_id);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    return 1;
}

static int lesion_gui_seleccionar_estado(int *opcion_out)
{
    const char *estados[] =
    {
        "1. ACTIVA",
        "2. EN TRATAMIENTO",
        "3. MEJORANDO",
        "4. RECUPERADO",
        "5. RECAIDA"
    };
    Rectangle botones[5];

    if (!opcion_out)
    {
        return 0;
    }

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = (sw > 900) ? 760 : sw - 40;
        int panel_h = 360;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        Vector2 mouse = GetMousePosition();

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("ACTUALIZAR ESTADO", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);
        gui_text("Seleccione el nuevo estado", (float)(panel_x + 24), (float)(panel_y + 30), 22.0f,
                 gui_get_active_theme()->text_primary);

        for (int i = 0; i < 5; i++)
        {
            botones[i] = (Rectangle){(float)(panel_x + 24), (float)(panel_y + 74 + i * 50), (float)(panel_w - 48), 40.0f};
            DrawRectangleRec(botones[i], CheckCollisionPointRec(mouse, botones[i]) ? (Color){28,56,40,255} : (Color){18,36,28,255});
            DrawRectangleLines((int)botones[i].x, (int)botones[i].y, (int)botones[i].width, (int)botones[i].height,
                               gui_get_active_theme()->card_border);
            gui_text(estados[i], botones[i].x + 12.0f, botones[i].y + 10.0f, 18.0f, gui_get_active_theme()->text_primary);
        }

        gui_draw_footer_hint("Click para elegir | Esc para cancelar", (float)panel_x, sh);
        EndDrawing();

        if (click)
        {
            int idx = lesion_gui_estado_clicked(botones, 5, mouse);
            if (idx >= 0)
            {
                *opcion_out = idx + 1;
                return 1;
            }
        }

        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static int actualizar_estados_lesiones_gui(void)
{
    int lesion_id = 0;
    int opcion_estado = 0;
    const char *estado;

    if (!hay_registros("lesion"))
    {
        lesion_gui_feedback("ACTUALIZAR ESTADOS", "No hay lesiones registradas", 0);
        return 0;
    }

    if (!lesion_gui_seleccionar_lesion_id("ACTUALIZAR ESTADOS", "Click una lesion activa | Esc/Enter: volver", 1, &lesion_id))
    {
        return 0;
    }

    if (!lesion_gui_seleccionar_estado(&opcion_estado))
    {
        return 0;
    }

    estado = estado_por_opcion(opcion_estado);
    if (!lesion_gui_actualizar_estado_por_id(lesion_id, estado))
    {
        lesion_gui_feedback("ACTUALIZAR ESTADOS", "No se pudo actualizar el estado", 0);
        return 0;
    }

    lesion_gui_feedback("ACTUALIZAR ESTADOS", TextFormat("Estado actualizado a %s", estado), 1);
    return 1;
}

static int lesion_gui_estado_clicked(const Rectangle *botones, int total, Vector2 mouse)
{
    if (!botones || total <= 0)
    {
        return -1;
    }

    for (int i = 0; i < total; i++)
    {
        if (CheckCollisionPointRec(mouse, botones[i]))
        {
            return i;
        }
    }

    return -1;
}

static void lesion_gui_dibujar_diferencias_rows(const LesionDiffRow *rows_in, int count_in, int scroll_in, int row_h_in, Rectangle area_in)
{
    BeginScissorMode((int)area_in.x, (int)area_in.y, (int)area_in.width, (int)area_in.height);
    for (int i = scroll_in; i < count_in; i++)
    {
        int row = i - scroll_in;
        int y = (int)area_in.y + row * row_h_in;
        Rectangle fr;
        const char *texto_diferencia;

        if (y + row_h_in > (int)area_in.y + (int)area_in.height)
        {
            break;
        }

        fr = (Rectangle){area_in.x + 2.0f, (float)y, area_in.width - 4.0f, (float)(row_h_in - 1)};
        gui_draw_list_row_bg(fr, row, CheckCollisionPointRec(GetMousePosition(), fr));

        gui_text(TextFormat("%3d", rows_in[i].id), area_in.x + 10.0f, (float)(y + 7), 18.0f, gui_get_active_theme()->text_primary);
        gui_text(rows_in[i].fecha, area_in.x + 70.0f, (float)(y + 7), 18.0f, gui_get_active_theme()->text_secondary);
        gui_text(rows_in[i].tipo, area_in.x + 260.0f, (float)(y + 7), 18.0f, gui_get_active_theme()->text_primary);
        texto_diferencia = rows_in[i].primera ? "Primera lesion" : TextFormat("%d dias", rows_in[i].diferencia_dias);
        gui_text(texto_diferencia, area_in.x + area_in.width - 220.0f, (float)(y + 7), 17.0f,
                 gui_get_active_theme()->text_secondary);
    }
    EndScissorMode();
}

static int mostrar_diferencias_lesiones_gui(void)
{
    LesionDiffRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 34;
    const int panel_y = 130;

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    if (!cargar_diferencias_gui_rows(&rows, &count))
    {
        return 0;
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = 0;
        int panel_w = 0;
        int panel_h = 0;
        int content_h;
        int visible_rows;
        Rectangle area;

        lesion_gui_calcular_panel(sw, sh, &panel_x, &panel_w, &panel_h);
        content_h = panel_h - 32;
        if (content_h < row_h)
        {
            content_h = row_h;
        }

        visible_rows = content_h / row_h;
        if (visible_rows < 1)
        {
            visible_rows = 1;
        }

        area = (Rectangle){(float)panel_x, (float)(panel_y + 32), (float)panel_w, (float)content_h};
        scroll = lesion_gui_actualizar_scroll(scroll, count, visible_rows, area);

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("DIFERENCIAS ENTRE LESIONES", sw);
        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                            "ID/FECHA", 10.0f,
                            "TIPO / DIFERENCIA", 260.0f);

        if (count == 0)
        {
            gui_text("No hay lesiones registradas.", (float)(panel_x + 24), (float)(panel_y + 70), 22.0f,
                     gui_get_active_theme()->text_primary);
        }
        else
        {
            lesion_gui_dibujar_diferencias_rows(rows, count, scroll, row_h, area);
        }

        gui_draw_footer_hint("Rueda: scroll | Esc/Enter: volver", (float)panel_x, sh);
        EndDrawing();

        if (lesion_gui_should_close())
        {
            break;
        }
    }

    free(rows);
    return 1;
}

static int lesion_stats_scalar_int(const char *sql, int *out)
{
    sqlite3_stmt *stmt = NULL;
    int value = 0;

    if (!out)
    {
        return 0;
    }

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        value = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    *out = value;
    return 1;
}

static int lesion_stats_cargar_snapshot(LesionStatsSnapshot *stats)
{
    sqlite3_stmt *stmt = NULL;

    if (!stats)
    {
        return 0;
    }

    memset(stats, 0, sizeof(*stats));
    copy_texto_seguro(stats->top_tipo, sizeof(stats->top_tipo), "N/A", "N/A");

    if (!lesion_stats_scalar_int("SELECT COUNT(*) FROM lesion", &stats->total))
    {
        return 0;
    }
    if (!lesion_stats_scalar_int("SELECT COUNT(*) FROM lesion WHERE COALESCE(estado, 'ACTIVA') != 'RECUPERADO'", &stats->activas))
    {
        return 0;
    }
    if (!lesion_stats_scalar_int("SELECT COUNT(*) FROM lesion WHERE COALESCE(estado, '') = 'RECUPERADO'", &stats->recuperadas))
    {
        return 0;
    }
    if (!lesion_stats_scalar_int("SELECT COUNT(*) FROM lesion WHERE partido_id IS NOT NULL", &stats->con_partido))
    {
        return 0;
    }

    if (preparar_stmt("SELECT tipo, COUNT(*) AS c FROM lesion GROUP BY tipo ORDER BY c DESC LIMIT 1", &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            copy_texto_seguro(stats->top_tipo, sizeof(stats->top_tipo), (const char *)sqlite3_column_text(stmt, 0), "N/A");
            stats->top_tipo_count = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    return 1;
}

static void mostrar_estadisticas_lesiones_gui(void)
{
    LesionStatsSnapshot stats;

    if (!lesion_stats_cargar_snapshot(&stats))
    {
        lesion_gui_feedback("ESTADISTICAS LESIONES", "No se pudieron cargar estadisticas", 0);
        return;
    }

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = (sw > 980) ? 920 : sw - 40;
        int panel_h = 420;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("ESTADISTICAS DE LESIONES", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text(TextFormat("Total de lesiones: %d", stats.total), (float)(panel_x + 24), (float)(panel_y + 40), 24.0f,
                 gui_get_active_theme()->text_primary);
        gui_text(TextFormat("Lesiones activas: %d", stats.activas), (float)(panel_x + 24), (float)(panel_y + 92), 22.0f,
                 gui_get_active_theme()->text_primary);
        gui_text(TextFormat("Lesiones recuperadas: %d", stats.recuperadas), (float)(panel_x + 24), (float)(panel_y + 140), 22.0f,
                 gui_get_active_theme()->text_primary);
        gui_text(TextFormat("Lesiones asociadas a partido: %d", stats.con_partido), (float)(panel_x + 24), (float)(panel_y + 188), 22.0f,
                 gui_get_active_theme()->text_primary);
        gui_text(TextFormat("Tipo mas frecuente: %s (%d)", stats.top_tipo, stats.top_tipo_count),
                 (float)(panel_x + 24), (float)(panel_y + 236), 22.0f, gui_get_active_theme()->text_primary);

        gui_text("Para estadisticas detalladas use Exportes y modulo de reportes", (float)(panel_x + 24), (float)(panel_y + 300), 18.0f,
                 gui_get_active_theme()->text_secondary);

        gui_draw_footer_hint("Esc/Enter: volver", (float)panel_x, sh);
        EndDrawing();

        if (lesion_gui_should_close())
        {
            return;
        }
    }
}

static void mostrar_estadisticas_lesiones_menu_adapter(void)
{
    mostrar_estadisticas_lesiones_gui();
}

/**
 * @brief Crea una nueva lesion en la base de datos
 *
 * Solicita al usuario el tipo, descripcion de la lesion, el ID de la camiseta asociada
 * y el estado inicial, y la inserta en la tabla 'lesion'. El nombre del jugador se obtiene del usuario actual.
 * Utiliza el ID mas pequeno disponible para reutilizar IDs eliminados.
 */
void crear_lesion()
{
    (void)crear_lesion_gui();
}

/**
 * @brief Muestra un listado de todas las lesiones registradas
 *
 * Consulta la base de datos y muestra en pantalla todas las lesiones
 * con sus respectivos datos: ID, tipo, descripcion, fecha y estado.
 * Si no hay lesiones registradas, muestra un mensaje informativo.
 */
void listar_lesiones()
{
    (void)listar_lesiones_gui();
}

/**
 * @brief Permite modificar una lesion existente
 *
 * Muestra la lista de lesiones disponibles, solicita el ID a modificar,
 * verifica que exista y muestra un menu con opciones para modificar campos individuales o todos.
 */
void modificar_lesion()
{
    (void)modificar_lesion_gui();
}

/**
 * @brief Elimina una lesion de la base de datos
 *
 * Muestra la lista de lesiones disponibles, solicita el ID a eliminar,
 * verifica que exista y solicita confirmacion antes de eliminar.
 * Una vez eliminada, el ID queda disponible para reutilizacion.
 */
void eliminar_lesion()
{
    (void)eliminar_lesion_gui();
}

/**
 * @brief Calcula la diferencia en dias entre dos fechas de lesiones
 *
 * @param fecha1 Primera fecha en formato string
 * @param fecha2 Segunda fecha en formato string
 * @return Diferencia en dias (positivo si fecha2 es posterior a fecha1)
 */
static int calcular_diferencia_dias(const char *fecha1, const char *fecha2)
{
    int dia1 = 0;
    int mes1 = 0;
    int ano1 = 0;
    int dia2 = 0;
    int mes2 = 0;
    int ano2 = 0;
    int ok1;
    int ok2;

    if (!fecha1 || !fecha2)
    {
        return 0;
    }

    ok1 = sscanf(fecha1, "%d/%d/%d", &dia1, &mes1, &ano1);
    ok2 = sscanf(fecha2, "%d/%d/%d", &dia2, &mes2, &ano2);
    if (ok1 != 3 || ok2 != 3)
    {
        return 0;
    }

    return (ano2 * 365 + mes2 * 30 + dia2) - (ano1 * 365 + mes1 * 30 + dia1);
}

/**
 * @brief Muestra diferencias de dias entre lesiones consecutivas
 */
void mostrar_diferencias_lesiones()
{
    (void)mostrar_diferencias_lesiones_gui();
}

/**
 * @brief Pregunta al usuario si desea actualizar el estado de las lesiones activas
 */
void actualizar_estados_lesiones()
{
    (void)actualizar_estados_lesiones_gui();
}

/**
 * @brief Muestra el menu principal de gestion de lesiones
 *
 * Presenta un menu interactivo con opciones para crear, listar, editar
 * y eliminar lesiones. Utiliza la funcion ejecutar_menu para manejar
 * la navegacion del menu y delega las operaciones a las funciones correspondientes.
 */
void menu_lesiones()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_lesion},
        {2, "Listar", listar_lesiones},
        {3, "Modificar", modificar_lesion},
        {4, "Eliminar", eliminar_lesion},
        {5, "Estadisticas", mostrar_estadisticas_lesiones_menu_adapter},
        {6, "Diferencias entre Lesiones", mostrar_diferencias_lesiones},
        {7, "Actualizar Estados", actualizar_estados_lesiones},
        {0, "Volver", NULL}
    };
    ejecutar_menu_estandar("LESIONES", items, 8);
}
