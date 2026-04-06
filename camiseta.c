#include "camiseta.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
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
static int configurar_visor_preferido_imagen_gui(void);
static int ver_imagen_camiseta_card_gui(int id, const char *nombre);
static void gui_show_camiseta_action_feedback(const char *title, const char *msg, int success);

static int sortear_camiseta_gui(void);

/* Prototipos de helpers usados por GUI antes de su definición */
static void marcar_camiseta_sorteada(int id);
static char* obtener_nombre_camiseta(int id);

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql);
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

#ifndef _WIN32
static int instalar_paquete_linux(const char *package_name)
{
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
}
#endif

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

static void menu_ajustes_imagen_camiseta()
{
    (void)configurar_visor_preferido_imagen_gui();
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

/**
 * @brief Crea una nueva camiseta en la base de datos
 *
 * Permite a los usuarios agregar camisetas para gestion y sorteos,
 * reutilizando IDs eliminados para mantener la secuencia.
 */
void crear_camiseta()
{
    (void)crear_camiseta_gui();
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
    app_log_event("CAMISETA", "Listado de camisetas consultado");

    (void)listar_camisetas_gui();
}

/**
 * @brief Permite editar el nombre de una camiseta existente
 *
 * Permite correcciones en la informacion de camisetas sin necesidad
 * de eliminar y recrear registros, mejorando la usabilidad.
 */
void editar_camiseta()
{
    (void)editar_camiseta_gui();
}

void cargar_imagen_camiseta()
{
    (void)cargar_imagen_camiseta_gui();
}

void ver_imagen_camiseta()
{
    (void)ver_imagen_camiseta_gui();
}

/**
 * @brief Elimina una camiseta de la base de datos
 *
 * Permite remover camisetas que ya no son necesarias mientras
 * mantiene la integridad de los datos con validaciones y confirmaciones.
 */
void eliminar_camiseta()
{
    (void)eliminar_camiseta_gui();
}

/**
 * @brief Reinicia el estado de sorteo de todas las camisetas
 *
 * Necesario cuando todas las camisetas han sido sorteadas para permitir
 * nuevos sorteos sin requerir recrear registros.
 */
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

typedef struct
{
    int id;
    char nombre[256];
    int partidos_jugados;
    int goles_totales;
    int asistencias_totales;
    char ruta_imagen[1200];
    Texture2D textura;
    int textura_cargada;
} CamisetaListadoCard;

typedef struct
{
    int panel_x;
    int panel_y;
    int panel_w;
    int panel_h;
    int area_y;
    int area_h;
    int card_w;
    int card_h;
    int gap_x;
    int gap_y;
    int cols;
    int total_rows;
    int visible_rows;
} CamisetaCardsLayout;

static int resolver_ruta_fallback_camiseta(char *ruta, size_t size)
{
    const char *candidatas[] = {
        "./Icons/CamisetaSinImagen.png",
        "../../Icons/CamisetaSinImagen.png",
        "../Icons/CamisetaSinImagen.png",
        "Icons/CamisetaSinImagen.png",
        NULL
    };

    if (!ruta || size == 0)
    {
        return 0;
    }

    ruta[0] = '\0';
    for (int i = 0; candidatas[i] != NULL; i++)
    {
        if (FileExists(candidatas[i]))
        {
            return strncpy_s(ruta, size, candidatas[i], _TRUNCATE) == 0;
        }
    }

    {
        const char *app_dir = GetApplicationDirectory();
        if (app_dir && app_dir[0] != '\0')
        {
            const char *sufijos[] = {
                "../../Icons/CamisetaSinImagen.png",
                "../Icons/CamisetaSinImagen.png",
                "Icons/CamisetaSinImagen.png",
                NULL
            };
            char candidata[512] = {0};

            for (int i = 0; sufijos[i] != NULL; i++)
            {
                snprintf(candidata, sizeof(candidata), "%s%s", app_dir, sufijos[i]);
                if (FileExists(candidata))
                {
                    return strncpy_s(ruta, size, candidata, _TRUNCATE) == 0;
                }
            }
        }
    }

    return 0;
}

static int cargar_textura_listado_camiseta(const char *ruta, Texture2D *textura)
{
    Texture2D t;

    if (!ruta || ruta[0] == '\0' || !textura || !FileExists(ruta))
    {
        return 0;
    }

    t = LoadTexture(ruta);
    if (t.id == 0)
    {
        return 0;
    }

    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    *textura = t;
    return 1;
}

static int cargar_camisetas_listado_cards(CamisetaListadoCard **cards_out, int *count_out)
{
    sqlite3_stmt *stmt = NULL;
    int cap = 48;
    int count = 0;
    CamisetaListadoCard *cards;

    if (!cards_out || !count_out)
    {
        return 0;
    }

    cards = (CamisetaListadoCard *)calloc((size_t)cap, sizeof(CamisetaListadoCard));
    if (!cards)
    {
        return 0;
    }

    if (!preparar_stmt(&stmt,
                       "SELECT c.id, c.nombre, c.imagen_ruta, "
                       "COUNT(p.id), IFNULL(SUM(p.goles),0), IFNULL(SUM(p.asistencias),0) "
                       "FROM camiseta c "
                       "LEFT JOIN partido p ON p.camiseta_id = c.id "
                       "GROUP BY c.id, c.nombre, c.imagen_ruta "
                       "ORDER BY c.id"))
    {
        free(cards);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (count >= cap)
        {
            int new_cap = cap * 2;
            CamisetaListadoCard *tmp = (CamisetaListadoCard *)realloc(cards,
                                                                       (size_t)new_cap * sizeof(CamisetaListadoCard));
            if (!tmp)
            {
                sqlite3_finalize(stmt);
                free(cards);
                return 0;
            }
            memset(tmp + cap, 0, (size_t)(new_cap - cap) * sizeof(CamisetaListadoCard));
            cards = tmp;
            cap = new_cap;
        }

        cards[count].id = sqlite3_column_int(stmt, 0);
    cards[count].partidos_jugados = sqlite3_column_int(stmt, 3);
    cards[count].goles_totales = sqlite3_column_int(stmt, 4);
    cards[count].asistencias_totales = sqlite3_column_int(stmt, 5);

        {
            const unsigned char *nombre_db = sqlite3_column_text(stmt, 1);
            if (nombre_db)
            {
                latin1_to_utf8(nombre_db, cards[count].nombre, sizeof(cards[count].nombre));
            }
            else
            {
                snprintf(cards[count].nombre, sizeof(cards[count].nombre), "%s", "(sin nombre)");
            }
        }

        {
            const unsigned char *ruta_db = sqlite3_column_text(stmt, 2);
            if (ruta_db && ruta_db[0] != '\0')
            {
                char ruta_db_buf[300] = {0};
                if (strncpy_s(ruta_db_buf,
                              sizeof(ruta_db_buf),
                              (const char *)ruta_db,
                              _TRUNCATE) == 0)
                {
                    (void)db_resolve_image_absolute_path(ruta_db_buf,
                                                         cards[count].ruta_imagen,
                                                         sizeof(cards[count].ruta_imagen));
                }
            }
        }

        if (cards[count].ruta_imagen[0] != '\0' &&
            cargar_textura_listado_camiseta(cards[count].ruta_imagen, &cards[count].textura))
        {
            cards[count].textura_cargada = 1;
        }

        count++;
    }

    sqlite3_finalize(stmt);
    *cards_out = cards;
    *count_out = count;
    return 1;
}

static void liberar_camisetas_listado_cards(CamisetaListadoCard *cards, int count)
{
    if (!cards)
    {
        return;
    }

    for (int i = 0; i < count; i++)
    {
        if (cards[i].textura_cargada && cards[i].textura.id != 0)
        {
            UnloadTexture(cards[i].textura);
        }
    }

    free(cards);
}

static void calcular_layout_cards_camisetas(int sw, int sh, int count, CamisetaCardsLayout *layout)
{
    if (!layout)
    {
        return;
    }

    layout->panel_x = (sw > 1000) ? 54 : 16;
    layout->panel_y = 112;
    layout->panel_w = sw - (layout->panel_x * 2);
    layout->panel_h = sh - 190;
    if (layout->panel_w < 250)
    {
        layout->panel_w = 250;
    }
    if (layout->panel_h < 240)
    {
        layout->panel_h = 240;
    }

    layout->card_w = (sw > 1400) ? 276 : 248;
    layout->card_h = 312;
    layout->gap_x = 18;
    layout->gap_y = 18;

    layout->area_y = layout->panel_y + 16;
    layout->area_h = layout->panel_h - 32;
    if (layout->area_h < layout->card_h)
    {
        layout->area_h = layout->card_h;
    }

    layout->cols = (layout->panel_w + layout->gap_x) / (layout->card_w + layout->gap_x);
    if (layout->cols < 1)
    {
        layout->cols = 1;
    }

    layout->visible_rows = (layout->area_h + layout->gap_y) / (layout->card_h + layout->gap_y);
    if (layout->visible_rows < 1)
    {
        layout->visible_rows = 1;
    }

    layout->total_rows = (count + layout->cols - 1) / layout->cols;
}

static Rectangle calcular_dst_textura_en_caja(Texture2D tex, Rectangle caja)
{
    float sx = caja.width / (float)tex.width;
    float sy = caja.height / (float)tex.height;
    float scale = (sx < sy) ? sx : sy;
    float w = (float)tex.width * scale;
    float h = (float)tex.height * scale;
    float x = caja.x + (caja.width - w) * 0.5f;
    float y = caja.y + (caja.height - h) * 0.5f;

    return (Rectangle){x, y, w, h};
}

static int dibujar_cards_listado_camisetas(const CamisetaListadoCard *cards,
                                           int count,
                                           int scroll_rows,
                                           const CamisetaCardsLayout *layout,
                                           const Texture2D *fallback_tex,
                                           int fallback_loaded)
{
    const GuiTheme *theme = gui_get_active_theme();
    int panel_x;
    int panel_w;
    int area_y;
    int area_h;
    int x_start;
    int clicked_id = 0;

    if (!cards || !layout)
    {
        return 0;
    }

    panel_x = layout->panel_x;
    panel_w = layout->panel_w;
    area_y = layout->area_y;
    area_h = layout->area_h;
    x_start = panel_x + 12;

    BeginScissorMode(panel_x, area_y, panel_w, area_h);
    for (int i = 0; i < count; i++)
    {
        int row = i / layout->cols;
        int col = i % layout->cols;
        int row_on_screen = row - scroll_rows;
        int x = x_start + col * (layout->card_w + layout->gap_x);
        int y = area_y + row_on_screen * (layout->card_h + layout->gap_y);
        Rectangle card;
        Rectangle image_box;
        int hovered;

        if (row_on_screen < 0)
        {
            continue;
        }

        if (y >= area_y + area_h)
        {
            break;
        }

        card = (Rectangle){(float)x, (float)y, (float)layout->card_w, (float)layout->card_h};
        image_box = (Rectangle){card.x + 12.0f, card.y + 12.0f, card.width - 24.0f, 156.0f};
        hovered = CheckCollisionPointRec(GetMousePosition(), card);

        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            clicked_id = cards[i].id;
        }

        DrawRectangleRounded(card, 0.09f, 12, hovered ? (Color){28, 55, 39, 255} : theme->card_bg);
        DrawRectangleRoundedLines(card, 0.09f, 12,
                                  hovered ? theme->accent_primary : theme->card_border);

        DrawRectangleRec(image_box, (Color){12, 24, 17, 255});
        DrawRectangleLinesEx(image_box, 1.0f, (Color){56, 96, 69, 255});

        {
            const Texture2D *tex = NULL;
            if (cards[i].textura_cargada && cards[i].textura.id != 0)
            {
                tex = &cards[i].textura;
            }
            else if (fallback_loaded && fallback_tex && fallback_tex->id != 0)
            {
                tex = fallback_tex;
            }

            if (tex)
            {
                Rectangle src = {0.0f, 0.0f, (float)tex->width, (float)tex->height};
                Rectangle dst = calcular_dst_textura_en_caja(*tex, image_box);
                DrawTexturePro(*tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
            }
            else
            {
                gui_text("Sin imagen", image_box.x + 16.0f,
                         image_box.y + image_box.height * 0.5f - 10.0f,
                         18.0f, theme->text_secondary);
            }
        }

        gui_text(TextFormat("ID %d", cards[i].id), card.x + 14.0f, card.y + 176.0f,
                 16.0f, theme->text_secondary);
        gui_text_truncated(cards[i].nombre,
                           card.x + 14.0f,
                           card.y + 198.0f,
                           19.0f,
                           card.width - 28.0f,
                           theme->text_primary);
        gui_text(TextFormat("Goles: %d", cards[i].goles_totales),
                 card.x + 14.0f,
                 card.y + 230.0f,
                 17.0f,
                 theme->text_secondary);
        gui_text(TextFormat("Asistencias: %d", cards[i].asistencias_totales),
                 card.x + 14.0f,
                 card.y + 252.0f,
                 17.0f,
                 theme->text_secondary);
        gui_text(TextFormat("Partidos Jugados: %d", cards[i].partidos_jugados),
                 card.x + 14.0f,
                 card.y + 274.0f,
                 17.0f,
                 theme->text_secondary);
    }
    EndScissorMode();
    return clicked_id;
}

static const char *buscar_nombre_card_listado(const CamisetaListadoCard *cards,
                                              int count,
                                              int id)
{
    if (!cards || count <= 0 || id <= 0)
    {
        return NULL;
    }

    for (int i = 0; i < count; i++)
    {
        if (cards[i].id == id)
        {
            return cards[i].nombre;
        }
    }

    return NULL;
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
    CamisetaListadoCard *cards = NULL;
    int count = 0;
    int scroll_rows = 0;
    Texture2D fallback_tex = (Texture2D){0};
    int fallback_loaded = 0;
    char fallback_path[512] = {0};

    /* Consumir posibles pulsaciones previas de ESC/ENTER para evitar que
        el loop modal devuelva un evento que el GUI principal vuelva a
        procesar inmediatamente. */
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    if (!cargar_camisetas_listado_cards(&cards, &count))
    {
        return 0;
    }

    if (resolver_ruta_fallback_camiseta(fallback_path, sizeof(fallback_path)) &&
        cargar_textura_listado_camiseta(fallback_path, &fallback_tex))
    {
        fallback_loaded = 1;
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int clicked_id = 0;
        const char *clicked_nombre = NULL;
        CamisetaCardsLayout layout;
        Rectangle area;

        calcular_layout_cards_camisetas(sw, sh, count, &layout);
        area = (Rectangle){(float)layout.panel_x, (float)layout.area_y,
                           (float)layout.panel_w, (float)layout.area_h};
        scroll_rows = actualizar_scroll_listado(scroll_rows,
                                                layout.total_rows,
                                                layout.visible_rows,
                                                area);

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("LISTADO DE CAMISETAS", sw);

        DrawRectangle(layout.panel_x, layout.panel_y, layout.panel_w, layout.panel_h,
                      (Color){19,40,27,255});
        DrawRectangleLines(layout.panel_x, layout.panel_y, layout.panel_w, layout.panel_h,
                           (Color){110,161,125,255});

        if (count > 0)
        {
            clicked_id = dibujar_cards_listado_camisetas(cards,
                                                         count,
                                                         scroll_rows,
                                                         &layout,
                                                         &fallback_tex,
                                                         fallback_loaded);
        }
        else
        {
            gui_text("No hay camisetas cargadas.",
                     (float)layout.panel_x + 24.0f,
                     (float)layout.panel_y + 28.0f,
                     24.0f,
                     (Color){233,247,236,255});
        }

        gui_draw_footer_hint("Click: agrandar imagen | Rueda: scroll | ESC/Enter: volver", (float)layout.panel_x, sh);
        EndDrawing();

        if (clicked_id > 0)
        {
            clicked_nombre = buscar_nombre_card_listado(cards, count, clicked_id);

            if (!ver_imagen_camiseta_card_gui(clicked_id, clicked_nombre))
            {
                gui_show_camiseta_action_feedback("VER IMAGEN",
                                                  "No se pudo abrir la imagen",
                                                  0);
            }
            continue;
        }

        if (listado_camisetas_should_close())
        {
            break;
        }
    }

    if (fallback_loaded && fallback_tex.id != 0)
    {
        UnloadTexture(fallback_tex);
    }
    liberar_camisetas_listado_cards(cards, count);
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

static int guardar_nueva_camiseta_gui(const char *nombre, int *id_creada)
{
    long long id = obtener_siguiente_id("camiseta");
    sqlite3_stmt *stmt = NULL;

    if (!preparar_stmt(&stmt, "INSERT INTO camiseta(id, nombre) VALUES(?, ?)"))
    {
        return 0;
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
        if (id_creada)
        {
            *id_creada = (int)id;
        }
        return 1;
    }

    return 0;
}

static void gui_draw_input_caret(const char *text, Rectangle input_rect)
{
    int blink_on = (((int)(GetTime() * 2.0)) % 2) == 0;
    if (!blink_on)
    {
        return;
    }

    Vector2 text_size = gui_text_measure(text, 18.0f);
    float caret_x = input_rect.x + 8.0f + text_size.x + 1.0f;
    float caret_right_limit = input_rect.x + input_rect.width - 8.0f;
    caret_x = (caret_x > caret_right_limit) ? caret_right_limit : caret_x;

    DrawLineEx((Vector2){caret_x, input_rect.y + 7.0f},
               (Vector2){caret_x, input_rect.y + input_rect.height - 7.0f},
               2.0f,
               (Color){241,252,244,255});
}

static void gui_draw_crear_camiseta_toast(int sw, int sh, int toast_kind)
{
    int toast_w = 480;
    int toast_h = 44;
    int toast_x = (sw - toast_w) / 2;
    int toast_y = sh - 120;
    Color bg = (toast_kind == 1) ? (Color){28,94,52,240} : (Color){120,45,38,240};
    const char *msg = "No se pudo crear la camiseta";

    if (toast_kind == 1)
    {
        msg = "Camiseta creada correctamente";
    }
    else if (toast_kind == 3)
    {
        msg = "El nombre no puede estar vacio";
    }

    DrawRectangle(toast_x, toast_y, toast_w, toast_h, bg);
    DrawRectangleLines(toast_x, toast_y, toast_w, toast_h, (Color){198,230,205,255});
    gui_text(msg, (float)toast_x + 14.0f, (float)toast_y + 12.0f, 18.0f, (Color){241,252,244,255});
}

static int gui_tick_crear_camiseta_toast(float *toast_timer, int toast_kind)
{
    if (*toast_timer <= 0.0f)
    {
        return 0;
    }

    *toast_timer -= GetFrameTime();
    if (*toast_timer > 0.0f)
    {
        return 0;
    }

    return toast_kind == 1;
}

static void gui_submit_crear_camiseta(char *nombre,
                                      int *toast_kind,
                                      float *toast_timer,
                                      int *id_creada,
                                      int *preguntar_imagen)
{
    input_consume_key(KEY_ENTER);
    if (preguntar_imagen)
    {
        *preguntar_imagen = 0;
    }

    trim_whitespace(nombre);
    if (nombre[0] == '\0')
    {
        *toast_kind = 3;
        *toast_timer = 1.2f;
        return;
    }

    if (guardar_nueva_camiseta_gui(nombre, id_creada))
    {
        *toast_kind = 1;
        *toast_timer = 1.1f;
        if (preguntar_imagen)
        {
            *preguntar_imagen = 1;
        }
        return;
    }

    *toast_kind = 2;
    *toast_timer = 1.4f;
}

static void gui_draw_action_toast(int sw, int sh, const char *msg, int success)
{
    int toast_w = 520;
    int toast_h = 44;
    int toast_x = (sw - toast_w) / 2;
    int toast_y = sh - 120;
    Color bg = success ? (Color){28,94,52,240} : (Color){120,45,38,240};

    DrawRectangle(toast_x, toast_y, toast_w, toast_h, bg);
    DrawRectangleLines(toast_x, toast_y, toast_w, toast_h, (Color){198,230,205,255});
    gui_text(msg ? msg : "Operacion completada", (float)toast_x + 14.0f, (float)toast_y + 12.0f,
             18.0f, (Color){241,252,244,255});
}

static int gui_tick_action_toast(float *timer)
{
    if (!timer || *timer <= 0.0f)
    {
        return 0;
    }

    *timer -= GetFrameTime();
    return *timer <= 0.0f;
}

static void gui_draw_carga_imagen_modal(int sw, int sh, const char *mensaje)
{
    const GuiTheme *theme = gui_get_active_theme();
    const int panel_w = sw > 900 ? 620 : sw - 40;
    const int panel_h = 210;
    const int panel_x = (sw - panel_w) / 2;
    const int panel_y = (sh - panel_h) / 2;
    const float spinner_r = 26.0f;
    const float base_time = (float)GetTime();
    const int active_dot = ((int)(base_time * 12.0f)) % 12;
    const float cx = (float)panel_x + (float)panel_w * 0.5f;
    const float cy = (float)panel_y + 88.0f;

    DrawRectangle(0, 0, sw, sh, (Color){4, 10, 7, 165});
    DrawRectangle(panel_x, panel_y, panel_w, panel_h, theme->card_bg);
    DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, theme->card_border);

    gui_text("CARGANDO IMAGEN", (float)panel_x + 24.0f, (float)panel_y + 24.0f,
             23.0f, theme->text_primary);
    gui_text(mensaje ? mensaje : "Procesando imagen...",
             (float)panel_x + 24.0f, (float)panel_y + 56.0f,
             18.0f, theme->text_secondary);

    for (int i = 0; i < 12; i++)
    {
        const float a = ((float)i / 12.0f) * (2.0f * 3.14159265f);
        const float x = cx + cosf(a) * spinner_r;
        const float y = cy + sinf(a) * spinner_r;
        const int tail = (active_dot - i + 12) % 12;
        Color dot = theme->accent_primary;
        dot.a = (unsigned char)(50 + (12 - tail) * 16);
        DrawCircleV((Vector2){x, y}, i == active_dot ? 4.8f : 3.7f, dot);
    }

    {
        const float bar_x = (float)panel_x + 26.0f;
        const float bar_y = (float)panel_y + (float)panel_h - 42.0f;
        const float bar_w = (float)panel_w - 52.0f;
        const float bar_h = 11.0f;
        const float segment_w = bar_w * 0.22f;
        const float phase = fmodf(base_time * 260.0f, bar_w + segment_w) - segment_w;

        DrawRectangleRounded((Rectangle){bar_x, bar_y, bar_w, bar_h}, 0.45f, 10,
                             (Color){22, 45, 31, 255});
        DrawRectangleRounded((Rectangle){bar_x + phase, bar_y, segment_w, bar_h}, 0.45f, 10,
                             theme->accent_primary);
        DrawRectangleRoundedLines((Rectangle){bar_x, bar_y, bar_w, bar_h}, 0.45f, 10,
                                  theme->border);
    }
}

#ifndef _WIN32
static void gui_spin_carga_imagen_por(float segundos, const char *mensaje)
{
    const double t0 = GetTime();

    while (!WindowShouldClose())
    {
        const double dt = GetTime() - t0;
        if (dt >= (double)segundos)
        {
            break;
        }

        {
            const int sw = GetScreenWidth();
            const int sh = GetScreenHeight();

            BeginDrawing();
            ClearBackground((Color){14,27,20,255});
            gui_draw_module_header("CARGAR IMAGEN", sw);
            gui_draw_carga_imagen_modal(sw, sh, mensaje);
            EndDrawing();
        }
    }
}
#endif

typedef struct
{
    int camiseta_id;
    volatile int done;
    int ok;
} GuiCargaImagenJob;

#ifdef _WIN32
static void __cdecl gui_carga_imagen_worker(void *arg)
{
    GuiCargaImagenJob *job = (GuiCargaImagenJob *)arg;
    if (!job)
    {
        return;
    }

    job->ok = cargar_imagen_para_camiseta_id(job->camiseta_id);
    job->done = 1;
}
#endif

static int gui_cargar_imagen_con_spinner(int camiseta_id)
{
#ifdef _WIN32
    GuiCargaImagenJob job;
    job.camiseta_id = camiseta_id;
    job.done = 0;
    job.ok = 0;

    if (_beginthread(gui_carga_imagen_worker, 0, &job) == (uintptr_t)-1L)
    {
        return cargar_imagen_para_camiseta_id(camiseta_id);
    }

    while (!job.done)
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("CARGAR IMAGEN", sw);
        gui_draw_carga_imagen_modal(sw, sh,
                                    "Selecciona imagen y espera mientras se procesa...");
        gui_draw_footer_hint("Procesando...", 40.0f, sh);
        EndDrawing();
    }

    return job.ok;
#else
    gui_spin_carga_imagen_por(0.35f, "Procesando imagen...");
    return cargar_imagen_para_camiseta_id(camiseta_id);
#endif
}

static void gui_show_camiseta_action_feedback(const char *title, const char *msg, int success)
{
    float timer = 1.15f;
    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header(title ? title : "CAMISETAS", sw);
        gui_draw_action_toast(sw, sh, msg, success);
        gui_draw_footer_hint("Continuando...", 40.0f, sh);
        EndDrawing();

        if (gui_tick_action_toast(&timer))
        {
            return;
        }
    }
}

static int gui_draw_action_button(Rectangle rect, const char *label, int primary)
{
    const GuiTheme *theme = gui_get_active_theme();
    int hovered = CheckCollisionPointRec(GetMousePosition(), rect);
    Color fill = primary ? theme->accent_primary : theme->bg_sidebar;
    Color border = primary ? theme->accent_primary_hv : theme->border;
    Color text = primary ? (Color){255, 255, 255, 255} : theme->text_primary;

    if (hovered)
    {
        fill = primary ? theme->accent_primary_hv : theme->row_hover;
    }

    DrawRectangleRec(rect, fill);
    DrawRectangleLinesEx(rect, 1.0f, border);

    {
        Vector2 m = gui_text_measure(label ? label : "", 18.0f);
        float tx = rect.x + (rect.width - m.x) * 0.5f;
        float ty = rect.y + (rect.height - m.y) * 0.5f;
        gui_text(label ? label : "", tx, ty, 18.0f, text);
    }

    return hovered;
}

static void gui_config_visor_button_rects(int panel_x, int panel_y, int panel_w, int panel_h,
                                          Rectangle *btn_save, Rectangle *btn_cancel)
{
    float btn_w = 168.0f;
    float btn_h = 38.0f;
    float y = (float)(panel_y + panel_h) - btn_h - 16.0f;
    if (btn_save)
    {
        *btn_save = (Rectangle){(float)(panel_x + panel_w) - (btn_w * 2.0f) - 28.0f, y, btn_w, btn_h};
    }
    if (btn_cancel)
    {
        *btn_cancel = (Rectangle){(float)(panel_x + panel_w) - btn_w - 14.0f, y, btn_w, btn_h};
    }
}

static int gui_confirmar_cargar_imagen_camiseta(int id)
{
    input_consume_key(KEY_ENTER);
    input_consume_key(KEY_ESCAPE);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 760 : sw - 40;
        int panel_h = 220;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle btn_yes;
        Rectangle btn_no;
        const GuiTheme *theme = gui_get_active_theme();
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        gui_config_visor_button_rects(panel_x, panel_y, panel_w, panel_h, &btn_yes, &btn_no);

        BeginDrawing();
        ClearBackground(theme->bg_main);
        gui_draw_module_header("NUEVA CAMISETA", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, theme->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, theme->card_border);

        gui_text(TextFormat("Camiseta ID %d creada correctamente", id),
                 (float)panel_x + 24.0f, (float)panel_y + 42.0f, 22.0f, theme->text_primary);
        gui_text("Desea agregarle una imagen ahora?", (float)panel_x + 24.0f,
                 (float)panel_y + 86.0f, 18.0f, theme->text_secondary);

        gui_draw_action_button(btn_yes, "Si, cargar", 1);
        gui_draw_action_button(btn_no, "No, omitir", 0);
        gui_draw_footer_hint("ENTER/Y: si | ESC/N: no", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_yes)) ||
            IsKeyPressed(KEY_Y) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_Y);
            input_consume_key(KEY_ENTER);
            return 1;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_no)) ||
            IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_N);
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static void gui_post_crear_camiseta(int id_creada, int preguntar_imagen)
{
    char log_msg[256];

    if (!preguntar_imagen || id_creada <= 0)
    {
        return;
    }

    if (!gui_confirmar_cargar_imagen_camiseta(id_creada))
    {
        snprintf(log_msg, sizeof(log_msg),
                 "Camiseta id=%d creada sin imagen inicial (opcional)", id_creada);
        app_log_event("CAMISETA", log_msg);
        return;
    }

    if (!cargar_imagen_para_camiseta_id(id_creada))
    {
        snprintf(log_msg, sizeof(log_msg),
                 "Camiseta id=%d creada, pero fallo carga de imagen inicial", id_creada);
        app_log_event("CAMISETA", log_msg);
        gui_show_camiseta_action_feedback("CARGAR IMAGEN", "No se pudo cargar la imagen", 0);
        return;
    }

    snprintf(log_msg, sizeof(log_msg), "Camiseta id=%d creada con imagen inicial", id_creada);
    app_log_event("CAMISETA", log_msg);
    gui_show_camiseta_action_feedback("CARGAR IMAGEN", "Imagen cargada correctamente", 1);
}

static int gui_tick_crear_camiseta_flow(char *nombre,
                                        int *cursor,
                                        size_t nombre_size,
                                        int *toast_kind,
                                        float *toast_timer,
                                        int *id_creada,
                                        int *preguntar_imagen)
{
    if (gui_tick_crear_camiseta_toast(toast_timer, *toast_kind))
    {
        gui_post_crear_camiseta(*id_creada, *preguntar_imagen);
        return 1;
    }

    if (*toast_timer > 0.0f)
    {
        return 0;
    }

    gui_append_printable_ascii(nombre, cursor, nombre_size);
    gui_handle_backspace(nombre, nombre_size, cursor);

    if (IsKeyPressed(KEY_ESCAPE))
    {
        input_consume_key(KEY_ESCAPE);
        return 1;
    }

    if (IsKeyPressed(KEY_ENTER))
    {
        gui_submit_crear_camiseta(nombre, toast_kind, toast_timer, id_creada, preguntar_imagen);
    }

    return 0;
}

static int crear_camiseta_gui(void)
{
    char nombre[50] = {0};
    int cursor = 0;
    int toast_kind = 0;     /* 0: no toast, 1: exito, 2: error */
    float toast_timer = 0.0f;
    int id_creada = 0;
    int preguntar_imagen = 0;

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

        if (toast_timer <= 0.0f)
        {
            gui_draw_input_caret(nombre, input_rect);
        }

        if (toast_timer > 0.0f)
        {
            gui_draw_crear_camiseta_toast(sw, sh, toast_kind);
        }

        gui_text("ENTER \u2192 Guardar | ESC \u2192 Volver", panel_x, sh - 62, 18.0f, (Color){178,214,188,255});

        EndDrawing();

        if (gui_tick_crear_camiseta_flow(nombre,
                                         &cursor,
                                         sizeof(nombre),
                                         &toast_kind,
                                         &toast_timer,
                                         &id_creada,
                                         &preguntar_imagen))
        {
            return 1;
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
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 760 : sw - 40;
        int panel_h = 240;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle input_rect = {(float)(panel_x + 24), (float)(panel_y + 104), (float)(panel_w - 48), 38.0f};
        const GuiTheme *theme = gui_get_active_theme();

        BeginDrawing();
        ClearBackground(theme->bg_main);
        gui_draw_module_header("EDITAR CAMISETA", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, theme->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, theme->card_border);

        gui_text(TextFormat("ID seleccionado: %d", id), (float)panel_x + 24.0f, (float)panel_y + 32.0f, 20.0f, theme->text_secondary);
        gui_text("Nuevo nombre:", (float)panel_x + 24.0f, (float)panel_y + 72.0f, 18.0f, theme->text_primary);

        DrawRectangleRec(input_rect, theme->bg_list);
        DrawRectangleLines((int)input_rect.x, (int)input_rect.y, (int)input_rect.width, (int)input_rect.height, theme->border);
        gui_text(nombre_nuevo, input_rect.x + 8.0f, input_rect.y + 8.0f, 18.0f, theme->text_primary);
        gui_draw_input_caret(nombre_nuevo, input_rect);

        gui_draw_footer_hint("ENTER: guardar cambios | ESC: cancelar", (float)panel_x, sh);
        EndDrawing();

        gui_append_printable_ascii(nombre_nuevo, &cursor, sizeof(nombre_nuevo));
        gui_handle_backspace(nombre_nuevo, sizeof(nombre_nuevo), &cursor);

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
    const GuiTheme *theme = gui_get_active_theme();

    if (count == 0)
    {
        gui_text("No hay camisetas.", (float)panel_x + 24.0f, (float)panel_y + 24.0f, 24.0f, theme->text_primary);
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
        int hovered = CheckCollisionPointRec(GetMousePosition(), r);

        gui_draw_list_row_bg(r, row, hovered);
        if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            EndScissorMode();
            return rows[i].id;
        }

        gui_text(TextFormat("%3d", rows[i].id), (float)panel_x + 12.0f, (float)y + 6.0f, 18.0f, theme->text_secondary);
        gui_text_truncated(rows[i].nombre, (float)panel_x + 78.0f, (float)y + 6.0f, 18.0f,
                           (float)panel_w - 96.0f, theme->text_primary);
    }
    EndScissorMode();

    return 0;
}

static int editar_camiseta_gui(void)
{
    CamisetaGuiRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 34;
    const int panel_y = 130;

    /* Consumir ESC/ENTER previo al modal de editar */
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
        int clicked_id = 0;
        Rectangle content_area;

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

        content_area = (Rectangle){(float)panel_x, (float)(panel_y + 32), (float)panel_w, (float)content_h};
        scroll = actualizar_scroll_listado(scroll, count, visible_rows, content_area);

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("EDITAR CAMISETA", sw);
        gui_text("Selecciona una camiseta para modificar su nombre", (float)panel_x, 92.0f, 18.0f, gui_get_active_theme()->text_secondary);
        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                            "ID", 12.0f,
                            "NOMBRE", 78.0f);

        clicked_id = dibujar_y_detectar_click_editar_camisetas(rows, count, scroll, row_h, content_area);
        gui_draw_footer_hint("Click: editar | Rueda: scroll | ESC: volver", (float)panel_x, sh);
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
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 700 : sw - 40;
        int panel_h = 210;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle btn_confirm;
        Rectangle btn_cancel;
        const GuiTheme *theme = gui_get_active_theme();
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        gui_config_visor_button_rects(panel_x, panel_y, panel_w, panel_h, &btn_confirm, &btn_cancel);

        BeginDrawing();
        ClearBackground(theme->bg_main);
        gui_draw_module_header("ELIMINAR CAMISETA", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, theme->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, theme->card_border);

        gui_text(TextFormat("Confirmar eliminacion de camiseta ID %d", id),
                 (float)panel_x + 24.0f, (float)panel_y + 44.0f, 22.0f, theme->text_primary);
        gui_text("Usa botones o teclas ENTER/ESC", (float)panel_x + 24.0f,
                 (float)panel_y + 92.0f, 18.0f, theme->text_secondary);

        gui_draw_action_button(btn_confirm, "Eliminar", 1);
        gui_draw_action_button(btn_cancel, "Cancelar", 0);
        gui_draw_footer_hint("Esta accion no se puede deshacer", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_confirm)) ||
            IsKeyPressed(KEY_Y) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_Y);
            input_consume_key(KEY_ENTER);
            return eliminar_camiseta_por_id_gui(id);
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_cancel)) ||
            IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_N);
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
    const GuiTheme *theme = gui_get_active_theme();

    if (count == 0)
    {
        gui_text("No hay camisetas.", (float)panel_x + 24.0f, (float)panel_y + 24.0f, 24.0f, theme->text_primary);
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
        int hovered = CheckCollisionPointRec(GetMousePosition(), r);

        gui_draw_list_row_bg(r, row, hovered);
        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            EndScissorMode();
            return rows[i].id;
        }

        gui_text(TextFormat("%3d", rows[i].id), (float)panel_x + 12.0f, (float)y + 6.0f,
                 18.0f, theme->text_secondary);
        gui_text_truncated(rows[i].nombre, (float)panel_x + 78.0f, (float)y + 6.0f, 18.0f,
                           (float)panel_w - 96.0f, theme->text_primary);
    }
    EndScissorMode();

    return 0;
}

static int eliminar_camiseta_gui(void)
{
    CamisetaGuiRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 34;
    const int panel_y = 130;
    float toast_timer = 0.0f;
    int toast_ok = 0;
    char toast_msg[96] = {0};

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
        int content_h = 0;
        int visible_rows = 1;
        int clicked_id = 0;
        Rectangle area;

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

        area = (Rectangle){(float)panel_x, (float)(panel_y + 32), (float)panel_w, (float)content_h};
        scroll = actualizar_scroll_listado(scroll, count, visible_rows, area);

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("ELIMINAR CAMISETA", sw);
        gui_text("Selecciona una camiseta para eliminar", (float)panel_x, 92.0f, 18.0f,
                 gui_get_active_theme()->text_secondary);
        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                            "ID", 12.0f,
                            "NOMBRE", 78.0f);

        clicked_id = dibujar_y_detectar_click_eliminar_camisetas(rows, count, scroll, row_h, area);

        if (toast_timer > 0.0f)
        {
            gui_draw_action_toast(sw, sh, toast_msg, toast_ok);
        }

        gui_draw_footer_hint("Click: eliminar | Rueda: scroll | ESC: volver", (float)panel_x, sh);
        EndDrawing();

        if (gui_tick_action_toast(&toast_timer))
        {
            return toast_ok ? 1 : 0;
        }

        if (toast_timer > 0.0f)
        {
            continue;
        }

        if (clicked_id > 0)
        {
            if (modal_confirmar_eliminar_camiseta_gui(clicked_id))
            {
                toast_ok = 1;
                snprintf(toast_msg, sizeof(toast_msg), "Camiseta eliminada correctamente");
                toast_timer = 1.2f;
            }
            else
            {
                toast_ok = 0;
                snprintf(toast_msg, sizeof(toast_msg), "Eliminacion cancelada o fallida");
                toast_timer = 1.2f;
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
    const GuiTheme *theme = gui_get_active_theme();

    if (count == 0)
    {
        gui_text("No hay camisetas.", (float)panel_x + 24.0f, (float)panel_y + 24.0f, 24.0f, theme->text_primary);
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
        int hovered = CheckCollisionPointRec(GetMousePosition(), r);

        gui_draw_list_row_bg(r, row, hovered);
        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            EndScissorMode();
            return rows[i].id;
        }

        gui_text(TextFormat("%3d", rows[i].id), (float)panel_x + 12.0f, (float)y + 6.0f,
                 18.0f, theme->text_secondary);
        gui_text_truncated(rows[i].nombre, (float)panel_x + 78.0f, (float)y + 6.0f, 18.0f,
                           (float)panel_w - 96.0f, theme->text_primary);
    }
    EndScissorMode();

    return 0;
}

static int cargar_imagen_camiseta_gui(void)
{
    CamisetaGuiRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 34;
    const int panel_y = 130;
    float toast_timer = 0.0f;
    int toast_ok = 0;
    char toast_msg[96] = {0};

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
        int content_h = 0;
        int visible_rows = 1;
        int clicked_id = 0;
        Rectangle area;

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

        area = (Rectangle){(float)panel_x, (float)(panel_y + 32), (float)panel_w, (float)content_h};
        scroll = actualizar_scroll_listado(scroll, count, visible_rows, area);

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("CARGAR IMAGEN", sw);
        gui_text("Selecciona una camiseta para cargar imagen", (float)panel_x, 92.0f, 18.0f,
                 gui_get_active_theme()->text_secondary);
        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                            "ID", 12.0f,
                            "NOMBRE", 78.0f);

        clicked_id = dibujar_y_detectar_click_cargar_imagen_camisetas(rows, count, scroll, row_h, area);

        if (toast_timer > 0.0f)
        {
            gui_draw_action_toast(sw, sh, toast_msg, toast_ok);
        }

        gui_draw_footer_hint("Click: cargar imagen | Rueda: scroll | ESC: volver", (float)panel_x, sh);
        EndDrawing();

        if (gui_tick_action_toast(&toast_timer))
        {
            return toast_ok ? 1 : 0;
        }

        if (toast_timer > 0.0f)
        {
            continue;
        }

        if (clicked_id > 0)
        {
            if (gui_cargar_imagen_con_spinner(clicked_id))
            {
                toast_ok = 1;
                snprintf(toast_msg, sizeof(toast_msg), "Imagen cargada correctamente");
            }
            else
            {
                toast_ok = 0;
                snprintf(toast_msg, sizeof(toast_msg), "No se pudo cargar la imagen");
            }
            toast_timer = 1.2f;
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

static int cargar_textura_preview_camiseta(int id,
                                           char *ruta,
                                           size_t ruta_size,
                                           Texture2D *tex,
                                           int *image_w,
                                           int *image_h)
{
    Image image;
    Texture2D texture;

    if (!ruta || ruta_size == 0 || !tex || !image_w || !image_h)
    {
        return 0;
    }

    if (!construir_ruta_absoluta_imagen_por_id(id, ruta, ruta_size))
    {
        return 0;
    }

    image = LoadImage(ruta);
    if (!image.data || image.width <= 0 || image.height <= 0)
    {
        if (image.data)
        {
            UnloadImage(image);
        }
        return 0;
    }

    texture = LoadTextureFromImage(image);
    *image_w = image.width;
    *image_h = image.height;
    UnloadImage(image);

    if (texture.id == 0)
    {
        return 0;
    }

    *tex = texture;
    return 1;
}

static Rectangle calcular_rect_preview_camiseta(int panel_x,
                                                int panel_y,
                                                int panel_w,
                                                int panel_h,
                                                int image_w,
                                                int image_h)
{
    float avail_w = (float)panel_w - 72.0f;
    float avail_h = (float)panel_h - 150.0f;
    float scale_x = avail_w / (float)image_w;
    float scale_y = avail_h / (float)image_h;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    float draw_w;
    float draw_h;
    float draw_x;
    float draw_y;

    if (scale > 1.0f || scale <= 0.0f)
    {
        scale = 1.0f;
    }

    draw_w = (float)image_w * scale;
    draw_h = (float)image_h * scale;
    draw_x = (float)panel_x + ((float)panel_w - draw_w) * 0.5f;
    draw_y = (float)panel_y + 98.0f + (avail_h - draw_h) * 0.5f;

    return (Rectangle){draw_x, draw_y, draw_w, draw_h};
}

typedef struct
{
    int sw;
    int sh;
    int panel_x;
    int panel_y;
    int panel_w;
    int panel_h;
    int id;
    const char *nombre;
    Texture2D tex;
    Rectangle dst;
} GuiCardPreviewCamisetaView;

static void dibujar_card_preview_camiseta(const GuiCardPreviewCamisetaView *v)
{
    Rectangle src;
    const GuiTheme *theme = gui_get_active_theme();

    if (!v)
    {
        return;
    }

    src = (Rectangle){0.0f, 0.0f, (float)v->tex.width, (float)v->tex.height};

    BeginDrawing();
    ClearBackground(theme->bg_main);
    gui_draw_module_header("VER IMAGEN", v->sw);

    DrawRectangle(v->panel_x, v->panel_y, v->panel_w, v->panel_h, theme->card_bg);
    DrawRectangleLines(v->panel_x, v->panel_y, v->panel_w, v->panel_h, theme->card_border);

    gui_text(TextFormat("Camiseta ID %d", v->id), (float)v->panel_x + 24.0f,
             (float)v->panel_y + 24.0f, 24.0f, theme->text_primary);
    gui_text_truncated(v->nombre ? v->nombre : "(sin nombre)", (float)v->panel_x + 24.0f,
                       (float)v->panel_y + 56.0f, 18.0f, (float)v->panel_w - 48.0f,
                       theme->text_secondary);

    DrawRectangle((int)v->dst.x - 6, (int)v->dst.y - 6,
                  (int)v->dst.width + 12, (int)v->dst.height + 12,
                  (Color){10, 20, 15, 220});
    DrawRectangleLines((int)v->dst.x - 6, (int)v->dst.y - 6,
                       (int)v->dst.width + 12, (int)v->dst.height + 12,
                       theme->border);
    DrawTexturePro(v->tex, src, v->dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);

    gui_draw_footer_hint("ESC/ENTER: volver | O: abrir en visor externo", (float)v->panel_x, v->sh);
    EndDrawing();
}

static int procesar_input_card_preview_camiseta(int id)
{
    if (IsKeyPressed(KEY_O))
    {
        input_consume_key(KEY_O);
        (void)abrir_imagen_camiseta_por_id_gui(id);
    }

    if (!IsKeyPressed(KEY_ESCAPE) && !IsKeyPressed(KEY_ENTER))
    {
        return 0;
    }

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);
    return 1;
}

static int ver_imagen_camiseta_card_gui(int id, const char *nombre)
{
    char ruta[1200] = {0};
    Texture2D tex;
    int image_w = 0;
    int image_h = 0;

    if (!cargar_textura_preview_camiseta(id, ruta, sizeof(ruta), &tex, &image_w, &image_h))
    {
        return abrir_imagen_camiseta_por_id_gui(id);
    }

    input_consume_key(KEY_ENTER);
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_O);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 1100 ? 920 : sw - 40;
        int panel_h = sh > 760 ? sh - 140 : sh - 80;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle dst = calcular_rect_preview_camiseta(panel_x, panel_y, panel_w, panel_h,
                                                       image_w, image_h);
        GuiCardPreviewCamisetaView view = {
            sw,
            sh,
            panel_x,
            panel_y,
            panel_w,
            panel_h,
            id,
            nombre,
            tex,
            dst
        };

        dibujar_card_preview_camiseta(&view);

        if (procesar_input_card_preview_camiseta(id))
        {
            break;
        }
    }

    UnloadTexture(tex);
    return 1;
}

static const char *buscar_nombre_row_camiseta(const CamisetaGuiRow *rows, int count, int id)
{
    if (!rows || count <= 0 || id <= 0)
    {
        return NULL;
    }

    for (int i = 0; i < count; i++)
    {
        if (rows[i].id == id)
        {
            return rows[i].nombre;
        }
    }

    return NULL;
}

static void mostrar_error_ver_imagen_camiseta(int *toast_ok,
                                              char *toast_msg,
                                              size_t toast_msg_size,
                                              float *toast_timer)
{
    if (!toast_ok || !toast_msg || toast_msg_size == 0 || !toast_timer)
    {
        return;
    }

    *toast_ok = 0;
    snprintf(toast_msg, toast_msg_size, "No se pudo cargar la imagen de la camiseta");
    *toast_timer = 1.2f;
}

static void procesar_click_ver_imagen_camiseta(int clicked_id,
                                               const CamisetaGuiRow *rows,
                                               int count,
                                               int *toast_ok,
                                               char *toast_msg,
                                               size_t toast_msg_size,
                                               float *toast_timer)
{
    const char *nombre;

    if (clicked_id <= 0)
    {
        return;
    }

    nombre = buscar_nombre_row_camiseta(rows, count, clicked_id);
    if (ver_imagen_camiseta_card_gui(clicked_id, nombre))
    {
        return;
    }

    mostrar_error_ver_imagen_camiseta(toast_ok, toast_msg, toast_msg_size, toast_timer);
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
    const GuiTheme *theme = gui_get_active_theme();

    if (count == 0)
    {
        gui_text("No hay camisetas.", (float)panel_x + 24.0f, (float)panel_y + 24.0f, 24.0f, theme->text_primary);
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
        int hovered = CheckCollisionPointRec(GetMousePosition(), r);

        gui_draw_list_row_bg(r, row, hovered);
        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            EndScissorMode();
            return rows[i].id;
        }

        gui_text(TextFormat("%3d", rows[i].id), (float)panel_x + 12.0f, (float)y + 6.0f,
                 18.0f, theme->text_secondary);
        gui_text_truncated(rows[i].nombre, (float)panel_x + 78.0f, (float)y + 6.0f, 18.0f,
                           (float)panel_w - 96.0f, theme->text_primary);
    }
    EndScissorMode();

    return 0;
}

static int ver_imagen_camiseta_gui(void)
{
    CamisetaGuiRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 34;
    const int panel_y = 130;
    float toast_timer = 0.0f;
    int toast_ok = 0;
    char toast_msg[112] = {0};

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
        int content_h = 0;
        int visible_rows = 1;
        int clicked_id = 0;
        Rectangle area;

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

        area = (Rectangle){(float)panel_x, (float)(panel_y + 32), (float)panel_w, (float)content_h};
        scroll = actualizar_scroll_listado(scroll, count, visible_rows, area);

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("VER IMAGEN", sw);
        gui_text("Selecciona una camiseta para abrir su imagen", (float)panel_x, 92.0f, 18.0f,
                 gui_get_active_theme()->text_secondary);
        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                            "ID", 12.0f,
                            "NOMBRE", 78.0f);

        clicked_id = dibujar_y_detectar_click_ver_imagen_camisetas(rows, count, scroll, row_h, area);

        if (toast_timer > 0.0f)
        {
            gui_draw_action_toast(sw, sh, toast_msg, toast_ok);
        }

        gui_draw_footer_hint("Click: ver card de imagen | Rueda: scroll | ESC: volver", (float)panel_x, sh);
        EndDrawing();

        if (gui_tick_action_toast(&toast_timer))
        {
            toast_timer = 0.0f;
        }

        if (toast_timer > 0.0f)
        {
            continue;
        }

        procesar_click_ver_imagen_camiseta(clicked_id,
                                           rows,
                                           count,
                                           &toast_ok,
                                           toast_msg,
                                           sizeof(toast_msg),
                                           &toast_timer);

        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            break;
        }
    }

    free(rows);
    return 1;
}

typedef struct
{
    int sw;
    int sh;
    int panel_x;
    int panel_y;
    int panel_w;
    int panel_h;
    Rectangle input_rect;
    const char *actual;
    const char *nuevo;
    float toast_timer;
    int toast_ok;
    const char *toast_msg;
} GuiConfigVisorView;

static void gui_draw_config_visor_screen(const GuiConfigVisorView *v)
{
    const GuiTheme *theme = gui_get_active_theme();
    Rectangle btn_save;
    Rectangle btn_cancel;
    if (!v)
    {
        return;
    }

    gui_config_visor_button_rects(v->panel_x, v->panel_y, v->panel_w, v->panel_h, &btn_save, &btn_cancel);

    BeginDrawing();
    ClearBackground(theme->bg_main);
    gui_draw_module_header("AJUSTES DE IMAGEN", v->sw);

    DrawRectangle(v->panel_x, v->panel_y, v->panel_w, v->panel_h, theme->card_bg);
    DrawRectangleLines(v->panel_x, v->panel_y, v->panel_w, v->panel_h, theme->card_border);

    gui_text("Configurar visor preferido", (float)v->panel_x + 24.0f, (float)v->panel_y + 32.0f,
             22.0f, theme->text_primary);
    gui_text(TextFormat("Actual: %s", (v->actual && v->actual[0]) ? v->actual : "auto"),
             (float)v->panel_x + 24.0f, (float)v->panel_y + 74.0f, 18.0f, theme->text_secondary);
    gui_text("Nuevo visor (auto, mspaint, xdg-open, etc):", (float)v->panel_x + 24.0f,
             (float)v->panel_y + 102.0f, 18.0f, theme->text_secondary);

    DrawRectangleRec(v->input_rect, theme->bg_list);
    DrawRectangleLines((int)v->input_rect.x, (int)v->input_rect.y, (int)v->input_rect.width,
                       (int)v->input_rect.height, theme->border);
    gui_text(v->nuevo ? v->nuevo : "", v->input_rect.x + 8.0f, v->input_rect.y + 8.0f,
             18.0f, theme->text_primary);

    if (v->toast_timer <= 0.0f)
    {
        gui_draw_input_caret(v->nuevo ? v->nuevo : "", v->input_rect);
    }

    if (v->toast_timer > 0.0f)
    {
        gui_draw_action_toast(v->sw, v->sh, v->toast_msg, v->toast_ok);
    }

    if (v->toast_timer <= 0.0f)
    {
        gui_draw_action_button(btn_save, "Guardar", 1);
        gui_draw_action_button(btn_cancel, "Cancelar", 0);
    }

    gui_draw_footer_hint("Click o ENTER para guardar | ESC para cancelar", (float)v->panel_x, v->sh);
    EndDrawing();
}

static void gui_submit_config_visor(char *nuevo,
                                    int *toast_ok,
                                    char *toast_msg,
                                    size_t toast_msg_sz,
                                    float *toast_timer)
{
    const char *valor;

    if (!nuevo || !toast_ok || !toast_msg || toast_msg_sz == 0 || !toast_timer)
    {
        return;
    }

    trim_whitespace(nuevo);
    valor = (nuevo[0] == '\0') ? "" : nuevo;

    if (guardar_visor_preferido(valor))
    {
        *toast_ok = 1;
        snprintf(toast_msg, toast_msg_sz, "Visor guardado correctamente");
        *toast_timer = 1.2f;
        return;
    }

    *toast_ok = 0;
    snprintf(toast_msg, toast_msg_sz, "No se pudo guardar el visor");
    *toast_timer = 1.3f;
}

static int configurar_visor_preferido_imagen_gui(void)
{
    char nuevo[64] = {0};
    int cursor = 0;
    char actual[64] = {0};
    float toast_timer = 0.0f;
    int toast_ok = 0;
    char toast_msg[96] = {0};

    obtener_visor_preferido(actual, sizeof(actual));
    /* Prefill 'nuevo' with current value */
    if (actual[0] != '\0')
    {
        (void)strncpy_s(nuevo, sizeof(nuevo), actual, _TRUNCATE);
        cursor = (int)strlen_s(nuevo, sizeof(nuevo));
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 760 : sw - 40;
        int panel_h = 250;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle input_rect = {(float)(panel_x + 24), (float)(panel_y + 126), (float)(panel_w - 48), 38.0f};
        Rectangle btn_save;
        Rectangle btn_cancel;
        GuiConfigVisorView view = {
            sw,
            sh,
            panel_x,
            panel_y,
            panel_w,
            panel_h,
            input_rect,
            actual,
            nuevo,
            toast_timer,
            toast_ok,
            toast_msg
        };
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        gui_config_visor_button_rects(panel_x, panel_y, panel_w, panel_h, &btn_save, &btn_cancel);

        gui_draw_config_visor_screen(&view);

        if (gui_tick_action_toast(&toast_timer))
        {
            return toast_ok ? 1 : 0;
        }

        if (toast_timer > 0.0f)
        {
            continue;
        }

        gui_append_printable_ascii(nuevo, &cursor, sizeof(nuevo));
        gui_handle_backspace(nuevo, sizeof(nuevo), &cursor);

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_cancel)) || IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_save)) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ENTER);
            gui_submit_config_visor(nuevo, &toast_ok, toast_msg, sizeof(toast_msg), &toast_timer);
        }
    }

    return 0;
}

static int sortear_camiseta_gui(void)
{
    int ids[MAX_CAMISETAS_SORTEO];
    int count = obtener_ids_disponibles(ids, MAX_CAMISETAS_SORTEO);
    int reinicio_automatico = 0;

    if (count == 0)
    {
        int total = obtener_total("SELECT COUNT(*) FROM camiseta");
        if (total > 0)
        {
            sqlite3_exec(db, "UPDATE camiseta SET sorteada = 0", 0, 0, 0);
            count = obtener_ids_disponibles(ids, MAX_CAMISETAS_SORTEO);
            reinicio_automatico = (count > 0);
        }
    }

    if (count == 0)
    {
        while (!WindowShouldClose())
        {
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();
            const GuiTheme *theme = gui_get_active_theme();

            BeginDrawing();
            ClearBackground(theme->bg_main);
            gui_draw_module_header("SORTEO DE CAMISETA", sw);
            gui_text("No hay camisetas disponibles para sorteo.", 40.0f, 130.0f, 24.0f, theme->text_primary);
            gui_draw_action_toast(sw, sh, "No hay camisetas disponibles", 0);
            gui_draw_footer_hint("ESC: volver", 40.0f, sh);
            EndDrawing();

            if (IsKeyPressed(KEY_ESCAPE))
            {
                input_consume_key(KEY_ESCAPE);
                break;
            }
        }
        return 0;
    }

    int seleccionado = seleccionar_id_aleatorio(ids, count);
    if (seleccionado == -1)
    {
        return 0;
    }

    marcar_camiseta_sorteada(seleccionado);
    char *nombre = obtener_nombre_camiseta(seleccionado);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 760 : sw - 40;
        int panel_h = 250;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        const GuiTheme *theme = gui_get_active_theme();

        BeginDrawing();
        ClearBackground(theme->bg_main);
        gui_draw_module_header("SORTEO DE CAMISETA", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, theme->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, theme->card_border);

        gui_text("CAMISETA SORTEADA", (float)panel_x + 24.0f, (float)panel_y + 34.0f,
                 26.0f, theme->text_highlight);
        gui_text(TextFormat("ID: %d", seleccionado), (float)panel_x + 24.0f,
                 (float)panel_y + 84.0f, 20.0f, theme->text_secondary);
        gui_text_truncated(nombre ? nombre : "Desconocida", (float)panel_x + 24.0f,
                           (float)panel_y + 118.0f, 30.0f, (float)panel_w - 48.0f,
                           theme->text_primary);

        if (reinicio_automatico)
        {
            gui_text("Se reinicio el sorteo automaticamente", (float)panel_x + 24.0f,
                     (float)panel_y + 160.0f, 17.0f, theme->text_secondary);
        }

        gui_draw_action_toast(sw, sh, "Sorteo realizado correctamente", 1);
        gui_draw_footer_hint("ESC o ENTER: volver", (float)panel_x, sh);
        EndDrawing();

        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            break;
        }
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
    (void)sortear_camiseta_gui();
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
