#include "cancha.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "raylib.h"
#include "gui_components.h"
#include "input.h"
#ifdef _WIN32
#include <process.h>
#include <io.h>
#else
#include "process.h"
#include <strings.h>
#endif


static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

typedef struct
{
    int id;
    char nombre[128];
} CanchaRow;

typedef struct
{
    int x;
    int y;
    int w;
    int h;
    int visible_rows;
} CanchaGuiPanel;

static int ampliar_capacidad_canchas_gui(CanchaRow **rows, int *cap)
{
    int new_cap = (*cap) * 2;
    CanchaRow *tmp = (CanchaRow *)realloc(*rows, (size_t)new_cap * sizeof(CanchaRow));
    if (!tmp)
    {
        return 0;
    }

    memset(tmp + (*cap), 0, (size_t)(new_cap - (*cap)) * sizeof(CanchaRow));
    *rows = tmp;
    *cap = new_cap;
    return 1;
}

static int cargar_canchas_gui_rows(CanchaRow **rows_out, int *count_out)
{
    sqlite3_stmt *stmt = NULL;
    int cap = 32;
    int count = 0;
    CanchaRow *rows = (CanchaRow *)calloc((size_t)cap, sizeof(CanchaRow));
    if (!rows)
    {
        return 0;
    }

    if (!preparar_stmt(&stmt, "SELECT id, nombre FROM cancha ORDER BY id"))
    {
        free(rows);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (count >= cap && !ampliar_capacidad_canchas_gui(&rows, &cap))
        {
            sqlite3_finalize(stmt);
            free(rows);
            return 0;
        }

        rows[count].id = sqlite3_column_int(stmt, 0);
        snprintf(rows[count].nombre,
                 sizeof(rows[count].nombre),
                 "%s",
                 (const char *)(sqlite3_column_text(stmt, 1) ? sqlite3_column_text(stmt, 1) : (const unsigned char *)"(sin nombre)"));
        count++;
    }

    sqlite3_finalize(stmt);
    *rows_out = rows;
    *count_out = count;
    return 1;
}

static int clamp_scroll_canchas_gui(int scroll, int count, int visible_rows)
{
    int max_scroll = count - visible_rows;
    if (max_scroll < 0)
    {
        max_scroll = 0;
    }

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

static int actualizar_scroll_canchas_gui(int scroll, int count, int visible_rows, Rectangle area)
{
    float wheel = GetMouseWheelMove();
    if (CheckCollisionPointRec(GetMousePosition(), area))
    {
        if (wheel < 0.0f && scroll < count - visible_rows)
        {
            scroll++;
        }
        else if (wheel > 0.0f && scroll > 0)
        {
            scroll--;
        }
    }

    return clamp_scroll_canchas_gui(scroll, count, visible_rows);
}

static void calcular_panel_listado_canchas_gui(int sw, int sh, int row_h, CanchaGuiPanel *panel)
{
    panel->x = (sw > 900) ? 80 : 20;
    panel->y = 130;
    panel->w = sw - (panel->x * 2);
    panel->h = sh - 220;
    if (panel->h < 200)
    {
        panel->h = 200;
    }

    panel->visible_rows = panel->h / row_h;
    if (panel->visible_rows < 1)
    {
        panel->visible_rows = 1;
    }
}

static void dibujar_filas_canchas_gui(const CanchaRow *rows,
                                      int count,
                                      int scroll,
                                      int row_h,
                                      const CanchaGuiPanel *panel)
{
    if (count == 0)
    {
        gui_text("No hay canchas cargadas.", (float)(panel->x + 24), (float)(panel->y + 24), 24.0f, (Color){233, 247, 236, 255});
        return;
    }

    BeginScissorMode(panel->x, panel->y, panel->w, panel->h);
    for (int i = scroll; i < count; i++)
    {
        int row = i - scroll;
        int y = panel->y + row * row_h;
        if (y + row_h > panel->y + panel->h)
            break;

        {
            Rectangle fr = {(float)(panel->x + 2), (float)y, (float)(panel->w - 4), (float)(row_h - 1)};
            int hovered = CheckCollisionPointRec(GetMousePosition(), fr);
            gui_draw_list_row_bg(fr, row, hovered);
        }

        gui_text(TextFormat("%3d", rows[i].id), (float)(panel->x + 12), (float)(y + 7), 19.0f, (Color){227, 242, 232, 255});
        gui_text(rows[i].nombre, (float)(panel->x + 78), (float)(y + 7), 19.0f, (Color){241, 252, 244, 255});
    }
    EndScissorMode();
}

static void dibujar_listado_canchas_gui(const CanchaRow *rows,
                                        int count,
                                        int scroll,
                                        int row_h,
                                        const CanchaGuiPanel *panel,
                                        int sw,
                                        int sh)
{
    BeginDrawing();
    ClearBackground((Color){14, 27, 20, 255});

    gui_draw_module_header("LISTADO DE CANCHAS", sw);

    {
        Rectangle content_rect = gui_draw_list_shell(
            (Rectangle){(float)panel->x, (float)panel->y, (float)panel->w, (float)panel->h},
            "ID", 12.0f,
            "NOMBRE", 78.0f);
        CanchaGuiPanel content =
        {
            (int)content_rect.x,
            (int)content_rect.y,
            (int)content_rect.width,
            (int)content_rect.height,
            panel->visible_rows
        };
        dibujar_filas_canchas_gui(rows, count, scroll, row_h, &content);
    }

    gui_draw_footer_hint("Rueda: scroll | ESC/Enter: volver", (float)panel->x, sh);
    EndDrawing();
}

static int listar_canchas_gui(void)
{
    CanchaRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 34;
    CanchaGuiPanel panel = {80, 130, 1100, 520, 1};

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    if (!cargar_canchas_gui_rows(&rows, &count))
        return 0;

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        Rectangle area = {0};
        int content_h = 0;
        int visible_rows = 1;

        calcular_panel_listado_canchas_gui(sw, sh, row_h, &panel);

        content_h = panel.h - 32;
        if (content_h < row_h)
        {
            content_h = row_h;
        }
        visible_rows = content_h / row_h;
        if (visible_rows < 1)
        {
            visible_rows = 1;
        }

        area = (Rectangle){(float)panel.x, (float)(panel.y + 32), (float)panel.w, (float)content_h};
        scroll = actualizar_scroll_canchas_gui(scroll, count, visible_rows, area);

        dibujar_listado_canchas_gui(rows,
                                    count,
                                    scroll,
                                    row_h,
                                    &panel,
                                    sw,
                                    sh);

        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            break;
        }
    }

    free(rows);
    return 1;
}

static int abrir_imagen_en_sistema(const char *ruta)
{
    if (!ruta || ruta[0] == '\0')
    {
        return 0;
    }

    char cmd[1400];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", ruta);
#else
    snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" >/dev/null 2>&1", ruta);
#endif
    return system(cmd) == 0;
}

static int obtener_ruta_imagen_cancha_db(int id, char *ruta, size_t size)
{
    if (!ruta || size == 0)
    {
        return 0;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT imagen_ruta FROM cancha WHERE id=?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, id);
    int ok = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *valor = sqlite3_column_text(stmt, 0);
        if (valor && valor[0] != '\0' && strncpy_s(ruta, size, (const char *)valor, _TRUNCATE) == 0)
        {
            ok = 1;
        }
    }

    sqlite3_finalize(stmt);
    return ok;
}

static int construir_ruta_absoluta_imagen_cancha_por_id(int id, char *ruta_absoluta, size_t size)
{
    if (!ruta_absoluta || size == 0)
    {
        return 0;
    }

    char ruta_db[300] = {0};
    if (!obtener_ruta_imagen_cancha_db(id, ruta_db, sizeof(ruta_db)))
    {
        return 0;
    }

    return db_resolve_image_absolute_path(ruta_db, ruta_absoluta, size);
}

static void solicitar_nombre_cancha(const char *prompt, char *buffer, int size)
{
    while (1)
    {
        input_string(prompt, buffer, size);
        trim_whitespace(buffer);

        if (buffer[0] != '\0')
            return;

        printf("El nombre no puede estar vacio.\n");
    }
}

static int cargar_imagen_para_cancha_id(int id)
{
    return app_cargar_imagen_entidad(id, "cancha", "mifutbol_imagen_sel_cancha.txt");
}

static void cancha_gui_append_printable_ascii(char *buffer, int *cursor, size_t buffer_size)
{
    int key = GetCharPressed();
    while (key > 0)
    {
        if (key >= 32 && key <= 126 && *cursor < (int)buffer_size - 1)
        {
            buffer[*cursor] = (char)key;
            (*cursor)++;
            buffer[*cursor] = '\0';
        }
        key = GetCharPressed();
    }
}

static void cancha_gui_handle_backspace(char *buffer, int *cursor)
{
    if (!IsKeyPressed(KEY_BACKSPACE))
    {
        return;
    }

    size_t len = strlen(buffer);
    if (len > 0)
    {
        buffer[len - 1] = '\0';
        *cursor = (int)len - 1;
    }
}

static int dibujar_y_detectar_click_canchas_gui(const CanchaRow *rows,
                                                 int count,
                                                 int scroll,
                                                 int row_h,
                                                 const CanchaGuiPanel *panel)
{
    if (count == 0)
    {
        gui_text("No hay canchas cargadas.", (float)(panel->x + 24), (float)(panel->y + 24), 24.0f, (Color){233, 247, 236, 255});
        return 0;
    }

    BeginScissorMode(panel->x, panel->y, panel->w, panel->h);
    for (int i = scroll; i < count; i++)
    {
        int row = i - scroll;
        int y = panel->y + row * row_h;
        if (y + row_h > panel->y + panel->h)
        {
            break;
        }

        Rectangle fila = {(float)(panel->x + 6), (float)y, (float)(panel->w - 12), (float)(row_h - 2)};
        gui_draw_list_row_bg(fila, row, CheckCollisionPointRec(GetMousePosition(), fila));
        if (CheckCollisionPointRec(GetMousePosition(), fila) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            EndScissorMode();
            return rows[i].id;
        }

        gui_text(TextFormat("%3d", rows[i].id), (float)(panel->x + 12), (float)(y + 7), 18.0f, (Color){220, 238, 225, 255});
        gui_text(rows[i].nombre, (float)(panel->x + 78), (float)(y + 7), 18.0f, (Color){233, 247, 236, 255});
    }
    EndScissorMode();

    return 0;
}

static int seleccionar_cancha_gui(const char *titulo, const char *ayuda, int *id_out)
{
    CanchaRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 34;
    CanchaGuiPanel panel = {80, 130, 1100, 520, 1};

    if (!id_out)
    {
        return 0;
    }
    *id_out = 0;

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    if (!cargar_canchas_gui_rows(&rows, &count))
    {
        return 0;
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int clicked_id = 0;
        Rectangle area = {0};
        Rectangle content_rect = {0};
        CanchaGuiPanel content_panel = {0};

        calcular_panel_listado_canchas_gui(sw, sh, row_h, &panel);

        content_rect = (Rectangle){(float)panel.x, (float)(panel.y + 32), (float)panel.w, (float)(panel.h - 32)};
        if (content_rect.height < (float)row_h)
        {
            content_rect.height = (float)row_h;
        }
        content_panel.visible_rows = (int)(content_rect.height / (float)row_h);
        if (content_panel.visible_rows < 1)
        {
            content_panel.visible_rows = 1;
        }
        area = content_rect;
        scroll = actualizar_scroll_canchas_gui(scroll, count, content_panel.visible_rows, area);

        content_panel.x = (int)content_rect.x;
        content_panel.y = (int)content_rect.y;
        content_panel.w = (int)content_rect.width;
        content_panel.h = (int)content_rect.height;

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header(titulo, sw);

        gui_draw_list_shell((Rectangle){(float)panel.x, (float)panel.y, (float)panel.w, (float)panel.h},
                            "ID", 12.0f,
                            "NOMBRE", 78.0f);

        clicked_id = dibujar_y_detectar_click_canchas_gui(rows, count, scroll, row_h, &content_panel);
        gui_draw_footer_hint(ayuda, (float)panel.x, sh);
        EndDrawing();

        if (clicked_id > 0)
        {
            *id_out = clicked_id;
            free(rows);
            return 1;
        }

        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            break;
        }
    }

    free(rows);
    return 0;
}

static int guardar_nueva_cancha_gui(const char *nombre, int *id_out)
{
    sqlite3_stmt *stmt = NULL;
    long long id = obtener_siguiente_id("cancha");

    if (!preparar_stmt(&stmt, "INSERT INTO cancha(id, nombre) VALUES(?, ?)"))
    {
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Error al crear cancha nombre=%.180s", nombre);
        app_log_event("CANCHA", log_msg);
        return 0;
    }

    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Creada cancha id=%lld nombre=%.180s", id, nombre);
        app_log_event("CANCHA", log_msg);
    }

    if (id_out)
    {
        *id_out = (int)id;
    }

    return 1;
}

static int crear_cancha_gui(void)
{
    char nombre[128] = {0};
    int cursor = 0;

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = (sw > 900) ? 80 : 20;
        int panel_y = 140;
        int panel_w = sw - panel_x * 2;
        int panel_h = 220;

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header("CREAR CANCHA", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){19, 40, 27, 255});
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){110, 161, 125, 255});

        gui_text("Nombre:", (float)(panel_x + 24), (float)(panel_y + 36), 18.0f, (Color){233, 247, 236, 255});

        Rectangle input_rect = {(float)(panel_x + 24), (float)(panel_y + 70), (float)(panel_w - 48), 36.0f};
        DrawRectangleRec(input_rect, (Color){18, 36, 28, 255});
        DrawRectangleLines((int)input_rect.x,
                           (int)input_rect.y,
                           (int)input_rect.width,
                           (int)input_rect.height,
                           (Color){55, 100, 72, 255});
        gui_text(nombre, input_rect.x + 8.0f, input_rect.y + 8.0f, 18.0f, (Color){233, 247, 236, 255});

        gui_draw_footer_hint("ENTER: Guardar | ESC: Volver", (float)panel_x, sh);
        EndDrawing();

        cancha_gui_append_printable_ascii(nombre, &cursor, sizeof(nombre));
        cancha_gui_handle_backspace(nombre, &cursor);

        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 1;
        }

        if (IsKeyPressed(KEY_ENTER))
        {
            int id_nueva_cancha = 0;
            input_consume_key(KEY_ENTER);
            trim_whitespace(nombre);

            if (nombre[0] != '\0' && guardar_nueva_cancha_gui(nombre, &id_nueva_cancha))
            {
                return 1;
            }
        }
    }

    return 1;
}

static int guardar_nombre_editado_cancha_gui(int id, const char *nombre_nuevo)
{
    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "UPDATE cancha SET nombre=? WHERE id=?"))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, nombre_nuevo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Editada cancha id=%d nuevo_nombre=%.180s", id, nombre_nuevo);
        app_log_event("CANCHA", log_msg);
    }

    return 1;
}

static int modal_editar_nombre_cancha_gui(int id)
{
    char nombre_nuevo[128] = {0};
    int cursor = 0;

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = (sw > 900) ? 80 : 20;
        int panel_y = 140;
        int panel_w = sw - panel_x * 2;
        int panel_h = 220;

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header(TextFormat("MODIFICAR CANCHA %d", id), sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){19, 40, 27, 255});
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){110, 161, 125, 255});

        gui_text("Nuevo nombre:", (float)(panel_x + 24), (float)(panel_y + 36), 18.0f, (Color){233, 247, 236, 255});

        Rectangle input_rect = {(float)(panel_x + 24), (float)(panel_y + 70), (float)(panel_w - 48), 36.0f};
        DrawRectangleRec(input_rect, (Color){18, 36, 28, 255});
        DrawRectangleLines((int)input_rect.x,
                           (int)input_rect.y,
                           (int)input_rect.width,
                           (int)input_rect.height,
                           (Color){55, 100, 72, 255});
        gui_text(nombre_nuevo, input_rect.x + 8.0f, input_rect.y + 8.0f, 18.0f, (Color){233, 247, 236, 255});

        gui_draw_footer_hint("ENTER: Guardar | ESC: Volver", (float)panel_x, sh);
        EndDrawing();

        cancha_gui_append_printable_ascii(nombre_nuevo, &cursor, sizeof(nombre_nuevo));
        cancha_gui_handle_backspace(nombre_nuevo, &cursor);

        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }

        if (IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ENTER);
            trim_whitespace(nombre_nuevo);
            if (nombre_nuevo[0] != '\0')
            {
                guardar_nombre_editado_cancha_gui(id, nombre_nuevo);
                return 1;
            }
        }
    }

    return 1;
}

static int modificar_cancha_gui(void)
{
    int id = 0;
    if (!seleccionar_cancha_gui("MODIFICAR CANCHA", "Click para seleccionar | ESC/Enter: volver", &id))
    {
        return 1;
    }

    if (id <= 0)
    {
        return 1;
    }

    return modal_editar_nombre_cancha_gui(id);
}

static int eliminar_cancha_por_id_gui(int id)
{
    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "DELETE FROM cancha WHERE id = ?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Eliminada cancha id=%d", id);
        app_log_event("CANCHA", log_msg);
    }

    return 1;
}

static int modal_confirmar_eliminar_cancha_gui(int id)
{
    input_consume_key(KEY_Y);
    input_consume_key(KEY_N);
    input_consume_key(KEY_ESCAPE);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header("CONFIRMAR ELIMINACION", sw);
        gui_text(TextFormat("Eliminar cancha %d?", id), 80.0f, 140.0f, 28.0f, (Color){241, 252, 244, 255});
        gui_draw_footer_hint("Y: confirmar | N o ESC: cancelar", 80.0f, sh);
        EndDrawing();

        if (IsKeyPressed(KEY_Y))
        {
            input_consume_key(KEY_Y);
            return eliminar_cancha_por_id_gui(id);
        }

        if (IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_N);
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static int eliminar_cancha_gui(void)
{
    int id = 0;
    if (!seleccionar_cancha_gui("ELIMINAR CANCHA", "Click para eliminar | ESC/Enter: volver", &id))
    {
        return 1;
    }

    if (id <= 0)
    {
        return 1;
    }

    if (!modal_confirmar_eliminar_cancha_gui(id))
    {
        return 1;
    }

    return 1;
}

static int abrir_imagen_cancha_por_id_gui(int id)
{
    char ruta_absoluta[1200] = {0};
    if (!construir_ruta_absoluta_imagen_cancha_por_id(id, ruta_absoluta, sizeof(ruta_absoluta)))
    {
        return 0;
    }

    return abrir_imagen_en_sistema(ruta_absoluta);
}

static int cargar_imagen_cancha_gui(void)
{
    int id = 0;
    if (!seleccionar_cancha_gui("CARGAR IMAGEN DE CANCHA", "Click para cargar imagen | ESC/Enter: volver", &id))
    {
        return 1;
    }

    if (id > 0)
    {
        cargar_imagen_para_cancha_id(id);
    }

    return 1;
}

static int ver_imagen_cancha_gui(void)
{
    int id = 0;
    if (!seleccionar_cancha_gui("VER IMAGEN DE CANCHA", "Click para abrir imagen | ESC/Enter: volver", &id))
    {
        return 1;
    }

    if (id > 0)
    {
        abrir_imagen_cancha_por_id_gui(id);
    }

    return 1;
}

void cargar_imagen_cancha()
{
    if (menu_is_gui_enabled())
    {
        if (cargar_imagen_cancha_gui())
            return;
    }

    mostrar_pantalla("CARGAR IMAGEN DE CANCHA");

    if (!hay_registros("cancha"))
    {
        mostrar_no_hay_registros("canchas");
        pause_console();
        return;
    }

    listar_canchas();
    int id = input_int("\nID de cancha (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("cancha", id))
    {
        printf("ID inexistente.\n");
        pause_console();
        return;
    }

    if (!cargar_imagen_para_cancha_id(id))
    {
        printf("No se pudo completar la carga de imagen.\n");
    }

    pause_console();
}

void ver_imagen_cancha()
{
    if (menu_is_gui_enabled())
    {
        if (ver_imagen_cancha_gui())
            return;
    }

    mostrar_pantalla("VER IMAGEN DE CANCHA");

    if (!hay_registros("cancha"))
    {
        mostrar_no_hay_registros("canchas");
        pause_console();
        return;
    }

    listar_canchas();
    int id = input_int("\nID de cancha (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("cancha", id))
    {
        printf("ID inexistente.\n");
        pause_console();
        return;
    }

    char ruta_absoluta[1200] = {0};
    if (!construir_ruta_absoluta_imagen_cancha_por_id(id, ruta_absoluta, sizeof(ruta_absoluta)))
    {
        printf("No se encontro imagen cargada para esa cancha.\n");
        pause_console();
        return;
    }

    if (!abrir_imagen_en_sistema(ruta_absoluta))
    {
        printf("No se pudo abrir la imagen en el sistema.\n");
        pause_console();
        return;
    }

    printf("Abriendo imagen...\n");
    pause_console();
}



/**
 * @brief Crea una nueva cancha en la base de datos
 *
 * Permite a los usuarios agregar canchas para asignacion en partidos,
 * reutilizando IDs eliminados para mantener la secuencia.
 */
void crear_cancha()
{
    if (menu_is_gui_enabled())
    {
        if (crear_cancha_gui())
            return;
    }

    char nombre[100];
    solicitar_nombre_cancha("Nombre de la cancha: ", nombre, sizeof(nombre));

    long long id = obtener_siguiente_id("cancha");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "INSERT INTO cancha(id, nombre) VALUES(?, ?)") )
    {
        printf("Error al crear la cancha.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, (int)id);
    sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Creada cancha id=%lld nombre=%.180s", id, nombre);
        app_log_event("CANCHA", log_msg);
        if (confirmar("Desea cargar imagen para esta cancha ahora?") &&
                !cargar_imagen_para_cancha_id((int)id))
        {
            printf("No se pudo cargar la imagen en este momento.\n");
        }
        printf("Cancha creada correctamente\n");
    }
    else
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Error al crear cancha nombre=%.180s", nombre);
        app_log_event("CANCHA", log_msg);
        printf("Error al crear la cancha.\n");
    }

    pause_console();
}

/**
 * @brief Muestra un listado de todas las canchas registradas
 *
 * Proporciona visibilidad de canchas disponibles para seleccion
 * en partidos y operaciones de gestion.
 */
void listar_canchas()
{
    app_log_event("CANCHA", "Listado de canchas consultado");

    if (menu_is_gui_enabled())
    {
        if (listar_canchas_gui())
            return;
    }

    listar_entidades("cancha", "LISTADO DE CANCHAS", "No hay canchas cargadas.");
}

/**
 * @brief Elimina una cancha de la base de datos
 *
 * Permite remover canchas obsoletas mientras mantiene integridad
 * de datos con validaciones y confirmaciones de usuario.
 */
void eliminar_cancha()
{
    if (menu_is_gui_enabled())
    {
        if (eliminar_cancha_gui())
            return;
    }

    mostrar_pantalla("ELIMINAR CANCHA");

    if (!hay_registros("cancha"))
    {
        mostrar_no_hay_registros("canchas");
        pause_console();
        return;
    }

    listar_canchas();
    printf("\n");

    int id = input_int("ID Cancha a Eliminar (0 para cancelar): ");

    if (!existe_id("cancha", id))
    {
        mostrar_no_existe("cancha");
        return;
    }

    if (!confirmar("Seguro que desea eliminar esta cancha?"))
        return;

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "DELETE FROM cancha WHERE id = ?"))
    {
        printf("Error al eliminar la cancha.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("Cancha Eliminada Correctamente\n");
    pause_console();
}

/**
 * @brief Permite modificar el nombre de una cancha existente
 *
 * Permite correcciones en nombres de canchas sin necesidad
 * de eliminar y recrear registros, mejorando usabilidad.
 */
void modificar_cancha()
{
    if (menu_is_gui_enabled())
    {
        if (modificar_cancha_gui())
            return;
    }

    mostrar_pantalla("MODIFICAR CANCHA");

    if (!hay_registros("cancha"))
    {
        mostrar_no_hay_registros("canchas");
        pause_console();
        return;
    }

    listar_canchas();
    printf("\n");

    int id = input_int("ID Cancha a Modificar (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("cancha", id))
    {
        mostrar_no_existe("cancha");
        return;
    }

    char nombre[100];
    solicitar_nombre_cancha("Nuevo nombre de la cancha: ", nombre, sizeof(nombre));

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "UPDATE cancha SET nombre = ? WHERE id = ?"))
    {
        printf("Error al modificar la cancha.\n");
        pause_console();
        return;
    }

    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("Cancha Modificada Correctamente\n");
    pause_console();
}

/**
 * @brief Muestra el menu principal de gestion de canchas
 *
 * Proporciona interfaz centralizada para operaciones CRUD de canchas,
 * facilitando la navegacion y delegacion de tareas especificas.
 */
void menu_canchas()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_cancha},
        {2, "Listar", listar_canchas},
        {3, "Modificar", modificar_cancha},
        {4, "Eliminar", eliminar_cancha},
        {5, "Cargar Imagen", cargar_imagen_cancha},
        {6, "Ver Imagen", ver_imagen_cancha},
        {0, "Volver", NULL}
    };

    ejecutar_menu_estandar("CANCHAS", items, 7);
}
