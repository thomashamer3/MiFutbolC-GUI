#include "camiseta.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <process.h>
#include <io.h>
#else
#include "process.h"
#include <strings.h>
#endif
#include <ctype.h>
#include <limits.h>


#define MAX_CAMISETAS_SORTEO 150

#include "raylib.h"
#include "gui_components.h"
#include "input.h"

/* Prototipos para funciones definidas más abajo (evita implicit-declaration) */
static int listar_camisetas_gui(void);
static int crear_camiseta_gui(void);
static int editar_camiseta_gui(void);
static int eliminar_camiseta_gui(void);
static int cargar_imagen_camiseta_gui(void);
static int ver_imagen_camiseta_gui(void);
static void configurar_visor_preferido_imagen_gui(void);

static int sortear_camiseta_gui(void);

/* Prototipos de helpers usados por GUI antes de su definición */
static void marcar_camiseta_sorteada(int id);
static char* obtener_nombre_camiseta(int id);

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql);
static void listar_camisetas_simple(void);
static int cargar_imagen_para_camiseta_id(int id);

static void asegurar_fila_settings()
{
    sqlite3_exec(db,
                 "INSERT OR IGNORE INTO settings(id, theme, language, mode, text_size, image_viewer) "
                 "VALUES(1, 0, 0, 0, 1, '');",
                 NULL,
                 NULL,
                 NULL);
}

/* Convierte una cadena asumida en ISO-8859-1 (Latin1) a UTF-8 en el buffer destino.
    Solo se necesita en las rutas GUI (cuando la app corre con Raylib).
 */
static void latin1_to_utf8(const unsigned char *src, char *dst, size_t dst_sz)
{
    if (!dst || dst_sz == 0)
        return;

    if (!src)
    {
        if (dst_sz > 0)
        {
            dst[0] = '\0';
        }
        return;
    }

    size_t di = 0;
    const unsigned char *s = src;
    while (*s && di + 1 < dst_sz)
    {
        unsigned char c = *s++;
        if (c < 0x80)
        {
            dst[di++] = (char)c;
        }
        else
        {
            /* mapear Latin1 single-byte a UTF-8 de 2 bytes */
            if (di + 2 < dst_sz)
            {
                dst[di++] = (char)(0xC0 | (c >> 6));
                dst[di++] = (char)(0x80 | (c & 0x3F));
            }
            else
            {
                break;
            }
        }
    }
    dst[di] = '\0';
}

static int obtener_visor_preferido(char *buffer, size_t size)
{
    if (!buffer || size == 0)
    {
        return 0;
    }

    asegurar_fila_settings();

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT image_viewer FROM settings WHERE id = 1"))
    {
        return 0;
    }

    int ok = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *v = sqlite3_column_text(stmt, 0);
        if (v && strncpy_s(buffer, size, (const char *)v, _TRUNCATE) == 0)
        {
            trim_whitespace(buffer);
            ok = 1;
        }
    }

    sqlite3_finalize(stmt);
    return ok;
}

static int guardar_visor_preferido(const char *viewer)
{
    asegurar_fila_settings();

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "UPDATE settings SET image_viewer = ? WHERE id = 1"))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, viewer ? viewer : "", -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

static int instalar_paquete_linux(const char *package_name)
{
#ifdef _WIN32
    (void)package_name;
    return 0;
#else
    if (!package_name || package_name[0] == '\0')
    {
        return 0;
    }

    char install_cmd[512] = {0};
    if (app_command_exists("apt-get"))
    {
        snprintf(install_cmd, sizeof(install_cmd), "sudo apt-get update && sudo apt-get install -y %s", package_name);
    }
    else if (app_command_exists("dnf"))
    {
        snprintf(install_cmd, sizeof(install_cmd), "sudo dnf install -y %s", package_name);
    }
    else if (app_command_exists("pacman"))
    {
        snprintf(install_cmd, sizeof(install_cmd), "sudo pacman -Sy --noconfirm %s", package_name);
    }
    else if (app_command_exists("zypper"))
    {
        snprintf(install_cmd, sizeof(install_cmd), "sudo zypper --non-interactive install %s", package_name);
    }
    else
    {
        return 0;
    }

    return system(install_cmd) == 0;
#endif
}

static int construir_ruta_absoluta_imagen_por_id(int id, char *ruta_absoluta, size_t size)
{
    if (!ruta_absoluta || size == 0)
    {
        return 0;
    }

    char ruta_db[300] = {0};
    if (!db_get_image_path_by_id("camiseta", id, ruta_db, sizeof(ruta_db)))
    {
        return 0;
    }

    return db_resolve_image_absolute_path(ruta_db, ruta_absoluta, size);
}


static int abrir_imagen_en_sistema(const char *ruta)
{
    if (!ruta || ruta[0] == '\0')
    {
        return 0;
    }

#ifdef _WIN32
    char viewer[64] = {0};
    obtener_visor_preferido(viewer, sizeof(viewer));

    char cmd[1200];
    if (viewer[0] != '\0' && _stricmp(viewer, "auto") != 0)
    {
        if (_stricmp(viewer, "mspaint") == 0)
        {
            snprintf(cmd, sizeof(cmd), "mspaint \"%s\"", ruta);
            if (system(cmd) == 0)
            {
                return 1;
            }
        }
        else
        {
            printf("Visor preferido no soportado en Windows: %s\n", viewer);
        }
    }

    snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", ruta);
    if (system(cmd) == 0)
    {
        return 1;
    }

    snprintf(cmd, sizeof(cmd), "mspaint \"%s\"", ruta);
    return system(cmd) == 0;
#else
    char viewer[64] = {0};
    obtener_visor_preferido(viewer, sizeof(viewer));

    char cmd_open[1400];

    const char *visores[] = {"xdg-open", "gio", "feh", "eog", "gwenview", NULL};
    const char *visor = NULL;

    if (viewer[0] != '\0' && strcmp(viewer, "auto") != 0)
    {
        if (app_command_exists(viewer))
        {
            visor = viewer;
        }
        else
        {
            printf("El visor preferido '%s' no esta instalado.\n", viewer);
        }
    }

    for (int i = 0; visores[i] != NULL; i++)
    {
        if (visor)
        {
            break;
        }

        if (app_command_exists(visores[i]))
        {
            visor = visores[i];
            break;
        }
    }

    if (!visor)
    {
        if (!confirmar("No se detecto visor de imagen. Desea instalar 'feh' automaticamente?"))
        {
            return 0;
        }

        printf("Instalando visor liviano 'feh'...\n");
        if (!instalar_paquete_linux("feh"))
        {
            printf("No se pudo instalar 'feh'.\n");
            return 0;
        }

        visor = "feh";
        if (viewer[0] == '\0' || strcmp(viewer, "auto") == 0)
        {
            guardar_visor_preferido("feh");
        }
    }

    if (strcmp(visor, "gio") == 0)
    {
        snprintf(cmd_open, sizeof(cmd_open), "gio open \"%s\" >/dev/null 2>&1", ruta);
    }
    else
    {
        snprintf(cmd_open, sizeof(cmd_open), "%s \"%s\" >/dev/null 2>&1", visor, ruta);
    }

    return system(cmd_open) == 0;
#endif
}

static void configurar_visor_preferido_imagen()
{
    clear_screen();
    print_header("CONFIGURAR VISOR DE IMAGEN");

    char actual[64] = {0};
    obtener_visor_preferido(actual, sizeof(actual));
    printf("Visor actual: %s\n\n", actual[0] ? actual : "auto");

    printf("Escribe el visor a usar o 'auto'.\n");
#ifdef _WIN32
    printf("Opciones recomendadas: auto, mspaint\n");
#else
    printf("Opciones recomendadas: auto, xdg-open, gio, feh, eog, gwenview\n");
#endif

    char nuevo[64] = {0};
    input_string("Visor: ", nuevo, (int)sizeof(nuevo));
    trim_whitespace(nuevo);

    if (nuevo[0] == '\0')
    {
        printf("No se realizaron cambios.\n");
        pause_console();
        return;
    }

#ifdef _WIN32
    if (_stricmp(nuevo, "auto") != 0 && _stricmp(nuevo, "mspaint") != 0)
    {
        printf("Visor no soportado en Windows. Usa 'auto' o 'mspaint'.\n");
        pause_console();
        return;
    }
#else
    if (strcasecmp(nuevo, "auto") != 0 && !app_command_exists(nuevo))
    {
        printf("No se encontro ese comando en el sistema.\n");
        pause_console();
        return;
    }
#endif

    const char *valor =
#ifdef _WIN32
        (_stricmp(nuevo, "auto") == 0) ? "" : nuevo;
#else
        (strcasecmp(nuevo, "auto") == 0) ? "" : nuevo;
#endif

    if (!guardar_visor_preferido(valor))
    {
        printf("No se pudo guardar la configuracion del visor.\n");
        pause_console();
        return;
    }

    printf("Visor guardado: %s\n", valor[0] ? valor : "auto");
    pause_console();
}

static int pedir_imagen_camiseta_y_resolver_ruta(char *ruta_absoluta, size_t size)
{
    if (!hay_registros("camiseta"))
    {
        mostrar_no_hay_registros("camisetas");
        return 0;
    }

    listar_camisetas_simple();
    int id = input_int("\nID de camiseta (0 para cancelar): ");
    if (id == 0)
    {
        return 0;
    }

    if (!existe_id("camiseta", id))
    {
        printf("ID inexistente.\n");
        return 0;
    }

    if (!construir_ruta_absoluta_imagen_por_id(id, ruta_absoluta, size))
    {
        printf("No se encontro imagen cargada en disco para esa camiseta.\n");
        return 0;
    }

    return 1;
}

static void previsualizar_imagen_camiseta_consola()
{
    clear_screen();
    print_header("PREVISUALIZAR IMAGEN (CONSOLA)");

    char ruta_absoluta[1200] = {0};
    if (!pedir_imagen_camiseta_y_resolver_ruta(ruta_absoluta, sizeof(ruta_absoluta)))
    {
        pause_console();
        return;
    }

    if (!app_command_exists("chafa"))
    {
        if (!confirmar("No se detecto 'chafa'. Desea instalarlo automaticamente?"))
        {
            pause_console();
            return;
        }

        printf("Instalando 'chafa'...\n");
        if (!instalar_paquete_linux("chafa"))
        {
            printf("No se pudo instalar 'chafa'.\n");
            pause_console();
            return;
        }
    }

    char cmd[1400];
    snprintf(cmd, sizeof(cmd), "chafa --size 80x40 \"%s\"", ruta_absoluta);

    if (system(cmd) != 0)
    {
        printf("No se pudo previsualizar con chafa.\n");
        pause_console();
        return;
    }

    pause_console();
}

static void probar_visor_imagen_actual()
{
    clear_screen();
    print_header("PROBAR VISOR DE IMAGEN");

    char ruta_absoluta[1200] = {0};
    if (!pedir_imagen_camiseta_y_resolver_ruta(ruta_absoluta, sizeof(ruta_absoluta)))
    {
        pause_console();
        return;
    }

    if (!abrir_imagen_en_sistema(ruta_absoluta))
    {
        printf("No se pudo abrir la imagen con el visor actual.\n");
        pause_console();
        return;
    }

    printf("Visor ejecutado correctamente.\n");
    pause_console();
}

static void menu_ajustes_imagen_camiseta()
{
    if (menu_is_gui_enabled())
    {
        configurar_visor_preferido_imagen_gui();
        return;
    }
    MenuItem items[] =
    {
        {1, "Configurar visor", configurar_visor_preferido_imagen},
        {2, "Probar visor", probar_visor_imagen_actual},
        {3, "Previsualizar en consola", previsualizar_imagen_camiseta_consola},
        {0, "Volver", NULL}
    };
    ejecutar_menu_estandar("AJUSTES IMAGEN", items, 4);
}

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

static int obtener_total(const char *sql)
{
    sqlite3_stmt *stmt;

    if (!preparar_stmt(&stmt, sql))
    {
        return 0;
    }

    sqlite3_step(stmt);
    int total = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return total;
}

static void solicitar_nombre_camiseta(const char *prompt, char *buffer, int size)
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

static void listar_camisetas_simple()
{
    sqlite3_stmt *stmt;

    if (!preparar_stmt(&stmt, "SELECT id, nombre FROM camiseta"))
    {
        printf("Error al consultar la base de datos.\n");
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ui_printf_centered_line("%d - %s",
                                sqlite3_column_int(stmt, 0),
                                sqlite3_column_text(stmt, 1));
        hay = 1;
    }

    if (!hay)
        mostrar_no_hay_registros("camisetas cargadas");

    sqlite3_finalize(stmt);
}

/**
 * @brief Crea una nueva camiseta en la base de datos
 *
 * Permite a los usuarios agregar camisetas para gestion y sorteos,
 * reutilizando IDs eliminados para mantener la secuencia.
 */
void crear_camiseta()
{
    if (menu_is_gui_enabled())
    {
        if (crear_camiseta_gui())
            return;
    }
    clear_screen();
    char nombre[50];
    solicitar_nombre_camiseta("Nombre y Numero: ", nombre, sizeof(nombre));

    long long id = obtener_siguiente_id("camiseta");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "INSERT INTO camiseta(id, nombre) VALUES(?, ?)"))
    {
        printf("Error al crear la camiseta.\n");
        pause_console();
        return;
    }
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Creada camiseta id=%lld nombre=%.180s", id, nombre);
        app_log_event("CAMISETA", log_msg);

        int desea_cargar_imagen = confirmar("Desea cargar imagen para esta camiseta ahora?");
        if (desea_cargar_imagen)
        {
            if (!cargar_imagen_para_camiseta_id((int)id))
            {
                printf("No se pudo cargar la imagen en este momento.\n");
                snprintf(log_msg, sizeof(log_msg), "Camiseta id=%lld creada, pero fallo carga de imagen inicial", id);
                app_log_event("CAMISETA", log_msg);
            }
            else
            {
                snprintf(log_msg, sizeof(log_msg), "Camiseta id=%lld creada con imagen inicial", id);
                app_log_event("CAMISETA", log_msg);
            }
        }
        else
        {
            snprintf(log_msg, sizeof(log_msg), "Camiseta id=%lld creada sin imagen inicial (opcional)", id);
            app_log_event("CAMISETA", log_msg);
        }
    }
    else
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Error al crear camiseta nombre=%.180s", nombre);
        app_log_event("CAMISETA", log_msg);
    }

    pause_console();
}

/**
 * @brief Muestra un listado de todas las camisetas registradas
 *
 * Proporciona visibilidad a los usuarios de las camisetas disponibles
 * para facilitar la toma de decisiones en otras operaciones.
 */
static int cargar_imagen_para_camiseta_id(int id)
{
    return app_cargar_imagen_entidad(id, "camiseta", "mifutbol_imagen_sel.txt");
}

void listar_camisetas()
{
    if (menu_is_gui_enabled())
    {
        if (listar_camisetas_gui())
            return;
    }
    clear_screen();
    print_header("LISTADO DE CAMISETAS");

    app_log_event("CAMISETA", "Listado de camisetas consultado");

    listar_camisetas_simple();
    pause_console();
}

/**
 * @brief Permite editar el nombre de una camiseta existente
 *
 * Permite correcciones en la informacion de camisetas sin necesidad
 * de eliminar y recrear registros, mejorando la usabilidad.
 */
void editar_camiseta()
{
    if (menu_is_gui_enabled())
    {
        if (editar_camiseta_gui())
            return;
    }
    clear_screen();
    print_header("EDITAR CAMISETA");

    if (!hay_registros("camiseta"))
    {
        mostrar_no_hay_registros("camisetas para editar");
        pause_console();
        return;
    }

    ui_printf_centered_line("Camisetas disponibles:");
    ui_printf("\n");
    listar_camisetas_simple();

    int id = input_int("\nID a editar (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("camiseta", id))
    {
        printf("ID inexistente\n");
        pause_console();
        return;
    }

    char nombre[100];
    solicitar_nombre_camiseta("Nuevo nombre: ", nombre, sizeof(nombre));

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "UPDATE camiseta SET nombre=? WHERE id=?"))
    {
        printf("Error al actualizar la camiseta.\n");
        pause_console();
        return;
    }

    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Editada camiseta id=%d nuevo_nombre=%.180s", id, nombre);
    app_log_event("CAMISETA", log_msg);

    printf("\nCamiseta actualizada correctamente\n");
    pause_console();
}

void cargar_imagen_camiseta()
{
    if (menu_is_gui_enabled())
    {
        if (cargar_imagen_camiseta_gui())
            return;
    }
    clear_screen();
    print_header("CARGAR IMAGEN DE CAMISETA");

    if (!hay_registros("camiseta"))
    {
        mostrar_no_hay_registros("camisetas");
        pause_console();
        return;
    }

    listar_camisetas_simple();
    int id = input_int("\nID de camiseta (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("camiseta", id))
    {
        printf("ID inexistente.\n");
        pause_console();
        return;
    }

    if (!cargar_imagen_para_camiseta_id(id))
    {
        printf("No se pudo completar la carga de imagen.\n");
    }

    pause_console();
}

void ver_imagen_camiseta()
{
    if (menu_is_gui_enabled())
    {
        if (ver_imagen_camiseta_gui())
            return;
    }
    clear_screen();
    print_header("VER IMAGEN DE CAMISETA");

    char ruta_absoluta[1200] = {0};
    if (!pedir_imagen_camiseta_y_resolver_ruta(ruta_absoluta, sizeof(ruta_absoluta)))
    {
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
 * @brief Elimina una camiseta de la base de datos
 *
 * Permite remover camisetas que ya no son necesarias mientras
 * mantiene la integridad de los datos con validaciones y confirmaciones.
 */
void eliminar_camiseta()
{
    if (menu_is_gui_enabled())
    {
        if (eliminar_camiseta_gui())
            return;
    }
    clear_screen();
    print_header("ELIMINAR CAMISETA");

    if (!hay_registros("camiseta"))
    {
        mostrar_no_hay_registros("camisetas para eliminar");
        pause_console();
        return;
    }

    ui_printf_centered_line("Camisetas disponibles:");
    int id = input_int("\nID a eliminar (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("camiseta", id))
    {
        printf("ID inexistente\n");
        pause_console();
        return;
    }

    if (!confirmar("Seguro que desea eliminar esta camiseta?"))
        return;

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "DELETE FROM camiseta WHERE id=?"))
    {
        printf("Error al eliminar la camiseta.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Eliminada camiseta id=%d", id);
    app_log_event("CAMISETA", log_msg);

    printf("\nCamiseta eliminada correctamente\n");
    pause_console();
}

/**
 * @brief Reinicia el estado de sorteo de todas las camisetas
 *
 * Necesario cuando todas las camisetas han sido sorteadas para permitir
 * nuevos sorteos sin requerir recrear registros.
 */
static void reiniciar_sorteo()
{
    sqlite3_exec(db, "UPDATE camiseta SET sorteada = 0", 0, 0, 0);
    printf("Todas las camisetas han sido sorteadas. Reiniciando sorteo...\n\n");
}

/**
 * @brief Obtiene la lista de IDs de camisetas disponibles para sorteo
 *
 * @param ids Array donde almacenar los IDs
 * @param max Tamano maximo del array
 * @return Numero de IDs obtenidos
 */
static int obtener_ids_disponibles(int ids[], int max)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT id FROM camiseta WHERE sorteada = 0"))
    {
        return 0;
    }

    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < max)
    {
        ids[i++] = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return i;
}

/**
 * @brief Selecciona aleatoriamente un ID de la lista proporcionada
 *
 * @param ids Array de IDs disponibles
 * @param count Numero de IDs en el array
 * @return ID seleccionado aleatoriamente
 */
static int seleccionar_id_aleatorio(const int ids[], int count)
{
    static int seeded = 0;
    if (!seeded)
    {
        srand((unsigned int)(time(NULL) ^ _getpid()));
        seeded = 1;
    }

    if (count <= 0)
    {
        return -1;
    }

    return ids[rand() % count];
}

typedef struct
{
    int id;
    char nombre[256];
} CamisetaGuiRow;

static int cargar_camisetas_gui_rows(CamisetaGuiRow **rows_out, int *count_out)
{
    sqlite3_stmt *stmt = NULL;
    int cap = 64;
    int count = 0;
    CamisetaGuiRow *rows = (CamisetaGuiRow *)calloc((size_t)cap, sizeof(CamisetaGuiRow));

    if (!rows)
    {
        return 0;
    }

    if (!preparar_stmt(&stmt, "SELECT id, nombre FROM camiseta ORDER BY id"))
    {
        free(rows);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (count >= cap)
        {
            int new_cap = cap * 2;
            CamisetaGuiRow *tmp = (CamisetaGuiRow *)realloc(rows, (size_t)new_cap * sizeof(CamisetaGuiRow));
            if (!tmp)
            {
                sqlite3_finalize(stmt);
                free(rows);
                return 0;
            }
            memset(tmp + cap, 0, (size_t)(new_cap - cap) * sizeof(CamisetaGuiRow));
            rows = tmp;
            cap = new_cap;
        }

        rows[count].id = sqlite3_column_int(stmt, 0);
        {
            const unsigned char *col = sqlite3_column_text(stmt, 1);
            if (col)
            {
                latin1_to_utf8(col, rows[count].nombre, sizeof(rows[count].nombre));
            }
            else
            {
                snprintf(rows[count].nombre, sizeof(rows[count].nombre), "%s", "(sin nombre)");
            }
        }
        count++;
    }

    sqlite3_finalize(stmt);
    *rows_out = rows;
    *count_out = count;
    return 1;
}

static void calcular_panel_listado(int sw, int sh, int *panel_x, int *panel_w, int *panel_h)
{
    *panel_x = (sw > 900) ? 80 : 20;
    *panel_w = sw - (*panel_x * 2);
    *panel_h = sh - 220;
    if (*panel_h < 200)
    {
        *panel_h = 200;
    }
}

static int clamp_scroll_listado(int scroll, int count, int visible_rows)
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

static int actualizar_scroll_listado(int scroll, int count, int visible_rows, Rectangle area)
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

    return clamp_scroll_listado(scroll, count, visible_rows);
}

static void dibujar_filas_listado_camisetas(const CamisetaGuiRow *rows,
                                            int count,
                                            int scroll,
                                            int row_h,
                                            Rectangle panel)
{
    int panel_x = (int)panel.x;
    int panel_y = (int)panel.y;
    int panel_w = (int)panel.width;
    int panel_h = (int)panel.height;

    if (count == 0)
    {
        gui_text("No hay camisetas cargadas.", panel_x + 24, panel_y + 24, 24.0f, (Color){233,247,236,255});
        return;
    }

    BeginScissorMode(panel_x, panel_y, panel_w, panel_h);
    for (int i = scroll; i < count; i++)
    {
        int row = i - scroll;
        int y = panel_y + row * row_h;
        if (y + row_h > panel_y + panel_h)
        {
            break;
        }

        {
            Rectangle fr = {(float)(panel_x + 2), (float)y, (float)(panel_w - 4), (float)(row_h - 1)};
            int hovered = CheckCollisionPointRec(GetMousePosition(), fr);
            gui_draw_list_row_bg(fr, row, hovered);
        }

        gui_text(TextFormat("%3d", rows[i].id), panel_x + 12, y + 7, 19.0f, (Color){227,242,232,255});
        gui_text(rows[i].nombre, panel_x + 78, y + 7, 19.0f, (Color){241,252,244,255});
    }
    EndScissorMode();
}

static int listado_camisetas_should_close(void)
{
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
    {
        input_consume_key(KEY_ESCAPE);
        input_consume_key(KEY_ENTER);
        return 1;
    }
    return 0;
}

static int listar_camisetas_gui(void)
{
    CamisetaGuiRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 34;
    const int panel_y = 130;

    /* Consumir posibles pulsaciones previas de ESC/ENTER para evitar que
        el loop modal devuelva un evento que el GUI principal vuelva a
        procesar inmediatamente. */
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    if (!cargar_camisetas_gui_rows(&rows, &count))
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
        calcular_panel_listado(sw, sh, &panel_x, &panel_w, &panel_h);

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

        Rectangle area = (Rectangle){(float)panel_x, (float)(panel_y + 32), (float)panel_w, (float)content_h};
        scroll = actualizar_scroll_listado(scroll, count, visible_rows, area);

        BeginDrawing();
        /* Match Home menu aesthetics */
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("LISTADO DE CAMISETAS", sw);
        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                    "ID", 12.0f,
                    "NOMBRE", 78.0f);

        {
            Rectangle content_area = {(float)panel_x, (float)(panel_y + 32), (float)panel_w, (float)content_h};
            dibujar_filas_listado_camisetas(rows, count, scroll, row_h, content_area);
        }

        gui_draw_footer_hint("Rueda: scroll | ESC/Enter: volver", (float)panel_x, sh);
        EndDrawing();

        if (listado_camisetas_should_close())
        {
            break;
        }
    }

    free(rows);
    return 1;
}

static void gui_append_printable_ascii(char *buffer, int *cursor, size_t buffer_size)
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

static void gui_handle_backspace(char *buffer, size_t buffer_size, int *cursor)
{
    if (!IsKeyPressed(KEY_BACKSPACE))
    {
        return;
    }

    int l = (int)strlen_s(buffer, buffer_size);
    if (l > 0)
    {
        buffer[l - 1] = '\0';
        *cursor = l - 1;
    }
}

static void guardar_nueva_camiseta_gui(const char *nombre)
{
    long long id = obtener_siguiente_id("camiseta");
    sqlite3_stmt *stmt = NULL;

    if (!preparar_stmt(&stmt, "INSERT INTO camiseta(id, nombre) VALUES(?, ?)"))
    {
        return;
    }

    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE)
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Creada camiseta id=%lld nombre=%.180s", id, nombre);
        app_log_event("CAMISETA", log_msg);
    }
}

static int crear_camiseta_gui(void)
{
    char nombre[256] = {0};
    int cursor = 0;

    /* Consumir ESC/ENTER previo al mostrar el modal */
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = sw > 900 ? 80 : 20;
        int panel_y = 140;
        int panel_w = sw - panel_x * 2;
        int panel_h = 220;

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        DrawRectangle(0, 0, sw, 84, (Color){17,54,33,255});
        gui_text("MiFutbolC", 26, 20, 36.0f, (Color){241,252,244,255});
        gui_text("CREAR CAMISETA", 230, 34, 20.0f, (Color){198,230,205,255});

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){19,40,27,255});
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){110,161,125,255});

        gui_text("Nombre:", panel_x + 24, panel_y + 36, 18.0f, (Color){233,247,236,255});

        /* input box */
        Rectangle input_rect = { (float)(panel_x + 24), (float)(panel_y + 70), (float)(panel_w - 48), 36.0f };
        DrawRectangleRec(input_rect, (Color){18,36,28,255});
        DrawRectangleLines((int)input_rect.x, (int)input_rect.y, (int)input_rect.width, (int)input_rect.height, (Color){55,100,72,255});
        gui_text(nombre, input_rect.x + 8, input_rect.y + 8, 18.0f, (Color){233,247,236,255});

        gui_text("ENTER \u2192 Guardar | ESC \u2192 Volver", panel_x, sh - 62, 18.0f, (Color){178,214,188,255});

        EndDrawing();

        gui_append_printable_ascii(nombre, &cursor, sizeof(nombre));
        gui_handle_backspace(nombre, sizeof(nombre), &cursor);

        if (IsKeyPressed(KEY_ESCAPE)) { input_consume_key(KEY_ESCAPE); return 1; }
        if (IsKeyPressed(KEY_ENTER))
        {
            trim_whitespace(nombre);
            if (nombre[0] != '\0')
            {
                guardar_nueva_camiseta_gui(nombre);
                return 1;
            }
        }
    }

    return 1;
}

static int guardar_nombre_editado_camiseta_gui(int id, const char *nombre_nuevo)
{
    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "UPDATE camiseta SET nombre=? WHERE id=?"))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, nombre_nuevo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    {
        char logm[256];
        snprintf(logm, sizeof(logm), "Editada camiseta id=%d nuevo_nombre=%.180s", id, nombre_nuevo);
        app_log_event("CAMISETA", logm);
    }

    return 1;
}

static int modal_editar_nombre_camiseta_gui(int id)
{
    char nombre_nuevo[256] = {0};
    int cursor = 0;

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground((Color){18,36,28,255});
        DrawText(TextFormat("Editar camiseta %d (ENTER guardar)", id), 40, 40, 20, (Color){241,252,244,255});
        DrawText("Nombre:", 40, 100, 18, (Color){198,230,205,255});
        DrawText(nombre_nuevo, 140, 100, 18, (Color){233,247,236,255});
        EndDrawing();

        int key = GetCharPressed();
        while (key > 0)
        {
            if (key >= 32 && key <= 126 && cursor < (int)sizeof(nombre_nuevo) - 1)
            {
                nombre_nuevo[cursor++] = (char)key;
                nombre_nuevo[cursor] = '\0';
            }
            key = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE))
        {
            int l = (int)strlen_s(nombre_nuevo, sizeof(nombre_nuevo));
            if (l > 0)
            {
                nombre_nuevo[l - 1] = '\0';
                cursor = l - 1;
            }
        }

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
                guardar_nombre_editado_camiseta_gui(id, nombre_nuevo);
            }
            return 1;
        }
    }

    return 1;
}

static int dibujar_y_detectar_click_editar_camisetas(const CamisetaGuiRow *rows,
                                                      int count,
                                                      int scroll,
                                                      int row_h,
                                                      Rectangle panel)
{
    int panel_x = (int)panel.x;
    int panel_y = (int)panel.y;
    int panel_w = (int)panel.width;
    int panel_h = (int)panel.height;

    if (count == 0)
    {
        DrawText("No hay camisetas.", panel_x + 24, panel_y + 24, 24, (Color){233,247,236,255});
        return 0;
    }

    BeginScissorMode(panel_x, panel_y, panel_w, panel_h);
    for (int i = scroll; i < count; i++)
    {
        int row = i - scroll;
        int y = panel_y + row * row_h;
        if (y + row_h > panel_y + panel_h)
        {
            break;
        }

        Rectangle r = {(float)(panel_x + 6), (float)y, (float)(panel_w - 12), (float)(row_h - 2)};
        if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            EndScissorMode();
            return rows[i].id;
        }

        DrawText(TextFormat("%3d", rows[i].id), panel_x + 10, y + 7, 18, (Color){220,238,225,255});
        DrawText(rows[i].nombre, panel_x + 70, y + 7, 18, (Color){233,247,236,255});
    }
    EndScissorMode();

    return 0;
}

static int editar_camiseta_gui(void)
{
    CamisetaGuiRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 30;
    const int panel_y = 130;

    if (!cargar_camisetas_gui_rows(&rows, &count))
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
        int visible_rows = 0;
        int clicked_id = 0;
        Rectangle area;

        calcular_panel_listado(sw, sh, &panel_x, &panel_w, &panel_h);
        visible_rows = panel_h / row_h;
        if (visible_rows < 1)
        {
            visible_rows = 1;
        }

        area = (Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h};
        scroll = actualizar_scroll_listado(scroll, count, visible_rows, area);

        BeginDrawing();
        ClearBackground((Color){18,36,28,255});
        DrawText("Selecciona camiseta para editar (clic)", 40, 40, 20, (Color){241,252,244,255});
        DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){19,40,27,255});
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){110,161,125,255});
        clicked_id = dibujar_y_detectar_click_editar_camisetas(rows, count, scroll, row_h, area);
        DrawText("Esc: salir", panel_x, sh - 62, 18, (Color){178,214,188,255});
        EndDrawing();

        if (clicked_id > 0)
        {
            if (modal_editar_nombre_camiseta_gui(clicked_id))
            {
                free(rows);
                return 1;
            }
        }

        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            break;
        }
    }

    free(rows);
    return 1;
}

static int eliminar_camiseta_por_id_gui(int id)
{
    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "DELETE FROM camiseta WHERE id=?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    {
        char logm[256];
        snprintf(logm, sizeof(logm), "Eliminada camiseta id=%d", id);
        app_log_event("CAMISETA", logm);
    }

    return 1;
}

static int modal_confirmar_eliminar_camiseta_gui(int id)
{
    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground((Color){18,36,28,255});
        DrawText(TextFormat("Eliminar camiseta %d? (Y/N)", id), 40, 40, 20, (Color){241,252,244,255});
        EndDrawing();

        if (IsKeyPressed(KEY_Y))
        {
            input_consume_key(KEY_Y);
            return eliminar_camiseta_por_id_gui(id);
        }
        if (IsKeyPressed(KEY_N))
        {
            input_consume_key(KEY_N);
            return 0;
        }
        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static int dibujar_y_detectar_click_eliminar_camisetas(const CamisetaGuiRow *rows,
                                                        int count,
                                                        int scroll,
                                                        int row_h,
                                                        Rectangle panel)
{
    int panel_x = (int)panel.x;
    int panel_y = (int)panel.y;
    int panel_w = (int)panel.width;
    int panel_h = (int)panel.height;

    if (count == 0)
    {
        DrawText("No hay camisetas.", panel_x + 24, panel_y + 24, 24, (Color){233,247,236,255});
        return 0;
    }

    BeginScissorMode(panel_x, panel_y, panel_w, panel_h);
    for (int i = scroll; i < count; i++)
    {
        int row = i - scroll;
        int y = panel_y + row * row_h;
        if (y + row_h > panel_y + panel_h)
        {
            break;
        }

        Rectangle r = {(float)(panel_x + 6), (float)y, (float)(panel_w - 12), (float)(row_h - 2)};
        if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            EndScissorMode();
            return rows[i].id;
        }

        DrawText(TextFormat("%3d", rows[i].id), panel_x + 10, y + 7, 18, (Color){220,238,225,255});
        DrawText(rows[i].nombre, panel_x + 70, y + 7, 18, (Color){233,247,236,255});
    }
    EndScissorMode();

    return 0;
}

static int eliminar_camiseta_gui(void)
{
    CamisetaGuiRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 30;
    const int panel_y = 130;

    /* Consumir ESC/ENTER previo al modal de eliminar */
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    if (!cargar_camisetas_gui_rows(&rows, &count))
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
        int visible_rows = panel_h / row_h;
        int clicked_id = 0;
        Rectangle area;

        calcular_panel_listado(sw, sh, &panel_x, &panel_w, &panel_h);

        visible_rows = panel_h / row_h;
        if (visible_rows < 1)
        {
            visible_rows = 1;
        }

        area = (Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h};
        scroll = actualizar_scroll_listado(scroll, count, visible_rows, area);

        BeginDrawing();
        ClearBackground((Color){18,36,28,255});
        DrawText("Selecciona camiseta para eliminar (clic)", 40, 40, 20, (Color){241,252,244,255});
        DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){19,40,27,255});
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){110,161,125,255});

        clicked_id = dibujar_y_detectar_click_eliminar_camisetas(rows, count, scroll, row_h, area);

        gui_text("Esc: salir", panel_x, GetScreenHeight() - 62, 18.0f, (Color){178,214,188,255});
        EndDrawing();

        if (clicked_id > 0)
        {
            if (modal_confirmar_eliminar_camiseta_gui(clicked_id))
            {
                free(rows);
                return 1;
            }
        }

        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            break;
        }
    }

    free(rows);
    return 1;
}

static int dibujar_y_detectar_click_cargar_imagen_camisetas(const CamisetaGuiRow *rows,
                                                             int count,
                                                             int scroll,
                                                             int row_h,
                                                             Rectangle panel)
{
    int panel_x = (int)panel.x;
    int panel_y = (int)panel.y;
    int panel_w = (int)panel.width;
    int panel_h = (int)panel.height;

    if (count == 0)
    {
        DrawText("No hay camisetas.", panel_x + 24, panel_y + 24, 24, (Color){233,247,236,255});
        return 0;
    }

    BeginScissorMode(panel_x, panel_y, panel_w, panel_h);
    for (int i = scroll; i < count; i++)
    {
        int row = i - scroll;
        int y = panel_y + row * row_h;
        if (y + row_h > panel_y + panel_h)
        {
            break;
        }

        Rectangle r = {(float)(panel_x + 6), (float)y, (float)(panel_w - 12), (float)(row_h - 2)};
        if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            EndScissorMode();
            return rows[i].id;
        }

        DrawText(TextFormat("%3d", rows[i].id), panel_x + 10, y + 7, 18, (Color){220,238,225,255});
        DrawText(rows[i].nombre, panel_x + 70, y + 7, 18, (Color){233,247,236,255});
    }
    EndScissorMode();

    return 0;
}

static int cargar_imagen_camiseta_gui(void)
{
    CamisetaGuiRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 30;
    const int panel_y = 130;

    /* Consumir ESC/ENTER previo al modal de carga de imagen */
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    if (!cargar_camisetas_gui_rows(&rows, &count))
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
        int visible_rows = 0;
        int clicked_id = 0;
        Rectangle area;

        calcular_panel_listado(sw, sh, &panel_x, &panel_w, &panel_h);

        visible_rows = panel_h / row_h;

        if (visible_rows < 1)
        {
            visible_rows = 1;
        }

        area = (Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h};
        scroll = actualizar_scroll_listado(scroll, count, visible_rows, area);

        BeginDrawing();
        ClearBackground((Color){18,36,28,255});
        DrawText("Selecciona camiseta para cargar imagen (clic)", 40, 40, 20, (Color){241,252,244,255});
        DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){19,40,27,255});
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){110,161,125,255});

        clicked_id = dibujar_y_detectar_click_cargar_imagen_camisetas(rows, count, scroll, row_h, area);

        DrawText("Esc: salir", panel_x, sh - 62, 18, (Color){178,214,188,255});
        EndDrawing();

        if (clicked_id > 0)
        {
            cargar_imagen_para_camiseta_id(clicked_id);
            free(rows);
            return 1;
        }

        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            break;
        }
    }

    free(rows);
    return 1;
}

static int abrir_imagen_camiseta_por_id_gui(int id)
{
    char ruta[1200] = {0};
    if (!construir_ruta_absoluta_imagen_por_id(id, ruta, sizeof(ruta)))
    {
        return 0;
    }

    return abrir_imagen_en_sistema(ruta);
}

static int dibujar_y_detectar_click_ver_imagen_camisetas(const CamisetaGuiRow *rows,
                                                          int count,
                                                          int scroll,
                                                          int row_h,
                                                          Rectangle panel)
{
    int panel_x = (int)panel.x;
    int panel_y = (int)panel.y;
    int panel_w = (int)panel.width;
    int panel_h = (int)panel.height;

    if (count == 0)
    {
        DrawText("No hay camisetas.", panel_x + 24, panel_y + 24, 24, (Color){233,247,236,255});
        return 0;
    }

    BeginScissorMode(panel_x, panel_y, panel_w, panel_h);
    for (int i = scroll; i < count; i++)
    {
        int row = i - scroll;
        int y = panel_y + row * row_h;
        if (y + row_h > panel_y + panel_h)
        {
            break;
        }

        Rectangle r = {(float)(panel_x + 6), (float)y, (float)(panel_w - 12), (float)(row_h - 2)};
        if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            EndScissorMode();
            return rows[i].id;
        }

        DrawText(TextFormat("%3d", rows[i].id), panel_x + 10, y + 7, 18, (Color){220,238,225,255});
        DrawText(rows[i].nombre, panel_x + 70, y + 7, 18, (Color){233,247,236,255});
    }
    EndScissorMode();

    return 0;
}

static int ver_imagen_camiseta_gui(void)
{
    CamisetaGuiRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 30;
    const int panel_y = 130;

    /* Consumir ESC/ENTER previo al modal de ver imagen */
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    if (!cargar_camisetas_gui_rows(&rows, &count))
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
        int visible_rows = 0;
        int clicked_id = 0;
        Rectangle area;

        calcular_panel_listado(sw, sh, &panel_x, &panel_w, &panel_h);

        visible_rows = panel_h / row_h;

        if (visible_rows < 1)
        {
            visible_rows = 1;
        }

        area = (Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h};
        scroll = actualizar_scroll_listado(scroll, count, visible_rows, area);

        BeginDrawing();
        ClearBackground((Color){18,36,28,255});
        DrawText("Selecciona camiseta para ver imagen (clic)", 40, 40, 20, (Color){241,252,244,255});
        DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){19,40,27,255});
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){110,161,125,255});

        clicked_id = dibujar_y_detectar_click_ver_imagen_camisetas(rows, count, scroll, row_h, area);

        DrawText("Esc: salir", panel_x, sh - 62, 18, (Color){178,214,188,255});
        EndDrawing();

        if (clicked_id > 0)
        {
            abrir_imagen_camiseta_por_id_gui(clicked_id);
            free(rows);
            return 1;
        }

        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            break;
        }
    }

    free(rows);
    return 1;
}

static void configurar_visor_preferido_imagen_gui(void)
{
    char nuevo[64] = {0}; int cursor = 0;
    char actual[64] = {0}; obtener_visor_preferido(actual, sizeof(actual));
    /* Prefill 'nuevo' with current value */
    if (actual[0] != '\0')
    {
        (void)strncpy_s(nuevo, sizeof(nuevo), actual, _TRUNCATE);
        cursor = (int)strlen_s(nuevo, sizeof(nuevo));
    }

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground((Color){18,36,28,255});
        DrawText("Configurar visor de imagen (ENTER guardar, ESC cancelar)", 40, 40, 20, (Color){241,252,244,255});
        DrawText("Visor:", 40, 100, 18, (Color){198,230,205,255});
        DrawText(nuevo, 120, 100, 18, (Color){233,247,236,255});
        EndDrawing();

        int k = GetCharPressed();
        while (k > 0)
        {
            if (k >= 32 && k <= 126 && cursor < (int)sizeof(nuevo) - 1)
            {
                nuevo[cursor++] = (char)k;
                nuevo[cursor] = '\0';
            }
            k = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) { int l = (int)strlen_s(nuevo, sizeof(nuevo)); if (l>0) nuevo[l-1]='\0', cursor=l-1; }
        if (IsKeyPressed(KEY_ESCAPE)) return;
        if (IsKeyPressed(KEY_ENTER))
        {
            trim_whitespace(nuevo);
            const char *valor = (nuevo[0] == '\0') ? "" : nuevo;
            guardar_visor_preferido(valor);
            return;
        }
    }
}

static int sortear_camiseta_gui(void)
{
    int ids[MAX_CAMISETAS_SORTEO];
    int count = obtener_ids_disponibles(ids, MAX_CAMISETAS_SORTEO);
    if (count == 0)
    {
        while (!WindowShouldClose())
        {
            BeginDrawing(); ClearBackground((Color){18,36,28,255}); DrawText("No hay camisetas disponibles para sorteo.", 40, 40, 20, (Color){241,252,244,255}); DrawText("Esc para volver", 40, 80, 18, (Color){198,230,205,255}); EndDrawing();
            if (IsKeyPressed(KEY_ESCAPE)) break;
        }
        return 0;
    }

    int seleccionado = seleccionar_id_aleatorio(ids, count);
    if (seleccionado == -1) return 0;

    marcar_camiseta_sorteada(seleccionado);
    char *nombre = obtener_nombre_camiseta(seleccionado);

    while (!WindowShouldClose())
    {
        BeginDrawing(); ClearBackground((Color){18,36,28,255}); DrawText("CAMISETA SORTEADA!", 40, 40, 28, (Color){241,252,244,255}); DrawText(nombre, 40, 100, 36, (Color){255,230,120,255}); DrawText("Presiona ESC o ENTER para continuar", 40, 160, 18, (Color){198,230,205,255}); EndDrawing();
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) break;
    }

    free(nombre);
    return 1;
}


/**
 * @brief Marca una camiseta como sorteada en la base de datos
 *
 * @param id ID de la camiseta a marcar
 */
static void marcar_camiseta_sorteada(int id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "UPDATE camiseta SET sorteada = 1 WHERE id = ?"))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/**
 * @brief Obtiene el nombre de una camiseta por su ID
 *
 * @param id ID de la camiseta
 * @return Nombre de la camiseta (debe ser liberado con free)
 */
static char* obtener_nombre_camiseta(int id)
{
    char nombre_buffer[256];

    if (obtener_nombre_entidad("camiseta", id, nombre_buffer, sizeof(nombre_buffer)))
    {
        return strdup(nombre_buffer);
    }

    return strdup("Desconocida");
}

/**
 * @brief Realiza un sorteo aleatorio entre las camisetas disponibles
 *
 * Permite sorteos continuos reutilizando camisetas ya sorteadas cuando
 * se agotan las disponibles, manteniendo la funcionalidad del sistema
 * sin necesidad de intervencion manual del usuario.
 */
void sortear_camiseta()
{
    if (menu_is_gui_enabled())
    {
        if (sortear_camiseta_gui())
            return;
    }
    clear_screen();
    print_header("SORTEO DE CAMISETAS");

    int disponibles = obtener_total("SELECT COUNT(*) FROM camiseta WHERE sorteada = 0");

    if (disponibles == 0)
    {
        reiniciar_sorteo();
        disponibles = obtener_total("SELECT COUNT(*) FROM camiseta");
    }

    if (disponibles == 0)
    {
        printf("No hay camisetas para sortear.\n");
        pause_console();
        return;
    }

    int ids[MAX_CAMISETAS_SORTEO];
    int count = obtener_ids_disponibles(ids, MAX_CAMISETAS_SORTEO);
    int seleccionado = seleccionar_id_aleatorio(ids, count);

    // Check if random selection failed
    if (seleccionado == -1)
    {
        printf("Error al seleccionar camiseta aleatoria.\n");
        pause_console();
        return;
    }

    marcar_camiseta_sorteada(seleccionado);
    char *nombre = obtener_nombre_camiseta(seleccionado);

    printf("CAMISETA SORTEADA!\n\n");
    printf("La camiseta seleccionada es: %s\n", nombre);
    printf("Quedan %d camisetas por sortear.\n", disponibles - 1);

    free(nombre);
    pause_console();
}

/**
 * @brief Muestra el menu principal de gestion de camisetas
 *
 * Proporciona una interfaz estructurada para las operaciones de
 * gestion de camisetas, centralizando el acceso a todas las funcionalidades.
 */
void menu_camisetas()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_camiseta},
        {2, "Listar", listar_camisetas},
        {3, "Modificar", editar_camiseta},
        {4, "Eliminar", eliminar_camiseta},
        {5, "Sortear", sortear_camiseta},
        {6, "Cargar Imagen", cargar_imagen_camiseta},
        {7, "Ver Camiseta", ver_imagen_camiseta},
        {8, "Ajustes Imagen", menu_ajustes_imagen_camiseta},
        {0, "Volver", NULL}
    };
    ejecutar_menu_estandar("CAMISETAS", items, 9);
}
