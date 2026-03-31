#include "partido.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "camiseta.h"
#include "equipo.h"
#include "ascii_art.h"
#include "entrenador_ia.h"
#include "financiamiento.h"
#include "settings.h"
#ifdef ENABLE_RAYLIB_GUI
#include "input.h"
#include "gui_components.h"
#endif
#include <stdio.h>
#include <string.h>
#if defined(_WIN32) && !defined(ENABLE_RAYLIB_GUI)
#include <windows.h>
#else
#include "compat_windows.h"
#endif
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
/* GUI (Raylib) helper: listado de partidos en modo gráfico */
#ifdef ENABLE_RAYLIB_GUI
static int listar_partidos_gui(void);
#endif

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
#ifdef ENABLE_RAYLIB_GUI
/* Convierte Latin1 -> UTF-8 (solo para GUI) */
static void latin1_to_utf8(const unsigned char *src, char *dst, size_t dst_sz)
{
    if (!dst || dst_sz == 0) return;
    if (!src) { if (dst_sz > 0) dst[0] = '\0'; return; }
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

static int listar_partidos_gui(void)
{
    sqlite3_stmt *stmt = NULL;
    int cap = 64;
    int count = 0;
    int scroll = 0;
    int row_h = 30;
    int panel_x = 80;
    int panel_y = 130;
    int panel_w = 1100;
    int panel_h = 520;

    typedef struct { int id; char fecha[64]; char cancha[128]; char camiseta[128]; char marcador[32]; char resultado[64]; char rendimiento_general[64]; char cansancio[64]; char estado_animo[64]; char comentario_personal[256]; char clima[64]; char dia[64]; double precio; } Row;
    Row *rows = (Row*)calloc((size_t)cap, sizeof(Row));
    if (!rows) return 0;

    /* Consumir posibles pulsaciones previas que afectarían al modal */
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

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
        if (count >= cap)
        {
            int new_cap = cap * 2;
            Row *tmp = (Row*)realloc(rows, (size_t)new_cap * sizeof(Row));
            if (!tmp) { sqlite3_finalize(stmt); free(rows); return 0; }
            memset(tmp + cap, 0, (size_t)(new_cap - cap) * sizeof(Row));
            rows = tmp; cap = new_cap;
        }
        rows[count].id = sqlite3_column_int(stmt, 0);

        /* Fecha formateada */
        {
            char fecha_buf[64] = {0};
            const unsigned char *col_fecha = sqlite3_column_text(stmt, 2);
            if (col_fecha)
                format_date_for_display((const char*)col_fecha, fecha_buf, sizeof(fecha_buf));
            else
                snprintf(fecha_buf, sizeof(fecha_buf), "(sin fecha)");
            strncpy_s(rows[count].fecha, sizeof(rows[count].fecha), fecha_buf, _TRUNCATE);
        }

        /* Cancha y camiseta (posible Latin1) */
        {
            const unsigned char *col_cancha = sqlite3_column_text(stmt, 1);
            if (col_cancha)
                latin1_to_utf8(col_cancha, rows[count].cancha, sizeof(rows[count].cancha));
            else
                snprintf(rows[count].cancha, sizeof(rows[count].cancha), "(sin cancha)");

            const unsigned char *col_cami = sqlite3_column_text(stmt, 5);
            if (col_cami)
                latin1_to_utf8(col_cami, rows[count].camiseta, sizeof(rows[count].camiseta));
            else
                snprintf(rows[count].camiseta, sizeof(rows[count].camiseta), "(sin camiseta)");

            int goles = sqlite3_column_int(stmt, 3);
            int asist = sqlite3_column_int(stmt, 4);
            snprintf(rows[count].marcador, sizeof(rows[count].marcador), "G:%d A:%d", goles, asist);
        }

        count++;
    }
    sqlite3_finalize(stmt);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        panel_x = sw > 900 ? 80 : 20;
        panel_w = sw - panel_x * 2;
        panel_h = sh - 220;
        if (panel_h < 200) panel_h = 200;

        int visible_rows = panel_h / row_h;
        if (visible_rows < 1) visible_rows = 1;

        float wheel = GetMouseWheelMove();
        Rectangle area = (Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h};
        if (CheckCollisionPointRec(GetMousePosition(), area))
        {
            if (wheel < 0.0f && scroll < count - visible_rows) scroll++;
            else if (wheel > 0.0f && scroll > 0) scroll--;
        }

        if (scroll < 0) scroll = 0;
        if (scroll > count - visible_rows) scroll = count - visible_rows;

        BeginDrawing();
        ClearBackground((Color){14,27,20,255});
        DrawRectangle(0, 0, sw, 84, (Color){17,54,33,255});
        gui_text("MiFutbolC", 26, 20, 36.0f, (Color){241,252,244,255});
        gui_text("LISTADO DE PARTIDOS", 230, 34, 20.0f, (Color){198,230,205,255});

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){19,40,27,255});
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){110,161,125,255});

        if (count == 0)
        {
            DrawText("No hay partidos cargados.", panel_x + 24, panel_y + 24, 24, (Color){233,247,236,255});
        }
        else
        {
            BeginScissorMode(panel_x, panel_y, panel_w, panel_h);
            for (int i = scroll; i < count; i++)
            {
                int row = i - scroll;
                int y = panel_y + row * row_h;
                if (y + row_h > panel_y + panel_h) break;
                gui_text(TextFormat("%3d", rows[i].id), panel_x + 10, y + 7, 18.0f, (Color){220,238,225,255});
                gui_text(rows[i].fecha, panel_x + 70, y + 7, 18.0f, (Color){220,238,225,255});
                gui_text(rows[i].cancha, panel_x + 270, y + 7, 18.0f, (Color){233,247,236,255});
                gui_text(rows[i].marcador, panel_x + panel_w - 120, y + 7, 18.0f, (Color){233,247,236,255});
            }
            EndScissorMode();
        }

        gui_text("Rueda: scroll | ESC/Enter: volver", panel_x, sh - 62, 18.0f, (Color){178,214,188,255});
        EndDrawing();

        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) {
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            break;
        }
    }

    free(rows);
    return 1;
}
#endif
void listar_partidos()
{
#ifdef ENABLE_RAYLIB_GUI
    if (menu_is_gui_enabled())
    {
        if (listar_partidos_gui())
            return;
    }
#endif
    clear_screen();
    print_header("LISTADO DE PARTIDOS");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, rendimiento_general, cansancio, estado_animo, comentario_personal, clima, dia, precio "
                "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
                "JOIN cancha can ON p.cancha_id = can.id ORDER BY p.id ASC",
                &stmt))
    {
        pause_console();
        return;
    }


    int hay = mostrar_partidos_desde_stmt(stmt);

    if (!hay)
        ui_printf_centered_line("No hay partidos cargados.");

    sqlite3_finalize(stmt);
    pause_console();
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

    ejecutar_menu("MODIFICAR PARTIDO", items, 12);
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

    ejecutar_menu("BUSQUEDA DE PARTIDOS", items, 5);
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
    printf("%s\n", ASCII_SIMULACION);
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

/**
 * @brief Simula un partido entre dos equipos guardados en la base de datos
 *
 * Permite al usuario seleccionar dos equipos existentes de la base de datos
 * y simular un partido entre ellos sin guardar resultados en Partidos.
 */
void simular_partido_guardados()
{
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

static void tactica_ver_diagrama()
{
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

void menu_tacticas_partido()
{
    MenuItem items[] =
    {
        {1, "Crear diagrama", tactica_crear_diagrama},
        {2, "Ver diagramas", tactica_ver_diagrama},
        {0, "Volver", NULL}
    };

    clear_screen();
    print_header("ANALISIS TACTICO");
    ejecutar_menu("ANALISIS TACTICO", items, 3);
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

    ejecutar_menu("PARTIDOS", items, 7);
}
