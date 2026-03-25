#include "cancha.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#ifdef ENABLE_RAYLIB_GUI
#include "raylib.h"
#endif
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

#ifdef ENABLE_RAYLIB_GUI
static int listar_canchas_gui(void)
{
    sqlite3_stmt *stmt = NULL;
    int cap = 32;
    int count = 0;
    int scroll = 0;
    int row_h = 34;
    int panel_x = 80;
    int panel_y = 130;
    int panel_w = 1100;
    int panel_h = 520;
    int visible_rows;

    typedef struct
    {
        int id;
        char nombre[128];
    } CanchaRow;

    CanchaRow *rows = (CanchaRow *)calloc((size_t)cap, sizeof(CanchaRow));
    if (!rows)
        return 0;

    if (!preparar_stmt(&stmt, "SELECT id, nombre FROM cancha ORDER BY id"))
    {
        free(rows);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (count >= cap)
        {
            int new_cap = cap * 2;
            CanchaRow *tmp = (CanchaRow *)realloc(rows, (size_t)new_cap * sizeof(CanchaRow));
            if (!tmp)
            {
                sqlite3_finalize(stmt);
                free(rows);
                return 0;
            }
            /* zero-initialize newly allocated region */
            memset(tmp + cap, 0, (size_t)(new_cap - cap) * sizeof(CanchaRow));
            rows = tmp;
            cap = new_cap;
        }

        rows[count].id = sqlite3_column_int(stmt, 0);
        snprintf(rows[count].nombre,
                 sizeof(rows[count].nombre),
                 "%s",
                 (const char *)(sqlite3_column_text(stmt, 1) ? sqlite3_column_text(stmt, 1) : (const unsigned char *)"(sin nombre)"));
        count++;
    }
    sqlite3_finalize(stmt);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        float wheel;
        Rectangle area;

        panel_x = sw > 900 ? 80 : 20;
        panel_y = 130;
        panel_w = sw - panel_x * 2;
        panel_h = sh - 220;
        if (panel_h < 200)
            panel_h = 200;

        visible_rows = panel_h / row_h;
        if (visible_rows < 1)
            visible_rows = 1;

        area = (Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h};
        wheel = GetMouseWheelMove();
        if (CheckCollisionPointRec(GetMousePosition(), area))
        {
            if (wheel < 0.0f && scroll < count - visible_rows)
                scroll++;
            else if (wheel > 0.0f && scroll > 0)
                scroll--;
        }

        if (scroll < 0)
            scroll = 0;
        if (scroll > count - visible_rows)
            scroll = count - visible_rows;
        if (scroll < 0)
            scroll = 0;

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});

        DrawRectangle(0, 0, sw, 84, (Color){17, 54, 33, 255});
        DrawText("MiFutbolC", 26, 20, 36, (Color){241, 252, 244, 255});
        DrawText("LISTADO DE CANCHAS", 230, 34, 20, (Color){198, 230, 205, 255});

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){19, 40, 27, 255});
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){110, 161, 125, 255});

        if (count == 0)
        {
            DrawText("No hay canchas cargadas.", panel_x + 24, panel_y + 24, 24, (Color){233, 247, 236, 255});
        }
        else
        {
            BeginScissorMode(panel_x, panel_y, panel_w, panel_h);
            for (int i = scroll; i < count; i++)
            {
                int row = i - scroll;
                int y = panel_y + row * row_h;
                if (y + row_h > panel_y + panel_h)
                    break;

                DrawText(TextFormat("%3d", rows[i].id), panel_x + 10, y + 7, 18, (Color){220, 238, 225, 255});
                DrawText(rows[i].nombre, panel_x + 70, y + 7, 18, (Color){233, 247, 236, 255});
            }
            EndScissorMode();
        }

        DrawText("Rueda: scroll | ESC/Enter: volver", panel_x, sh - 62, 18, (Color){178, 214, 188, 255});
        EndDrawing();

        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
            break;
    }

    free(rows);
    return 1;
}
#endif

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

void cargar_imagen_cancha()
{
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

#ifdef ENABLE_RAYLIB_GUI
    if (menu_is_gui_enabled())
    {
        if (listar_canchas_gui())
            return;
    }
#endif

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

    ejecutar_menu("CANCHAS", items, 7);
}
