#include "partido.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "camiseta.h"
#include "equipo.h"
#include "entrenador_ia.h"
#include "financiamiento.h"
#include "settings.h"
#include "input.h"
#include "gui_components.h"
#include <stdio.h>
#include <string.h>
#include "compat_windows.h"
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#ifdef _WIN32
#include <process.h>
#else
#include "process.h"
#endif
#include <memory.h>
#include <limits.h>

#ifndef UNUSED
#  if defined(__GNUC__) || defined(__clang__)
#    define UNUSED __attribute__((unused))
#  else
#    define UNUSED
#  endif
#endif

// Prototipos de funciones estaticas usadas antes de su definicion
static int cargar_equipo_desde_bd(int equipo_id, Equipo *equipo);
static int cargar_jugadores_equipo(int equipo_id, Equipo *equipo);
static UNUSED void guardar_estadisticas_equipo(const Equipo *equipo, int const *estadisticas, int const *asistencias,
        int resultado, int cancha_id, char const *fecha_simulacion);
static void tactica_gui_dibujar_cancha_preview(Rectangle rect);
static void tactica_gui_dibujar_card_accion(Rectangle rect, const char *title, const char *subtitle,
                        Color fill, int hovered);
/* GUI (Raylib) helper: listado de partidos en modo gráfico */
static int listar_partidos_gui(void);
static int crear_partido_gui(void);
static int modificar_partido_gui(void);
static int eliminar_partido_gui(void);

// Declaracion externa para funcion de financiamiento
extern void obtener_fecha_actual(char *fecha);

/**
 * @brief Preparar statement y reportar errores
 */
static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, NULL) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

/**
 * @brief Muestra la informacion de un partido desde un statement preparado
 *
 * Esta funcion generica imprime todos los detalles de un partido obtenido
 * de una consulta SQL. Asume que el statement tiene las columnas en el orden:
 * id, cancha, fecha_hora, goles, asistencias, camiseta, resultado,
 * rendimiento_general, cansancio, estado_animo, comentario_personal, clima, dia, precio
 *
 * @param stmt Statement preparado con los datos del partido
 * @return 1 si se mostro al menos un partido, 0 si no hay resultados
 */
static int mostrar_partidos_desde_stmt(sqlite3_stmt *stmt)
{
    int hay = 0;
    char fecha_formateada[20];

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        // Formatear la fecha para visualizacion
        format_date_for_display((const char *)sqlite3_column_text(stmt, 2), fecha_formateada, sizeof(fecha_formateada));

        ui_printf_centered_line("ID: %d", sqlite3_column_int(stmt, 0));
        ui_printf_centered_line("Cancha: %s", sqlite3_column_text(stmt, 1));
        ui_printf_centered_line("Fecha: %s", fecha_formateada);
        ui_printf_centered_line("Goles: %d, Asistencias: %d", sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4));
        ui_printf_centered_line("Camiseta: %s", sqlite3_column_text(stmt, 5));
        ui_printf_centered_line("Resultado: %s", resultado_to_text(sqlite3_column_int(stmt, 6)));
        ui_printf_centered_line("Rendimiento General: %d/10", sqlite3_column_int(stmt, 7));
        ui_printf_centered_line("Cansancio: %d/10", sqlite3_column_int(stmt, 8));
        ui_printf_centered_line("Estado de Animo: %d/10", sqlite3_column_int(stmt, 9));
        ui_printf_centered_line("Comentario Personal: %s", sqlite3_column_text(stmt, 10) ? (const char *)sqlite3_column_text(stmt, 10) : "N/A");
        ui_printf_centered_line("Clima: %s", clima_to_text(sqlite3_column_int(stmt, 11)));
        ui_printf_centered_line("Dia: %s", dia_to_text(sqlite3_column_int(stmt, 12)));
        ui_printf_centered_line("Precio: %d", sqlite3_column_int(stmt, 13));
        ui_printf_centered_line("----------------------------------------");
        hay = 1;
    }

    return hay;
}


/**
 * @brief Estructura para agrupar los datos de un partido
 *
 * Esta estructura se utiliza para reducir el numero de parametros en funciones
 * y mejorar la organizacion del codigo.
 */
typedef struct
{
    int cancha_id;
    int goles;
    int asistencias;
    int camiseta;
    int resultado;
    int rendimiento_general;
    int cansancio;
    int estado_animo;
    char comentario_personal[256];
    int clima;
    int dia;
    int precio;
} DatosPartido;

/**
 * @brief Estructura para agrupar estadisticas de un partido
 *
 * Esta estructura se utiliza para reducir el numero de parametros en funciones
 * de simulacion y resultados, agrupando las estadisticas de ambos equipos.
 */
typedef struct
{
    int estadisticas_local[11];
    int estadisticas_visitante[11];
    int asistencias_local[11];
    int asistencias_visitante[11];
    int goles_local;
    int goles_visitante;
} EstadisticasPartido;

/**
 * @brief Estructura para agrupar datos de simulacion de partido
 *
 * Esta estructura agrupa todos los datos necesarios para la simulacion
 * y guardado de resultados de un partido, reduciendo la cantidad de
 * parametros en las funciones relacionadas.
 */
typedef struct
{
    Equipo equipo_local;
    Equipo equipo_visitante;
    int estadisticas_local[11];
    int estadisticas_visitante[11];
    int asistencias_local[11];
    int asistencias_visitante[11];
    int goles_local;
    int goles_visitante;
} DatosSimulacion;

/**
 * @brief Generates a random number using standard rand()
 *
 * This function provides a simple random number generator for non-critical uses.
 * For cryptographic applications, consider using a CSPRNG library.
 *
 * @param max Maximum value (exclusive)
 * @return Random number in range [0, max)
 */
static int secure_rand(int max)
{
    if (max <= 0)
        return 0;
    return (unsigned int)rand() % max;
}

/**
 * @brief Verifica que existan canchas y camisetas antes de crear un partido
 *
 * Para mantener la integridad de los datos, se asegura de que haya entidades relacionadas
 * disponibles antes de permitir la creacion de un nuevo partido.
 *
 * @return 1 si hay entidades disponibles, 0 si no
 */
static int verificar_prerrequisitos_partido()
{
    sqlite3_stmt *stmt_count_canchas;
    if (!preparar_stmt("SELECT COUNT(*) FROM cancha", &stmt_count_canchas))
    {
        return 0;
    }
    sqlite3_step(stmt_count_canchas);
    int count_canchas = sqlite3_column_int(stmt_count_canchas, 0);
    sqlite3_finalize(stmt_count_canchas);

    sqlite3_stmt *stmt_count_camisetas;
    if (!preparar_stmt("SELECT COUNT(*) FROM camiseta", &stmt_count_camisetas))
    {
        return 0;
    }
    sqlite3_step(stmt_count_camisetas);
    int count_camisetas = sqlite3_column_int(stmt_count_camisetas, 0);
    sqlite3_finalize(stmt_count_camisetas);

    if (count_canchas == 0 && count_camisetas == 0)
    {
        printf("No se puede crear un partido porque no hay canchas ni camisetas registradas.\n");
        pause_console();
        return 0;
    }
    return 1;
}

/**
 * @brief Muestra la lista de canchas disponibles para seleccion
 *
 * Facilita la seleccion de cancha al usuario mostrando las opciones disponibles.
 */
static void listar_canchas_disponibles()
{
    ui_printf_centered_line("Canchas disponibles:");
    sqlite3_stmt *stmt_canchas;
    if (!preparar_stmt("SELECT id, nombre FROM cancha ORDER BY id", &stmt_canchas))
    {
        return;
    }
    while (sqlite3_step(stmt_canchas) == SQLITE_ROW)
    {
        ui_printf_centered_line("%d | %s", sqlite3_column_int(stmt_canchas, 0), sqlite3_column_text(stmt_canchas, 1));
    }
    sqlite3_finalize(stmt_canchas);
}

/**
 * @brief Recopila todos los datos necesarios para un partido desde el usuario
 *
 * Valida cada entrada para asegurar que los datos sean correctos antes de proceder.
 * Utiliza bucles para reintentar entradas invalidas, mejorando la experiencia del usuario.
 *
 * @param datos Puntero a la estructura DatosPartido que contendra los datos recopilados
 * @return 1 si los datos son validos, 0 si se cancela o hay error
 */
static int recopilar_datos_partido(DatosPartido *datos)
{
    // Initialize all fields to safe default values
    datos->cancha_id = 0;
    datos->goles = 0;
    datos->asistencias = 0;
    datos->camiseta = 0;
    datos->resultado = 0;
    datos->rendimiento_general = 0;
    datos->cansancio = 0;
    datos->estado_animo = 0;
    datos->clima = 0;
    datos->dia = 0;
    datos->precio = 0;
    strcpy_s(datos->comentario_personal, sizeof(datos->comentario_personal), "");

    while (1)
    {
        datos->cancha_id = input_int("ID Cancha, (0 para Cancelar): ");
        if (datos->cancha_id == 0)
            return 0;
        if (existe_id("cancha", datos->cancha_id))
            break;
        printf("La cancha no existe. Intente nuevamente.\n");
    }

    datos->goles = input_int("Goles: ");
    while (datos->goles < 0)
    {
        datos->goles = input_int("Goles invalidos. Ingrese 0 o mas: ");
    }

    datos->asistencias = input_int("Asistencias: ");
    while (datos->asistencias < 0)
    {
        datos->asistencias = input_int("Asistencias invalidas. Ingrese 0 o mas: ");
    }
    datos->resultado = input_int("Resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA): ");
    while (datos->resultado < 1 || datos->resultado > 3)
    {
        datos->resultado = input_int("Resultado invalido. (1=VICTORIA, 2=EMPATE, 3=DERROTA):");
    }
    listar_camisetas();
    while (1)
    {
        datos->camiseta = input_int("ID Camiseta: ");
        if (existe_id("camiseta", datos->camiseta))
            break;
        printf("La camiseta no existe. Intente nuevamente.\n");
    }
    datos->rendimiento_general = input_int("Rendimiento general (1-10): ");
    while (datos->rendimiento_general < 1 || datos->rendimiento_general > 10)
    {
        datos->rendimiento_general = input_int("Rendimiento invalido. Ingrese entre 1 y 10: ");
    }
    datos->cansancio = input_int("Cansancio (1-10): ");
    while (datos->cansancio < 1 || datos->cansancio > 10)
    {
        datos->cansancio = input_int("Cansancio invalido. Ingrese entre 1 y 10:  ");
    }
    datos->estado_animo = input_int("Estado de Animo (1-10): ");
    while (datos->estado_animo < 1 || datos->estado_animo > 10)
    {
        datos->estado_animo = input_int("Estado de Animo invalido. Ingrese entre 1 y 10: ");
    }
    input_string("Comentario personal: ", datos->comentario_personal, 256);
    datos->clima = input_int("Clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio):");
    while (datos->clima < 1 || datos->clima > 6)
    {
        datos->clima = input_int("Clima invalido (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio): ");
    }
    datos->dia = input_int("Dia (1=Dia, 2=Tarde, 3=Noche): ");
    while (datos->dia < 1 || datos->dia > 3)
    {
        datos->dia = input_int("Dia invalido (1=Dia, 2=Tarde, 3=Noche): ");
    }
    datos->precio = input_int("Precio del partido: ");
    while (datos->precio < 0)
    {
        datos->precio = input_int("Precio invalido. Ingrese 0 o mas: ");
    }

    return 1;
}

/**
 * @brief Inserta un nuevo partido en la base de datos
 *
 * Utiliza prepared statements para evitar inyeccion SQL y asegurar integridad de datos.
 * Maneja errores de SQLite para informar al usuario si la insercion falla.
 *
 * @param id ID del partido
 * @param datos Puntero a la estructura DatosPartido que contiene los datos del partido
 * @param fecha Fecha y hora
 */
static void insertar_partido(long long id, DatosPartido const *datos, char const *fecha)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "INSERT INTO partido(id, cancha_id,fecha_hora,goles,asistencias,camiseta_id,resultado,rendimiento_general,cansancio,estado_animo,comentario_personal,clima,dia,precio)"
                "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                &stmt))
    {
        return;
    }
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_int(stmt, 2, datos->cancha_id);
    sqlite3_bind_text(stmt, 3, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, datos->goles);
    sqlite3_bind_int(stmt, 5, datos->asistencias);
    sqlite3_bind_int(stmt, 6, datos->camiseta);
    sqlite3_bind_int(stmt, 7, datos->resultado);
    sqlite3_bind_int(stmt, 8, datos->rendimiento_general);
    sqlite3_bind_int(stmt, 9, datos->cansancio);
    sqlite3_bind_int(stmt, 10, datos->estado_animo);
    sqlite3_bind_text(stmt, 11, datos->comentario_personal, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 12, datos->clima);
    sqlite3_bind_int(stmt, 13, datos->dia);
    sqlite3_bind_int(stmt, 14, datos->precio);
    int result = sqlite3_step(stmt);
    if (result == SQLITE_DONE)
    {
        printf("Partido creado correctamente con ID %lld\n", id);
    }
    else
    {
        printf("Error al crear el partido: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Crea una transaccion financiera para el partido si no tiene precio asignado
 *
 * @param partido_id ID del partido
 * @param precio Precio del partido
 * @param cancha_id ID de la cancha
 */
static void crear_transaccion_partido(long long partido_id, int precio)
{
    // Verificar si el partido ya tiene una transaccion asociada
    sqlite3_stmt *stmt_check;
    const char *sql_check = "SELECT COUNT(*) FROM financiamiento WHERE tipo = 1 AND categoria = 6 AND item_especifico LIKE ?";
    char item_pattern[256];
    snprintf(item_pattern, sizeof(item_pattern), "Partido ID: %lld%%", partido_id);

    if (preparar_stmt(sql_check, &stmt_check))
    {
        sqlite3_bind_text(stmt_check, 1, item_pattern, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt_check) == SQLITE_ROW)
        {
            int count = sqlite3_column_int(stmt_check, 0);
            if (count > 0)
            {
                printf("El partido ya tiene una transaccion financiera asociada.\n");
                sqlite3_finalize(stmt_check);
                return;
            }
        }
        sqlite3_finalize(stmt_check);
    }

    // Crear la transaccion financiera
    TransaccionFinanciera transaccion;
    transaccion.id = (int)obtener_siguiente_id("financiamiento");
    obtener_fecha_actual(transaccion.fecha);
    transaccion.tipo = GASTO;
    transaccion.categoria = CANCHAS;
    transaccion.monto = precio;
    strcpy_s(transaccion.descripcion, sizeof(transaccion.descripcion), "Pago por alquiler de cancha");

    // Obtener detalles del partido para el item_especifico
    sqlite3_stmt *stmt_partido;
    const char *sql_partido = "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, clima, dia "
                              "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
                              "JOIN cancha can ON p.cancha_id = can.id WHERE p.id = ?";

    if (preparar_stmt(sql_partido, &stmt_partido))
    {
        sqlite3_bind_int64(stmt_partido, 1, partido_id);

        if (sqlite3_step(stmt_partido) == SQLITE_ROW)
        {
            // Formatear la fecha para visualizacion
            char fecha_formateada[20];
            format_date_for_display((const char *)sqlite3_column_text(stmt_partido, 2), fecha_formateada, sizeof(fecha_formateada));

            // Crear el string con los detalles del partido
            snprintf(transaccion.item_especifico, sizeof(transaccion.item_especifico), "(%lld |Cancha:%s |Fecha:%s | G:%d A:%d |Camiseta:%s | %s)",
                     sqlite3_column_int64(stmt_partido, 0),
                     sqlite3_column_text(stmt_partido, 1),
                     fecha_formateada,
                     sqlite3_column_int(stmt_partido, 3),
                     sqlite3_column_int(stmt_partido, 4),
                     sqlite3_column_text(stmt_partido, 5),
                     resultado_to_text(sqlite3_column_int(stmt_partido, 6)));
        }
        else
        {
            snprintf(transaccion.item_especifico, sizeof(transaccion.item_especifico), "Partido ID: %lld (no encontrado)", partido_id);
        }
        sqlite3_finalize(stmt_partido);
    }
    else
    {
        snprintf(transaccion.item_especifico, sizeof(transaccion.item_especifico), "Partido ID: %lld", partido_id);
    }

    // Insertar la transaccion en la base de datos
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO financiamiento (id, fecha, tipo, categoria, descripcion, monto, item_especifico) VALUES (?, ?, ?, ?, ?, ?, ?);";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, transaccion.id);
        sqlite3_bind_text(stmt, 2, transaccion.fecha, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, transaccion.tipo);
        sqlite3_bind_int(stmt, 4, transaccion.categoria);
        sqlite3_bind_text(stmt, 5, transaccion.descripcion, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 6, transaccion.monto);
        sqlite3_bind_text(stmt, 7, transaccion.item_especifico, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Transaccion financiera creada para el partido con ID %lld\n", partido_id);
        }
        else
        {
            printf("Error al crear la transaccion financiera: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        printf("Error al preparar la consulta de transaccion: %s\n", sqlite3_errmsg(db));
    }
}

/**
 * @brief Crea un nuevo partido en la base de datos
 *
 * Coordina la verificacion de prerrequisitos, recopilacion de datos y insercion
 * para asegurar un proceso robusto y modular de creacion de partidos.
 */
void crear_partido()
{
    if (IsWindowReady())
    {
        (void)crear_partido_gui();
        return;
    }

    // Activar IA antes de crear partido
    activar_ia_antes_partido();

    if (!verificar_prerrequisitos_partido())
        return;

    DatosPartido datos;
    listar_canchas_disponibles();
    if (!recopilar_datos_partido(&datos))
        return;

    char fecha[20];
    get_datetime(fecha, sizeof(fecha));
    long long id = obtener_siguiente_id("partido");
    insertar_partido(id, &datos, fecha);

    // Crear transaccion financiera si el precio es mayor a 0
    if (datos.precio > 0)
    {
        crear_transaccion_partido(id, datos.precio);
    }
}

/**
 * @brief Muestra un listado de todos los partidos registrados
 *
 * Consulta la base de datos y muestra en pantalla todos los partidos
 * con sus respectivos datos: ID, cancha, fecha/hora, goles, asistencias
 * y nombre de la camiseta utilizada. Realiza un JOIN con la tabla camiseta
 * para obtener el nombre de la camiseta.
 *
 * @note Si no hay partidos registrados, muestra un mensaje informativo
 */
/* Convierte Latin1 -> UTF-8 (solo para GUI) */
static void latin1_to_utf8(const unsigned char *src, char *dst, size_t dst_sz)
{
    if (!dst || dst_sz == 0)
    {
        return;
    }
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

typedef struct
{
    int id;
    int goles;
    int asistencias;
    int resultado;
    int rendimiento_general;
    int cansancio;
    int estado_animo;
    int clima;
    int dia;
    int precio;
    char fecha[64];
    char cancha[128];
    char camiseta[128];
    char marcador[48];
    char comentario[256];
} PartidoGuiRow;

static void partido_gui_set_fallback(char *dst, size_t dst_sz, const char *fallback)
{
    strncpy_s(dst, dst_sz, fallback, _TRUNCATE);
}

static int partido_gui_expand_rows(PartidoGuiRow **rows, int *cap)
{
    int new_cap = (*cap) * 2;
    PartidoGuiRow *tmp = (PartidoGuiRow *)realloc(*rows, (size_t)new_cap * sizeof(PartidoGuiRow));
    if (!tmp)
    {
        return 0;
    }

    memset(tmp + *cap, 0, (size_t)(new_cap - *cap) * sizeof(PartidoGuiRow));
    *rows = tmp;
    *cap = new_cap;
    return 1;
}

static void partido_gui_fill_row_from_stmt(PartidoGuiRow *row, sqlite3_stmt *stmt)
{
    row->id = sqlite3_column_int(stmt, 0);
    row->goles = sqlite3_column_int(stmt, 3);
    row->asistencias = sqlite3_column_int(stmt, 4);
    row->resultado = sqlite3_column_int(stmt, 6);
    row->rendimiento_general = sqlite3_column_int(stmt, 7);
    row->cansancio = sqlite3_column_int(stmt, 8);
    row->estado_animo = sqlite3_column_int(stmt, 9);
    row->clima = sqlite3_column_int(stmt, 11);
    row->dia = sqlite3_column_int(stmt, 12);
    row->precio = sqlite3_column_int(stmt, 13);

    {
        char fecha_buf[64] = {0};
        const unsigned char *col_fecha = sqlite3_column_text(stmt, 2);
        if (col_fecha)
        {
            format_date_for_display((const char *)col_fecha, fecha_buf, sizeof(fecha_buf));
        }
        else
        {
            partido_gui_set_fallback(fecha_buf, sizeof(fecha_buf), "(sin fecha)");
        }
        strncpy_s(row->fecha, sizeof(row->fecha), fecha_buf, _TRUNCATE);
    }

    {
        const unsigned char *col_cancha = sqlite3_column_text(stmt, 1);
        if (col_cancha)
        {
            latin1_to_utf8(col_cancha, row->cancha, sizeof(row->cancha));
        }
        else
        {
            partido_gui_set_fallback(row->cancha, sizeof(row->cancha), "(sin cancha)");
        }

        const unsigned char *col_cami = sqlite3_column_text(stmt, 5);
        if (col_cami)
        {
            latin1_to_utf8(col_cami, row->camiseta, sizeof(row->camiseta));
        }
        else
        {
            partido_gui_set_fallback(row->camiseta, sizeof(row->camiseta), "(sin camiseta)");
        }

        {
            const unsigned char *col_comentario = sqlite3_column_text(stmt, 10);
            if (col_comentario)
            {
                latin1_to_utf8(col_comentario, row->comentario, sizeof(row->comentario));
            }
            else
            {
                partido_gui_set_fallback(row->comentario, sizeof(row->comentario), "");
            }

            snprintf(row->marcador, sizeof(row->marcador), "Goles: %d Asistencias: %d", row->goles, row->asistencias);
        }
    }
}

static int partido_gui_detectar_click_fila_detalle(const PartidoGuiRow *rows, int count, int scroll, int row_h, Rectangle panel)
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

static const PartidoGuiRow *partido_gui_buscar_row_por_id(const PartidoGuiRow *rows, int count, int id)
{
    for (int i = 0; i < count; i++)
    {
        if (rows[i].id == id)
        {
            return &rows[i];
        }
    }

    return NULL;
}

static void partido_gui_mostrar_detalle(const PartidoGuiRow *row)
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
        int panel_w = (sw > 980) ? 860 : sw - 40;
        int panel_h = (sh > 760) ? 560 : sh - 120;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        int y = panel_y + 34;

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("DETALLE DEL PARTIDO", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text(TextFormat("ID: %d", row->id), (float)(panel_x + 24), (float)y, 21.0f, gui_get_active_theme()->text_primary);
        y += 34;
        gui_text(TextFormat("Fecha: %s", row->fecha), (float)(panel_x + 24), (float)y, 18.0f, gui_get_active_theme()->text_secondary);
        y += 30;
        gui_text_truncated(TextFormat("Cancha: %s", row->cancha), (float)(panel_x + 24), (float)y,
                           18.0f, (float)(panel_w - 48), gui_get_active_theme()->text_secondary);
        y += 30;
        gui_text_truncated(TextFormat("Camiseta: %s", row->camiseta), (float)(panel_x + 24), (float)y,
                           18.0f, (float)(panel_w - 48), gui_get_active_theme()->text_secondary);
        y += 30;
        gui_text(TextFormat("Goles: %d | Asistencias: %d", row->goles, row->asistencias), (float)(panel_x + 24), (float)y,
                 18.0f, gui_get_active_theme()->text_secondary);
        y += 30;
        gui_text(TextFormat("Resultado: %s", resultado_to_text(row->resultado)), (float)(panel_x + 24), (float)y,
                 18.0f, gui_get_active_theme()->text_secondary);
        y += 30;
        gui_text(TextFormat("Rendimiento General: %d/10", row->rendimiento_general), (float)(panel_x + 24), (float)y,
                 18.0f, gui_get_active_theme()->text_secondary);
        y += 30;
        gui_text(TextFormat("Cansancio: %d/10", row->cansancio), (float)(panel_x + 24), (float)y,
                 18.0f, gui_get_active_theme()->text_secondary);
        y += 30;
        gui_text(TextFormat("Estado de Animo: %d/10", row->estado_animo), (float)(panel_x + 24), (float)y,
                 18.0f, gui_get_active_theme()->text_secondary);
        y += 30;
        gui_text_truncated(TextFormat("Comentario Personal: %s", row->comentario[0] ? row->comentario : "N/A"),
                           (float)(panel_x + 24), (float)y, 18.0f,
                           (float)(panel_w - 48), gui_get_active_theme()->text_secondary);
        y += 30;
        gui_text(TextFormat("Clima: %s", clima_to_text(row->clima)), (float)(panel_x + 24), (float)y,
                 18.0f, gui_get_active_theme()->text_secondary);
        y += 30;
        gui_text(TextFormat("Dia: %s", dia_to_text(row->dia)), (float)(panel_x + 24), (float)y,
                 18.0f, gui_get_active_theme()->text_secondary);
        y += 30;
        gui_text(TextFormat("Precio: %d", row->precio), (float)(panel_x + 24), (float)y,
                 18.0f, gui_get_active_theme()->text_secondary);

        gui_draw_footer_hint("Esc/Enter: volver al listado", (float)panel_x, sh);
        EndDrawing();

        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            return;
        }
    }
}

static int cargar_partidos_gui_rows(PartidoGuiRow **rows_out, int *count_out)
{
    sqlite3_stmt *stmt = NULL;
    int cap = 64;
    int count = 0;
    PartidoGuiRow *rows = (PartidoGuiRow *)calloc((size_t)cap, sizeof(PartidoGuiRow));

    if (!rows)
    {
        return 0;
    }

    if (!preparar_stmt(
            "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, rendimiento_general, cansancio, estado_animo, comentario_personal, clima, dia, precio "
            "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
            "JOIN cancha can ON p.cancha_id = can.id ORDER BY p.id ASC",
            &stmt))
    {
        free(rows);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (count >= cap && !partido_gui_expand_rows(&rows, &cap))
        {
            sqlite3_finalize(stmt);
            free(rows);
            return 0;
        }

        partido_gui_fill_row_from_stmt(&rows[count], stmt);
        count++;
    }

    sqlite3_finalize(stmt);
    *rows_out = rows;
    *count_out = count;
    return 1;
}

static void partido_gui_calcular_panel(int sw, int sh, int *panel_x, int *panel_w, int *panel_h)
{
    *panel_x = (sw > 900) ? 80 : 20;
    *panel_w = sw - (*panel_x * 2);
    *panel_h = sh - 220;
    if (*panel_h < 200)
    {
        *panel_h = 200;
    }
}

static int partido_gui_clamp_scroll(int scroll, int count, int visible_rows)
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

static int partido_gui_actualizar_scroll(int scroll, int count, int visible_rows, Rectangle area)
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

    return partido_gui_clamp_scroll(scroll, count, visible_rows);
}

static void partido_gui_dibujar_filas(const PartidoGuiRow *rows,
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
        gui_text("No hay partidos cargados.", panel_x + 24, panel_y + 24, 24.0f, (Color){233,247,236,255});
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

        gui_text(TextFormat("%3d", rows[i].id), panel_x + 10, y + 7, 18.0f, (Color){220,238,225,255});
        gui_text(rows[i].fecha, panel_x + 70, y + 7, 18.0f, (Color){220,238,225,255});
        gui_text(rows[i].cancha, panel_x + 270, y + 7, 18.0f, (Color){233,247,236,255});
        gui_text(rows[i].marcador, panel_x + panel_w - 230, y + 7, 18.0f, (Color){233,247,236,255});
    }
    EndScissorMode();
}

static int partido_gui_should_close(void)
{
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
    {
        input_consume_key(KEY_ESCAPE);
        input_consume_key(KEY_ENTER);
        return 1;
    }
    return 0;
}

static int listar_partidos_gui(void)
{
    PartidoGuiRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 34;
    const int panel_y = 130;

    /* Consumir posibles pulsaciones previas que afectarían al modal */
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    if (!cargar_partidos_gui_rows(&rows, &count))
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

        partido_gui_calcular_panel(sw, sh, &panel_x, &panel_w, &panel_h);

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
        scroll = partido_gui_actualizar_scroll(scroll, count, visible_rows, area);
        clicked_id = partido_gui_detectar_click_fila_detalle(rows, count, scroll, row_h, area);

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("LISTADO DE PARTIDOS", sw);
        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                            "ID/FECHA", 10.0f,
                            "CANCHA / GOLES-ASISTENCIAS", 270.0f);

        partido_gui_dibujar_filas(rows, count, scroll, row_h, area);

        gui_draw_footer_hint("Click: ver detalle | Rueda: scroll | ESC/Enter: volver", (float)panel_x, sh);
        EndDrawing();

        if (clicked_id > 0)
        {
            const PartidoGuiRow *row = partido_gui_buscar_row_por_id(rows, count, clicked_id);
            partido_gui_mostrar_detalle(row);
            continue;
        }

        if (partido_gui_should_close())
        {
            break;
        }
    }

    free(rows);
    return 1;
}

void listar_partidos()
{
    (void)listar_partidos_gui();
}

typedef struct
{
    char cancha_id[16];
    char goles[16];
    char asistencias[16];
    char camiseta_id[16];
    char resultado[16];
    char rendimiento_general[16];
    char cansancio[16];
    char estado_animo[16];
    char clima[16];
    char dia[16];
    char precio[16];
    char comentario[256];
    int active_field;
    char error_msg[192];
} PartidoGuiEditForm;

enum
{
    PARTIDO_GUI_FIELD_CANCHA_ID = 0,
    PARTIDO_GUI_FIELD_GOLES,
    PARTIDO_GUI_FIELD_ASISTENCIAS,
    PARTIDO_GUI_FIELD_CAMISETA_ID,
    PARTIDO_GUI_FIELD_RESULTADO,
    PARTIDO_GUI_FIELD_RENDIMIENTO,
    PARTIDO_GUI_FIELD_CANSANCIO,
    PARTIDO_GUI_FIELD_ESTADO_ANIMO,
    PARTIDO_GUI_FIELD_CLIMA,
    PARTIDO_GUI_FIELD_DIA,
    PARTIDO_GUI_FIELD_PRECIO,
    PARTIDO_GUI_FIELD_COMENTARIO,
    PARTIDO_GUI_FIELD_COUNT
};

static const char *partido_gui_field_label(int field)
{
    switch (field)
    {
    case PARTIDO_GUI_FIELD_CANCHA_ID:
        return "Cancha ID";
    case PARTIDO_GUI_FIELD_GOLES:
        return "Goles";
    case PARTIDO_GUI_FIELD_ASISTENCIAS:
        return "Asistencias";
    case PARTIDO_GUI_FIELD_CAMISETA_ID:
        return "Camiseta ID";
    case PARTIDO_GUI_FIELD_RESULTADO:
        return "Resultado (1-3)";
    case PARTIDO_GUI_FIELD_RENDIMIENTO:
        return "Rendimiento (1-10)";
    case PARTIDO_GUI_FIELD_CANSANCIO:
        return "Cansancio (1-10)";
    case PARTIDO_GUI_FIELD_ESTADO_ANIMO:
        return "Estado animo (1-10)";
    case PARTIDO_GUI_FIELD_CLIMA:
        return "Clima (1-6)";
    case PARTIDO_GUI_FIELD_DIA:
        return "Dia (1-3)";
    case PARTIDO_GUI_FIELD_PRECIO:
        return "Precio";
    case PARTIDO_GUI_FIELD_COMENTARIO:
        return "Comentario";
    default:
        return "";
    }
}

static int partido_gui_field_is_numeric(int field)
{
    return field != PARTIDO_GUI_FIELD_COMENTARIO;
}

static char *partido_gui_field_value_mut(PartidoGuiEditForm *form, int field) /* NOSONAR */
{
    switch (field)
    {
    case PARTIDO_GUI_FIELD_CANCHA_ID:
        return form->cancha_id;
    case PARTIDO_GUI_FIELD_GOLES:
        return form->goles;
    case PARTIDO_GUI_FIELD_ASISTENCIAS:
        return form->asistencias;
    case PARTIDO_GUI_FIELD_CAMISETA_ID:
        return form->camiseta_id;
    case PARTIDO_GUI_FIELD_RESULTADO:
        return form->resultado;
    case PARTIDO_GUI_FIELD_RENDIMIENTO:
        return form->rendimiento_general;
    case PARTIDO_GUI_FIELD_CANSANCIO:
        return form->cansancio;
    case PARTIDO_GUI_FIELD_ESTADO_ANIMO:
        return form->estado_animo;
    case PARTIDO_GUI_FIELD_CLIMA:
        return form->clima;
    case PARTIDO_GUI_FIELD_DIA:
        return form->dia;
    case PARTIDO_GUI_FIELD_PRECIO:
        return form->precio;
    case PARTIDO_GUI_FIELD_COMENTARIO:
        return form->comentario;
    default:
        return NULL;
    }
}

static const char *partido_gui_field_value(const PartidoGuiEditForm *form, int field) /* NOSONAR */
{
    switch (field)
    {
    case PARTIDO_GUI_FIELD_CANCHA_ID:
        return form->cancha_id;
    case PARTIDO_GUI_FIELD_GOLES:
        return form->goles;
    case PARTIDO_GUI_FIELD_ASISTENCIAS:
        return form->asistencias;
    case PARTIDO_GUI_FIELD_CAMISETA_ID:
        return form->camiseta_id;
    case PARTIDO_GUI_FIELD_RESULTADO:
        return form->resultado;
    case PARTIDO_GUI_FIELD_RENDIMIENTO:
        return form->rendimiento_general;
    case PARTIDO_GUI_FIELD_CANSANCIO:
        return form->cansancio;
    case PARTIDO_GUI_FIELD_ESTADO_ANIMO:
        return form->estado_animo;
    case PARTIDO_GUI_FIELD_CLIMA:
        return form->clima;
    case PARTIDO_GUI_FIELD_DIA:
        return form->dia;
    case PARTIDO_GUI_FIELD_PRECIO:
        return form->precio;
    case PARTIDO_GUI_FIELD_COMENTARIO:
        return form->comentario;
    default:
        return "";
    }
}

static size_t partido_gui_field_capacity(int field)
{
    if (field == PARTIDO_GUI_FIELD_COMENTARIO)
    {
        return 256;
    }
    return 16;
}

static int partido_gui_parse_int(const char *text, int *out)
{
    char *end = NULL;
    long value = 0;

    if (!text || !out || text[0] == '\0')
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

static int partido_gui_form_load_by_id(int partido_id, PartidoGuiEditForm *form)
{
    sqlite3_stmt *stmt = NULL;

    if (!form)
    {
        return 0;
    }

    if (!preparar_stmt(
            "SELECT cancha_id, goles, asistencias, camiseta_id, resultado, rendimiento_general, cansancio, estado_animo, comentario_personal, clima, dia, precio "
            "FROM partido WHERE id = ?",
            &stmt))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, partido_id);
    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    memset(form, 0, sizeof(*form));
    snprintf(form->cancha_id, sizeof(form->cancha_id), "%d", sqlite3_column_int(stmt, 0));
    snprintf(form->goles, sizeof(form->goles), "%d", sqlite3_column_int(stmt, 1));
    snprintf(form->asistencias, sizeof(form->asistencias), "%d", sqlite3_column_int(stmt, 2));
    snprintf(form->camiseta_id, sizeof(form->camiseta_id), "%d", sqlite3_column_int(stmt, 3));
    snprintf(form->resultado, sizeof(form->resultado), "%d", sqlite3_column_int(stmt, 4));
    snprintf(form->rendimiento_general, sizeof(form->rendimiento_general), "%d", sqlite3_column_int(stmt, 5));
    snprintf(form->cansancio, sizeof(form->cansancio), "%d", sqlite3_column_int(stmt, 6));
    snprintf(form->estado_animo, sizeof(form->estado_animo), "%d", sqlite3_column_int(stmt, 7));
    snprintf(form->comentario, sizeof(form->comentario), "%s",
             sqlite3_column_text(stmt, 8) ? (const char *)sqlite3_column_text(stmt, 8) : "");
    snprintf(form->clima, sizeof(form->clima), "%d", sqlite3_column_int(stmt, 9));
    snprintf(form->dia, sizeof(form->dia), "%d", sqlite3_column_int(stmt, 10));
    snprintf(form->precio, sizeof(form->precio), "%d", sqlite3_column_int(stmt, 11));
    form->active_field = PARTIDO_GUI_FIELD_CANCHA_ID;
    form->error_msg[0] = '\0';

    sqlite3_finalize(stmt);
    return 1;
}

static int partido_gui_form_validate(const PartidoGuiEditForm *form, DatosPartido *datos, char *err, size_t err_sz)
{
    if (!form || !datos || !err || err_sz == 0)
    {
        return 0;
    }

    if (!partido_gui_parse_int(form->cancha_id, &datos->cancha_id) || datos->cancha_id <= 0 || !existe_id("cancha", datos->cancha_id))
    {
        snprintf(err, err_sz, "Cancha ID invalido");
        return 0;
    }

    if (!partido_gui_parse_int(form->goles, &datos->goles) || datos->goles < 0)
    {
        snprintf(err, err_sz, "Goles invalido");
        return 0;
    }

    if (!partido_gui_parse_int(form->asistencias, &datos->asistencias) || datos->asistencias < 0)
    {
        snprintf(err, err_sz, "Asistencias invalido");
        return 0;
    }

    if (!partido_gui_parse_int(form->camiseta_id, &datos->camiseta) || datos->camiseta <= 0 || !existe_id("camiseta", datos->camiseta))
    {
        snprintf(err, err_sz, "Camiseta ID invalido");
        return 0;
    }

    if (!partido_gui_parse_int(form->resultado, &datos->resultado) || datos->resultado < 1 || datos->resultado > 3)
    {
        snprintf(err, err_sz, "Resultado debe ser 1, 2 o 3");
        return 0;
    }

    if (!partido_gui_parse_int(form->rendimiento_general, &datos->rendimiento_general) ||
            datos->rendimiento_general < 1 || datos->rendimiento_general > 10)
    {
        snprintf(err, err_sz, "Rendimiento debe estar entre 1 y 10");
        return 0;
    }

    if (!partido_gui_parse_int(form->cansancio, &datos->cansancio) || datos->cansancio < 1 || datos->cansancio > 10)
    {
        snprintf(err, err_sz, "Cansancio debe estar entre 1 y 10");
        return 0;
    }

    if (!partido_gui_parse_int(form->estado_animo, &datos->estado_animo) || datos->estado_animo < 1 || datos->estado_animo > 10)
    {
        snprintf(err, err_sz, "Estado de animo debe estar entre 1 y 10");
        return 0;
    }

    if (!partido_gui_parse_int(form->clima, &datos->clima) || datos->clima < 1 || datos->clima > 6)
    {
        snprintf(err, err_sz, "Clima debe estar entre 1 y 6");
        return 0;
    }

    if (!partido_gui_parse_int(form->dia, &datos->dia) || datos->dia < 1 || datos->dia > 3)
    {
        snprintf(err, err_sz, "Dia debe ser 1, 2 o 3");
        return 0;
    }

    if (!partido_gui_parse_int(form->precio, &datos->precio) || datos->precio < 0)
    {
        snprintf(err, err_sz, "Precio invalido");
        return 0;
    }

    snprintf(datos->comentario_personal, sizeof(datos->comentario_personal), "%s", form->comentario);
    return 1;
}

static int partido_gui_insertar_datos(const DatosPartido *datos, long long *id_out)
{
    sqlite3_stmt *stmt = NULL;
    long long id;
    char fecha[20] = {0};

    if (!datos)
    {
        return 0;
    }

    id = obtener_siguiente_id("partido");
    get_datetime(fecha, sizeof(fecha));

    if (!preparar_stmt(
            "INSERT INTO partido(id, cancha_id, fecha_hora, goles, asistencias, camiseta_id, resultado, rendimiento_general, cansancio, estado_animo, comentario_personal, clima, dia, precio) "
            "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
            &stmt))
    {
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_int(stmt, 2, datos->cancha_id);
    sqlite3_bind_text(stmt, 3, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, datos->goles);
    sqlite3_bind_int(stmt, 5, datos->asistencias);
    sqlite3_bind_int(stmt, 6, datos->camiseta);
    sqlite3_bind_int(stmt, 7, datos->resultado);
    sqlite3_bind_int(stmt, 8, datos->rendimiento_general);
    sqlite3_bind_int(stmt, 9, datos->cansancio);
    sqlite3_bind_int(stmt, 10, datos->estado_animo);
    sqlite3_bind_text(stmt, 11, datos->comentario_personal, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 12, datos->clima);
    sqlite3_bind_int(stmt, 13, datos->dia);
    sqlite3_bind_int(stmt, 14, datos->precio);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    if (datos->precio > 0)
    {
        crear_transaccion_partido(id, datos->precio);
    }
    if (id_out)
    {
        *id_out = id;
    }
    return 1;
}

static int partido_gui_actualizar_datos(int partido_id, const DatosPartido *datos)
{
    sqlite3_stmt *stmt = NULL;

    if (!datos)
    {
        return 0;
    }

    if (!preparar_stmt(
            "UPDATE partido SET cancha_id=?, goles=?, asistencias=?, camiseta_id=?, resultado=?, rendimiento_general=?, cansancio=?, estado_animo=?, comentario_personal=?, clima=?, dia=?, precio=? WHERE id=?",
            &stmt))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, datos->cancha_id);
    sqlite3_bind_int(stmt, 2, datos->goles);
    sqlite3_bind_int(stmt, 3, datos->asistencias);
    sqlite3_bind_int(stmt, 4, datos->camiseta);
    sqlite3_bind_int(stmt, 5, datos->resultado);
    sqlite3_bind_int(stmt, 6, datos->rendimiento_general);
    sqlite3_bind_int(stmt, 7, datos->cansancio);
    sqlite3_bind_int(stmt, 8, datos->estado_animo);
    sqlite3_bind_text(stmt, 9, datos->comentario_personal, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 10, datos->clima);
    sqlite3_bind_int(stmt, 11, datos->dia);
    sqlite3_bind_int(stmt, 12, datos->precio);
    sqlite3_bind_int(stmt, 13, partido_id);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    return 1;
}

static int partido_gui_eliminar_por_id(int partido_id)
{
    sqlite3_stmt *stmt = NULL;

    if (!preparar_stmt("DELETE FROM partido WHERE id = ?", &stmt))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, partido_id);
    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    return 1;
}

static Rectangle partido_gui_form_field_rect(int panel_x, int panel_y, int panel_w, int field)
{
    int col_w = (panel_w - 64) / 2;
    int col = field / 6;
    int row = field % 6;
    float x = (float)(panel_x + 24 + col * (col_w + 16));
    float y = (float)(panel_y + 96 + row * 58);
    return (Rectangle){x, y, (float)col_w, 34.0f};
}

static void partido_gui_form_handle_input(PartidoGuiEditForm *form)
{
    char *value;
    size_t cap;
    int key;
    size_t len;

    if (!form)
    {
        return;
    }

    value = partido_gui_field_value_mut(form, form->active_field);
    if (!value)
    {
        return;
    }

    cap = partido_gui_field_capacity(form->active_field);
    key = GetCharPressed();
    while (key > 0)
    {
        if (key >= 32 && key <= 126 &&
                (!partido_gui_field_is_numeric(form->active_field) || isdigit((unsigned char)key)))
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

static int partido_gui_detectar_click_fila(const PartidoGuiRow *rows, int count, int scroll, int row_h, Rectangle panel)
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

static void partido_gui_feedback(const char *title, const char *message, int ok)
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
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, ok ? (Color){86,170,120,255} : (Color){170,86,86,255});
        gui_text(message, (float)(panel_x + 24), (float)(panel_y + 62), 22.0f, gui_get_active_theme()->text_primary);
        DrawRectangleRec(btn_ok, gui_get_active_theme()->accent_primary);
        DrawRectangleLines((int)btn_ok.x, (int)btn_ok.y, (int)btn_ok.width, (int)btn_ok.height, gui_get_active_theme()->card_border);
        gui_text("Aceptar", btn_ok.x + 40.0f, btn_ok.y + 8.0f, 18.0f, gui_get_active_theme()->text_primary);
        gui_draw_footer_hint("Enter/Esc: volver", (float)panel_x, sh);
        EndDrawing();

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || (click && CheckCollisionPointRec(GetMousePosition(), btn_ok)))
        {
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            return;
        }
    }
}

static int partido_gui_confirmar_eliminar(int partido_id)
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
        gui_draw_module_header("ELIMINAR PARTIDO", sw);
        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);
        gui_text(TextFormat("Desea eliminar el partido ID %d?", partido_id),
                 (float)(panel_x + 24), (float)(panel_y + 66), 22.0f, gui_get_active_theme()->text_primary);

        DrawRectangleRec(btn_yes, (Color){168,70,70,255});
        DrawRectangleRec(btn_no, gui_get_active_theme()->accent_primary);
        DrawRectangleLines((int)btn_yes.x, (int)btn_yes.y, (int)btn_yes.width, (int)btn_yes.height, gui_get_active_theme()->card_border);
        DrawRectangleLines((int)btn_no.x, (int)btn_no.y, (int)btn_no.width, (int)btn_no.height, gui_get_active_theme()->card_border);
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

static int partido_gui_seleccionar_partido_id(const char *title, const char *hint, int *id_out)
{
    PartidoGuiRow *rows = NULL;
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

    if (!cargar_partidos_gui_rows(&rows, &count))
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

        partido_gui_calcular_panel(sw, sh, &panel_x, &panel_w, &panel_h);
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
        scroll = partido_gui_actualizar_scroll(scroll, count, visible_rows, area);
        clicked_id = partido_gui_detectar_click_fila(rows, count, scroll, row_h, area);

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header(title, sw);
        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                            "ID/FECHA", 10.0f,
                            "CANCHA / GOLES-ASISTENCIAS", 270.0f);
        partido_gui_dibujar_filas(rows, count, scroll, row_h, area);
        gui_draw_footer_hint(hint, (float)panel_x, sh);
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

static int partido_gui_run_form_guardar(PartidoGuiEditForm *form, int is_create, int partido_id)
{
    DatosPartido datos;
    char err[160] = {0};
    int ok;

    if (!form)
    {
        return 0;
    }

    if (!partido_gui_form_validate(form, &datos, err, sizeof(err)))
    {
        snprintf(form->error_msg, sizeof(form->error_msg), "%s", err);
        return 0;
    }

    ok = is_create ? partido_gui_insertar_datos(&datos, NULL)
         : partido_gui_actualizar_datos(partido_id, &datos);

    if (!ok)
    {
        snprintf(form->error_msg, sizeof(form->error_msg), "No se pudo guardar el partido");
        return 0;
    }

    form->error_msg[0] = '\0';
    return 1;
}

static int partido_gui_run_form(const char *title, PartidoGuiEditForm *form, int is_create, int partido_id) /* NOSONAR */
{
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);
    input_consume_key(KEY_TAB);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = (sw > 980) ? 50 : 20;
        int panel_y = 98;
        int panel_w = sw - panel_x * 2;
        int panel_h = sh - 170;
        Rectangle btn_save;
        Rectangle btn_cancel;
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        int save_trigger = 0;
        int cancel_trigger = 0;
        Vector2 mouse = GetMousePosition();

        if (panel_h < 470)
        {
            panel_h = 470;
        }

        btn_save = (Rectangle){(float)(panel_x + panel_w - 360), (float)(panel_y + panel_h - 52), 160.0f, 36.0f};
        btn_cancel = (Rectangle){(float)(panel_x + panel_w - 184), (float)(panel_y + panel_h - 52), 160.0f, 36.0f};

        for (int i = 0; i < PARTIDO_GUI_FIELD_COUNT; i++)
        {
            Rectangle r = partido_gui_form_field_rect(panel_x, panel_y, panel_w, i);
            if (click && CheckCollisionPointRec(mouse, r))
            {
                form->active_field = i;
            }
        }

        if (click && CheckCollisionPointRec(mouse, btn_save))
        {
            save_trigger = 1;
        }
        else if (click && CheckCollisionPointRec(mouse, btn_cancel))
        {
            cancel_trigger = 1;
        }

        if (IsKeyPressed(KEY_TAB))
        {
            form->active_field = (form->active_field + 1) % PARTIDO_GUI_FIELD_COUNT;
        }

        partido_gui_form_handle_input(form);

        if (IsKeyPressed(KEY_ENTER))
        {
            save_trigger = 1;
        }
        if (IsKeyPressed(KEY_ESCAPE))
        {
            cancel_trigger = 1;
        }

        if (save_trigger && partido_gui_run_form_guardar(form, is_create, partido_id))
        {
            return 1;
        }

        if (cancel_trigger)
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header(title, sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);
        gui_text("TAB/click para cambiar campo | Enter para guardar", (float)(panel_x + 24), (float)(panel_y + 24),
                 18.0f, gui_get_active_theme()->text_secondary);

        for (int i = 0; i < PARTIDO_GUI_FIELD_COUNT; i++)
        {
            Rectangle r = partido_gui_form_field_rect(panel_x, panel_y, panel_w, i);
            int active = (i == form->active_field);

            gui_text(partido_gui_field_label(i), r.x, r.y - 20.0f, 16.0f, gui_get_active_theme()->text_secondary);
            DrawRectangleRec(r, active ? (Color){28,56,40,255} : (Color){18,36,28,255});
            DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height,
                               active ? gui_get_active_theme()->accent_primary : gui_get_active_theme()->card_border);
            gui_text_truncated(partido_gui_field_value(form, i), r.x + 8.0f, r.y + 8.0f, 16.0f,
                               r.width - 16.0f, gui_get_active_theme()->text_primary);
        }

        DrawRectangleRec(btn_save, gui_get_active_theme()->accent_primary);
        DrawRectangleRec(btn_cancel, (Color){77,92,80,255});
        DrawRectangleLines((int)btn_save.x, (int)btn_save.y, (int)btn_save.width, (int)btn_save.height, gui_get_active_theme()->card_border);
        DrawRectangleLines((int)btn_cancel.x, (int)btn_cancel.y, (int)btn_cancel.width, (int)btn_cancel.height, gui_get_active_theme()->card_border);
        gui_text("Guardar", btn_save.x + 42.0f, btn_save.y + 8.0f, 18.0f, gui_get_active_theme()->text_primary);
        gui_text("Cancelar", btn_cancel.x + 38.0f, btn_cancel.y + 8.0f, 18.0f, gui_get_active_theme()->text_primary);

        if (form->error_msg[0] != '\0')
        {
            gui_text(form->error_msg, (float)(panel_x + 24), (float)(panel_y + panel_h - 44),
                     18.0f, (Color){232,120,120,255});
        }

        gui_draw_footer_hint("Esc: volver", (float)panel_x, sh);
        EndDrawing();
    }

    return 0;
}

typedef struct
{
    int value;
    char label[128];
} PartidoGuiSelectOption;

enum
{
    PARTIDO_CREATE_DD_NONE = 0,
    PARTIDO_CREATE_DD_CANCHA,
    PARTIDO_CREATE_DD_CAMISETA,
    PARTIDO_CREATE_DD_RESULTADO,
    PARTIDO_CREATE_DD_CLIMA,
    PARTIDO_CREATE_DD_DIA,
    PARTIDO_CREATE_DD_COUNT
};

enum
{
    PARTIDO_CREATE_INPUT_GOLES = 0,
    PARTIDO_CREATE_INPUT_ASISTENCIAS,
    PARTIDO_CREATE_INPUT_RENDIMIENTO,
    PARTIDO_CREATE_INPUT_CANSANCIO,
    PARTIDO_CREATE_INPUT_ESTADO_ANIMO,
    PARTIDO_CREATE_INPUT_PRECIO,
    PARTIDO_CREATE_INPUT_COMENTARIO,
    PARTIDO_CREATE_INPUT_COUNT
};

typedef struct
{
    PartidoGuiSelectOption *canchas_storage;
    PartidoGuiSelectOption *camisetas_storage;
    const PartidoGuiSelectOption *dd_options[PARTIDO_CREATE_DD_COUNT];
    int dd_counts[PARTIDO_CREATE_DD_COUNT];
    int dd_selected[PARTIDO_CREATE_DD_COUNT];
    int dd_scroll[PARTIDO_CREATE_DD_COUNT];
    int open_dropdown;
    char goles[16];
    char asistencias[16];
    char rendimiento_general[16];
    char cansancio[16];
    char estado_animo[16];
    char precio[16];
    char comentario[256];
    int active_input;
    int input_touched[PARTIDO_CREATE_INPUT_COUNT];
    char error_msg[192];
} PartidoGuiCreateForm;

static const PartidoGuiSelectOption kPartidoResultadoOpts[] =
{
    {1, "Victoria"},
    {2, "Empate"},
    {3, "Derrota"}
};

static const PartidoGuiSelectOption kPartidoClimaOpts[] =
{
    {1, "Despejado"},
    {2, "Nublado"},
    {3, "Lluvia"},
    {4, "Ventoso"},
    {5, "Mucho Calor"},
    {6, "Mucho Frio"}
};

static const PartidoGuiSelectOption kPartidoDiaOpts[] =
{
    {1, "Dia"},
    {2, "Tarde"},
    {3, "Noche"}
};

static int partido_gui_cargar_opciones_select(const char *sql, PartidoGuiSelectOption **rows_out, int *count_out)
{
    sqlite3_stmt *stmt = NULL;
    int cap = 32;
    int count = 0;
    PartidoGuiSelectOption *rows;

    if (!rows_out || !count_out)
    {
        return 0;
    }

    rows = (PartidoGuiSelectOption *)calloc((size_t)cap, sizeof(PartidoGuiSelectOption));
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
        const unsigned char *txt;

        if (count >= cap)
        {
            int new_cap = cap * 2;
            PartidoGuiSelectOption *tmp = (PartidoGuiSelectOption *)realloc(rows, (size_t)new_cap * sizeof(PartidoGuiSelectOption));
            if (!tmp)
            {
                sqlite3_finalize(stmt);
                free(rows);
                return 0;
            }

            memset(tmp + cap, 0, (size_t)(new_cap - cap) * sizeof(PartidoGuiSelectOption));
            rows = tmp;
            cap = new_cap;
        }

        rows[count].value = sqlite3_column_int(stmt, 0);
        txt = sqlite3_column_text(stmt, 1);
        if (txt)
        {
            latin1_to_utf8(txt, rows[count].label, sizeof(rows[count].label));
        }
        else
        {
            snprintf(rows[count].label, sizeof(rows[count].label), "(sin nombre)");
        }
        count++;
    }

    sqlite3_finalize(stmt);

    *rows_out = rows;
    *count_out = count;
    return 1;
}

static Rectangle partido_gui_create_dropdown_rect(int panel_x, int panel_y, int panel_w, int dd)
{
    int two_col_w = (panel_w - 64) / 2;
    int three_col_w = (panel_w - 80) / 3;

    switch (dd)
    {
    case PARTIDO_CREATE_DD_CANCHA:
        return (Rectangle){(float)(panel_x + 24), (float)(panel_y + 94), (float)two_col_w, 36.0f};
    case PARTIDO_CREATE_DD_CAMISETA:
        return (Rectangle){(float)(panel_x + 40 + two_col_w), (float)(panel_y + 94), (float)two_col_w, 36.0f};
    case PARTIDO_CREATE_DD_RESULTADO:
        return (Rectangle){(float)(panel_x + 24), (float)(panel_y + 160), (float)three_col_w, 36.0f};
    case PARTIDO_CREATE_DD_CLIMA:
        return (Rectangle){(float)(panel_x + 40 + three_col_w), (float)(panel_y + 160), (float)three_col_w, 36.0f};
    case PARTIDO_CREATE_DD_DIA:
        return (Rectangle){(float)(panel_x + 56 + three_col_w * 2), (float)(panel_y + 160), (float)three_col_w, 36.0f};
    default:
        return (Rectangle){0};
    }
}

static Rectangle partido_gui_create_input_rect(int panel_x, int panel_y, int panel_w, int input_idx)
{
    int margin = 24;
    int gap = 16;
    int col_w = (panel_w - margin * 2 - gap * 2) / 3;
    int x1 = panel_x + margin;
    int x2 = x1 + col_w + gap;
    int x3 = x2 + col_w + gap;
    int y0 = panel_y + 246;
    int y1 = y0 + 60;
    int y2 = y1 + 60;

    switch (input_idx)
    {
    case PARTIDO_CREATE_INPUT_GOLES:
        return (Rectangle){(float)x1, (float)y0, (float)col_w, 34.0f};
    case PARTIDO_CREATE_INPUT_ASISTENCIAS:
        return (Rectangle){(float)x2, (float)y0, (float)col_w, 34.0f};
    case PARTIDO_CREATE_INPUT_RENDIMIENTO:
        return (Rectangle){(float)x3, (float)y0, (float)col_w, 34.0f};
    case PARTIDO_CREATE_INPUT_CANSANCIO:
        return (Rectangle){(float)x1, (float)y1, (float)col_w, 34.0f};
    case PARTIDO_CREATE_INPUT_ESTADO_ANIMO:
        return (Rectangle){(float)x2, (float)y1, (float)col_w, 34.0f};
    case PARTIDO_CREATE_INPUT_PRECIO:
        return (Rectangle){(float)x3, (float)y1, (float)col_w, 34.0f};
    case PARTIDO_CREATE_INPUT_COMENTARIO:
        return (Rectangle){(float)x1, (float)y2, (float)(panel_w - margin * 2), 36.0f};
    default:
        return (Rectangle){0};
    }
}

static const char *partido_gui_create_dd_label(int dd)
{
    switch (dd)
    {
    case PARTIDO_CREATE_DD_CANCHA:
        return "Cancha";
    case PARTIDO_CREATE_DD_CAMISETA:
        return "Camiseta";
    case PARTIDO_CREATE_DD_RESULTADO:
        return "Resultado";
    case PARTIDO_CREATE_DD_CLIMA:
        return "Clima";
    case PARTIDO_CREATE_DD_DIA:
        return "Dia";
    default:
        return "";
    }
}

static const char *partido_gui_create_input_label(int input_idx)
{
    switch (input_idx)
    {
    case PARTIDO_CREATE_INPUT_GOLES:
        return "Goles";
    case PARTIDO_CREATE_INPUT_ASISTENCIAS:
        return "Asistencias";
    case PARTIDO_CREATE_INPUT_RENDIMIENTO:
        return "Rendimiento";
    case PARTIDO_CREATE_INPUT_CANSANCIO:
        return "Cansancio";
    case PARTIDO_CREATE_INPUT_ESTADO_ANIMO:
        return "Estado animo";
    case PARTIDO_CREATE_INPUT_PRECIO:
        return "Precio";
    case PARTIDO_CREATE_INPUT_COMENTARIO:
        return "Comentario";
    default:
        return "";
    }
}

static char *partido_gui_create_input_value_mut(PartidoGuiCreateForm *form, int input_idx)
{
    switch (input_idx)
    {
    case PARTIDO_CREATE_INPUT_GOLES:
        return form->goles;
    case PARTIDO_CREATE_INPUT_ASISTENCIAS:
        return form->asistencias;
    case PARTIDO_CREATE_INPUT_RENDIMIENTO:
        return form->rendimiento_general;
    case PARTIDO_CREATE_INPUT_CANSANCIO:
        return form->cansancio;
    case PARTIDO_CREATE_INPUT_ESTADO_ANIMO:
        return form->estado_animo;
    case PARTIDO_CREATE_INPUT_PRECIO:
        return form->precio;
    case PARTIDO_CREATE_INPUT_COMENTARIO:
        return form->comentario;
    default:
        return NULL;
    }
}

static const char *partido_gui_create_input_value(const PartidoGuiCreateForm *form, int input_idx)
{
    switch (input_idx)
    {
    case PARTIDO_CREATE_INPUT_GOLES:
        return form->goles;
    case PARTIDO_CREATE_INPUT_ASISTENCIAS:
        return form->asistencias;
    case PARTIDO_CREATE_INPUT_RENDIMIENTO:
        return form->rendimiento_general;
    case PARTIDO_CREATE_INPUT_CANSANCIO:
        return form->cansancio;
    case PARTIDO_CREATE_INPUT_ESTADO_ANIMO:
        return form->estado_animo;
    case PARTIDO_CREATE_INPUT_PRECIO:
        return form->precio;
    case PARTIDO_CREATE_INPUT_COMENTARIO:
        return form->comentario;
    default:
        return "";
    }
}

static size_t partido_gui_create_input_cap(int input_idx)
{
    if (input_idx == PARTIDO_CREATE_INPUT_COMENTARIO)
    {
        return 256;
    }
    return 16;
}

static int partido_gui_create_input_numeric(int input_idx)
{
    return input_idx != PARTIDO_CREATE_INPUT_COMENTARIO;
}

static void partido_gui_create_focus_input(PartidoGuiCreateForm *form, int input_idx)
{
    char *value;

    if (!form || input_idx < 0 || input_idx >= PARTIDO_CREATE_INPUT_COUNT)
    {
        return;
    }

    form->active_input = input_idx;
    if (form->input_touched[input_idx])
    {
        return;
    }

    value = partido_gui_create_input_value_mut(form, input_idx);
    if (value && partido_gui_create_input_numeric(input_idx) && strcmp(value, "0") == 0)
    {
        value[0] = '\0';
    }

    form->input_touched[input_idx] = 1;
}

static void partido_gui_create_draw_active_indicator(Rectangle rect, const char *value)
{
    int txt_w = MeasureText(value ? value : "", 15);
    int caret_x = (int)rect.x + 8 + txt_w;
    int caret_y = (int)rect.y + 7;
    int max_caret_x = (int)(rect.x + rect.width) - 10;

    if (caret_x > max_caret_x)
    {
        caret_x = max_caret_x;
    }

    DrawRectangle((int)rect.x, (int)(rect.y + rect.height - 2.0f), (int)rect.width, 2, gui_get_active_theme()->accent_primary);
    DrawRectangle(caret_x, caret_y, 2, 18, gui_get_active_theme()->accent_primary);
}

static void partido_gui_create_defaults(PartidoGuiCreateForm *form)
{
    memset(form, 0, sizeof(*form));

    form->dd_options[PARTIDO_CREATE_DD_RESULTADO] = kPartidoResultadoOpts;
    form->dd_counts[PARTIDO_CREATE_DD_RESULTADO] = (int)(sizeof(kPartidoResultadoOpts) / sizeof(kPartidoResultadoOpts[0]));
    form->dd_options[PARTIDO_CREATE_DD_CLIMA] = kPartidoClimaOpts;
    form->dd_counts[PARTIDO_CREATE_DD_CLIMA] = (int)(sizeof(kPartidoClimaOpts) / sizeof(kPartidoClimaOpts[0]));
    form->dd_options[PARTIDO_CREATE_DD_DIA] = kPartidoDiaOpts;
    form->dd_counts[PARTIDO_CREATE_DD_DIA] = (int)(sizeof(kPartidoDiaOpts) / sizeof(kPartidoDiaOpts[0]));

    form->dd_selected[PARTIDO_CREATE_DD_RESULTADO] = 1; /* Empate */
    form->dd_selected[PARTIDO_CREATE_DD_CLIMA] = 0;     /* Despejado */
    form->dd_selected[PARTIDO_CREATE_DD_DIA] = 0;       /* Dia */

    snprintf(form->goles, sizeof(form->goles), "%d", 0);
    snprintf(form->asistencias, sizeof(form->asistencias), "%d", 0);
    snprintf(form->rendimiento_general, sizeof(form->rendimiento_general), "%d", 5);
    snprintf(form->cansancio, sizeof(form->cansancio), "%d", 5);
    snprintf(form->estado_animo, sizeof(form->estado_animo), "%d", 5);
    snprintf(form->precio, sizeof(form->precio), "%d", 0);
    form->comentario[0] = '\0';
    partido_gui_create_focus_input(form, PARTIDO_CREATE_INPUT_GOLES);
}

static void partido_gui_create_free(PartidoGuiCreateForm *form)
{
    free(form->canchas_storage);
    free(form->camisetas_storage);
    form->canchas_storage = NULL;
    form->camisetas_storage = NULL;
}

static int partido_gui_create_load_catalogs(PartidoGuiCreateForm *form)
{
    int ok_canchas;
    int ok_camisetas;

    ok_canchas = partido_gui_cargar_opciones_select("SELECT id, nombre FROM cancha ORDER BY id", &form->canchas_storage, &form->dd_counts[PARTIDO_CREATE_DD_CANCHA]);
    ok_camisetas = partido_gui_cargar_opciones_select("SELECT id, nombre FROM camiseta ORDER BY id", &form->camisetas_storage, &form->dd_counts[PARTIDO_CREATE_DD_CAMISETA]);
    if (!ok_canchas || !ok_camisetas)
    {
        partido_gui_create_free(form);
        return 0;
    }

    form->dd_options[PARTIDO_CREATE_DD_CANCHA] = form->canchas_storage;
    form->dd_options[PARTIDO_CREATE_DD_CAMISETA] = form->camisetas_storage;
    return (form->dd_counts[PARTIDO_CREATE_DD_CANCHA] > 0 && form->dd_counts[PARTIDO_CREATE_DD_CAMISETA] > 0) ? 1 : 0;
}

static Rectangle partido_gui_create_dropdown_list_rect(Rectangle base, int options_count)
{
    int visible = options_count;
    if (visible > 6)
    {
        visible = 6;
    }
    return (Rectangle){base.x, base.y + base.height + 4.0f, base.width, (float)(visible * 30)};
}

static void partido_gui_create_draw_dropdown(const PartidoGuiCreateForm *form, int dd, Rectangle rect)
{
    const PartidoGuiSelectOption *opts = form->dd_options[dd];
    int count = form->dd_counts[dd];
    int selected = form->dd_selected[dd];
    const char *label = (count > 0 && selected >= 0 && selected < count) ? opts[selected].label : "(sin opciones)";
    int open = (form->open_dropdown == dd);

    gui_text(partido_gui_create_dd_label(dd), rect.x, rect.y - 18.0f, 16.0f, gui_get_active_theme()->text_secondary);
    DrawRectangleRec(rect, open ? (Color){28,56,40,255} : (Color){18,36,28,255});
    DrawRectangleLines((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height,
                       open ? gui_get_active_theme()->accent_primary : gui_get_active_theme()->card_border);
    gui_text_truncated(label, rect.x + 8.0f, rect.y + 8.0f, 16.0f, rect.width - 28.0f, gui_get_active_theme()->text_primary);
    gui_text(open ? "^" : "v", rect.x + rect.width - 16.0f, rect.y + 8.0f, 16.0f, gui_get_active_theme()->text_secondary);
}

static void partido_gui_create_draw_dropdown_popup(const PartidoGuiCreateForm *form, int dd, Rectangle rect)
{
    const PartidoGuiSelectOption *opts = form->dd_options[dd];
    int count = form->dd_counts[dd];
    int selected = form->dd_selected[dd];
    int scroll;
    int visible;
    Rectangle list_rect;

    if (form->open_dropdown != dd || count <= 0)
    {
        return;
    }

    scroll = form->dd_scroll[dd];
    visible = count;
    list_rect = partido_gui_create_dropdown_list_rect(rect, count);

    if (visible > 6)
    {
        visible = 6;
    }

    DrawRectangleRec(list_rect, (Color){18,36,28,255});
    DrawRectangleLines((int)list_rect.x, (int)list_rect.y, (int)list_rect.width, (int)list_rect.height, gui_get_active_theme()->card_border);

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
        gui_text_truncated(opts[idx].label, row_rect.x + 8.0f, row_rect.y + 7.0f, 15.0f, row_rect.width - 12.0f, gui_get_active_theme()->text_primary);
    }
}

static int partido_gui_create_dd_handle_list(PartidoGuiCreateForm *form, int dd, Rectangle rect, int click, Vector2 mouse)
{
    const PartidoGuiSelectOption *opts = form->dd_options[dd];
    int count = form->dd_counts[dd];
    Rectangle list_rect;
    int visible;

    (void)opts;
    if (form->open_dropdown != dd || count <= 0)
    {
        return 0;
    }

    list_rect = partido_gui_create_dropdown_list_rect(rect, count);
    visible = count;
    if (visible > 6)
    {
        visible = 6;
    }

    if (CheckCollisionPointRec(mouse, list_rect))
    {
        float wheel = GetMouseWheelMove();
        int max_scroll = count - visible;
        if (max_scroll < 0)
        {
            max_scroll = 0;
        }
        if (wheel < 0.0f && form->dd_scroll[dd] < max_scroll)
        {
            form->dd_scroll[dd]++;
        }
        else if (wheel > 0.0f && form->dd_scroll[dd] > 0)
        {
            form->dd_scroll[dd]--;
        }
    }

    if (click && CheckCollisionPointRec(mouse, list_rect))
    {
        int row = (int)((mouse.y - list_rect.y) / 30.0f);
        int idx = form->dd_scroll[dd] + row;
        if (idx >= 0 && idx < count)
        {
            form->dd_selected[dd] = idx;
        }
        form->open_dropdown = PARTIDO_CREATE_DD_NONE;
        return 1;
    }

    if (click && !CheckCollisionPointRec(mouse, rect) && !CheckCollisionPointRec(mouse, list_rect))
    {
        form->open_dropdown = PARTIDO_CREATE_DD_NONE;
        return 1;
    }

    return 0;
}

static void partido_gui_create_handle_text(PartidoGuiCreateForm *form)
{
    char *value = partido_gui_create_input_value_mut(form, form->active_input);
    size_t cap = partido_gui_create_input_cap(form->active_input);
    int key = GetCharPressed();

    if (!value)
    {
        return;
    }

    while (key > 0)
    {
        if (key >= 32 && key <= 126 &&
                (!partido_gui_create_input_numeric(form->active_input) || isdigit((unsigned char)key)))
        {
            size_t len = strlen(value);
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
        size_t len = strlen(value);
        if (len > 0)
        {
            value[len - 1] = '\0';
        }
    }
}

static int partido_gui_create_validate_and_build(const PartidoGuiCreateForm *form, DatosPartido *datos, char *err, size_t err_sz)
{
    int idx;

    if (!form || !datos || !err || err_sz == 0)
    {
        return 0;
    }

    idx = form->dd_selected[PARTIDO_CREATE_DD_CANCHA];
    if (idx < 0 || idx >= form->dd_counts[PARTIDO_CREATE_DD_CANCHA])
    {
        snprintf(err, err_sz, "Seleccione una cancha");
        return 0;
    }
    datos->cancha_id = form->dd_options[PARTIDO_CREATE_DD_CANCHA][idx].value;

    idx = form->dd_selected[PARTIDO_CREATE_DD_CAMISETA];
    if (idx < 0 || idx >= form->dd_counts[PARTIDO_CREATE_DD_CAMISETA])
    {
        snprintf(err, err_sz, "Seleccione una camiseta");
        return 0;
    }
    datos->camiseta = form->dd_options[PARTIDO_CREATE_DD_CAMISETA][idx].value;

    idx = form->dd_selected[PARTIDO_CREATE_DD_RESULTADO];
    datos->resultado = form->dd_options[PARTIDO_CREATE_DD_RESULTADO][idx].value;

    idx = form->dd_selected[PARTIDO_CREATE_DD_CLIMA];
    datos->clima = form->dd_options[PARTIDO_CREATE_DD_CLIMA][idx].value;

    idx = form->dd_selected[PARTIDO_CREATE_DD_DIA];
    datos->dia = form->dd_options[PARTIDO_CREATE_DD_DIA][idx].value;

    if (!partido_gui_parse_int(form->goles, &datos->goles) || datos->goles < 0)
    {
        snprintf(err, err_sz, "Goles invalido");
        return 0;
    }
    if (!partido_gui_parse_int(form->asistencias, &datos->asistencias) || datos->asistencias < 0)
    {
        snprintf(err, err_sz, "Asistencias invalido");
        return 0;
    }
    if (!partido_gui_parse_int(form->rendimiento_general, &datos->rendimiento_general) ||
            datos->rendimiento_general < 1 || datos->rendimiento_general > 10)
    {
        snprintf(err, err_sz, "Rendimiento debe estar entre 1 y 10");
        return 0;
    }
    if (!partido_gui_parse_int(form->cansancio, &datos->cansancio) || datos->cansancio < 1 || datos->cansancio > 10)
    {
        snprintf(err, err_sz, "Cansancio debe estar entre 1 y 10");
        return 0;
    }
    if (!partido_gui_parse_int(form->estado_animo, &datos->estado_animo) ||
            datos->estado_animo < 1 || datos->estado_animo > 10)
    {
        snprintf(err, err_sz, "Estado de animo debe estar entre 1 y 10");
        return 0;
    }
    if (!partido_gui_parse_int(form->precio, &datos->precio) || datos->precio < 0)
    {
        snprintf(err, err_sz, "Precio invalido");
        return 0;
    }

    snprintf(datos->comentario_personal, sizeof(datos->comentario_personal), "%s", form->comentario);
    return 1;
}

static void partido_gui_create_handle_dropdowns(PartidoGuiCreateForm *form, int panel_x, int panel_y, int panel_w, int click, Vector2 mouse)
{
    for (int dd = PARTIDO_CREATE_DD_CANCHA; dd < PARTIDO_CREATE_DD_COUNT; dd++)
    {
        Rectangle r = partido_gui_create_dropdown_rect(panel_x, panel_y, panel_w, dd);
        if (click && CheckCollisionPointRec(mouse, r))
        {
            form->open_dropdown = (form->open_dropdown == dd) ? PARTIDO_CREATE_DD_NONE : dd;
        }
    }

    if (form->open_dropdown != PARTIDO_CREATE_DD_NONE)
    {
        Rectangle open_rect = partido_gui_create_dropdown_rect(panel_x, panel_y, panel_w, form->open_dropdown);
        (void)partido_gui_create_dd_handle_list(form, form->open_dropdown, open_rect, click, mouse);
    }
}

static void partido_gui_create_handle_inputs(PartidoGuiCreateForm *form, int panel_x, int panel_y, int panel_w, int click, Vector2 mouse)
{
    if (form->open_dropdown != PARTIDO_CREATE_DD_NONE)
    {
        return;
    }

    for (int i = 0; i < PARTIDO_CREATE_INPUT_COUNT; i++)
    {
        Rectangle r = partido_gui_create_input_rect(panel_x, panel_y, panel_w, i);
        if (click && CheckCollisionPointRec(mouse, r))
        {
            partido_gui_create_focus_input(form, i);
        }
    }

    if (IsKeyPressed(KEY_TAB))
    {
        int next = (form->active_input + 1) % PARTIDO_CREATE_INPUT_COUNT;
        partido_gui_create_focus_input(form, next);
    }

    partido_gui_create_handle_text(form);
}

static int partido_gui_run_create_form(void) /* NOSONAR */
{
    PartidoGuiCreateForm form;

    partido_gui_create_defaults(&form);
    if (!partido_gui_create_load_catalogs(&form))
    {
        partido_gui_create_free(&form);
        return 0;
    }

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);
    input_consume_key(KEY_TAB);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = (sw > 980) ? 50 : 20;
        int panel_y = 98;
        int panel_w = sw - panel_x * 2;
        int panel_h = sh - 170;
        Rectangle btn_save;
        Rectangle btn_cancel;
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        int save_trigger = 0;
        int cancel_trigger = 0;
        Vector2 mouse = GetMousePosition();

        if (panel_h < 470)
        {
            panel_h = 470;
        }

        btn_save = (Rectangle){(float)(panel_x + panel_w - 360), (float)(panel_y + panel_h - 52), 160.0f, 36.0f};
        btn_cancel = (Rectangle){(float)(panel_x + panel_w - 184), (float)(panel_y + panel_h - 52), 160.0f, 36.0f};

        partido_gui_create_handle_dropdowns(&form, panel_x, panel_y, panel_w, click, mouse);
        partido_gui_create_handle_inputs(&form, panel_x, panel_y, panel_w, click, mouse);

        if (click && CheckCollisionPointRec(mouse, btn_save))
        {
            save_trigger = 1;
        }
        else if (click && CheckCollisionPointRec(mouse, btn_cancel))
        {
            cancel_trigger = 1;
        }

        if (IsKeyPressed(KEY_ENTER))
        {
            save_trigger = 1;
        }
        if (IsKeyPressed(KEY_ESCAPE))
        {
            cancel_trigger = 1;
        }

        if (save_trigger)
        {
            DatosPartido datos;
            char err[160] = {0};
            if (partido_gui_create_validate_and_build(&form, &datos, err, sizeof(err)) &&
                    partido_gui_insertar_datos(&datos, NULL))
            {
                partido_gui_create_free(&form);
                return 1;
            }

            if (err[0] != '\0')
            {
                snprintf(form.error_msg, sizeof(form.error_msg), "%s", err);
            }
            else
            {
                snprintf(form.error_msg, sizeof(form.error_msg), "No se pudo guardar el partido");
            }
        }

        if (cancel_trigger)
        {
            input_consume_key(KEY_ESCAPE);
            partido_gui_create_free(&form);
            return 0;
        }

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("CREAR PARTIDO", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);
        gui_text("Seleccion directa desde listas desplegables", (float)(panel_x + 24), (float)(panel_y + 24),
                 18.0f, gui_get_active_theme()->text_secondary);

        for (int dd = PARTIDO_CREATE_DD_CANCHA; dd < PARTIDO_CREATE_DD_COUNT; dd++)
        {
            Rectangle r = partido_gui_create_dropdown_rect(panel_x, panel_y, panel_w, dd);
            partido_gui_create_draw_dropdown(&form, dd, r);
        }

        for (int i = 0; i < PARTIDO_CREATE_INPUT_COUNT; i++)
        {
            Rectangle r = partido_gui_create_input_rect(panel_x, panel_y, panel_w, i);
            int active = (i == form.active_input && form.open_dropdown == PARTIDO_CREATE_DD_NONE);
            Color label_color = active ? gui_get_active_theme()->accent_primary : gui_get_active_theme()->text_secondary;
            gui_text(partido_gui_create_input_label(i), r.x, r.y - 18.0f, 15.0f, label_color);
            DrawRectangleRec(r, active ? (Color){28,56,40,255} : (Color){18,36,28,255});
            DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height,
                               active ? gui_get_active_theme()->accent_primary : gui_get_active_theme()->card_border);
            gui_text_truncated(partido_gui_create_input_value(&form, i), r.x + 8.0f, r.y + 8.0f, 15.0f,
                               r.width - 14.0f, gui_get_active_theme()->text_primary);
            if (active)
            {
                partido_gui_create_draw_active_indicator(r, partido_gui_create_input_value(&form, i));
            }
        }

        DrawRectangleRec(btn_save, gui_get_active_theme()->accent_primary);
        DrawRectangleRec(btn_cancel, (Color){77,92,80,255});
        DrawRectangleLines((int)btn_save.x, (int)btn_save.y, (int)btn_save.width, (int)btn_save.height, gui_get_active_theme()->card_border);
        DrawRectangleLines((int)btn_cancel.x, (int)btn_cancel.y, (int)btn_cancel.width, (int)btn_cancel.height, gui_get_active_theme()->card_border);
        gui_text("Guardar", btn_save.x + 42.0f, btn_save.y + 8.0f, 18.0f, gui_get_active_theme()->text_primary);
        gui_text("Cancelar", btn_cancel.x + 38.0f, btn_cancel.y + 8.0f, 18.0f, gui_get_active_theme()->text_primary);

        if (form.error_msg[0] != '\0')
        {
            gui_text(form.error_msg, (float)(panel_x + 24), (float)(panel_y + panel_h - 44),
                     18.0f, (Color){232,120,120,255});
        }

        if (form.open_dropdown != PARTIDO_CREATE_DD_NONE)
        {
            Rectangle open_rect = partido_gui_create_dropdown_rect(panel_x, panel_y, panel_w, form.open_dropdown);
            partido_gui_create_draw_dropdown_popup(&form, form.open_dropdown, open_rect);
        }

        gui_draw_footer_hint("Click para desplegar opciones | Enter: guardar | Esc: volver", (float)panel_x, sh);
        EndDrawing();
    }

    partido_gui_create_free(&form);
    return 0;
}

static int partido_gui_select_index_by_value(const PartidoGuiSelectOption *opts, int count, int value)
{
    for (int i = 0; i < count; i++)
    {
        if (opts[i].value == value)
        {
            return i;
        }
    }

    return 0;
}

static int partido_gui_cargar_create_form_desde_partido(int partido_id, PartidoGuiCreateForm *form)
{
    PartidoGuiEditForm edit_form;
    int cancha_id = 0;
    int camiseta_id = 0;
    int resultado = 2;
    int clima = 1;
    int dia = 1;

    if (!form || !partido_gui_form_load_by_id(partido_id, &edit_form))
    {
        return 0;
    }

    snprintf(form->goles, sizeof(form->goles), "%s", edit_form.goles);
    snprintf(form->asistencias, sizeof(form->asistencias), "%s", edit_form.asistencias);
    snprintf(form->rendimiento_general, sizeof(form->rendimiento_general), "%s", edit_form.rendimiento_general);
    snprintf(form->cansancio, sizeof(form->cansancio), "%s", edit_form.cansancio);
    snprintf(form->estado_animo, sizeof(form->estado_animo), "%s", edit_form.estado_animo);
    snprintf(form->precio, sizeof(form->precio), "%s", edit_form.precio);
    snprintf(form->comentario, sizeof(form->comentario), "%s", edit_form.comentario);

    (void)partido_gui_parse_int(edit_form.cancha_id, &cancha_id);
    (void)partido_gui_parse_int(edit_form.camiseta_id, &camiseta_id);
    (void)partido_gui_parse_int(edit_form.resultado, &resultado);
    (void)partido_gui_parse_int(edit_form.clima, &clima);
    (void)partido_gui_parse_int(edit_form.dia, &dia);

    form->dd_selected[PARTIDO_CREATE_DD_CANCHA] = partido_gui_select_index_by_value(
                form->dd_options[PARTIDO_CREATE_DD_CANCHA],
                form->dd_counts[PARTIDO_CREATE_DD_CANCHA],
                cancha_id);
    form->dd_selected[PARTIDO_CREATE_DD_CAMISETA] = partido_gui_select_index_by_value(
                form->dd_options[PARTIDO_CREATE_DD_CAMISETA],
                form->dd_counts[PARTIDO_CREATE_DD_CAMISETA],
                camiseta_id);
    form->dd_selected[PARTIDO_CREATE_DD_RESULTADO] = partido_gui_select_index_by_value(
                form->dd_options[PARTIDO_CREATE_DD_RESULTADO],
                form->dd_counts[PARTIDO_CREATE_DD_RESULTADO],
                resultado);
    form->dd_selected[PARTIDO_CREATE_DD_CLIMA] = partido_gui_select_index_by_value(
                form->dd_options[PARTIDO_CREATE_DD_CLIMA],
                form->dd_counts[PARTIDO_CREATE_DD_CLIMA],
                clima);
    form->dd_selected[PARTIDO_CREATE_DD_DIA] = partido_gui_select_index_by_value(
                form->dd_options[PARTIDO_CREATE_DD_DIA],
                form->dd_counts[PARTIDO_CREATE_DD_DIA],
                dia);

    for (int i = 0; i < PARTIDO_CREATE_INPUT_COUNT; i++)
    {
        form->input_touched[i] = 1;
    }
    form->active_input = PARTIDO_CREATE_INPUT_GOLES;
    form->error_msg[0] = '\0';
    return 1;
}

static int partido_gui_run_update_form(int partido_id) /* NOSONAR */
{
    PartidoGuiCreateForm form;

    partido_gui_create_defaults(&form);
    if (!partido_gui_create_load_catalogs(&form))
    {
        partido_gui_create_free(&form);
        return -1;
    }

    if (!partido_gui_cargar_create_form_desde_partido(partido_id, &form))
    {
        partido_gui_create_free(&form);
        return -1;
    }

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);
    input_consume_key(KEY_TAB);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = (sw > 980) ? 50 : 20;
        int panel_y = 98;
        int panel_w = sw - panel_x * 2;
        int panel_h = sh - 170;
        Rectangle btn_save;
        Rectangle btn_cancel;
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        int save_trigger = 0;
        int cancel_trigger = 0;
        Vector2 mouse = GetMousePosition();

        if (panel_h < 470)
        {
            panel_h = 470;
        }

        btn_save = (Rectangle){(float)(panel_x + panel_w - 360), (float)(panel_y + panel_h - 52), 160.0f, 36.0f};
        btn_cancel = (Rectangle){(float)(panel_x + panel_w - 184), (float)(panel_y + panel_h - 52), 160.0f, 36.0f};

        partido_gui_create_handle_dropdowns(&form, panel_x, panel_y, panel_w, click, mouse);
        partido_gui_create_handle_inputs(&form, panel_x, panel_y, panel_w, click, mouse);

        if (click && CheckCollisionPointRec(mouse, btn_save))
        {
            save_trigger = 1;
        }
        else if (click && CheckCollisionPointRec(mouse, btn_cancel))
        {
            cancel_trigger = 1;
        }

        if (IsKeyPressed(KEY_ENTER))
        {
            save_trigger = 1;
        }
        if (IsKeyPressed(KEY_ESCAPE))
        {
            cancel_trigger = 1;
        }

        if (save_trigger)
        {
            DatosPartido datos;
            char err[160] = {0};
            if (partido_gui_create_validate_and_build(&form, &datos, err, sizeof(err)) &&
                    partido_gui_actualizar_datos(partido_id, &datos))
            {
                partido_gui_create_free(&form);
                return 1;
            }

            if (err[0] != '\0')
            {
                snprintf(form.error_msg, sizeof(form.error_msg), "%s", err);
            }
            else
            {
                snprintf(form.error_msg, sizeof(form.error_msg), "No se pudo guardar el partido");
            }
        }

        if (cancel_trigger)
        {
            input_consume_key(KEY_ESCAPE);
            partido_gui_create_free(&form);
            return 0;
        }

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("MODIFICAR PARTIDO", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);
        gui_text("Edita los valores y guarda", (float)(panel_x + 24), (float)(panel_y + 24),
                 18.0f, gui_get_active_theme()->text_secondary);

        for (int dd = PARTIDO_CREATE_DD_CANCHA; dd < PARTIDO_CREATE_DD_COUNT; dd++)
        {
            Rectangle r = partido_gui_create_dropdown_rect(panel_x, panel_y, panel_w, dd);
            partido_gui_create_draw_dropdown(&form, dd, r);
        }

        for (int i = 0; i < PARTIDO_CREATE_INPUT_COUNT; i++)
        {
            Rectangle r = partido_gui_create_input_rect(panel_x, panel_y, panel_w, i);
            int active = (i == form.active_input && form.open_dropdown == PARTIDO_CREATE_DD_NONE);
            Color label_color = active ? gui_get_active_theme()->accent_primary : gui_get_active_theme()->text_secondary;
            gui_text(partido_gui_create_input_label(i), r.x, r.y - 18.0f, 15.0f, label_color);
            DrawRectangleRec(r, active ? (Color){28,56,40,255} : (Color){18,36,28,255});
            DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height,
                               active ? gui_get_active_theme()->accent_primary : gui_get_active_theme()->card_border);
            gui_text_truncated(partido_gui_create_input_value(&form, i), r.x + 8.0f, r.y + 8.0f, 15.0f,
                               r.width - 14.0f, gui_get_active_theme()->text_primary);
            if (active)
            {
                partido_gui_create_draw_active_indicator(r, partido_gui_create_input_value(&form, i));
            }
        }

        DrawRectangleRec(btn_save, gui_get_active_theme()->accent_primary);
        DrawRectangleRec(btn_cancel, (Color){77,92,80,255});
        DrawRectangleLines((int)btn_save.x, (int)btn_save.y, (int)btn_save.width, (int)btn_save.height, gui_get_active_theme()->card_border);
        DrawRectangleLines((int)btn_cancel.x, (int)btn_cancel.y, (int)btn_cancel.width, (int)btn_cancel.height, gui_get_active_theme()->card_border);
        gui_text("Guardar", btn_save.x + 42.0f, btn_save.y + 8.0f, 18.0f, gui_get_active_theme()->text_primary);
        gui_text("Cancelar", btn_cancel.x + 38.0f, btn_cancel.y + 8.0f, 18.0f, gui_get_active_theme()->text_primary);

        if (form.error_msg[0] != '\0')
        {
            gui_text(form.error_msg, (float)(panel_x + 24), (float)(panel_y + panel_h - 44),
                     18.0f, (Color){232,120,120,255});
        }

        if (form.open_dropdown != PARTIDO_CREATE_DD_NONE)
        {
            Rectangle open_rect = partido_gui_create_dropdown_rect(panel_x, panel_y, panel_w, form.open_dropdown);
            partido_gui_create_draw_dropdown_popup(&form, form.open_dropdown, open_rect);
        }

        gui_draw_footer_hint("Click para desplegar opciones | Enter: guardar | Esc: volver", (float)panel_x, sh);
        EndDrawing();
    }

    partido_gui_create_free(&form);
    return 0;
}

static int crear_partido_gui(void)
{
    if (!partido_gui_run_create_form())
    {
        if (!hay_registros("cancha") || !hay_registros("camiseta"))
        {
            partido_gui_feedback("CREAR PARTIDO", "Debe existir al menos una cancha y una camiseta", 0);
        }
        return 0;
    }

    partido_gui_feedback("CREAR PARTIDO", "Partido creado correctamente", 1);
    return 1;
}

static int modificar_partido_gui(void)
{
    PartidoGuiEditForm form;
    int partido_id = 0;
    int result;

    if (!hay_registros("partido"))
    {
        partido_gui_feedback("MODIFICAR PARTIDO", "No hay partidos registrados", 0);
        return 0;
    }

    if (!partido_gui_seleccionar_partido_id("MODIFICAR PARTIDO", "Click en una fila para editar | Esc/Enter: volver", &partido_id))
    {
        return 0;
    }

    result = partido_gui_run_update_form(partido_id);
    if (result == 1)
    {
        partido_gui_feedback("MODIFICAR PARTIDO", "Partido modificado correctamente", 1);
        return 1;
    }
    if (result == 0)
    {
        return 0;
    }

    /* Fallback al formulario anterior si falla la carga del formulario moderno. */
    if (!partido_gui_form_load_by_id(partido_id, &form))
    {
        partido_gui_feedback("MODIFICAR PARTIDO", "No se pudo cargar el partido", 0);
        return 0;
    }

    if (!partido_gui_run_form("MODIFICAR PARTIDO", &form, 0, partido_id))
    {
        return 0;
    }

    partido_gui_feedback("MODIFICAR PARTIDO", "Partido modificado correctamente", 1);
    return 1;
}

static int eliminar_partido_gui(void)
{
    int partido_id = 0;

    if (!hay_registros("partido"))
    {
        partido_gui_feedback("ELIMINAR PARTIDO", "No hay partidos registrados", 0);
        return 0;
    }

    if (!partido_gui_seleccionar_partido_id("ELIMINAR PARTIDO", "Click en una fila para eliminar | Esc/Enter: volver", &partido_id))
    {
        return 0;
    }

    if (!partido_gui_confirmar_eliminar(partido_id))
    {
        return 0;
    }

    if (!partido_gui_eliminar_por_id(partido_id))
    {
        partido_gui_feedback("ELIMINAR PARTIDO", "No se pudo eliminar el partido", 0);
        return 0;
    }

    partido_gui_feedback("ELIMINAR PARTIDO", "Partido eliminado correctamente", 1);
    return 1;
}

/**
 * @brief Elimina un partido de la base de datos.
 *
 * Esta funcion permite al usuario eliminar un partido existente. Primero muestra
 * la lista de partidos disponibles, solicita el ID del partido a eliminar,
 * verifica que el partido exista, solicita confirmacion al usuario y finalmente
 * elimina el registro de la base de datos si se confirma.
 *
 * @note Si el partido no existe, muestra un mensaje de error y no realiza la eliminacion.
 * @note Si el usuario no confirma la eliminacion, la operacion se cancela.
 */
void eliminar_partido()
{
    if (IsWindowReady())
    {
        (void)eliminar_partido_gui();
        return;
    }

    print_header("ELIMINAR PARTIDO");

    if (!hay_registros("partido"))
    {
        mostrar_no_hay_registros("partidos");
        pause_console();
        return;
    }

    listar_partidos();
    printf("\n");

    int id = input_int("ID Partido a Eliminar (0 para cancelar): ");

    if (!existe_id("partido", id))
    {
        printf("El Partido no Existe\n");
        return;
    }

    if (!confirmar("Seguro que desea eliminar este partido?"))
        return;

    sqlite3_stmt *stmt;
    if (!preparar_stmt("DELETE FROM partido WHERE id = ?", &stmt))
    {
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("Partido Eliminado Correctamente\n");
    pause_console();
}

/**
 * @brief Variable global para almacenar el ID del partido actualmente siendo modificado
 *
 * Esta variable se utiliza en las funciones de modificacion para identificar
 * que partido se esta editando en el menu de modificacion.
 */
static int current_partido_id;

/**
 * @brief Funcion generica para modificar un campo entero de un partido
 *
 * @param campo Nombre del campo en la base de datos
 * @param prompt Texto para solicitar el nuevo valor
 * @param mensaje_exito Mensaje de exito
 * @param min_val Valor minimo valido (0 si no aplica)
 * @param max_val Valor maximo valido (0 si no aplica)
 * @param mostrar_lista Funcion para mostrar lista de opciones (NULL si no aplica)
 */
static void modificar_campo_partido(const char *campo, const char *prompt, const char *mensaje_exito,
                                    int min_val, int max_val, void (*mostrar_lista)(void))
{
    if (mostrar_lista)
        mostrar_lista();

    int valor = input_int(prompt);

    if (min_val != 0 || max_val != 0)
    {
        while (valor < min_val || valor > max_val)
        {
            char prompt_error[256];
            snprintf(prompt_error, sizeof(prompt_error), "Valor invalido. Ingrese entre %d y %d: ", min_val, max_val);
            valor = input_int(prompt_error);
        }
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "UPDATE partido SET %s=? WHERE id=?", campo);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, valor);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("%s\n", mensaje_exito);
    pause_console();
}

/**
 * @brief Funcion generica para modificar un campo de texto de un partido
 *
 * @param campo Nombre del campo en la base de datos
 * @param prompt Texto para solicitar el nuevo valor
 * @param mensaje_exito Mensaje de exito
 * @param buffer_size Tamano del buffer para input
 */
static void modificar_campo_texto_partido(const char *campo, const char *prompt, const char *mensaje_exito, int buffer_size)
{
    char valor[buffer_size];
    printf("%s", prompt);
    size_t valor_size = sizeof(valor);
    if (valor_size > INT_MAX)
    {
        return;
    }
    fgets(valor, (int)valor_size, stdin);
    valor[strcspn(valor, "\n")] = 0;

    char sql[256];
    snprintf(sql, sizeof(sql), "UPDATE partido SET %s=? WHERE id=?", campo);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_text(stmt, 1, valor, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("%s\n", mensaje_exito);
    pause_console();
}

/**
 * @brief Funcion generica para buscar partidos por un criterio
 *
 * @param header Titulo del header
 * @param campo Campo por el que buscar
 * @param prompt Texto para solicitar el valor de busqueda
 * @param mostrar_lista Funcion para mostrar lista de opciones (NULL si no aplica)
 * @param validar_id Si debe validar que el ID existe
 */
static void buscar_partidos_generico(const char *header, const char *campo, const char *prompt,
                                     void (*mostrar_lista)(void), int validar_id)
{
    print_header(header);

    if (mostrar_lista)
        mostrar_lista();

    int valor = input_int(prompt);

    if (validar_id && !existe_id(campo, valor))
    {
        printf("El %s no existe.\n", campo);
        pause_console();
        return;
    }

    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, rendimiento_general, cansancio, estado_animo, comentario_personal, clima, dia, precio "
             "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
             "JOIN cancha can ON p.cancha_id = can.id "
             "WHERE p.%s = ?",
             campo);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, valor);

    int hay = mostrar_partidos_desde_stmt(stmt);

    if (!hay)
        printf("No se encontraron partidos con ese criterio.\n");

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Modifica la cancha de un partido existente
 */
static void modificar_cancha_partido()
{
    listar_canchas_disponibles();
    int cancha = 0;
    while (1)
    {
        cancha = input_int("Nuevo ID Cancha (0 para cancelar): ");
        if (cancha == 0)
            return;
        if (existe_id("cancha", cancha))
            break;
        printf("La cancha no existe. Intente nuevamente.\n");
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE partido SET cancha_id=? WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, cancha);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Cancha modificada correctamente\n");
    pause_console();
}

/**
 * @brief Modifica la fecha y hora de un partido existente
 *
 * Solicita al usuario la nueva fecha en formato dd/mm/yyyy y la nueva hora en formato hh:mm,
 * combina ambos en una cadena y actualiza el campo fecha_hora en la base de datos.
 */
static void modificar_fecha_hora_partido()
{
    char fecha[20];
    char hora[10];
    char fecha_hora[30];
    printf("Nueva fecha (dd/mm/yyyy): ");
    fgets(fecha, sizeof(fecha), stdin);
    fecha[strcspn(fecha, "\n")] = 0;
    printf("Nueva hora (hh:mm): ");
    fgets(hora, sizeof(hora), stdin);
    hora[strcspn(hora, "\n")] = 0;
    snprintf(fecha_hora, sizeof(fecha_hora), "%s %s", fecha, hora);
    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE partido SET fecha_hora=? WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_text(stmt, 1, fecha_hora, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Fecha y hora modificadas correctamente\n");
    pause_console();
}

/**
 * @brief Modifica el numero de goles de un partido existente
 */
static void modificar_goles_partido()
{
    modificar_campo_partido("goles", "Nuevos goles: ", "Goles modificados correctamente", 0, 0, NULL);
}

/**
 * @brief Modifica el numero de asistencias de un partido existente
 */
static void modificar_asistencias_partido()
{
    modificar_campo_partido("asistencias", "Nuevas asistencias: ", "Asistencias modificadas correctamente", 0, 0, NULL);
}

/**
 * @brief Modifica el resultado de un partido existente
 */
static void modificar_resultado_partido()
{
    modificar_campo_partido("resultado", "Nuevo resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA): ", "Resultado modificado correctamente", 1, 3, NULL);
}

/**
 * @brief Modifica la camiseta utilizada en un partido existente
 *
 * Muestra la lista de camisetas disponibles, solicita el nuevo ID de camiseta,
 * verifica que exista y actualiza el campo camiseta_id en la base de datos.
 */
static void modificar_camiseta_partido()
{
    listar_camisetas();
    int camiseta = input_int("Nuevo ID camiseta: ");
    if (!existe_id("camiseta", camiseta))
    {
        printf("La camiseta no existe\n");
        return;
    }
    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE partido SET camiseta_id=? WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, camiseta);
    sqlite3_bind_int(stmt, 2, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Camiseta modificada correctamente\n");
    pause_console();
}

/**
 * @brief Modifica el clima de un partido existente
 */
static void modificar_clima_partido()
{
    modificar_campo_partido("clima", "Nuevo clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio): ", "Clima modificado correctamente", 1, 6, NULL);
}

/**
 * @brief Modifica el dia de un partido existente
 */
static void modificar_dia_partido()
{
    modificar_campo_partido("dia", "Nuevo dia (1=Dia, 2=Tarde, 3=Noche): ", "Dia modificado correctamente", 1, 3, NULL);
}

/**
 * @brief Modifica el comentario personal de un partido existente
 */
static void modificar_comentario_partido()
{
    modificar_campo_texto_partido("comentario_personal", "Nuevo comentario personal: ", "Comentario modificado correctamente", 256);
}

/**
 * @brief Modifica el precio de un partido existente
 */
static void modificar_precio_partido()
{
    modificar_campo_partido("precio", "Nuevo precio del partido: ", "Precio modificado correctamente", 0, 0, NULL);
}

/**
 * @brief Recopila datos completos para modificar un partido
 *
 * Solicita al usuario todos los campos necesarios para actualizar un partido,
 * validando cada entrada para asegurar consistencia de datos.
 *
 * @param datos Puntero a la estructura DatosPartido donde almacenar los datos
 */
static void recopilar_datos_completos_partido(DatosPartido *datos)
{
    listar_canchas_disponibles();
    datos->cancha_id = input_int("Nuevo ID Cancha: ");
    if (!existe_id("cancha", datos->cancha_id))
        return;
    char fecha[20];
    char hora[10];
    input_date("Nueva fecha (DD/MM/AAAA, Enter=hoy): ", fecha, 20);
    input_date("Nueva hora (HH:MM, Enter=ahora): ", hora, 10);
    snprintf(datos->comentario_personal, sizeof(datos->comentario_personal), "%s %s", fecha, hora);
    datos->goles = input_int("Nuevos goles: ");
    datos->asistencias = input_int("Nuevas asistencias: ");
    datos->resultado = input_int("Nuevo resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA): ");
    while (datos->resultado < 1 || datos->resultado > 3)
    {
        datos->resultado = input_int("Resultado invalido. Ingrese 1, 2 o 3: ");
    }
    listar_camisetas();
    datos->camiseta = input_int("Nuevo ID camiseta: ");
    if (!existe_id("camiseta", datos->camiseta))
    {
        printf("La camiseta no existe\n");
        return;
    }
    datos->clima = input_int("Nuevo clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio): ");
    while (datos->clima < 1 || datos->clima > 6)
    {
        datos->clima = input_int("Clima invalido. Ingrese entre 1 y 6: ");
    }
    datos->dia = input_int("Nuevo dia (1=Dia, 2=Tarde, 3=Noche): ");
    while (datos->dia < 1 || datos->dia > 3)
    {
        datos->dia = input_int("Dia invalido. Ingrese 1, 2 o 3: ");
    }
    datos->precio = input_int("Nuevo precio del partido: ");
}

/**
 * @brief Actualiza todos los campos de un partido en la base de datos
 *
 * Realiza una actualizacion completa de un partido utilizando prepared statements
 * para prevenir inyeccion SQL y asegurar atomicidad de la operacion.
 *
 * @param datos Puntero a la estructura DatosPartido con los datos a actualizar
 * @param fecha_hora Fecha y hora combinadas
 */
static void actualizar_partido_completo(DatosPartido const *datos, char const *fecha_hora)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "UPDATE partido "
                "SET cancha_id=?, fecha_hora=?, goles=?, asistencias=?, camiseta_id=?, resultado=?, clima=?, dia=?, precio=? "
                "WHERE id=?",
                &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt, 1, datos->cancha_id);
    sqlite3_bind_text(stmt, 2, fecha_hora, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, datos->goles);
    sqlite3_bind_int(stmt, 4, datos->asistencias);
    sqlite3_bind_int(stmt, 5, datos->camiseta);
    sqlite3_bind_int(stmt, 6, datos->resultado);
    sqlite3_bind_int(stmt, 7, datos->clima);
    sqlite3_bind_int(stmt, 8, datos->dia);
    sqlite3_bind_int(stmt, 9, datos->precio);
    sqlite3_bind_int(stmt, 10, current_partido_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Partido Modificado Correctamente\n");
    pause_console();
}

/**
 * @brief Modifica todos los campos de un partido existente
 *
 * Coordina la recopilacion de datos y actualizacion para simplificar
 * la modificacion completa de un partido en una sola operacion.
 */
static void modificar_todo_partido()
{
    DatosPartido datos;
    recopilar_datos_completos_partido(&datos);
    actualizar_partido_completo(&datos, datos.comentario_personal);
}
/**
 * @brief Permite modificar los datos de un partido existente
 *
 * Muestra la lista de partidos disponibles, solicita el ID a modificar,
 * verifica que exista y muestra un menu con opciones para modificar campos individuales o todos.
 */
void modificar_partido()
{
    if (IsWindowReady())
    {
        (void)modificar_partido_gui();
        return;
    }

    print_header("MODIFICAR PARTIDO");

    if (!hay_registros("partido"))
    {
        mostrar_no_hay_registros("partidos");
        pause_console();
        return;
    }

    listar_partidos();
    printf("\n");

    int id = input_int("ID Partido a Modificar (0 para cancelar): ");

    if (id == 0)
        return;

    if (!existe_id("partido", id))
    {
        printf("El Partido no Existe\n");
        pause_console();
        return;
    }

    current_partido_id = id;

    MenuItem items[] =
    {
        {1, "Cancha", modificar_cancha_partido},
        {2, "Fecha y Hora", modificar_fecha_hora_partido},
        {3, "Goles", modificar_goles_partido},
        {4, "Asistencias", modificar_asistencias_partido},
        {5, "Resultado", modificar_resultado_partido},
        {6, "Camiseta", modificar_camiseta_partido},
        {7, "Clima", modificar_clima_partido},
        {8, "Dia", modificar_dia_partido},
        {9, "Comentario", modificar_comentario_partido},
        {10, "Precio", modificar_precio_partido},
        {11, "Modificar Todo", modificar_todo_partido},
        {0, "Volver", NULL}
    };

    ejecutar_menu_estandar("MODIFICAR PARTIDO", items, 12);
}
/** @brief Busca partidos por camiseta utilizada */
static void buscar_por_camiseta()
{
    buscar_partidos_generico("BUSCAR PARTIDOS POR CAMISETA", "camiseta_id", "ID de la camiseta: ", &listar_camisetas, 1);
}

/** @brief Busca partidos por numero de goles */
static void buscar_por_goles()
{
    buscar_partidos_generico("BUSCAR PARTIDOS POR GOLES", "goles", "Numero de goles: ", NULL, 0);
}

/** @brief Busca partidos por numero de asistencias */
static void buscar_por_asistencias()
{
    buscar_partidos_generico("BUSCAR PARTIDOS POR ASISTENCIAS", "asistencias", "Numero de asistencias: ", NULL, 0);
}

/** @brief Busca partidos por cancha */
static void buscar_por_cancha()
{
    buscar_partidos_generico("BUSCAR PARTIDOS POR CANCHA", "cancha_id", "ID de la cancha: ", &listar_canchas_disponibles, 1);
}

/**
 * @brief Permite buscar partidos segun diferentes criterios
 *
 * Presenta un submenu con opciones para buscar partidos por:
 * - Camiseta utilizada
 * - Numero de goles
 * - Numero de asistencias
 * - Cancha donde se jugo
 */
void buscar_partidos()
{
    MenuItem items[] =
    {
        {1, "Por Camiseta", buscar_por_camiseta},
        {2, "Por Goles", buscar_por_goles},
        {3, "Por Asistencias", buscar_por_asistencias},
        {4, "Por Cancha", buscar_por_cancha},
        {0, "Volver", NULL}
    };

    ejecutar_menu_estandar("BUSQUEDA DE PARTIDOS", items, 5);
}

/**
 * @brief Maneja un gol marcado por el equipo local durante la simulacion
 *
 * @param equipo_local Puntero al equipo local
 * @param minuto Minuto del partido
 * @param jugador_idx indice del jugador que marco
 * @param asistente_idx indice del asistente
 * @param estadisticas_local Array de estadisticas locales
 * @param asistencias_local Array de asistencias locales
 * @param goles_local Puntero al contador de goles locales
 */
static void manejar_gol_local(Equipo const *equipo_local, int minuto, int jugador_idx, int asistente_idx,
                              int *estadisticas_local, int *asistencias_local, int *goles_local)
{
    if (asistente_idx == jugador_idx && equipo_local->num_jugadores > 1)
    {
        asistente_idx = (asistente_idx + 1) % equipo_local->num_jugadores;
    }

    (*goles_local)++;
    estadisticas_local[jugador_idx]++;
    if (asistente_idx != jugador_idx)
    {
        asistencias_local[asistente_idx]++;
    }

    printf("*** ¡GOOOOL! Minuto %d ***\n", minuto);
    printf("   Gol de %s (%d) para %s\n",
           equipo_local->jugadores[jugador_idx].nombre,
           equipo_local->jugadores[jugador_idx].numero,
           equipo_local->nombre);
    if (asistente_idx != jugador_idx)
    {
        printf("   Asistencia de %s (%d)\n",
               equipo_local->jugadores[asistente_idx].nombre,
               equipo_local->jugadores[asistente_idx].numero);
    }
}

/**
 * @brief Maneja un gol marcado por el equipo visitante durante la simulacion
 *
 * @param equipo_visitante Puntero al equipo visitante
 * @param minuto Minuto del partido
 * @param jugador_idx indice del jugador que marco
 * @param asistente_idx indice del asistente
 * @param estadisticas_visitante Array de estadisticas visitantes
 * @param asistencias_visitante Array de asistencias visitantes
 * @param goles_visitante Puntero al contador de goles visitantes
 */
static void manejar_gol_visitante(Equipo const *equipo_visitante, int minuto, int jugador_idx, int asistente_idx,
                                  int *estadisticas_visitante, int *asistencias_visitante, int *goles_visitante)
{
    if (asistente_idx == jugador_idx && equipo_visitante->num_jugadores > 1)
    {
        asistente_idx = (asistente_idx + 1) % equipo_visitante->num_jugadores;
    }

    (*goles_visitante)++;
    estadisticas_visitante[jugador_idx]++;
    if (asistente_idx != jugador_idx)
    {
        asistencias_visitante[asistente_idx]++;
    }

    printf("*** ¡GOOOOL! Minuto %d ***\n", minuto);
    printf("   Gol de %s (%d) para %s\n",
           equipo_visitante->jugadores[jugador_idx].nombre,
           equipo_visitante->jugadores[jugador_idx].numero,
           equipo_visitante->nombre);
    if (asistente_idx != jugador_idx)
    {
        printf("   Asistencia de %s (%d)\n",
               equipo_visitante->jugadores[asistente_idx].nombre,
               equipo_visitante->jugadores[asistente_idx].numero);
    }
}

/**
 * @brief Verifica si hay suficientes equipos para simular un partido
 *
 * @return 1 si hay al menos 2 equipos, 0 si no
 */
static int verificar_equipos_disponibles()
{
    sqlite3_stmt *stmt_count;
    if (!preparar_stmt("SELECT COUNT(*) FROM equipo", &stmt_count))
    {
        return 0;
    }
    sqlite3_step(stmt_count);
    int total_equipos = sqlite3_column_int(stmt_count, 0);
    sqlite3_finalize(stmt_count);

    if (total_equipos < 2)
    {
        printf("Se necesitan al menos 2 equipos guardados para simular un partido.\n");
        printf("Por favor, cree equipos primero.\n");
        pause_console();
        return 0;
    }
    return 1;
}

/**
 * @brief Muestra la lista de equipos disponibles
 */
static void mostrar_equipos_disponibles()
{
    printf("=== EQUIPOS DISPONIBLES ===\n\n");
    sqlite3_stmt *stmt_equipos;
    if (!preparar_stmt("SELECT id, nombre FROM equipo ORDER BY id", &stmt_equipos))
    {
        return;
    }

    while (sqlite3_step(stmt_equipos) == SQLITE_ROW)
    {
        printf("%d. %s\n", sqlite3_column_int(stmt_equipos, 0),
               sqlite3_column_text(stmt_equipos, 1));
    }
    sqlite3_finalize(stmt_equipos);
}

/**
 * @brief Selecciona los equipos local y visitante
 *
 * @param equipo_local_id Puntero al ID del equipo local
 * @param equipo_visitante_id Puntero al ID del equipo visitante
 */
static void seleccionar_equipos(int *equipo_local_id, int *equipo_visitante_id)
{
    // Seleccionar equipo local
    do
    {
        *equipo_local_id = input_int("\nSeleccione el equipo LOCAL (ID): ");
        if (!existe_id("equipo", *equipo_local_id))
        {
            printf("Equipo no encontrado. Intente nuevamente.\n");
        }
    }
    while (!existe_id("equipo", *equipo_local_id));

    // Seleccionar equipo visitante (diferente al local)
    do
    {
        *equipo_visitante_id = input_int("Seleccione el equipo VISITANTE (ID): ");
        if (*equipo_visitante_id == *equipo_local_id)
        {
            printf("El equipo visitante debe ser diferente al local.\n");
        }
        else if (!existe_id("equipo", *equipo_visitante_id))
        {
            printf("Equipo no encontrado. Intente nuevamente.\n");
        }
    }
    while (*equipo_visitante_id == *equipo_local_id || !existe_id("equipo", *equipo_visitante_id));
}

/**
 * @brief Carga los equipos desde la base de datos
 *
 * @param equipo_local_id ID del equipo local
 * @param equipo_visitante_id ID del equipo visitante
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @return 1 si se cargaron exitosamente, 0 si hubo error
 */
static int cargar_equipos(int equipo_local_id, int equipo_visitante_id, Equipo *equipo_local, Equipo *equipo_visitante)
{
    memset(equipo_local, 0, sizeof(Equipo));
    memset(equipo_visitante, 0, sizeof(Equipo));

    if (!cargar_equipo_desde_bd(equipo_local_id, equipo_local))
    {
        printf("Error al cargar el equipo local.\n");
        pause_console();
        return 0;
    }

    if (!cargar_equipo_desde_bd(equipo_visitante_id, equipo_visitante))
    {
        printf("Error al cargar el equipo visitante.\n");
        pause_console();
        return 0;
    }
    return 1;
}

/**
 * @brief Muestra la informacion inicial del partido
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 */
static void mostrar_inicio_partido(Equipo const *equipo_local, Equipo const *equipo_visitante)
{
    printf("\n*** INICIANDO SIMULACION ***\n");
    printf("EQUIPO LOCAL: %s\n", equipo_local->nombre);
    printf("EQUIPO VISITANTE: %s\n\n", equipo_visitante->nombre);
}

/**
 * @brief Muestra la alineacion de los equipos
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 */
static void mostrar_alineacion(Equipo const *equipo_local, Equipo const *equipo_visitante)
{
    clear_screen();
    printf("========================================\n");
    printf("                    SIMULACION DE PARTIDO\n\n");

    printf("=== %s VS %s ===\n\n", equipo_local->nombre, equipo_visitante->nombre);

    // Mostrar cancha inicial
    mostrar_cancha_animada(0, 0);

    // Mostrar equipos alineados
    printf("EQUIPO LOCAL (%s):\n", equipo_local->nombre);
    for (int i = 0; i < equipo_local->num_jugadores; i++)
    {
        printf("  %d. %s", equipo_local->jugadores[i].numero, equipo_local->jugadores[i].nombre);
        if (equipo_local->jugadores[i].es_capitan)
            printf(" (C)");
        printf("\n");
    }

    printf("\nEQUIPO VISITANTE (%s):\n", equipo_visitante->nombre);
    for (int i = 0; i < equipo_visitante->num_jugadores; i++)
    {
        printf("  %d. %s", equipo_visitante->jugadores[i].numero, equipo_visitante->jugadores[i].nombre);
        if (equipo_visitante->jugadores[i].es_capitan)
            printf(" (C)");
        printf("\n");
    }

    printf("\n*** INICIO DEL PARTIDO ***\n");
    printf("La simulacion comenzara automaticamente en 3 segundos...\n");
    Sleep(3000);
}

/**
 * @brief Ejecuta la logica de simulacion del partido
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @param estadisticas Puntero a la estructura con todas las estadisticas del partido
 */
static void simular_partido_logica(Equipo const *equipo_local, Equipo const *equipo_visitante,
                                   EstadisticasPartido *estadisticas)
{
    for (int minuto = 1; minuto <= 60; minuto++)
    {
        clear_screen();
        print_header("SIMULACION DE PARTIDO");

        printf("=== %s %d - %d %s ===\n\n",
               equipo_local->nombre, estadisticas->goles_local, estadisticas->goles_visitante, equipo_visitante->nombre);
        printf("MINUTO: %d\n\n", minuto);

        // Generar eventos aleatorios
        int evento = secure_rand(100);

        if (evento < 25) // Gol local
        {
            int jugador_idx = secure_rand(equipo_local->num_jugadores);
            int asistente_idx = secure_rand(equipo_local->num_jugadores);
            if (asistente_idx == jugador_idx && equipo_local->num_jugadores > 1)
            {
                asistente_idx = (asistente_idx + 1) % equipo_local->num_jugadores;
            }

            manejar_gol_local(equipo_local, minuto, jugador_idx, asistente_idx,
                              estadisticas->estadisticas_local, estadisticas->asistencias_local, &estadisticas->goles_local);
        }
        else if (evento < 50) // Gol visitante
        {
            int jugador_idx = secure_rand(equipo_visitante->num_jugadores);
            int asistente_idx = secure_rand(equipo_visitante->num_jugadores);
            if (asistente_idx == jugador_idx && equipo_visitante->num_jugadores > 1)
            {
                asistente_idx = (asistente_idx + 1) % equipo_visitante->num_jugadores;
            }

            manejar_gol_visitante(equipo_visitante, minuto, jugador_idx, asistente_idx,
                                  estadisticas->estadisticas_visitante, estadisticas->asistencias_visitante, &estadisticas->goles_visitante);
        }
        else if (evento < 70)
        {
            printf("*** Oportunidad de gol ***\n");
        }
        else
        {
            printf("*** El partido continua... ***\n");
        }

        mostrar_cancha_animada(minuto, (evento < 4) ? 1 : 0);
        Sleep(1000);
    }
}

/**
 * @brief Muestra los resultados finales del partido
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @param estadisticas Puntero a la estructura con todas las estadisticas del partido
 */
static void mostrar_resultados(Equipo const *equipo_local, Equipo const *equipo_visitante,
                               EstadisticasPartido const *estadisticas)
{
    // Resultados finales
    clear_screen();
    print_header("FIN DEL PARTIDO");

    printf("*** RESULTADO FINAL ***\n\n");
    printf("*** 60 MINUTOS COMPLETADOS ***\n\n");

    printf("*** %s %d - %d %s ***\n\n",
           equipo_local->nombre, estadisticas->goles_local, estadisticas->goles_visitante, equipo_visitante->nombre);

    if (estadisticas->goles_local > estadisticas->goles_visitante)
    {
        printf("*** ¡%s GANA EL PARTIDO! ***\n\n", equipo_local->nombre);
    }
    else if (estadisticas->goles_visitante > estadisticas->goles_local)
    {
        printf("*** ¡%s GANA EL PARTIDO! ***\n\n", equipo_visitante->nombre);
    }
    else
    {
        printf("*** ¡EMPATE! ***\n\n");
    }

    // Mostrar estadisticas
    printf("*** ESTADISTICAS DEL PARTIDO ***\n\n");

    printf("EQUIPO LOCAL (%s):\n", equipo_local->nombre);
    for (int i = 0; i < equipo_local->num_jugadores; i++)
    {
        if (estadisticas->estadisticas_local[i] > 0 || estadisticas->asistencias_local[i] > 0)
        {
            printf("  %s (%d): %d Goles, %d Asistencias\n",
                   equipo_local->jugadores[i].nombre,
                   equipo_local->jugadores[i].numero,
                   estadisticas->estadisticas_local[i], estadisticas->asistencias_local[i]);
        }
    }

    printf("\nEQUIPO VISITANTE (%s):\n", equipo_visitante->nombre);
    for (int i = 0; i < equipo_visitante->num_jugadores; i++)
    {
        if (estadisticas->estadisticas_visitante[i] > 0 || estadisticas->asistencias_visitante[i] > 0)
        {
            printf("  %s (%d): %d Goles, %d Asistencias\n",
                   equipo_visitante->jugadores[i].nombre,
                   equipo_visitante->jugadores[i].numero,
                   estadisticas->estadisticas_visitante[i], estadisticas->asistencias_visitante[i]);
        }
    }
}

/**
 * @brief Carga un equipo desde la base de datos por su ID
 *
 * @param equipo_id ID del equipo a cargar
 * @param equipo Puntero al equipo donde cargar los datos
 * @return 1 si se cargo exitosamente, 0 si hubo error
 */
static int cargar_equipo_desde_bd(int equipo_id, Equipo *equipo)
{
    sqlite3_stmt *stmt_equipo;
    const char *sql_equipo = "SELECT nombre, tipo, tipo_futbol, num_jugadores FROM equipo WHERE id = ?";

    if (!preparar_stmt(sql_equipo, &stmt_equipo))
    {
        return 0;
    }

    sqlite3_bind_int(stmt_equipo, 1, equipo_id);

    if (sqlite3_step(stmt_equipo) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt_equipo);
        return 0;
    }

    equipo->id = equipo_id;
    strncpy_s(equipo->nombre, sizeof(equipo->nombre), (const char *)sqlite3_column_text(stmt_equipo, 0), _TRUNCATE);
    equipo->tipo = sqlite3_column_int(stmt_equipo, 1);
    equipo->tipo_futbol = sqlite3_column_int(stmt_equipo, 2);
    equipo->num_jugadores = sqlite3_column_int(stmt_equipo, 3);
    equipo->partido_id = -1;

    sqlite3_finalize(stmt_equipo);

    // Cargar jugadores
    return cargar_jugadores_equipo(equipo_id, equipo);
}

/**
 * @brief Carga los jugadores de un equipo desde la base de datos
 *
 * @param equipo_id ID del equipo cuyos jugadores se cargaran
 * @param equipo Puntero al equipo donde cargar los jugadores
 * @return 1 si se cargaron exitosamente, 0 si hubo error
 */
static int cargar_jugadores_equipo(int equipo_id, Equipo *equipo)
{
    sqlite3_stmt *stmt_jugadores;
    const char *sql_jugadores = "SELECT nombre, numero, posicion, es_capitan FROM jugador WHERE equipo_id = ? ORDER BY numero";

    if (!preparar_stmt(sql_jugadores, &stmt_jugadores))
    {
        return 0;
    }

    sqlite3_bind_int(stmt_jugadores, 1, equipo_id);

    int jugador_idx = 0;
    while (sqlite3_step(stmt_jugadores) == SQLITE_ROW && jugador_idx < 11)
    {
        strncpy_s(equipo->jugadores[jugador_idx].nombre,
                  sizeof(equipo->jugadores[jugador_idx].nombre),
                  (const char *)sqlite3_column_text(stmt_jugadores, 0),
                  _TRUNCATE);
        equipo->jugadores[jugador_idx].numero = sqlite3_column_int(stmt_jugadores, 1);
        equipo->jugadores[jugador_idx].posicion = sqlite3_column_int(stmt_jugadores, 2);
        equipo->jugadores[jugador_idx].es_capitan = sqlite3_column_int(stmt_jugadores, 3);
        jugador_idx++;
    }

    sqlite3_finalize(stmt_jugadores);
    return 1;
}

/**
 * @brief Determina el resultado de un partido basado en los goles
 *
 * @param goles_local Goles del equipo local
 * @param goles_visitante Goles del equipo visitante
 * @param resultado_local Puntero para almacenar el resultado del equipo local
 * @param resultado_visitante Puntero para almacenar el resultado del equipo visitante
 */
static void determinar_resultado_partido(int goles_local, int goles_visitante,
        int *resultado_local, int *resultado_visitante)
{
    if (goles_local > goles_visitante)
    {
        *resultado_local = 1;     // VICTORIA
        *resultado_visitante = 3; // DERROTA
    }
    else if (goles_visitante > goles_local)
    {
        *resultado_local = 3;     // DERROTA
        *resultado_visitante = 1; // VICTORIA
    }
    else
    {
        *resultado_local = 2;     // EMPATE
        *resultado_visitante = 2; // EMPATE
    }
}

/**
 * @brief Obtiene el ID de una cancha por defecto
 *
 * @return ID de la cancha por defecto
 */
static UNUSED int obtener_cancha_defecto()
{
    int cancha_id = 1;
    sqlite3_stmt *stmt_cancha;
    if (!preparar_stmt("SELECT id FROM cancha ORDER BY id LIMIT 1", &stmt_cancha))
    {
        return cancha_id;
    }
    if (sqlite3_step(stmt_cancha) == SQLITE_ROW)
    {
        cancha_id = sqlite3_column_int(stmt_cancha, 0);
    }
    sqlite3_finalize(stmt_cancha);
    return cancha_id;
}

/**
 * @brief Guarda los resultados de una simulacion de partido en la base de datos
 *
 * @param datos_simulacion Puntero a la estructura con todos los datos de la simulacion
 */
static UNUSED void guardar_resultados_simulacion(DatosSimulacion const *datos_simulacion)
{
    char fecha_simulacion[20] = "2023-01-01 00:00";
    get_datetime(fecha_simulacion, sizeof(fecha_simulacion));

    // Determinar resultados
    int resultado_local;
    int resultado_visitante;
    determinar_resultado_partido(datos_simulacion->goles_local, datos_simulacion->goles_visitante,
                                 &resultado_local, &resultado_visitante);

    // Obtener cancha por defecto
    int cancha_id = obtener_cancha_defecto();

    // Guardar estadisticas para cada jugador del equipo local
    guardar_estadisticas_equipo(&datos_simulacion->equipo_local, datos_simulacion->estadisticas_local,
                                datos_simulacion->asistencias_local, resultado_local, cancha_id, fecha_simulacion);

    // Guardar estadisticas para cada jugador del equipo visitante
    guardar_estadisticas_equipo(&datos_simulacion->equipo_visitante, datos_simulacion->estadisticas_visitante,
                                datos_simulacion->asistencias_visitante, resultado_visitante, cancha_id, fecha_simulacion);

    printf("*** RESULTADOS GUARDADOS EN LA BASE DE DATOS ***\n");
}

/**
 * @brief Guarda las estadisticas de un equipo en la base de datos
 *
 * @param equipo Puntero al equipo
 * @param estadisticas Array con estadisticas de goles por jugador
 * @param asistencias Array con estadisticas de asistencias por jugador
 * @param resultado Resultado del equipo
 * @param cancha_id ID de la cancha
 * @param fecha_simulacion Fecha de la simulacion
 */
static UNUSED void guardar_estadisticas_equipo(Equipo const *equipo, int const *estadisticas, int const *asistencias,
        int resultado, int cancha_id, char const *fecha_simulacion)
{
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        if (estadisticas[i] > 0 || asistencias[i] > 0)
        {
            // Buscar o crear camiseta para este jugador
            int camiseta_id = 1; // Usar camiseta por defecto

            DatosPartido datos = {0};
            datos.cancha_id = cancha_id;
            datos.goles = estadisticas[i];
            datos.asistencias = asistencias[i];
            datos.camiseta = camiseta_id;
            datos.resultado = resultado;
            datos.rendimiento_general = 8;
            datos.cansancio = 5;
            datos.estado_animo = 7;
            strncpy_s(datos.comentario_personal, sizeof(datos.comentario_personal), "Partido simulado", _TRUNCATE);
            datos.clima = 1;
            datos.dia = 1;

            long long partido_id = obtener_siguiente_id("partido");
            insertar_partido(partido_id, &datos, fecha_simulacion);
        }
    }
}

/**
 * @brief Crea la estructura de datos de simulacion a partir de las estadisticas
 *
 * @param equipo_local Equipo local
 * @param equipo_visitante Equipo visitante
 * @param estadisticas Estadisticas del partido
 * @return Estructura de datos de simulacion completa
 */
static UNUSED DatosSimulacion crear_datos_simulacion(Equipo equipo_local, Equipo equipo_visitante,
        EstadisticasPartido const *estadisticas)
{
    DatosSimulacion datos_simulacion = {0};
    datos_simulacion.equipo_local = equipo_local;
    datos_simulacion.equipo_visitante = equipo_visitante;
    memcpy(datos_simulacion.estadisticas_local, estadisticas->estadisticas_local, sizeof(estadisticas->estadisticas_local));
    memcpy(datos_simulacion.estadisticas_visitante, estadisticas->estadisticas_visitante, sizeof(estadisticas->estadisticas_visitante));
    memcpy(datos_simulacion.asistencias_local, estadisticas->asistencias_local, sizeof(estadisticas->asistencias_local));
    memcpy(datos_simulacion.asistencias_visitante, estadisticas->asistencias_visitante, sizeof(estadisticas->asistencias_visitante));
    datos_simulacion.goles_local = estadisticas->goles_local;
    datos_simulacion.goles_visitante = estadisticas->goles_visitante;
    return datos_simulacion;
}

typedef struct
{
    int id;
    char nombre[128];
} PartidoGuiEquipoRow;

static int partido_gui_contar_equipos(void)
{
    sqlite3_stmt *stmt = NULL;
    int total = 0;

    if (!preparar_stmt("SELECT COUNT(*) FROM equipo", &stmt))
    {
        return 0;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        total = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return total;
}

static int partido_gui_expand_equipos_rows(PartidoGuiEquipoRow **rows, int *cap)
{
    int new_cap = (*cap) * 2;
    PartidoGuiEquipoRow *tmp = (PartidoGuiEquipoRow *)realloc(*rows, (size_t)new_cap * sizeof(PartidoGuiEquipoRow));
    if (!tmp)
    {
        return 0;
    }

    memset(tmp + *cap, 0, (size_t)(new_cap - *cap) * sizeof(PartidoGuiEquipoRow));
    *rows = tmp;
    *cap = new_cap;
    return 1;
}

static int partido_gui_cargar_equipos_rows(PartidoGuiEquipoRow **rows_out, int *count_out)
{
    sqlite3_stmt *stmt = NULL;
    int cap = 32;
    int count = 0;
    PartidoGuiEquipoRow *rows;

    if (!rows_out || !count_out)
    {
        return 0;
    }

    rows = (PartidoGuiEquipoRow *)calloc((size_t)cap, sizeof(PartidoGuiEquipoRow));
    if (!rows)
    {
        return 0;
    }

    if (!preparar_stmt("SELECT id, nombre FROM equipo ORDER BY id", &stmt))
    {
        free(rows);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *col_nombre;
        if (count >= cap && !partido_gui_expand_equipos_rows(&rows, &cap))
        {
            sqlite3_finalize(stmt);
            free(rows);
            return 0;
        }

        rows[count].id = sqlite3_column_int(stmt, 0);
        col_nombre = sqlite3_column_text(stmt, 1);
        if (col_nombre)
        {
            latin1_to_utf8(col_nombre, rows[count].nombre, sizeof(rows[count].nombre));
        }
        else
        {
            snprintf(rows[count].nombre, sizeof(rows[count].nombre), "(sin nombre)");
        }
        count++;
    }

    sqlite3_finalize(stmt);
    *rows_out = rows;
    *count_out = count;
    return 1;
}

static int partido_gui_seleccionar_equipo_id(const char *title, const char *hint, int blocked_id, int *id_out) /* NOSONAR */
{
    PartidoGuiEquipoRow *rows = NULL;
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

    if (!partido_gui_cargar_equipos_rows(&rows, &count))
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
        Rectangle area;
        int clicked_id = 0;

        partido_gui_calcular_panel(sw, sh, &panel_x, &panel_w, &panel_h);
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
        scroll = partido_gui_actualizar_scroll(scroll, count, visible_rows, area);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            for (int i = scroll; i < count; i++) /* NOSONAR */
            {
                int row = i - scroll;
                int y = (int)area.y + row * row_h;
                Rectangle fr;
                if (y + row_h > (int)(area.y + area.height)) /* NOSONAR */
                {
                    break;
                }

                fr = (Rectangle){area.x + 2.0f, (float)y, area.width - 4.0f, (float)(row_h - 1)};
                if (CheckCollisionPointRec(GetMousePosition(), fr) && rows[i].id != blocked_id) /* NOSONAR */
                {
                    clicked_id = rows[i].id;
                    break;
                }
            }
        }

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header(title, sw);
        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                            "ID", 10.0f,
                            "EQUIPO", 120.0f);

        if (count == 0)
        {
            gui_text("No hay equipos disponibles.", panel_x + 24, panel_y + 24, 24.0f, (Color){233,247,236,255});
        }
        else
        {
            BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
            for (int i = scroll; i < count; i++)
            {
                int row = i - scroll;
                int y = (int)area.y + row * row_h;
                Rectangle fr;
                int hovered;
                Color text_color;
                if (y + row_h > (int)(area.y + area.height)) /* NOSONAR */
                {
                    break;
                }

                fr = (Rectangle){area.x + 2.0f, (float)y, area.width - 4.0f, (float)(row_h - 1)};
                hovered = CheckCollisionPointRec(GetMousePosition(), fr);
                gui_draw_list_row_bg(fr, row, hovered);

                if (rows[i].id == blocked_id) /* NOSONAR */
                {
                    DrawRectangleRec(fr, (Color){48,48,48,130});
                    text_color = (Color){150,150,150,255};
                }
                else
                {
                    text_color = (Color){233,247,236,255};
                }

                gui_text(TextFormat("%3d", rows[i].id), (int)area.x + 10, y + 7, 18.0f, text_color);
                gui_text_truncated(rows[i].nombre, (float)((int)area.x + 120), (float)(y + 7),
                                   18.0f, area.width - 132.0f, text_color);
                if (rows[i].id == blocked_id) /* NOSONAR */
                {
                    gui_text("(ya seleccionado)", (int)area.x + (int)area.width - 160, y + 7, 15.0f, (Color){180,180,180,255});
                }
            }
            EndScissorMode();
        }

        gui_draw_footer_hint(hint, (float)panel_x, sh);
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

typedef struct
{
    Vector2 pos;
    Vector2 target;
    float speed;
    int numero;
    Posicion posicion;
    int es_local;
} PartidoGuiAnimJugador;

static float partido_gui_randf_range(float min_v, float max_v)
{
    int r;
    float t;

    if (max_v <= min_v)
    {
        return min_v;
    }

    r = secure_rand(10000);
    t = (float)r / 9999.0f;
    return min_v + (max_v - min_v) * t;
}

static void partido_gui_anim_nuevo_objetivo(PartidoGuiAnimJugador *jugador, Rectangle zona)
{
    float x_ratio_min = 0.1f;
    float x_ratio_max = 0.9f;
    float y_ratio_min = 0.1f;
    float y_ratio_max = 0.9f;
    float speed_min = 42.0f;
    float speed_max = 84.0f;
    float min_x;
    float max_x;
    float min_y;
    float max_y;

    if (!jugador)
    {
        return;
    }

    switch (jugador->posicion)
    {
    case ARQUERO:
        y_ratio_min = 0.30f;
        y_ratio_max = 0.70f;
        speed_min = 30.0f;
        speed_max = 50.0f;
        if (jugador->es_local)
        {
            x_ratio_min = 0.03f;
            x_ratio_max = 0.26f;
        }
        else
        {
            x_ratio_min = 0.74f;
            x_ratio_max = 0.97f;
        }
        break;

    case DEFENSOR:
        y_ratio_min = 0.12f;
        y_ratio_max = 0.88f;
        speed_min = 36.0f;
        speed_max = 66.0f;
        if (jugador->es_local)
        {
            x_ratio_min = 0.14f;
            x_ratio_max = 0.52f;
        }
        else
        {
            x_ratio_min = 0.48f;
            x_ratio_max = 0.86f;
        }
        break;

    case MEDIOCAMPISTA:
        y_ratio_min = 0.08f;
        y_ratio_max = 0.92f;
        speed_min = 46.0f;
        speed_max = 84.0f;
        if (jugador->es_local)
        {
            x_ratio_min = 0.35f;
            x_ratio_max = 0.83f;
        }
        else
        {
            x_ratio_min = 0.17f;
            x_ratio_max = 0.65f;
        }
        break;

    case DELANTERO:
    default:
        y_ratio_min = 0.14f;
        y_ratio_max = 0.86f;
        speed_min = 54.0f;
        speed_max = 94.0f;
        if (jugador->es_local)
        {
            x_ratio_min = 0.56f;
            x_ratio_max = 0.95f;
        }
        else
        {
            x_ratio_min = 0.05f;
            x_ratio_max = 0.44f;
        }
        break;
    }

    min_x = zona.x + zona.width * x_ratio_min;
    max_x = zona.x + zona.width * x_ratio_max;
    min_y = zona.y + zona.height * y_ratio_min;
    max_y = zona.y + zona.height * y_ratio_max;

    if (max_x < min_x)
    {
        max_x = min_x;
    }
    if (max_y < min_y)
    {
        max_y = min_y;
    }

    jugador->target.x = partido_gui_randf_range(min_x, max_x);
    jugador->target.y = partido_gui_randf_range(min_y, max_y);
    jugador->speed = partido_gui_randf_range(speed_min, speed_max);
}

static void partido_gui_anim_init_equipo(PartidoGuiAnimJugador *jugadores, int count, Rectangle zona, const Equipo *equipo,
        int es_local)
{
    const int cols = 4;
    int rows = 1;

    if (!jugadores || count <= 0)
    {
        return;
    }

    rows = (count + cols - 1) / cols;
    if (rows < 1)
    {
        rows = 1;
    }

    for (int i = 0; i < count; i++)
    {
        int col = i % cols;
        int row = i / cols;
        float x = zona.x + (((float)(col + 1)) / ((float)(cols + 1))) * zona.width;
        float y = zona.y + (((float)(row + 1)) / ((float)(rows + 1))) * zona.height;

        jugadores[i].pos = (Vector2) {x, y};
        jugadores[i].numero = (equipo && i < equipo->num_jugadores) ? equipo->jugadores[i].numero : (i + 1);
        jugadores[i].posicion = (equipo && i < equipo->num_jugadores) ? equipo->jugadores[i].posicion : MEDIOCAMPISTA;
        jugadores[i].es_local = es_local;
        partido_gui_anim_nuevo_objetivo(&jugadores[i], zona);
    }
}

static void partido_gui_anim_update_equipo(PartidoGuiAnimJugador *jugadores, int count, Rectangle zona, float dt)
{
    if (!jugadores || count <= 0 || dt <= 0.0f)
    {
        return;
    }

    for (int i = 0; i < count; i++)
    {
        float step = jugadores[i].speed * dt;
        float dx = jugadores[i].target.x - jugadores[i].pos.x;
        float dy = jugadores[i].target.y - jugadores[i].pos.y;
        float remain_x;
        float remain_y;
        int cerca_x = 0;
        int cerca_y = 0;
        float min_x = zona.x + 8.0f;
        float max_x = zona.x + zona.width - 8.0f;
        float min_y = zona.y + 8.0f;
        float max_y = zona.y + zona.height - 8.0f;

        if (dx > step)
        {
            jugadores[i].pos.x += step;
        }
        else if (dx < -step)
        {
            jugadores[i].pos.x -= step;
        }
        else
        {
            jugadores[i].pos.x = jugadores[i].target.x;
        }

        if (dy > step)
        {
            jugadores[i].pos.y += step;
        }
        else if (dy < -step)
        {
            jugadores[i].pos.y -= step;
        }
        else
        {
            jugadores[i].pos.y = jugadores[i].target.y;
        }

        remain_x = jugadores[i].target.x - jugadores[i].pos.x;
        remain_y = jugadores[i].target.y - jugadores[i].pos.y;

        if (remain_x > -1.5f && remain_x < 1.5f)
        {
            cerca_x = 1;
        }
        if (remain_y > -1.5f && remain_y < 1.5f)
        {
            cerca_y = 1;
        }

        if (jugadores[i].pos.x < min_x)
        {
            jugadores[i].pos.x = min_x;
        }
        else if (jugadores[i].pos.x > max_x)
        {
            jugadores[i].pos.x = max_x;
        }
        if (jugadores[i].pos.y < min_y)
        {
            jugadores[i].pos.y = min_y;
        }
        else if (jugadores[i].pos.y > max_y)
        {
            jugadores[i].pos.y = max_y;
        }

        if (cerca_x && cerca_y)
        {
            partido_gui_anim_nuevo_objetivo(&jugadores[i], zona);
        }
    }
}

static void partido_gui_anim_move_point(Vector2 *pos, Vector2 target, float speed, float dt)
{
    float step;
    float dx;
    float dy;

    if (!pos || dt <= 0.0f)
    {
        return;
    }

    step = speed * dt;
    dx = target.x - pos->x;
    dy = target.y - pos->y;

    if (dx > step)
    {
        pos->x += step;
    }
    else if (dx < -step)
    {
        pos->x -= step;
    }
    else
    {
        pos->x = target.x;
    }

    if (dy > step)
    {
        pos->y += step;
    }
    else if (dy < -step)
    {
        pos->y -= step;
    }
    else
    {
        pos->y = target.y;
    }
}

typedef struct
{
    int goles_local_jugador[11];
    int asist_local_jugador[11];
    int goles_visit_jugador[11];
    int asist_visit_jugador[11];
    int ataques_local;
    int ataques_visit;
    int atajadas;
    int faltas;
    int posesion_local_ticks;
    int posesion_visit_ticks;
} PartidoGuiSimStats;

static const char *partido_gui_sim_nombre_jugador(const Equipo *equipo, int idx)
{
    if (equipo && idx >= 0 && idx < equipo->num_jugadores && equipo->jugadores[idx].nombre[0] != '\0')
    {
        return equipo->jugadores[idx].nombre;
    }

    if (equipo && equipo->nombre[0] != '\0')
    {
        return equipo->nombre;
    }

    return "Jugador";
}

static int partido_gui_sim_numero_jugador(const Equipo *equipo, int idx)
{
    if (equipo && idx >= 0 && idx < equipo->num_jugadores)
    {
        return equipo->jugadores[idx].numero;
    }

    return idx + 1;
}

static void partido_gui_sim_draw_toast_gol(float timer, const char *linea1, const char *linea2, int local_gol)
{
    int sw;
    int sh;
    int box_w;
    int box_h;
    int box_x;
    int box_y;
    int alpha;
    Color fondo;

    if (timer <= 0.0f || !linea1)
    {
        return;
    }

    sw = GetScreenWidth();
    sh = GetScreenHeight();
    box_w = (sw > 900) ? 620 : (sw - 40);
    if (box_w < 320)
    {
        box_w = 320;
    }
    box_h = 122;
    box_x = (sw - box_w) / 2;
    box_y = (sh - box_h) / 2;
    alpha = (int)(timer * 50.0f) + 55;

    if (alpha > 210)
    {
        alpha = 210;
    }
    if (alpha < 70)
    {
        alpha = 70;
    }

    fondo = local_gol ? (Color)
    {
        30, 88, 166, (unsigned char)alpha
    }
    : (Color)
    {
        173, 58, 58, (unsigned char)alpha
    };

    DrawRectangle(box_x, box_y, box_w, box_h, fondo);
    DrawRectangleLines(box_x, box_y, box_w, box_h, (Color){245,245,245,220});
    gui_text("GOOOOOOL", (float)(box_x + 24), (float)(box_y + 18), 28.0f, (Color){255,251,245,255});
    gui_text_truncated(linea1, (float)(box_x + 24), (float)(box_y + 56), 20.0f, (float)(box_w - 48), (Color){255,251,245,255});
    if (linea2 && linea2[0] != '\0')
    {
        gui_text_truncated(linea2, (float)(box_x + 24), (float)(box_y + 84), 18.0f, (float)(box_w - 48), (Color){240,245,255,255});
    }
}

static void partido_gui_sim_draw_stats_resumen(const Equipo *local,
        const Equipo *visitante,
        int goles_local,
        int goles_visit,
        const PartidoGuiSimStats *stats)
{
    int panel_x = 56;
    int panel_y = 132;
    int panel_w = GetScreenWidth() - 112;
    int panel_h = GetScreenHeight() - 222;
    int posesion_total;
    int pos_local;
    int pos_visit;
    const char *resultado = "EMPATE";

    if (panel_w < 420)
    {
        panel_w = 420;
    }
    if (panel_h < 250)
    {
        panel_h = 250;
    }

    posesion_total = stats->posesion_local_ticks + stats->posesion_visit_ticks;
    if (posesion_total <= 0)
    {
        posesion_total = 1;
    }
    pos_local = (stats->posesion_local_ticks * 100) / posesion_total;
    pos_visit = 100 - pos_local;

    if (goles_local > goles_visit)
    {
        resultado = "GANA LOCAL";
    }
    else if (goles_visit > goles_local)
    {
        resultado = "GANA VISITANTE";
    }

    DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){15,43,29,230});
    DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){188,228,203,220});

    gui_text("ESTADISTICAS DEL PARTIDO", (float)(panel_x + 20), (float)(panel_y + 18), 24.0f, (Color){230,247,236,255});
    gui_text(TextFormat("%s %d - %d %s", local->nombre, goles_local, goles_visit, visitante->nombre),
             (float)(panel_x + 20), (float)(panel_y + 52), 22.0f, (Color){230,247,236,255});
    gui_text(TextFormat("Resultado: %s", resultado), (float)(panel_x + 20), (float)(panel_y + 84), 20.0f, (Color){214,234,220,255});

    gui_text(TextFormat("Posesion: %s %d%% | %d%% %s", local->nombre, pos_local, pos_visit, visitante->nombre),
             (float)(panel_x + 20), (float)(panel_y + 118), 19.0f, (Color){208,227,214,255});
    gui_text(TextFormat("Ataques peligrosos: %s %d | %d %s", local->nombre, stats->ataques_local, stats->ataques_visit, visitante->nombre),
             (float)(panel_x + 20), (float)(panel_y + 148), 19.0f, (Color){208,227,214,255});
    gui_text(TextFormat("Atajadas clave: %d", stats->atajadas),
             (float)(panel_x + 20), (float)(panel_y + 178), 19.0f, (Color){208,227,214,255});
    gui_text(TextFormat("Faltas tacticas: %d", stats->faltas),
             (float)(panel_x + 20), (float)(panel_y + 208), 19.0f, (Color){208,227,214,255});
}

static void partido_gui_sim_draw_stats_jugadores(const Equipo *local,
        const Equipo *visitante,
        int count_local,
        int count_visit,
        const PartidoGuiSimStats *stats)
{
    int panel_x = 48;
    int panel_y = 132;
    int panel_w = GetScreenWidth() - 96;
    int panel_h = GetScreenHeight() - 222;
    int y_left;
    int y_right;

    if (panel_w < 460)
    {
        panel_w = 460;
    }
    if (panel_h < 250)
    {
        panel_h = 250;
    }

    DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){15,43,29,230});
    DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){188,228,203,220});

    gui_text("ESTADISTICAS POR JUGADOR", (float)(panel_x + 20), (float)(panel_y + 18), 24.0f, (Color){230,247,236,255});
    gui_text(local->nombre, (float)(panel_x + 20), (float)(panel_y + 52), 20.0f, (Color){191,220,255,255});
    gui_text(visitante->nombre, (float)(panel_x + panel_w / 2 + 12), (float)(panel_y + 52), 20.0f, (Color){255,198,198,255});

    y_left = panel_y + 84;
    for (int i = 0; i < count_local && i < 11; i++)
    {
        if (y_left > panel_y + panel_h - 24)
        {
            break;
        }
        if (stats->goles_local_jugador[i] > 0 || stats->asist_local_jugador[i] > 0)
        {
            gui_text_truncated(TextFormat("%s (%d) G:%d A:%d",
                                          partido_gui_sim_nombre_jugador(local, i),
                                          partido_gui_sim_numero_jugador(local, i),
                                          stats->goles_local_jugador[i],
                                          stats->asist_local_jugador[i]),
                               (float)(panel_x + 20),
                               (float)y_left,
                               17.0f,
                               (float)(panel_w / 2 - 30),
                               (Color){210,231,247,255});
            y_left += 24;
        }
    }
    if (y_left == panel_y + 84)
    {
        gui_text("Sin goles ni asistencias", (float)(panel_x + 20), (float)y_left, 17.0f, (Color){210,231,247,255});
    }

    y_right = panel_y + 84;
    for (int i = 0; i < count_visit && i < 11; i++)
    {
        if (y_right > panel_y + panel_h - 24)
        {
            break;
        }
        if (stats->goles_visit_jugador[i] > 0 || stats->asist_visit_jugador[i] > 0)
        {
            gui_text_truncated(TextFormat("%s (%d) G:%d A:%d",
                                          partido_gui_sim_nombre_jugador(visitante, i),
                                          partido_gui_sim_numero_jugador(visitante, i),
                                          stats->goles_visit_jugador[i],
                                          stats->asist_visit_jugador[i]),
                               (float)(panel_x + panel_w / 2 + 12),
                               (float)y_right,
                               17.0f,
                               (float)(panel_w / 2 - 30),
                               (Color){247,224,224,255});
            y_right += 24;
        }
    }
    if (y_right == panel_y + 84)
    {
        gui_text("Sin goles ni asistencias", (float)(panel_x + panel_w / 2 + 12), (float)y_right, 17.0f, (Color){247,224,224,255});
    }
}

static void partido_gui_simular_marcador(const Equipo *local, const Equipo *visitante) /* NOSONAR */
{
    const float duracion_segundos = 120.0f;
    const int jugadores_max = 11;
    PartidoGuiAnimJugador local_anim[11] = {0};
    PartidoGuiAnimJugador visita_anim[11] = {0};
    Rectangle zona_local = (Rectangle){0};
    Rectangle zona_visit = (Rectangle){0};
    Rectangle campo = (Rectangle){0};
    int count_local = local ? local->num_jugadores : 0;
    int count_visit = visitante ? visitante->num_jugadores : 0;
    int minuto = 1;
    int minuto_evento = 0;
    int vista_final = 1;
    int goles_local = 0;
    int goles_visit = 0;
    int balon_obj_idx = 0;
    int balon_obj_local = 1;
    int pausa = 0;
    int finalizado = 0;
    float elapsed = 0.0f;
    float balon_timer = 0.0f;
    float flash_evento = 0.0f;
    float toast_gol_timer = 0.0f;
    float last_campo_w = 0.0f;
    float last_campo_h = 0.0f;
    Vector2 balon = (Vector2){0};
    Vector2 balon_target = (Vector2){0};
    PartidoGuiSimStats stats = {0};
    char evento[160] = "Comienza el partido";
    char toast_linea1[200] = "";
    char toast_linea2[200] = "";
    int toast_local = 1;

    if (count_local < 1)
    {
        count_local = 1;
    }
    else if (count_local > jugadores_max)
    {
        count_local = jugadores_max;
    }

    if (count_visit < 1)
    {
        count_visit = 1;
    }
    else if (count_visit > jugadores_max)
    {
        count_visit = jugadores_max;
    }

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);
    input_consume_key(KEY_SPACE);
    input_consume_key(KEY_ONE);
    input_consume_key(KEY_TWO);
    input_consume_key(KEY_THREE);

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = (sw > 980) ? 60 : 20;
        int panel_y = 120;
        int panel_w = sw - panel_x * 2;
        int panel_h = sh - 220;
        float progreso;

        if (panel_h < 260)
        {
            panel_h = 260;
        }

        campo = (Rectangle){(float)(panel_x + 24), (float)(panel_y + 178), (float)(panel_w - 48), (float)(panel_h - 210)};
        if (campo.height < 120.0f)
        {
            campo.height = 120.0f;
        }
        if (campo.width < 280.0f)
        {
            campo.width = 280.0f;
        }

        zona_local = (Rectangle){campo.x + 8.0f, campo.y + 8.0f, campo.width * 0.5f - 16.0f, campo.height - 16.0f};
        zona_visit = (Rectangle){campo.x + campo.width * 0.5f + 8.0f, campo.y + 8.0f, campo.width * 0.5f - 16.0f, campo.height - 16.0f};

        if (last_campo_w != campo.width || last_campo_h != campo.height)
        {
            partido_gui_anim_init_equipo(local_anim, count_local, zona_local, local, 1);
            partido_gui_anim_init_equipo(visita_anim, count_visit, zona_visit, visitante, 0);
            balon = (Vector2){campo.x + campo.width * 0.5f, campo.y + campo.height * 0.5f};
            balon_target = balon;
            last_campo_w = campo.width;
            last_campo_h = campo.height;
        }

        if (!finalizado && IsKeyPressed(KEY_SPACE))
        {
            pausa = !pausa;
            input_consume_key(KEY_SPACE);
        }

        if (toast_gol_timer > 0.0f)
        {
            toast_gol_timer -= dt;
            if (toast_gol_timer < 0.0f)
            {
                toast_gol_timer = 0.0f;
            }
        }

        if (!finalizado && !pausa)
        {
            int evento_rand;
            elapsed += dt;
            if (elapsed > duracion_segundos)
            {
                elapsed = duracion_segundos;
            }

            minuto = (int)((elapsed * 60.0f) / duracion_segundos) + 1;
            if (minuto > 60)
            {
                minuto = 60;
            }

            partido_gui_anim_update_equipo(local_anim, count_local, zona_local, dt);
            partido_gui_anim_update_equipo(visita_anim, count_visit, zona_visit, dt);

            if (balon_obj_local)
            {
                stats.posesion_local_ticks++;
            }
            else
            {
                stats.posesion_visit_ticks++;
            }

            balon_timer += dt;
            if (balon_timer >= 0.55f)
            {
                balon_obj_local = (secure_rand(100) < 55) ? 1 : 0;
                balon_obj_idx = secure_rand(balon_obj_local ? count_local : count_visit);
                if (balon_obj_local)
                {
                    balon_target = local_anim[balon_obj_idx].pos;
                }
                else
                {
                    balon_target = visita_anim[balon_obj_idx].pos;
                }
                balon_timer = 0.0f;
            }

            partido_gui_anim_move_point(&balon, balon_target, 140.0f, dt);

            if (minuto != minuto_evento)
            {
                minuto_evento = minuto;

                evento_rand = secure_rand(100);
                if (evento_rand < 18) /* NOSONAR */
                {
                    int idx = secure_rand(count_local);
                    int asist_idx = -1;
                    goles_local++;
                    stats.goles_local_jugador[idx]++;
                    if (count_local > 1)
                    {
                        asist_idx = secure_rand(count_local);
                        if (asist_idx == idx)
                        {
                            asist_idx = (asist_idx + 1) % count_local;
                        }
                        stats.asist_local_jugador[asist_idx]++;
                    }

                    snprintf(evento, sizeof(evento), "Min %d: Gol de %s", minuto,
                             partido_gui_sim_nombre_jugador(local, idx));
                    snprintf(toast_linea1, sizeof(toast_linea1), "Min %d: %s (%d)", minuto,
                             partido_gui_sim_nombre_jugador(local, idx),
                             partido_gui_sim_numero_jugador(local, idx));
                    if (asist_idx >= 0)
                    {
                        snprintf(toast_linea2, sizeof(toast_linea2), "Asistencia: %s (%d)",
                                 partido_gui_sim_nombre_jugador(local, asist_idx),
                                 partido_gui_sim_numero_jugador(local, asist_idx));
                    }
                    else
                    {
                        snprintf(toast_linea2, sizeof(toast_linea2), "Sin asistencia");
                    }
                    toast_local = 1;
                    toast_gol_timer = 4.0f;
                    balon = (Vector2){campo.x + campo.width * 0.5f, campo.y + campo.height * 0.5f};
                    flash_evento = 0.75f;
                }
                else if (evento_rand < 36)
                {
                    int idx = secure_rand(count_visit);
                    int asist_idx = -1;
                    goles_visit++;
                    stats.goles_visit_jugador[idx]++;
                    if (count_visit > 1)
                    {
                        asist_idx = secure_rand(count_visit);
                        if (asist_idx == idx)
                        {
                            asist_idx = (asist_idx + 1) % count_visit;
                        }
                        stats.asist_visit_jugador[asist_idx]++;
                    }

                    snprintf(evento, sizeof(evento), "Min %d: Gol de %s", minuto,
                             partido_gui_sim_nombre_jugador(visitante, idx));
                    snprintf(toast_linea1, sizeof(toast_linea1), "Min %d: %s (%d)", minuto,
                             partido_gui_sim_nombre_jugador(visitante, idx),
                             partido_gui_sim_numero_jugador(visitante, idx));
                    if (asist_idx >= 0)
                    {
                        snprintf(toast_linea2, sizeof(toast_linea2), "Asistencia: %s (%d)",
                                 partido_gui_sim_nombre_jugador(visitante, asist_idx),
                                 partido_gui_sim_numero_jugador(visitante, asist_idx));
                    }
                    else
                    {
                        snprintf(toast_linea2, sizeof(toast_linea2), "Sin asistencia");
                    }
                    toast_local = 0;
                    toast_gol_timer = 4.0f;
                    balon = (Vector2){campo.x + campo.width * 0.5f, campo.y + campo.height * 0.5f};
                    flash_evento = 0.75f;
                }
                else if (evento_rand < 55)
                {
                    snprintf(evento, sizeof(evento), "Min %d: Ataque peligroso del local", minuto);
                    stats.ataques_local++;
                }
                else if (evento_rand < 74)
                {
                    snprintf(evento, sizeof(evento), "Min %d: Ataque peligroso del visitante", minuto);
                    stats.ataques_visit++;
                }
                else if (evento_rand < 87)
                {
                    snprintf(evento, sizeof(evento), "Min %d: Gran atajada del arquero", minuto);
                    stats.atajadas++;
                }
                else
                {
                    snprintf(evento, sizeof(evento), "Min %d: Falta tactica en mitad de cancha", minuto);
                    stats.faltas++;
                }
            }

            if (flash_evento > 0.0f)
            {
                flash_evento -= dt;
                if (flash_evento < 0.0f)
                {
                    flash_evento = 0.0f;
                }
            }

            if (elapsed >= duracion_segundos)
            {
                finalizado = 1;
                snprintf(evento, sizeof(evento), "Fin del partido");
                vista_final = 1;
            }
        }

        progreso = elapsed / duracion_segundos;
        if (progreso < 0.0f)
        {
            progreso = 0.0f;
        }
        else if (progreso > 1.0f)
        {
            progreso = 1.0f;
        }

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("SIMULACION DE PARTIDO 2D", sw);
        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text_truncated(TextFormat("%s %d - %d %s", local->nombre, goles_local, goles_visit, visitante->nombre),
                           (float)(panel_x + 24), (float)(panel_y + 34), 30.0f,
                           (float)(panel_w - 48), gui_get_active_theme()->text_primary);

        if (!finalizado)
        {
            gui_text(TextFormat("Tiempo real: %02d:%02d / 02:00", (int)elapsed / 60, (int)elapsed % 60),
                     (float)(panel_x + 24), (float)(panel_y + 74), 18.0f, gui_get_active_theme()->text_secondary);
            gui_text(TextFormat("Minuto simulado: %d/60", minuto),
                     (float)(panel_x + 24), (float)(panel_y + 98), 18.0f, gui_get_active_theme()->text_secondary);

            {
                int barra_w = panel_w - 48;
                int barra_fill = (int)((float)barra_w * progreso);
                if (barra_fill < 0)
                {
                    barra_fill = 0;
                }
                if (barra_fill > barra_w)
                {
                    barra_fill = barra_w;
                }

                DrawRectangle(panel_x + 24, panel_y + 128, barra_w, 16, (Color){30,52,40,255});
                DrawRectangle(panel_x + 24, panel_y + 128, barra_fill, 16, gui_get_active_theme()->accent_primary);
            }

            gui_text_truncated(evento, (float)(panel_x + 24), (float)(panel_y + 150), 20.0f,
                               (float)(panel_w - 48), gui_get_active_theme()->text_secondary);
        }
        else
        {
            gui_text("Estado: Partido finalizado", (float)(panel_x + 24), (float)(panel_y + 74),
                     20.0f, gui_get_active_theme()->text_secondary);
            gui_text("Tiempo final: 02:00 | Minuto simulado: 60/60", (float)(panel_x + 24), (float)(panel_y + 102),
                     18.0f, gui_get_active_theme()->text_secondary);
        }

        DrawRectangleRec(campo, (Color){24,101,54,255});
        for (int franja = 0; franja < 10; franja++)
        {
            Color tono = (franja % 2 == 0) ? (Color){28,114,60,255} : (Color){24,101,54,255};
            int fx = (int)campo.x + (int)((campo.width / 10.0f) * (float)franja);
            int fw = (int)(campo.width / 10.0f) + 1;
            DrawRectangle(fx, (int)campo.y, fw, (int)campo.height, tono);
        }

        DrawRectangleLines((int)campo.x, (int)campo.y, (int)campo.width, (int)campo.height, (Color){228,245,229,220});
        DrawLine((int)(campo.x + campo.width * 0.5f), (int)campo.y,
                 (int)(campo.x + campo.width * 0.5f), (int)(campo.y + campo.height), (Color){228,245,229,200});
        DrawCircleLines((int)(campo.x + campo.width * 0.5f), (int)(campo.y + campo.height * 0.5f),
                        (int)(campo.height * 0.14f), (Color){228,245,229,200});

        DrawRectangleLines((int)campo.x, (int)(campo.y + campo.height * 0.2f),
                           (int)(campo.width * 0.12f), (int)(campo.height * 0.6f), (Color){228,245,229,180});
        DrawRectangleLines((int)(campo.x + campo.width * 0.88f), (int)(campo.y + campo.height * 0.2f),
                           (int)(campo.width * 0.12f), (int)(campo.height * 0.6f), (Color){228,245,229,180});

        for (int i = 0; i < count_local; i++)
        {
            DrawCircleV((Vector2){local_anim[i].pos.x + 1.5f, local_anim[i].pos.y + 1.5f}, 9.0f, (Color){0,0,0,80});
            DrawCircleV(local_anim[i].pos, 9.0f, (Color){42,130,245,255});
            DrawText(TextFormat("%d", local_anim[i].numero), (int)local_anim[i].pos.x - 6,
                     (int)local_anim[i].pos.y - 5, 10, (Color){238,246,255,255});
        }

        for (int i = 0; i < count_visit; i++)
        {
            DrawCircleV((Vector2){visita_anim[i].pos.x + 1.5f, visita_anim[i].pos.y + 1.5f}, 9.0f, (Color){0,0,0,80});
            DrawCircleV(visita_anim[i].pos, 9.0f, (Color){232,79,79,255});
            DrawText(TextFormat("%d", visita_anim[i].numero), (int)visita_anim[i].pos.x - 6,
                     (int)visita_anim[i].pos.y - 5, 10, (Color){255,242,242,255});
        }

        DrawCircleV((Vector2){balon.x + 1.0f, balon.y + 1.0f}, 5.0f, (Color){0,0,0,80});
        DrawCircleV(balon, 5.0f, (Color){248,248,248,255});

        if (flash_evento > 0.0f && !finalizado)
        {
            int alpha = (int)(flash_evento * 155.0f);
            if (alpha > 155)
            {
                alpha = 155;
            }
            if (alpha < 0)
            {
                alpha = 0;
            }
            DrawRectangle(panel_x + 24, panel_y + 148, panel_w - 48, 30, (Color){255,213,79,(unsigned char)alpha});
        }

        if (!finalizado)
        {
            if (pausa)
            {
                gui_text("PAUSA", (float)(panel_x + panel_w - 126), (float)(panel_y + 34), 24.0f, (Color){255,219,111,255});
                gui_draw_footer_hint("Pausado | Espacio: reanudar", (float)panel_x, sh);
            }
            else
            {
                gui_draw_footer_hint("Espacio: pausar/reanudar | Duracion real: 2 minutos", (float)panel_x, sh);
            }
        }
        else
        {
            const char *resultado = "EMPATE";
            if (goles_local > goles_visit)
            {
                resultado = "GANA LOCAL";
            }
            else if (goles_visit > goles_local)
            {
                resultado = "GANA VISITANTE";
            }

            if (IsKeyPressed(KEY_ONE))
            {
                vista_final = 1;
                input_consume_key(KEY_ONE);
            }
            if (IsKeyPressed(KEY_TWO))
            {
                vista_final = 2;
                input_consume_key(KEY_TWO);
            }
            if (IsKeyPressed(KEY_THREE))
            {
                vista_final = 3;
                input_consume_key(KEY_THREE);
            }
            if (vista_final == 2)
            {
                partido_gui_sim_draw_stats_resumen(local, visitante, goles_local, goles_visit, &stats);
                gui_draw_footer_hint("1: pantalla final | 2: resumen | 3: por jugador | Esc/Enter: salir", (float)panel_x, sh);
            }
            else if (vista_final == 3)
            {
                partido_gui_sim_draw_stats_jugadores(local, visitante, count_local, count_visit, &stats);
                gui_draw_footer_hint("1: pantalla final | 2: resumen | 3: por jugador | Esc/Enter: salir", (float)panel_x, sh);
            }
            else
            {
                gui_text(TextFormat("Resultado: %s", resultado), (float)(panel_x + 24), (float)(panel_y + 124),
                         24.0f, gui_get_active_theme()->text_primary);
                gui_text_truncated("Opciones: [2] Resumen general  |  [3] Estadisticas por jugador",
                                   (float)(panel_x + 24), (float)(panel_y + 152), 17.0f,
                                   (float)(panel_w - 48), gui_get_active_theme()->text_secondary);
                gui_draw_footer_hint("Esc/Enter: volver | 2: resumen | 3: por jugador", (float)panel_x, sh);
            }
        }

        partido_gui_sim_draw_toast_gol(toast_gol_timer, toast_linea1, toast_linea2, toast_local);

        EndDrawing();

        if (finalizado && (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)))
        {
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            return;
        }
    }
}

static int simular_partido_guardados_gui(void)
{
    int equipo_local_id = 0;
    int equipo_visitante_id = 0;
    Equipo equipo_local;
    Equipo equipo_visitante;

    if (partido_gui_contar_equipos() < 2)
    {
        partido_gui_feedback("SIMULAR PARTIDO", "Se necesitan al menos 2 equipos", 0);
        return 0;
    }

    if (!partido_gui_seleccionar_equipo_id("SELECCIONAR LOCAL", "Click: seleccionar local | Esc/Enter: volver", 0, &equipo_local_id))
    {
        return 0;
    }

    if (!partido_gui_seleccionar_equipo_id("SELECCIONAR VISITANTE", "Click: seleccionar visitante | Esc/Enter: volver", equipo_local_id, &equipo_visitante_id))
    {
        return 0;
    }

    if (!cargar_equipos(equipo_local_id, equipo_visitante_id, &equipo_local, &equipo_visitante))
    {
        partido_gui_feedback("SIMULAR PARTIDO", "No se pudieron cargar los equipos", 0);
        return 0;
    }

    partido_gui_simular_marcador(&equipo_local, &equipo_visitante);
    return 1;
}

/**
 * @brief Simula un partido entre dos equipos guardados en la base de datos
 *
 * Permite al usuario seleccionar dos equipos existentes de la base de datos
 * y simular un partido entre ellos sin guardar resultados en Partidos.
 */
void simular_partido_guardados()
{
    if (IsWindowReady())
    {
        (void)simular_partido_guardados_gui();
        return;
    }

    clear_screen();
    print_header("SIMULAR PARTIDO CON EQUIPOS GUARDADOS");

    // Verificar que hay al menos 2 equipos disponibles
    if (!verificar_equipos_disponibles())
    {
        return;
    }

    // Mostrar equipos disponibles
    mostrar_equipos_disponibles();

    // Seleccionar equipos
    int equipo_local_id;
    int equipo_visitante_id;
    seleccionar_equipos(&equipo_local_id, &equipo_visitante_id);

    // Cargar equipos desde la base de datos
    Equipo equipo_local;
    Equipo equipo_visitante;
    if (!cargar_equipos(equipo_local_id, equipo_visitante_id, &equipo_local, &equipo_visitante))
    {
        return;
    }

    // Mostrar informacion inicial
    mostrar_inicio_partido(&equipo_local, &equipo_visitante);

    // Mostrar alineacion y comenzar partido
    mostrar_alineacion(&equipo_local, &equipo_visitante);

    // Preparar arrays para estadisticas
    EstadisticasPartido estadisticas = {0};

    // Ejecutar simulacion del partido
    simular_partido_logica(&equipo_local, &equipo_visitante, &estadisticas);

    // Mostrar resultados finales
    mostrar_resultados(&equipo_local, &equipo_visitante, &estadisticas);

    printf("Simulacion finalizada. Este partido no se guarda en Partidos.\n");

    printf("\nPresione Enter para volver al menu...");
    getchar();
}

#define TACTIC_W 40
#define TACTIC_H 20

static UNUSED void tactica_init_grid(char grid[TACTIC_H][TACTIC_W + 1])
{
    for (int y = 0; y < TACTIC_H; y++)
    {
        for (int x = 0; x < TACTIC_W; x++)
        {
            grid[y][x] = '.';
        }
        grid[y][TACTIC_W] = '\0';
    }
}

static UNUSED void tactica_build_grid_string(char grid[TACTIC_H][TACTIC_W + 1], char *out, size_t size)
{
    size_t used = 0;
    if (!out || size == 0)
    {
        return;
    }

    for (int y = 0; y < TACTIC_H; y++)
    {
        for (int x = 0; x < TACTIC_W; x++)
        {
            if (used + 1 >= size)
            {
                out[used] = '\0';
                return;
            }
            out[used++] = grid[y][x];
        }
        if (used + 1 >= size)
        {
            out[used] = '\0';
            return;
        }
        out[used++] = '\n';
    }
    out[used] = '\0';
}

static UNUSED void tactica_guardar_diagrama(int partido_id, const char *nombre, const char *grid_text)
{
    const char *sql =
        "INSERT INTO tactica_diagrama (partido_id, nombre, fecha, grid) "
        "VALUES (?, ?, date('now'), ?)";

    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error preparando insercion.\n");
        return;
    }

    sqlite3_bind_int(stmt, 1, partido_id);
    sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, grid_text, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        printf("Error guardando diagrama: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
}

#ifndef RSIZE_MAX_STR
#define RSIZE_MAX_STR (SIZE_MAX >> 1)
#endif

#ifndef TACTICA_TRUNCATE
#define TACTICA_TRUNCATE ((size_t)-1)
#endif

static int tactica_strncpy_s(char *dest, size_t destsz, const char *src, size_t count)
{
    if (!dest || !src || destsz == 0 || destsz > RSIZE_MAX_STR)
        return 1;

#if defined(__STDC_LIB_EXT1__)
    return strncpy_s(dest, destsz, src, count == TACTICA_TRUNCATE ? _TRUNCATE : count);
#elif defined(_MSC_VER)
    return strncpy_s(dest, destsz, src, count == TACTICA_TRUNCATE ? _TRUNCATE : count);
#else
    size_t max_src = (count == TACTICA_TRUNCATE) ? (destsz - 1) : count;
    size_t src_len = strnlen(src, max_src);
    size_t to_copy = src_len;
    if (to_copy > destsz - 1)
        to_copy = destsz - 1;

    if (to_copy > 0)
        memcpy(dest, src, to_copy);

    dest[to_copy] = '\0';
    return 0;
#endif
}

static size_t tactica_strlen_secure(const char *s, size_t max)
{
    if (!s)
        return 0;

#if defined(__STDC_LIB_EXT1__)
    return strlen_s(s, max);
#elif defined(_MSC_VER)
    return strnlen_s(s, max);
#else
    return strnlen(s, max);
#endif
}

static void tactica_trim_newline(char *s)
{
    if (!s)
        return;

    size_t len = tactica_strlen_secure(s, RSIZE_MAX_STR);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
    {
        s[--len] = '\0';
    }
}

static void tactica_leer_nombre_diagrama(char *nombre, size_t size)
{
    ui_printf("Nombre del diagrama: ");
    if (!fgets(nombre, (int)size, stdin))
    {
        if (size > 0)
            nombre[0] = '\0';
        return;
    }

    tactica_trim_newline(nombre);

    if (nombre[0] == '\0' && tactica_strncpy_s(nombre, size, "Diagrama", TACTICA_TRUNCATE) != 0 && size > 0)
    {
        nombre[0] = '\0';
    }
}


static void tactica_mostrar_partidos_disponibles(void)
{
    const char *sql = "SELECT p.id, can.nombre, p.fecha_hora FROM partido p "
                      "JOIN cancha can ON p.cancha_id = can.id ORDER BY p.id ASC";
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt)) return;

    ui_printf_centered_line("--- Partidos disponibles ---");
    ui_printf_centered_line("ID | Cancha | Fecha");
    ui_printf_centered_line("---------------------------");

    int count = 0;
    char fecha_fmt[20];
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        format_date_for_display((const char *)sqlite3_column_text(stmt, 2),
                                fecha_fmt, sizeof(fecha_fmt));
        ui_printf_centered_line("%d | %s | %s",
                                sqlite3_column_int(stmt, 0),
                                (const char *)sqlite3_column_text(stmt, 1),
                                fecha_fmt);
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0)
        ui_printf_centered_line("No hay partidos registrados.");
    ui_printf("\n");
}

static void tactica_mostrar_grid_con_ejes(char grid[TACTIC_H][TACTIC_W + 1])
{
    ui_printf("     ");
    for (int x = 0; x < TACTIC_W; x += 5)
        ui_printf("%-5d", x);
    ui_printf("\n");

    for (int y = 0; y < TACTIC_H; y++)
        ui_printf("%2d   %s\n", y, grid[y]);
}

static int tactica_colocar(const char *args, char grid[TACTIC_H][TACTIC_W + 1],
                           char *grid_text, size_t grid_text_size)
{
    int x = -1;
    int y = -1;
    char c = '\0';
#if defined(_WIN32) && defined(_MSC_VER)
    if (sscanf_s(args, "%d %d %c", &x, &y, &c, 1) == 3)
#else
    if (sscanf(args, "%d %d %c", &x, &y, &c) == 3)
#endif
    {
        if (x >= 0 && x < TACTIC_W && y >= 0 && y < TACTIC_H)
        {
            grid[y][x] = c;
            tactica_build_grid_string(grid, grid_text, grid_text_size);
        }
        else
        {
            ui_printf("Fuera de rango (Col: 0..%d, Fila: 0..%d).\n", TACTIC_W - 1, TACTIC_H - 1);
            pause_console();
        }
    }
    else
    {
        ui_printf("Formato: Col Fila Caracter (ej: 10 5 X)\n");
        pause_console();
    }
    return 0;
}

static int tactica_procesar_comando(const char *line, char grid[TACTIC_H][TACTIC_W + 1], char *grid_text,
                                    size_t grid_text_size, int partido_id, const char *nombre)
{
    if (!line || !grid || !grid_text)
        return 0;

    char cmd = line[0];
    switch (cmd)
    {
    case 'q':
    case 'Q':
        return 1;

    case 'r':
    case 'R':
        tactica_init_grid(grid);
        tactica_build_grid_string(grid, grid_text, grid_text_size);
        return 0;

    case 's':
    case 'S':
        tactica_build_grid_string(grid, grid_text, grid_text_size);
        tactica_guardar_diagrama(partido_id, nombre, grid_text);
        ui_printf("Diagrama guardado.\n");
        pause_console();
        return 1;

    case 'p':
    case 'P':
        return tactica_colocar(line + 1, grid, grid_text, grid_text_size);

    case 'd':
    case 'D':
    {
        int x = -1;
        int y = -1;
#if defined(_WIN32) && defined(_MSC_VER)
        if (sscanf_s(line + 1, "%d %d", &x, &y) == 2)
#else
        if (sscanf(line + 1, "%d %d", &x, &y) == 2)
#endif
        {
            if (x >= 0 && x < TACTIC_W && y >= 0 && y < TACTIC_H)
            {
                grid[y][x] = '.';
                tactica_build_grid_string(grid, grid_text, grid_text_size);
            }
            else
            {
                ui_printf("Fuera de rango.\n");
                pause_console();
            }
        }
        else
        {
            ui_printf("Formato: d Col Fila (ej: d 10 5)\n");
            pause_console();
        }
        return 0;
    }

    default:
        if (isdigit((unsigned char)cmd))
            return tactica_colocar(line, grid, grid_text, grid_text_size);
        ui_printf("Comando no reconocido.\n");
        pause_console();
        return 0;
    }
}

static int tactica_listar_diagramas_simple(int con_pause)
{
    const char *sql =
        "SELECT id, partido_id, nombre, fecha FROM tactica_diagrama ORDER BY id DESC LIMIT 20";
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error consultando diagramas.\n");
        if (con_pause)
        {
            pause_console();
        }
        return 0;
    }

    ui_printf_centered_line("ID | Partido | Fecha | Nombre");
    ui_printf_centered_line("-----------------------------------------------");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        int partido_id = sqlite3_column_int(stmt, 1);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 2);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 3);

        ui_printf_centered_line("%d | %d | %s | %s", id, partido_id, fecha ? fecha : "", nombre ? nombre : "");
        count++;
    }

    sqlite3_finalize(stmt);
    if (count == 0)
    {
        ui_printf_centered_line("No hay diagramas registrados.");
    }

    if (con_pause)
    {
        pause_console();
    }

    return count;
}

typedef struct
{
    int id;
    char cancha[96];
    char fecha[20];
} TacticaGuiPartidoRow;

typedef struct
{
    int id;
    int partido_id;
    char nombre[128];
    char fecha[20];
} TacticaGuiDiagramaRow;

static int tactica_gui_expand_rows(void **rows, int *cap, size_t elem_size)
{
    int new_cap;
    void *tmp;

    if (!rows || !cap || *cap <= 0 || elem_size == 0)
    {
        return 0;
    }

    new_cap = (*cap) * 2;
    tmp = realloc(*rows, (size_t)new_cap * elem_size);
    if (!tmp)
    {
        return 0;
    }

    memset((unsigned char *)tmp + (size_t)(*cap) * elem_size, 0, (size_t)(new_cap - *cap) * elem_size);
    *rows = tmp;
    *cap = new_cap;
    return 1;
}

static int tactica_gui_cargar_partidos_rows(TacticaGuiPartidoRow **rows_out, int *count_out)
{
    const char *sql = "SELECT p.id, can.nombre, p.fecha_hora FROM partido p "
                      "JOIN cancha can ON p.cancha_id = can.id "
                      "ORDER BY p.id DESC";
    sqlite3_stmt *stmt = NULL;
    TacticaGuiPartidoRow *rows;
    int cap = 32;
    int count = 0;

    if (!rows_out || !count_out)
    {
        return 0;
    }

    rows = (TacticaGuiPartidoRow *)calloc((size_t)cap, sizeof(TacticaGuiPartidoRow));
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
        const unsigned char *cancha;
        const unsigned char *fecha;

        if (count >= cap && !tactica_gui_expand_rows((void **)&rows, &cap, sizeof(TacticaGuiPartidoRow)))
        {
            sqlite3_finalize(stmt);
            free(rows);
            return 0;
        }

        rows[count].id = sqlite3_column_int(stmt, 0);
        cancha = sqlite3_column_text(stmt, 1);
        fecha = sqlite3_column_text(stmt, 2);

        if (cancha)
        {
            latin1_to_utf8(cancha, rows[count].cancha, sizeof(rows[count].cancha));
        }
        else
        {
            snprintf(rows[count].cancha, sizeof(rows[count].cancha), "(sin cancha)");
        }

        if (fecha)
        {
            format_date_for_display((const char *)fecha, rows[count].fecha, sizeof(rows[count].fecha));
        }
        else
        {
            snprintf(rows[count].fecha, sizeof(rows[count].fecha), "N/A");
        }

        count++;
    }

    sqlite3_finalize(stmt);
    *rows_out = rows;
    *count_out = count;
    return 1;
}

static int tactica_gui_cargar_diagramas_rows(TacticaGuiDiagramaRow **rows_out, int *count_out)
{
    const char *sql = "SELECT id, partido_id, nombre, fecha FROM tactica_diagrama "
                      "ORDER BY id DESC LIMIT 200";
    sqlite3_stmt *stmt = NULL;
    TacticaGuiDiagramaRow *rows;
    int cap = 32;
    int count = 0;

    if (!rows_out || !count_out)
    {
        return 0;
    }

    rows = (TacticaGuiDiagramaRow *)calloc((size_t)cap, sizeof(TacticaGuiDiagramaRow));
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
        const unsigned char *nombre;
        const unsigned char *fecha;

        if (count >= cap && !tactica_gui_expand_rows((void **)&rows, &cap, sizeof(TacticaGuiDiagramaRow)))
        {
            sqlite3_finalize(stmt);
            free(rows);
            return 0;
        }

        rows[count].id = sqlite3_column_int(stmt, 0);
        rows[count].partido_id = sqlite3_column_int(stmt, 1);
        nombre = sqlite3_column_text(stmt, 2);
        fecha = sqlite3_column_text(stmt, 3);

        if (nombre)
        {
            latin1_to_utf8(nombre, rows[count].nombre, sizeof(rows[count].nombre));
        }
        else
        {
            snprintf(rows[count].nombre, sizeof(rows[count].nombre), "Diagrama");
        }

        if (fecha)
        {
            snprintf(rows[count].fecha, sizeof(rows[count].fecha), "%s", (const char *)fecha);
        }
        else
        {
            snprintf(rows[count].fecha, sizeof(rows[count].fecha), "N/A");
        }

        count++;
    }

    sqlite3_finalize(stmt);
    *rows_out = rows;
    *count_out = count;
    return 1;
}

static void tactica_gui_input_text(char *value, size_t cap, int focused)
{
    int key;
    size_t len;

    if (!focused || !value || cap == 0)
    {
        return;
    }

    key = GetCharPressed();
    while (key > 0)
    {
        if (key >= 32 && key <= 126)
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

static int tactica_gui_detectar_click_partido(const TacticaGuiPartidoRow *rows, int count, int scroll, int row_h, Rectangle area)
{
    if (!rows || count <= 0 || !IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        return 0;
    }

    for (int i = scroll; i < count; i++)
    {
        int row = i - scroll;
        int y = (int)area.y + row * row_h;
        Rectangle fr;

        if (y + row_h > (int)(area.y + area.height))
        {
            break;
        }

        fr = (Rectangle){area.x + 2.0f, (float)y, area.width - 4.0f, (float)(row_h - 1)};
        if (CheckCollisionPointRec(GetMousePosition(), fr))
        {
            return rows[i].id;
        }
    }

    return 0;
}

static void tactica_gui_dibujar_partidos_rows(const TacticaGuiPartidoRow *rows, int count, int scroll, int row_h, Rectangle area, int selected_id)
{
    if (!rows || count <= 0)
    {
        return;
    }

    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    for (int i = scroll; i < count; i++)
    {
        int row = i - scroll;
        int y = (int)area.y + row * row_h;
        Rectangle fr;
        int hovered;

        if (y + row_h > (int)(area.y + area.height))
        {
            break;
        }

        fr = (Rectangle){area.x + 2.0f, (float)y, area.width - 4.0f, (float)(row_h - 1)};
        hovered = CheckCollisionPointRec(GetMousePosition(), fr);
        gui_draw_list_row_bg(fr, row, hovered);

        if (rows[i].id == selected_id)
        {
            DrawRectangleRec(fr, (Color){43,87,63,130});
        }

        gui_text(TextFormat("%d", rows[i].id), area.x + 10.0f, (float)(y + 7), 18.0f, gui_get_active_theme()->text_primary);
        gui_text_truncated(rows[i].cancha, area.x + 90.0f, (float)(y + 7), 18.0f,
                           area.width - 210.0f, gui_get_active_theme()->text_primary);
        gui_text(rows[i].fecha, area.x + area.width - 118.0f, (float)(y + 7), 16.0f, gui_get_active_theme()->text_secondary);
    }
    EndScissorMode();
}

static int tactica_gui_seleccionar_partido_y_nombre(int *partido_id_out, char *nombre_out, size_t nombre_size) /* NOSONAR */
{
    TacticaGuiPartidoRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    int selected_id = 0;
    int focused_name = 0;
    const int row_h = 34;

    if (!partido_id_out || !nombre_out || nombre_size == 0)
    {
        return 0;
    }

    if (!tactica_gui_cargar_partidos_rows(&rows, &count))
    {
        return 0;
    }

    if (count <= 0)
    {
        free(rows);
        partido_gui_feedback("CREAR DIAGRAMA", "No hay partidos disponibles", 0);
        return 0;
    }

    if (nombre_out[0] == '\0')
    {
        snprintf(nombre_out, nombre_size, "Diagrama");
    }

    selected_id = rows[0].id;

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_margin = 26;
        int panel_x = 0;
        int panel_w = 0;
        int panel_h = 0;
        int content_h;
        int visible_rows;
        int clicked_id;
        const TacticaGuiPartidoRow *sel = NULL;
        Rectangle list_panel;
        Rectangle list_area;
        Rectangle right_panel;
        Rectangle cancha_preview;
        Rectangle title_band;
        Rectangle name_box;
        Rectangle btn_continue;
        Rectangle btn_cancel;
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        int hover_continue;
        int hover_cancel;

        partido_gui_calcular_panel(sw, sh, &panel_x, &panel_w, &panel_h);
        panel_x = panel_margin;
        panel_w = sw - panel_margin * 2;
        panel_h = sh - 140;
        if (panel_h < 420)
        {
            panel_h = 420;
        }

        list_panel = (Rectangle){(float)panel_x, 156.0f, (float)(panel_w * 60 / 100), (float)(panel_h - 26)};
        right_panel = (Rectangle){list_panel.x + list_panel.width + 14.0f, 156.0f,
                                  (float)panel_x + (float)panel_w - (list_panel.x + list_panel.width + 14.0f), (float)(panel_h - 26)};
        title_band = (Rectangle){(float)panel_x, 106.0f, (float)panel_w, 44.0f};

        if (right_panel.width < 260.0f)
        {
            right_panel.width = 260.0f;
        }

        content_h = (int)list_panel.height - 32;
        if (content_h < row_h)
        {
            content_h = row_h;
        }

        visible_rows = content_h / row_h;
        if (visible_rows < 1)
        {
            visible_rows = 1;
        }

        list_area = (Rectangle){list_panel.x, list_panel.y + 32.0f, list_panel.width, (float)content_h};
        scroll = partido_gui_actualizar_scroll(scroll, count, visible_rows, list_area);
        clicked_id = tactica_gui_detectar_click_partido(rows, count, scroll, row_h, list_area);
        if (clicked_id > 0)
        {
            selected_id = clicked_id;
        }

        for (int i = 0; i < count; i++)
        {
            if (rows[i].id == selected_id)
            {
                sel = &rows[i];
                break;
            }
        }

        cancha_preview = (Rectangle){right_panel.x + 16.0f, right_panel.y + 120.0f, right_panel.width - 32.0f, 148.0f};
        name_box = (Rectangle){right_panel.x + 16.0f, right_panel.y + 300.0f, right_panel.width - 32.0f, 38.0f};
        btn_continue = (Rectangle){right_panel.x + 16.0f, right_panel.y + right_panel.height - 110.0f, right_panel.width - 32.0f, 56.0f};
        btn_cancel = (Rectangle){right_panel.x + 16.0f, right_panel.y + right_panel.height - 46.0f, right_panel.width - 32.0f, 34.0f};
        hover_continue = CheckCollisionPointRec(GetMousePosition(), btn_continue);
        hover_cancel = CheckCollisionPointRec(GetMousePosition(), btn_cancel);

        if (click)
        {
            focused_name = CheckCollisionPointRec(GetMousePosition(), name_box);
        }

        tactica_gui_input_text(nombre_out, nombre_size, focused_name);

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("CREAR DIAGRAMA TACTICO", sw);

        DrawRectangleRounded(title_band, 0.12f, 8, (Color){20,43,31,255});
        gui_text("Paso 1: Selecciona partido y configura nombre", title_band.x + 16.0f, title_band.y + 9.0f,
                 20.0f, gui_get_active_theme()->text_primary);
        gui_text("Disena una pizarra con enfoque futbolistico", title_band.x + 16.0f, title_band.y + 26.0f,
                 15.0f, gui_get_active_theme()->text_secondary);

        gui_draw_list_shell(list_panel, "ID", 10.0f, "CANCHA / FECHA", 84.0f);
        tactica_gui_dibujar_partidos_rows(rows, count, scroll, row_h, list_area, selected_id);

        DrawRectangleRounded(right_panel, 0.07f, 8, gui_get_active_theme()->card_bg);
        DrawRectangleRoundedLines(right_panel, 0.07f, 8, gui_get_active_theme()->card_border);
        gui_text("Configuracion", right_panel.x + 16.0f, right_panel.y + 18.0f, 21.0f, gui_get_active_theme()->text_primary);
        gui_text(TextFormat("Partido seleccionado: %d", selected_id), right_panel.x + 16.0f, right_panel.y + 46.0f,
                 16.0f, gui_get_active_theme()->text_secondary);

        if (sel)
        {
            gui_text_truncated(TextFormat("Cancha: %s", sel->cancha), right_panel.x + 16.0f, right_panel.y + 68.0f,
                               15.0f, right_panel.width - 32.0f, gui_get_active_theme()->text_secondary);
            gui_text(TextFormat("Fecha: %s", sel->fecha), right_panel.x + 16.0f, right_panel.y + 88.0f,
                     15.0f, gui_get_active_theme()->text_secondary);
        }

        tactica_gui_dibujar_cancha_preview(cancha_preview);

        DrawRectangleRec(name_box, (Color){18,39,29,255});
        DrawRectangleLines((int)name_box.x, (int)name_box.y, (int)name_box.width, (int)name_box.height,
                           focused_name ? gui_get_active_theme()->accent_primary : gui_get_active_theme()->card_border);
        gui_text("Nombre:", name_box.x, name_box.y - 22.0f, 16.0f, gui_get_active_theme()->text_secondary);
        gui_text_truncated(nombre_out[0] ? nombre_out : "Diagrama", name_box.x + 8.0f, name_box.y + 8.0f,
                           16.0f, name_box.width - 16.0f, gui_get_active_theme()->text_primary);

        tactica_gui_dibujar_card_accion(btn_continue,
                        "Abrir Editor",
                        "Continuar al tablero tactico",
                        hover_continue ? (Color){44,106,70,255} : (Color){36,92,61,255},
                        hover_continue);
        DrawRectangleRounded(btn_cancel, 0.2f, 8, hover_cancel ? (Color){96,112,100,255} : (Color){77,92,80,255});
        DrawRectangleRoundedLines(btn_cancel, 0.2f, 8,
                      hover_cancel ? (Color){214,237,224,255} : gui_get_active_theme()->card_border);
        gui_text("Cancelar", btn_cancel.x + 42.0f, btn_cancel.y + 7.0f, 17.0f, gui_get_active_theme()->text_primary);

        gui_draw_footer_hint("Click: seleccionar partido | Enter: editar | Esc: volver", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_continue)) || IsKeyPressed(KEY_ENTER))
        {
            if (selected_id > 0)
            {
                *partido_id_out = selected_id;
                if (nombre_out[0] == '\0') /* NOSONAR */
                {
                    snprintf(nombre_out, nombre_size, "Diagrama");
                }
                free(rows);
                input_consume_key(KEY_ENTER);
                return 1;
            }
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_cancel)) || IsKeyPressed(KEY_ESCAPE))
        {
            free(rows);
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    free(rows);
    return 0;
}

static int tactica_gui_detectar_click_celda(Rectangle grid_rect, int cell, int *x_out, int *y_out)
{
    Vector2 mouse;
    int rel_x;
    int rel_y;
    int gx;
    int gy;

    if (!x_out || !y_out || cell <= 0 || !IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        return 0;
    }

    mouse = GetMousePosition();
    if (!CheckCollisionPointRec(mouse, grid_rect))
    {
        return 0;
    }

    rel_x = (int)(mouse.x - grid_rect.x);
    rel_y = (int)(mouse.y - grid_rect.y);
    gx = rel_x / cell;
    gy = rel_y / cell;

    if (gx < 0 || gx >= TACTIC_W || gy < 0 || gy >= TACTIC_H)
    {
        return 0;
    }

    *x_out = gx;
    *y_out = gy;
    return 1;
}

static int tactica_gui_detectar_borrar_celda(Rectangle grid_rect, int cell, int *x_out, int *y_out)
{
    Vector2 mouse;
    int rel_x;
    int rel_y;
    int gx;
    int gy;

    if (!x_out || !y_out || cell <= 0 || !IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
        return 0;
    }

    mouse = GetMousePosition();
    if (!CheckCollisionPointRec(mouse, grid_rect))
    {
        return 0;
    }

    rel_x = (int)(mouse.x - grid_rect.x);
    rel_y = (int)(mouse.y - grid_rect.y);
    gx = rel_x / cell;
    gy = rel_y / cell;

    if (gx < 0 || gx >= TACTIC_W || gy < 0 || gy >= TACTIC_H)
    {
        return 0;
    }

    *x_out = gx;
    *y_out = gy;
    return 1;
}

static void tactica_gui_dibujar_grid(Rectangle grid_rect, int cell, char grid[TACTIC_H][TACTIC_W + 1], int draw_coords)
{
    float text_size = (float)(cell - 4);
    if (text_size < 8.0f)
    {
        text_size = 8.0f;
    }

    for (int y = 0; y < TACTIC_H; y++)
    {
        for (int x = 0; x < TACTIC_W; x++)
        {
            Rectangle c =
            {
                grid_rect.x + (float)(x * cell),
                grid_rect.y + (float)(y * cell),
                (float)cell,
                (float)cell
            };
            char t[2] = {grid[y][x], '\0'};
            Color fill = ((x + y) % 2 == 0) ? (Color){18,39,29,255} : (Color){22,45,33,255};

            DrawRectangleRec(c, fill);
            DrawRectangleLines((int)c.x, (int)c.y, (int)c.width, (int)c.height, (Color){42,72,56,255});

            if (grid[y][x] != '.')
            {
                gui_text(t, c.x + 3.0f, c.y + 1.0f, text_size, gui_get_active_theme()->text_primary);
            }
        }
    }

    if (draw_coords && cell >= 14)
    {
        for (int x = 0; x < TACTIC_W; x += 5)
        {
            gui_text(TextFormat("%d", x), grid_rect.x + (float)(x * cell), grid_rect.y - 18.0f, 14.0f,
                     gui_get_active_theme()->text_secondary);
        }
        for (int y = 0; y < TACTIC_H; y += 2)
        {
            gui_text(TextFormat("%d", y), grid_rect.x - 20.0f, grid_rect.y + (float)(y * cell), 14.0f,
                     gui_get_active_theme()->text_secondary);
        }
    }
}

static int tactica_gui_editor(int partido_id, const char *nombre) /* NOSONAR */
{
    char grid[TACTIC_H][TACTIC_W + 1];
    char grid_text[(TACTIC_H * (TACTIC_W + 1)) + 1];
    char brush = 'X';
    const char brush_options[] = {'X', 'O', 'L', 'V', 'M', 'A', 'D', 'N', '1', '2', '3', '.'};
    const int brush_count = (int)(sizeof(brush_options) / sizeof(brush_options[0]));

    tactica_init_grid(grid);
    tactica_build_grid_string(grid, grid_text, sizeof(grid_text));
    input_consume_key(KEY_ENTER);
    input_consume_key(KEY_ESCAPE);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        const int margin = 22;
        const int sidebar_w = 220;
        const int header_y = 132;
        const int footer_h = 84;
        int left_area_w = sw - (sidebar_w + margin * 3);
        int max_w = left_area_w / TACTIC_W;
        int max_h = (sh - header_y - footer_h) / TACTIC_H;
        int cell = (max_w < max_h) ? max_w : max_h;
        int gx;
        int gy;
        int key;
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        int saved_from_button = 0;
        int cancel_from_button = 0;
        Rectangle grid_rect;
        Rectangle sidebar_rect;
        Rectangle btn_reset;
        Rectangle btn_save;
        Rectangle btn_cancel;

        if (left_area_w < 280)
        {
            left_area_w = 280;
            max_w = left_area_w / TACTIC_W;
            cell = (max_w < max_h) ? max_w : max_h;
        }

        if (cell < 10)
        {
            cell = 10;
        }

        sidebar_rect = (Rectangle)
        {
            (float)(sw - margin - sidebar_w),
            (float)header_y,
            (float)sidebar_w,
            (float)(sh - header_y - footer_h + 6)
        };

        grid_rect = (Rectangle)
        {
            (float)(margin + (left_area_w - (TACTIC_W * cell)) / 2),
            (float)header_y,
            (float)(TACTIC_W * cell),
            (float)(TACTIC_H * cell)
        };

        if (tactica_gui_detectar_click_celda(grid_rect, cell, &gx, &gy))
        {
            grid[gy][gx] = brush;
        }

        if (tactica_gui_detectar_borrar_celda(grid_rect, cell, &gx, &gy))
        {
            grid[gy][gx] = '.';
        }

        key = GetCharPressed();
        while (key > 0)
        {
            if (key >= 33 && key <= 126)
            {
                brush = (char)toupper((unsigned char)key);
            }
            key = GetCharPressed();
        }

        if (IsKeyPressed(KEY_R))
        {
            tactica_init_grid(grid);
        }

        btn_reset = (Rectangle){sidebar_rect.x + 16.0f, sidebar_rect.y + sidebar_rect.height - 124.0f, sidebar_rect.width - 32.0f, 32.0f};
        btn_save = (Rectangle){sidebar_rect.x + 16.0f, sidebar_rect.y + sidebar_rect.height - 84.0f, sidebar_rect.width - 32.0f, 34.0f};
        btn_cancel = (Rectangle){sidebar_rect.x + 16.0f, sidebar_rect.y + sidebar_rect.height - 42.0f, sidebar_rect.width - 32.0f, 32.0f};

        if (click)
        {
            for (int i = 0; i < brush_count; i++)
            {
                int col = i % 4;
                int row = i / 4;
                Rectangle b =
                {
                    sidebar_rect.x + 16.0f + (float)(col * 48),
                    sidebar_rect.y + 62.0f + (float)(row * 44),
                    40.0f,
                    34.0f
                };
                if (CheckCollisionPointRec(GetMousePosition(), b)) /* NOSONAR */
                {
                    brush = brush_options[i];
                }
            }

            if (CheckCollisionPointRec(GetMousePosition(), btn_reset))
            {
                tactica_init_grid(grid);
            }

            if (CheckCollisionPointRec(GetMousePosition(), btn_save))
            {
                saved_from_button = 1;
            }

            if (CheckCollisionPointRec(GetMousePosition(), btn_cancel))
            {
                cancel_from_button = 1;
            }
        }

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("EDITAR DIAGRAMA", sw);
        gui_text(TextFormat("Partido ID: %d", partido_id), 32.0f, 86.0f, 17.0f, gui_get_active_theme()->text_secondary);
        gui_text_truncated(TextFormat("Nombre: %s", nombre && nombre[0] ? nombre : "Diagrama"),
                           210.0f, 86.0f, 17.0f, (float)(sw - 300), gui_get_active_theme()->text_secondary);
        gui_text(TextFormat("Pincel activo: %c", brush), 32.0f, 108.0f, 18.0f, gui_get_active_theme()->text_primary);

        tactica_gui_dibujar_grid(grid_rect, cell, grid, 1);

        DrawRectangleRec(sidebar_rect, gui_get_active_theme()->card_bg);
        DrawRectangleLines((int)sidebar_rect.x, (int)sidebar_rect.y, (int)sidebar_rect.width, (int)sidebar_rect.height,
                           gui_get_active_theme()->card_border);
        gui_text("Herramientas", sidebar_rect.x + 16.0f, sidebar_rect.y + 16.0f, 20.0f, gui_get_active_theme()->text_primary);
        gui_text("Pinceles", sidebar_rect.x + 16.0f, sidebar_rect.y + 40.0f, 16.0f, gui_get_active_theme()->text_secondary);

        for (int i = 0; i < brush_count; i++)
        {
            int col = i % 4;
            int row = i / 4;
            Rectangle b =
            {
                sidebar_rect.x + 16.0f + (float)(col * 48),
                sidebar_rect.y + 62.0f + (float)(row * 44),
                40.0f,
                34.0f
            };
            int selected = (brush == brush_options[i]);
            Color fill = selected ? gui_get_active_theme()->accent_primary : (Color){24,49,36,255};

            DrawRectangleRec(b, fill);
            DrawRectangleLines((int)b.x, (int)b.y, (int)b.width, (int)b.height,
                               selected ? (Color){220,240,230,255} : gui_get_active_theme()->card_border);
            gui_text(TextFormat("%c", brush_options[i]), b.x + 14.0f, b.y + 7.0f, 18.0f, gui_get_active_theme()->text_primary);
        }

        DrawRectangleRec(btn_reset, (Color){55,84,68,255});
        DrawRectangleRec(btn_save, gui_get_active_theme()->accent_primary);
        DrawRectangleRec(btn_cancel, (Color){95,66,66,255});
        DrawRectangleLines((int)btn_reset.x, (int)btn_reset.y, (int)btn_reset.width, (int)btn_reset.height, gui_get_active_theme()->card_border);
        DrawRectangleLines((int)btn_save.x, (int)btn_save.y, (int)btn_save.width, (int)btn_save.height, gui_get_active_theme()->card_border);
        DrawRectangleLines((int)btn_cancel.x, (int)btn_cancel.y, (int)btn_cancel.width, (int)btn_cancel.height, gui_get_active_theme()->card_border);
        gui_text("Reiniciar (R)", btn_reset.x + 34.0f, btn_reset.y + 7.0f, 16.0f, gui_get_active_theme()->text_primary);
        gui_text("Guardar (Enter)", btn_save.x + 26.0f, btn_save.y + 8.0f, 17.0f, gui_get_active_theme()->text_primary);
        gui_text("Cancelar (Esc)", btn_cancel.x + 26.0f, btn_cancel.y + 7.0f, 16.0f, gui_get_active_theme()->text_primary);

        gui_draw_footer_hint("Click izq: pintar | Click der: borrar | Tecla: cambiar pincel",
                             24.0f, sh);
        EndDrawing();

        if (saved_from_button || IsKeyPressed(KEY_ENTER))
        {
            tactica_build_grid_string(grid, grid_text, sizeof(grid_text));
            tactica_guardar_diagrama(partido_id, nombre && nombre[0] ? nombre : "Diagrama", grid_text);
            partido_gui_feedback("CREAR DIAGRAMA", "Diagrama guardado", 1);
            input_consume_key(KEY_ENTER);
            return 1;
        }

        if (cancel_from_button || IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static int tactica_crear_diagrama_gui(void)
{
    int partido_id = 0;
    char nombre[128] = "Diagrama";

    if (!tactica_gui_seleccionar_partido_y_nombre(&partido_id, nombre, sizeof(nombre)))
    {
        return 0;
    }

    return tactica_gui_editor(partido_id, nombre);
}

static void tactica_gui_parse_grid_text(const char *grid_text, char grid[TACTIC_H][TACTIC_W + 1])
{
    int x = 0;
    int y = 0;

    tactica_init_grid(grid);
    if (!grid_text)
    {
        return;
    }

    while (*grid_text && y < TACTIC_H)
    {
        char c = *grid_text++;

        if (c == '\r')
        {
            continue;
        }

        if (c == '\n')
        {
            y++;
            x = 0;
            continue;
        }

        if (x < TACTIC_W)
        {
            grid[y][x++] = c;
        }
    }
}

static int tactica_gui_cargar_diagrama(int diagrama_id, char *nombre, size_t nombre_size,
                                       int *partido_id, char *fecha, size_t fecha_size,
                                       char grid[TACTIC_H][TACTIC_W + 1])
{
    const char *sql = "SELECT partido_id, nombre, fecha, grid FROM tactica_diagrama WHERE id = ?";
    sqlite3_stmt *stmt = NULL;
    int ok = 0;

    if (!nombre || nombre_size == 0 || !partido_id || !fecha || fecha_size == 0 || !grid)
    {
        return 0;
    }

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, diagrama_id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *db_nombre = sqlite3_column_text(stmt, 1);
        const unsigned char *db_fecha = sqlite3_column_text(stmt, 2);
        const unsigned char *db_grid = sqlite3_column_text(stmt, 3);

        *partido_id = sqlite3_column_int(stmt, 0);
        if (db_nombre)
        {
            latin1_to_utf8(db_nombre, nombre, nombre_size);
        }
        else
        {
            snprintf(nombre, nombre_size, "Diagrama");
        }

        if (db_fecha)
        {
            snprintf(fecha, fecha_size, "%s", (const char *)db_fecha);
        }
        else
        {
            snprintf(fecha, fecha_size, "N/A");
        }

        tactica_gui_parse_grid_text((const char *)db_grid, grid);
        ok = 1;
    }

    sqlite3_finalize(stmt);
    return ok;
}

static int tactica_gui_ver_detalle_diagrama(int diagrama_id) /* NOSONAR */
{
    char nombre[128] = {0};
    char fecha[20] = {0};
    char grid[TACTIC_H][TACTIC_W + 1];
    int partido_id = 0;

    if (!tactica_gui_cargar_diagrama(diagrama_id, nombre, sizeof(nombre), &partido_id, fecha, sizeof(fecha), grid))
    {
        partido_gui_feedback("VER DIAGRAMA", "No se pudo cargar el diagrama", 0);
        return 0;
    }

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = 30;
        int panel_y = 120;
        int panel_w = sw - 60;
        int panel_h = sh - 170;
        Rectangle top_band;
        Rectangle meta_box;
        Rectangle preview_box;
        int max_w;
        int max_h;
        int cell = (max_w < max_h) ? max_w : max_h;
        Rectangle grid_rect;

        if (panel_h < 320)
        {
            panel_h = 320;
        }

        top_band = (Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, 56.0f};
        meta_box = (Rectangle){(float)(panel_x + 14), (float)(panel_y + 66), (float)(panel_w - 220), 84.0f};
        preview_box = (Rectangle){(float)(panel_x + panel_w - 190), (float)(panel_y + 66), 176.0f, 84.0f};

        max_w = ((int)meta_box.width - 28) / TACTIC_W;
        max_h = (panel_h - 170) / TACTIC_H;
        if (max_w < 10)
        {
            max_w = 10;
        }
        if (max_h < 10)
        {
            max_h = 10;
        }
        cell = (max_w < max_h) ? max_w : max_h;

        if (cell < 10)
        {
            cell = 10;
        }

        grid_rect = (Rectangle)
        {
            (float)(panel_x + (panel_w - (TACTIC_W * cell)) / 2),
            (float)(panel_y + 162),
            (float)(TACTIC_W * cell),
            (float)(TACTIC_H * cell)
        };

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("VER DIAGRAMA", sw);

        DrawRectangleRounded(top_band, 0.08f, 8, (Color){20,43,31,255});
        gui_text_truncated(nombre, top_band.x + 14.0f, top_band.y + 12.0f, 24.0f,
                           top_band.width - 28.0f, gui_get_active_theme()->text_primary);
        gui_text("Vista de pizarra tactica", top_band.x + 16.0f, top_band.y + 34.0f,
                 15.0f, gui_get_active_theme()->text_secondary);

        DrawRectangleRounded(meta_box, 0.10f, 8, gui_get_active_theme()->card_bg);
        DrawRectangleRoundedLines(meta_box, 0.10f, 8, gui_get_active_theme()->card_border);
        gui_text(TextFormat("ID: %d", diagrama_id), meta_box.x + 12.0f, meta_box.y + 12.0f, 17.0f, gui_get_active_theme()->text_primary);
        gui_text(TextFormat("Partido: %d", partido_id), meta_box.x + 108.0f, meta_box.y + 12.0f, 17.0f, gui_get_active_theme()->text_primary);
        gui_text(TextFormat("Fecha: %s", fecha), meta_box.x + 250.0f, meta_box.y + 12.0f, 17.0f, gui_get_active_theme()->text_primary);
        gui_text("Distribucion en cancha y ubicacion de marcas", meta_box.x + 12.0f, meta_box.y + 42.0f,
                 15.0f, gui_get_active_theme()->text_secondary);

        tactica_gui_dibujar_cancha_preview(preview_box);
        tactica_gui_dibujar_grid(grid_rect, cell, grid, 1);
        gui_draw_footer_hint("Esc/Enter: volver", 24.0f, sh);
        EndDrawing();

        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
        {
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            return 1;
        }
    }

    return 0;
}

static int tactica_gui_detectar_click_diagrama(const TacticaGuiDiagramaRow *rows, int count, int scroll, int row_h, Rectangle area)
{
    if (!rows || count <= 0 || !IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        return 0;
    }

    for (int i = scroll; i < count; i++)
    {
        int row = i - scroll;
        int y = (int)area.y + row * row_h;
        Rectangle fr;

        if (y + row_h > (int)(area.y + area.height))
        {
            break;
        }

        fr = (Rectangle){area.x + 2.0f, (float)y, area.width - 4.0f, (float)(row_h - 1)};
        if (CheckCollisionPointRec(GetMousePosition(), fr))
        {
            return rows[i].id;
        }
    }

    return 0;
}

static void tactica_gui_dibujar_diagramas_rows(const TacticaGuiDiagramaRow *rows, int count, int scroll, int row_h, Rectangle area, int selected_id)
{
    if (!rows || count <= 0)
    {
        return;
    }

    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    for (int i = scroll; i < count; i++)
    {
        int row = i - scroll;
        int y = (int)area.y + row * row_h;
        Rectangle fr;
        int hovered;

        if (y + row_h > (int)(area.y + area.height))
        {
            break;
        }

        fr = (Rectangle){area.x + 2.0f, (float)y, area.width - 4.0f, (float)(row_h - 1)};
        hovered = CheckCollisionPointRec(GetMousePosition(), fr);
        gui_draw_list_row_bg(fr, row, hovered);

        if (rows[i].id == selected_id)
        {
            DrawRectangleRec(fr, (Color){43,87,63,130});
        }

        gui_text(TextFormat("%d", rows[i].id), area.x + 10.0f, (float)(y + 7), 18.0f, gui_get_active_theme()->text_primary);
        gui_text(TextFormat("P:%d", rows[i].partido_id), area.x + 70.0f, (float)(y + 7), 16.0f, gui_get_active_theme()->text_secondary);
        gui_text_truncated(rows[i].nombre, area.x + 128.0f, (float)(y + 7), 18.0f,
                           area.width - 260.0f, gui_get_active_theme()->text_primary);
        gui_text(rows[i].fecha, area.x + area.width - 124.0f, (float)(y + 7), 16.0f, gui_get_active_theme()->text_secondary);
    }
    EndScissorMode();
}

static int tactica_ver_diagrama_gui(void) /* NOSONAR */
{
    TacticaGuiDiagramaRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    int selected_id = 0;
    const int row_h = 34;

    if (!tactica_gui_cargar_diagramas_rows(&rows, &count))
    {
        partido_gui_feedback("VER DIAGRAMAS", "Error consultando diagramas", 0);
        return 0;
    }

    if (count <= 0)
    {
        free(rows);
        partido_gui_feedback("VER DIAGRAMAS", "No hay diagramas registrados", 0);
        return 0;
    }

    selected_id = rows[0].id;

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_margin = 26;
        int panel_x = 0;
        int panel_w = 0;
        int panel_h = 0;
        int content_h;
        int visible_rows;
        int clicked_id;
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        const TacticaGuiDiagramaRow *selected_row = NULL;
        Rectangle title_band;
        Rectangle list_panel;
        Rectangle right_panel;
        Rectangle cancha_preview;
        Rectangle area;
        Rectangle btn_open;
        Rectangle btn_back;
        int hover_open;
        int hover_back;

        partido_gui_calcular_panel(sw, sh, &panel_x, &panel_w, &panel_h);
        panel_x = panel_margin;
        panel_w = sw - panel_margin * 2;
        panel_h = sh - 140;
        if (panel_h < 420)
        {
            panel_h = 420;
        }

        title_band = (Rectangle){(float)panel_x, 106.0f, (float)panel_w, 44.0f};
        list_panel = (Rectangle){(float)panel_x, 156.0f, (float)(panel_w * 64 / 100), (float)(panel_h - 26)};
        right_panel = (Rectangle){list_panel.x + list_panel.width + 14.0f, 156.0f,
                                  (float)panel_x + (float)panel_w - (list_panel.x + list_panel.width + 14.0f), (float)(panel_h - 26)};

        content_h = (int)list_panel.height - 66;
        if (content_h < row_h)
        {
            content_h = row_h;
        }

        visible_rows = content_h / row_h;
        if (visible_rows < 1)
        {
            visible_rows = 1;
        }

        area = (Rectangle){list_panel.x, list_panel.y + 32.0f, list_panel.width, (float)content_h};
        scroll = partido_gui_actualizar_scroll(scroll, count, visible_rows, area);
        clicked_id = tactica_gui_detectar_click_diagrama(rows, count, scroll, row_h, area);
        if (clicked_id > 0)
        {
            selected_id = clicked_id;
        }

        for (int i = 0; i < count; i++)
        {
            if (rows[i].id == selected_id)
            {
                selected_row = &rows[i];
                break;
            }
        }

        cancha_preview = (Rectangle){right_panel.x + 16.0f, right_panel.y + 110.0f, right_panel.width - 32.0f, 148.0f};
        btn_open = (Rectangle){right_panel.x + 16.0f, right_panel.y + right_panel.height - 110.0f, right_panel.width - 32.0f, 56.0f};
        btn_back = (Rectangle){right_panel.x + 16.0f, right_panel.y + right_panel.height - 46.0f, right_panel.width - 32.0f, 34.0f};
        hover_open = CheckCollisionPointRec(GetMousePosition(), btn_open);
        hover_back = CheckCollisionPointRec(GetMousePosition(), btn_back);

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("DIAGRAMAS TACTICOS", sw);

        DrawRectangleRounded(title_band, 0.12f, 8, (Color){20,43,31,255});
        gui_text("Paso 2: Revisa y abre tus diagramas", title_band.x + 16.0f, title_band.y + 9.0f,
                 20.0f, gui_get_active_theme()->text_primary);
        gui_text("Selecciona un registro para ver el tablero completo", title_band.x + 16.0f, title_band.y + 26.0f,
                 15.0f, gui_get_active_theme()->text_secondary);

        gui_draw_list_shell(list_panel,
                            "ID/PARTIDO", 10.0f, "NOMBRE / FECHA", 128.0f);
        tactica_gui_dibujar_diagramas_rows(rows, count, scroll, row_h, area, selected_id);

        DrawRectangleRounded(right_panel, 0.07f, 8, gui_get_active_theme()->card_bg);
        DrawRectangleRoundedLines(right_panel, 0.07f, 8, gui_get_active_theme()->card_border);
        gui_text("Resumen", right_panel.x + 16.0f, right_panel.y + 18.0f, 21.0f, gui_get_active_theme()->text_primary);

        if (selected_row)
        {
            gui_text(TextFormat("ID: %d", selected_row->id), right_panel.x + 16.0f, right_panel.y + 48.0f,
                     16.0f, gui_get_active_theme()->text_secondary);
            gui_text(TextFormat("Partido: %d", selected_row->partido_id), right_panel.x + 16.0f, right_panel.y + 68.0f,
                     16.0f, gui_get_active_theme()->text_secondary);
            gui_text(TextFormat("Fecha: %s", selected_row->fecha), right_panel.x + 16.0f, right_panel.y + 88.0f,
                     16.0f, gui_get_active_theme()->text_secondary);
            gui_text_truncated(selected_row->nombre, right_panel.x + 16.0f, right_panel.y + 270.0f,
                               18.0f, right_panel.width - 32.0f, gui_get_active_theme()->text_primary);
        }

        tactica_gui_dibujar_cancha_preview(cancha_preview);

        tactica_gui_dibujar_card_accion(btn_open,
                                        "Abrir Diagrama",
                                        "Ver pizarra completa",
                                        hover_open ? (Color){44,106,70,255} : (Color){36,92,61,255},
                                        hover_open);

        DrawRectangleRounded(btn_back, 0.2f, 8, hover_back ? (Color){96,112,100,255} : (Color){77,92,80,255});
        DrawRectangleRoundedLines(btn_back, 0.2f, 8,
                                  hover_back ? (Color){214,237,224,255} : gui_get_active_theme()->card_border);
        gui_text("Volver", btn_back.x + 42.0f, btn_back.y + 7.0f, 17.0f, gui_get_active_theme()->text_primary);

        gui_draw_footer_hint("Click: seleccionar | Enter: abrir | Esc: volver", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_open)) || IsKeyPressed(KEY_ENTER))
        {
            if (selected_id > 0)
            {
                tactica_gui_ver_detalle_diagrama(selected_id);
                input_consume_key(KEY_ENTER);
                continue;
            }
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_back)) || IsKeyPressed(KEY_ESCAPE))
        {
            free(rows);
            input_consume_key(KEY_ESCAPE);
            return 1;
        }
    }

    free(rows);
    return 0;
}

static void tactica_ver_diagrama()
{
    if (IsWindowReady())
    {
        (void)tactica_ver_diagrama_gui();
        return;
    }

    clear_screen();
    print_header("DIAGRAMAS TACTICOS");

    int count = tactica_listar_diagramas_simple(0);
    if (count == 0)
    {
        pause_console();
        return;
    }

    int id = input_int("ID del diagrama (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    const char *sql = "SELECT nombre, grid FROM tactica_diagrama WHERE id = ?";
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error consultando diagrama.\n");
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
        const char *grid = (const char *)sqlite3_column_text(stmt, 1);

        clear_screen();
        print_header(nombre ? nombre : "DIAGRAMA");
        if (grid && grid[0])
        {
            ui_puts(grid);
        }
        else
        {
            ui_puts("(Sin contenido)");
        }
    }
    else
    {
        printf("Diagrama no encontrado.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}


static void tactica_crear_diagrama()
{
    if (IsWindowReady())
    {
        (void)tactica_crear_diagrama_gui();
        return;
    }

    clear_screen();
    print_header("CREAR DIAGRAMA TACTICO");

    tactica_mostrar_partidos_disponibles();

    int partido_id = input_int("ID de partido (0 para cancelar): ");
    if (partido_id <= 0)
        return;

    if (!existe_id("partido", partido_id))
    {
        printf("Partido no encontrado.\n");
        pause_console();
        return;
    }

    char nombre[128] = {0};
    tactica_leer_nombre_diagrama(nombre, sizeof(nombre));

    char grid[TACTIC_H][TACTIC_W + 1];
    tactica_init_grid(grid);

    char grid_text[(TACTIC_H * (TACTIC_W + 1)) + 1];
    tactica_build_grid_string(grid, grid_text, sizeof(grid_text));

    for (;;)
    {
        clear_screen();
        print_header("EDITAR DIAGRAMA");
        tactica_mostrar_grid_con_ejes(grid);
        ui_printf("\nComandos:\n");
        ui_printf("  Col Fila C -> colocar caracter C (ej: 10 5 X)\n");
        ui_printf("  d Col Fila -> borrar posicion\n");
        ui_printf("  r          -> reiniciar diagrama\n");
        ui_printf("  s          -> guardar diagrama\n");
        ui_printf("  q          -> cancelar\n");
        ui_printf("\nIngrese comando: ");

        char line[64] = {0};
        if (!fgets(line, sizeof(line), stdin))
            return;

        tactica_trim_newline(line);

        if (line[0] == '\0')
            continue;

        if (tactica_procesar_comando(line, grid, grid_text, sizeof(grid_text), partido_id, nombre))
            return;
    }
}

static void tactica_gui_dibujar_cancha_preview(Rectangle rect)
{
    float cx = rect.x + rect.width * 0.5f;
    float cy = rect.y + rect.height * 0.5f;
    float box_w = rect.width * 0.18f;
    float box_h = rect.height * 0.52f;
    float small_box_w = rect.width * 0.08f;
    float small_box_h = rect.height * 0.28f;
    float r = rect.height * 0.16f;
    Color line = (Color){214,237,224,225};

    DrawRectangleRounded(rect, 0.04f, 8, (Color){22,68,41,255});
    DrawRectangleRoundedLines(rect, 0.04f, 8, line);

    DrawLineEx((Vector2){cx, rect.y + 2.0f}, (Vector2){cx, rect.y + rect.height - 2.0f}, 2.0f, line);
    DrawCircleLines((int)cx, (int)cy, r, line);

    DrawRectangleLinesEx((Rectangle){rect.x + 10.0f, cy - box_h * 0.5f, box_w, box_h}, 2.0f, line);
    DrawRectangleLinesEx((Rectangle){rect.x + 10.0f, cy - small_box_h * 0.5f, small_box_w, small_box_h}, 2.0f, line);
    DrawRectangleLinesEx((Rectangle){rect.x + rect.width - 10.0f - box_w, cy - box_h * 0.5f, box_w, box_h}, 2.0f, line);
    DrawRectangleLinesEx((Rectangle){rect.x + rect.width - 10.0f - small_box_w, cy - small_box_h * 0.5f, small_box_w, small_box_h}, 2.0f, line);

    DrawCircle((int)(rect.x + rect.width * 0.18f), (int)(rect.y + rect.height * 0.50f), 5.5f, (Color){250,232,189,255});
    DrawCircle((int)(rect.x + rect.width * 0.34f), (int)(rect.y + rect.height * 0.30f), 5.0f, (Color){250,232,189,255});
    DrawCircle((int)(rect.x + rect.width * 0.34f), (int)(rect.y + rect.height * 0.70f), 5.0f, (Color){250,232,189,255});
    DrawCircle((int)(rect.x + rect.width * 0.50f), (int)(rect.y + rect.height * 0.50f), 5.5f, (Color){250,232,189,255});
    DrawCircle((int)(rect.x + rect.width * 0.66f), (int)(rect.y + rect.height * 0.28f), 5.0f, (Color){250,232,189,255});
    DrawCircle((int)(rect.x + rect.width * 0.66f), (int)(rect.y + rect.height * 0.72f), 5.0f, (Color){250,232,189,255});
    DrawCircle((int)(rect.x + rect.width * 0.82f), (int)(rect.y + rect.height * 0.50f), 5.5f, (Color){250,232,189,255});
}

static void tactica_gui_dibujar_card_accion(Rectangle rect, const char *title, const char *subtitle,
                                            Color fill, int hovered)
{
    Color border = hovered ? (Color){214,237,224,255} : gui_get_active_theme()->card_border;
    DrawRectangleRounded(rect, 0.12f, 8, fill);
    DrawRectangleRoundedLines(rect, 0.12f, 8, border);
    gui_text_truncated(title, rect.x + 16.0f, rect.y + 12.0f, 20.0f, rect.width - 24.0f, gui_get_active_theme()->text_primary);
    gui_text_truncated(subtitle, rect.x + 16.0f, rect.y + 38.0f, 15.0f, rect.width - 24.0f, gui_get_active_theme()->text_secondary);
}

static int menu_tacticas_partido_gui(void)
{
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = (sw > 1120) ? 980 : sw - 40;
        int panel_h = (sh > 760) ? 560 : sh - 140;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle cancha;
        Rectangle btn_crear;
        Rectangle btn_ver;
        Rectangle btn_volver;
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        int hover_crear;
        int hover_ver;
        int hover_volver;

        if (panel_h < 420)
        {
            panel_h = 420;
            panel_y = (sh - panel_h) / 2;
        }

        cancha = (Rectangle)
        {
            (float)(panel_x + (panel_w - 640) / 2),
            (float)(panel_y + 108),
            640.0f,
            220.0f
        };

        if (cancha.x < panel_x + 22)
        {
            cancha.x = (float)(panel_x + 22);
            cancha.width = (float)(panel_w - 44);
        }

        btn_crear = (Rectangle)
        {
            (float)(panel_x + panel_w / 2 - 240),
            (float)(panel_y + panel_h - 168),
            220.0f,
            84.0f
        };
        btn_ver = (Rectangle)
        {
            (float)(panel_x + panel_w / 2 + 20),
            (float)(panel_y + panel_h - 168),
            220.0f,
            84.0f
        };
        btn_volver = (Rectangle)
        {
            (float)(panel_x + panel_w / 2 - 110),
            (float)(panel_y + panel_h - 74),
            220.0f,
            44.0f
        };

        hover_crear = CheckCollisionPointRec(GetMousePosition(), btn_crear);
        hover_ver = CheckCollisionPointRec(GetMousePosition(), btn_ver);
        hover_volver = CheckCollisionPointRec(GetMousePosition(), btn_volver);

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        gui_draw_module_header("ANALISIS TACTICO", sw);
        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        DrawRectangleRounded((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, 56.0f},
                             0.08f, 8, (Color){20,43,31,255});

        gui_text("Centro de Tacticas de Futbol", (float)(panel_x + 24), (float)(panel_y + 18),
                 24.0f, gui_get_active_theme()->text_primary);
        gui_text("Diseña esquemas, ajusta roles y consulta tus tableros guardados",
                 (float)(panel_x + 24), (float)(panel_y + 44),
                 16.0f, gui_get_active_theme()->text_secondary);

        tactica_gui_dibujar_cancha_preview(cancha);

        tactica_gui_dibujar_card_accion(btn_crear,
                                        "Crear Diagrama",
                                        "Arma una pizarra nueva desde cero",
                                        hover_crear ? (Color){44,106,70,255} : (Color){36,92,61,255},
                                        hover_crear);

        tactica_gui_dibujar_card_accion(btn_ver,
                                        "Ver Diagramas",
                                        "Explora y abre tacticas guardadas",
                                        hover_ver ? (Color){41,97,86,255} : (Color){33,84,74,255},
                                        hover_ver);

        DrawRectangleRounded(btn_volver, 0.2f, 8, hover_volver ? (Color){96,112,100,255} : (Color){77,92,80,255});
        DrawRectangleRoundedLines(btn_volver, 0.2f, 8,
                                  hover_volver ? (Color){214,237,224,255} : gui_get_active_theme()->card_border);
        gui_text("Volver", btn_volver.x + 74.0f, btn_volver.y + 11.0f, 18.0f, gui_get_active_theme()->text_primary);

        gui_draw_footer_hint("Click: elegir | C: crear | V: ver | Esc: volver", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_crear)) || IsKeyPressed(KEY_C))
        {
            tactica_crear_diagrama();
            continue;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_ver)) || IsKeyPressed(KEY_V))
        {
            tactica_ver_diagrama();
            continue;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_volver)) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            return 1;
        }
    }

    return 0;
}

void menu_tacticas_partido()
{
    if (IsWindowReady())
    {
        (void)menu_tacticas_partido_gui();
        return;
    }

    MenuItem items[] =
    {
        {1, "Crear diagrama", tactica_crear_diagrama},
        {2, "Ver diagramas", tactica_ver_diagrama},
        {0, "Volver", NULL}
    };

    clear_screen();
    print_header("ANALISIS TACTICO");
    ejecutar_menu_estandar("ANALISIS TACTICO", items, 3);
}

/**
 * @brief Muestra el menu principal de gestion de partidos
 *
 * Presenta un menu interactivo con opciones para crear, listar, modificar,
 * eliminar partidos y simular partidos con equipos guardados.
 * Utiliza la funcion ejecutar_menu para manejar la navegacion del menu
 * y delega las operaciones a las funciones correspondientes.
 */
void menu_partidos()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_partido},
        {2, "Listar", listar_partidos},
        {3, "Modificar", modificar_partido},
        {4, "Eliminar", eliminar_partido},
        {5, "Simular con Equipos Guardados", simular_partido_guardados},
        {6, "Analisis Tactico", menu_tacticas_partido},
        {0, "Volver", NULL}
    };

    ejecutar_menu_estandar("PARTIDOS", items, 7);
}
