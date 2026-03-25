/**
 * @file carrera.c
 * @brief Modulo de Carrera Futbolistica del usuario
 *
 * Implementa un resumen integral de toda la trayectoria deportiva,
 * con tres secciones: carrera (resultados globales), historia
 * (cronologia y mejor anio) y resumen general (promedios por anio).
 */

#include "carrera.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ======== helpers internos ======== */

#define SQL_EXPR_ANIO_FECHA_SEGURO \
    "CASE " \
    "  WHEN fecha_hora LIKE '____-__-__%' THEN CAST(substr(fecha_hora, 1, 4) AS INTEGER) " \
    "  WHEN fecha_hora LIKE '__/__/____%' THEN CAST(substr(fecha_hora, 7, 4) AS INTEGER) " \
    "  ELSE NULL " \
    "END"

#define SQL_EXPR_ANIO_FECHA_FLEX \
    "CASE " \
    "  WHEN fecha_hora LIKE '____-__-__%' THEN CAST(substr(fecha_hora, 1, 4) AS INTEGER) " \
    "  ELSE CAST(substr(fecha_hora, 7, 4) AS INTEGER) " \
    "END"

static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

static int preparar_stmt_msg(sqlite3_stmt **stmt, const char *sql)
{
    if (preparar_stmt(stmt, sql))
        return 1;
    printf("Error al consultar la base de datos.\n");
    return 0;
}

static void imprimir_titulo_carrera_usuario(void)
{
    char *nombre_usuario = get_user_name();

    if (nombre_usuario && nombre_usuario[0] != '\0')
        printf("\n  Carrera Futbolistica de %s\n", nombre_usuario);
    else
        printf("\n  Carrera Futbolistica del Usuario\n");

    free(nombre_usuario);
}

static int iniciar_vista_carrera(const char *titulo)
{
    clear_screen();
    print_header(titulo);

    if (!hay_registros("partido"))
    {
        mostrar_no_hay_registros("partidos");
        pause_console();
        return 0;
    }

    return 1;
}

typedef struct
{
    int tipo; /* 1=victorias, 2=invicto, 3=derrotas */
    int victorias;
    int derrotas;
    int invicto;
} RachaActual;

static void actualizar_racha_actual(RachaActual *racha, int resultado, int *detener)
{
    if (racha->tipo == 0)
    {
        if (resultado == 1)
            racha->tipo = 1;
        else if (resultado == 3)
            racha->tipo = 3;
        else
            racha->tipo = 2;
    }

    if (racha->tipo == 1)
    {
        if (resultado == 1)
            racha->victorias++;
        else
            *detener = 1;
        return;
    }

    if (racha->tipo == 3)
    {
        if (resultado == 3)
            racha->derrotas++;
        else
            *detener = 1;
        return;
    }

    if (resultado != 3)
        racha->invicto++;
    else
        *detener = 1;
}

static int obtener_racha_actual(RachaActual *racha)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT resultado FROM partido ORDER BY fecha_hora DESC";

    racha->tipo = 0;
    racha->victorias = 0;
    racha->derrotas = 0;
    racha->invicto = 0;

    if (!preparar_stmt(&stmt, sql))
        return 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int detener = 0;
        int resultado = sqlite3_column_int(stmt, 0);
        actualizar_racha_actual(racha, resultado, &detener);
        if (detener)
            break;
    }

    sqlite3_finalize(stmt);
    return 1;
}

static void imprimir_racha_actual(const RachaActual *racha)
{
    printf("\n  ---- Racha actual ----\n");

    if (racha->tipo == 1 && racha->victorias > 0)
        printf("  Victorias consecutivas : %d\n", racha->victorias);
    else if (racha->tipo == 3 && racha->derrotas > 0)
        printf("  Derrotas consecutivas  : %d\n", racha->derrotas);
    else if (racha->tipo == 2 && racha->invicto > 0)
        printf("  Partidos invicto       : %d\n", racha->invicto);
}

/* ========================================================
 * 1. CARRERA FUTBOLISTICA
 * Partidos, Victorias, Empates, Derrotas, Goles, Asistencias,
 * Promedio goles/partido
 * ======================================================== */
static void mostrar_carrera_futbolistica(void)
{
    if (!iniciar_vista_carrera("CARRERA FUTBOLISTICA"))
        return;

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT "
        "  COUNT(*) AS partidos, "
        "  SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END) AS victorias, "
        "  SUM(CASE WHEN resultado = 2 THEN 1 ELSE 0 END) AS empates, "
        "  SUM(CASE WHEN resultado = 3 THEN 1 ELSE 0 END) AS derrotas, "
        "  COALESCE(SUM(goles), 0) AS goles, "
        "  COALESCE(SUM(asistencias), 0) AS asistencias "
        "FROM partido";

    if (!preparar_stmt_msg(&stmt, sql))
    {
        pause_console();
        return;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        printf("No hay datos disponibles.\n");
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }

    int partidos     = sqlite3_column_int(stmt, 0);
    int victorias    = sqlite3_column_int(stmt, 1);
    int empates      = sqlite3_column_int(stmt, 2);
    int derrotas     = sqlite3_column_int(stmt, 3);
    int goles        = sqlite3_column_int(stmt, 4);
    int asistencias  = sqlite3_column_int(stmt, 5);
    double prom_goles = (partidos > 0) ? (double)goles / partidos : 0.0;

    sqlite3_finalize(stmt);

    imprimir_titulo_carrera_usuario();
    printf("  ========================================\n\n");
    printf("  Partidos jugados : %d\n", partidos);
    printf("  Victorias        : %d\n", victorias);
    printf("  Empates          : %d\n", empates);
    printf("  Derrotas         : %d\n\n", derrotas);
    printf("  Goles            : %d\n", goles);
    printf("  Asistencias      : %d\n\n", asistencias);
    printf("  Promedio goles/partido : %.2f\n", prom_goles);

    /* Porcentajes de resultados */
    if (partidos > 0)
    {
        printf("\n  ---- Distribucion de resultados ----\n");
        printf("  Victorias : %5.1f%%\n", (double)victorias / partidos * 100.0);
        printf("  Empates   : %5.1f%%\n", (double)empates   / partidos * 100.0);
        printf("  Derrotas  : %5.1f%%\n", (double)derrotas  / partidos * 100.0);
    }

    pause_console();
}

/* ========================================================
 * 2. TU HISTORIA FUTBOLISTICA
 * Primer partido (anio), Partidos, Goles, Asistencias,
 * Mejor anio con sus goles
 * ======================================================== */
static void mostrar_historia_futbolistica(void)
{
    if (!iniciar_vista_carrera("TU HISTORIA FUTBOLISTICA"))
        return;

    /* --- Datos globales y primer partido --- */
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT "
        "  COUNT(*), "
        "  COALESCE(SUM(goles), 0), "
        "  COALESCE(SUM(asistencias), 0), "
        "  MIN(" SQL_EXPR_ANIO_FECHA_SEGURO "), "
        "  MIN(fecha_hora) "
        "FROM partido";

    if (!preparar_stmt_msg(&stmt, sql))
    {
        pause_console();
        return;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        printf("No hay datos disponibles.\n");
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }

    int partidos    = sqlite3_column_int(stmt, 0);
    int goles       = sqlite3_column_int(stmt, 1);
    int asistencias = sqlite3_column_int(stmt, 2);
    int anio_inicio = sqlite3_column_int(stmt, 3);
    const char *primera_fecha = (const char *)sqlite3_column_text(stmt, 4);

    sqlite3_finalize(stmt);

    /* --- Mejor anio (mas goles) --- */
    sqlite3_stmt *stmt2;
    const char *sql_mejor =
        "SELECT "
        "  " SQL_EXPR_ANIO_FECHA_FLEX " AS anio, "
        "  SUM(goles) AS total_goles "
        "FROM partido "
        "GROUP BY anio "
        "ORDER BY total_goles DESC "
        "LIMIT 1";

    int mejor_anio = 0;
    int goles_mejor_anio = 0;

    if (preparar_stmt(&stmt2, sql_mejor))
    {
        if (sqlite3_step(stmt2) == SQLITE_ROW)
        {
            mejor_anio = sqlite3_column_int(stmt2, 0);
            goles_mejor_anio = sqlite3_column_int(stmt2, 1);
        }
        sqlite3_finalize(stmt2);
    }

    printf("\n  Tu historia futbolistica\n");
    printf("  ========================================\n\n");

    if (anio_inicio > 0)
        printf("  Primer partido   : %d\n", anio_inicio);
    else
        printf("  Primer partido   : %s\n", primera_fecha ? primera_fecha : "N/A");

    printf("  Partidos jugados : %d\n", partidos);
    printf("  Goles            : %d\n", goles);
    printf("  Asistencias      : %d\n\n", asistencias);

    if (mejor_anio > 0)
    {
        printf("  Tu mejor anio    : %d\n", mejor_anio);
        printf("    Goles          : %d\n", goles_mejor_anio);
    }

    /* --- Desglose por anio --- */
    sqlite3_stmt *stmt3;
    const char *sql_anios =
        "SELECT "
        "  " SQL_EXPR_ANIO_FECHA_FLEX " AS anio, "
        "  COUNT(*) AS partidos, "
        "  SUM(goles) AS goles, "
        "  SUM(asistencias) AS asistencias, "
        "  ROUND(AVG(rendimiento_general), 1) AS rend_prom "
        "FROM partido "
        "GROUP BY anio "
        "ORDER BY anio ASC";

    if (preparar_stmt(&stmt3, sql_anios))
    {
        printf("\n  ---- Desglose por anio ----\n");
        printf("  %-6s  %8s  %6s  %6s  %6s\n", "Anio", "Partidos", "Goles", "Asist", "Rend");
        printf("  %-6s  %8s  %6s  %6s  %6s\n", "------", "--------", "------", "------", "------");

        while (sqlite3_step(stmt3) == SQLITE_ROW)
        {
            int a = sqlite3_column_int(stmt3, 0);
            int p = sqlite3_column_int(stmt3, 1);
            int g = sqlite3_column_int(stmt3, 2);
            int as = sqlite3_column_int(stmt3, 3);
            double r = sqlite3_column_double(stmt3, 4);
            printf("  %-6d  %8d  %6d  %6d  %6.1f\n", a, p, g, as, r);
        }
        sqlite3_finalize(stmt3);
    }

    pause_console();
}

/* ========================================================
 * 3. RESUMEN GENERAL DE CARRERA
 * Inicio carrera, Anios jugando, Promedio goles/anio
 * ======================================================== */
static void mostrar_resumen_carrera(void)
{
    if (!iniciar_vista_carrera("RESUMEN GENERAL DE CARRERA"))
        return;

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT "
        "  MIN(" SQL_EXPR_ANIO_FECHA_FLEX ") AS inicio, "
        "  MAX(" SQL_EXPR_ANIO_FECHA_FLEX ") AS fin, "
        "  COUNT(*) AS partidos, "
        "  COALESCE(SUM(goles), 0) AS goles, "
        "  COALESCE(SUM(asistencias), 0) AS asistencias, "
        "  ROUND(AVG(rendimiento_general), 2) AS rend_prom "
        "FROM partido";

    if (!preparar_stmt_msg(&stmt, sql))
    {
        pause_console();
        return;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        printf("No hay datos disponibles.\n");
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }

    int anio_inicio  = sqlite3_column_int(stmt, 0);
    int anio_fin     = sqlite3_column_int(stmt, 1);
    int partidos     = sqlite3_column_int(stmt, 2);
    int goles        = sqlite3_column_int(stmt, 3);
    int asistencias  = sqlite3_column_int(stmt, 4);
    double rend_prom = sqlite3_column_double(stmt, 5);

    sqlite3_finalize(stmt);

    int anios_jugando = (anio_fin > anio_inicio) ? (anio_fin - anio_inicio + 1) : 1;
    double goles_por_anio = (double)goles / anios_jugando;
    double asist_por_anio = (double)asistencias / anios_jugando;
    double partidos_por_anio = (double)partidos / anios_jugando;

    printf("\n  Resumen General de Carrera\n");
    printf("  ========================================\n\n");
    printf("  Inicio carrera       : %d\n", anio_inicio);
    printf("  Anios jugando        : %d\n", anios_jugando);
    printf("  Total partidos       : %d\n\n", partidos);

    printf("  ---- Promedios por anio ----\n");
    printf("  Partidos/anio        : %.1f\n", partidos_por_anio);
    printf("  Goles/anio           : %.1f\n", goles_por_anio);
    printf("  Asistencias/anio     : %.1f\n\n", asist_por_anio);

    printf("  ---- Rendimiento global ----\n");
    printf("  Rendimiento promedio : %.2f\n", rend_prom);

    RachaActual racha;
    if (obtener_racha_actual(&racha))
        imprimir_racha_actual(&racha);

    pause_console();
}

/* ========================================================
 * MENU PRINCIPAL DE CARRERA FUTBOLISTICA
 * ======================================================== */
void menu_carrera_futbolistica(void)
{
    MenuItem items[] =
    {
        {1, "Carrera Futbolistica", &mostrar_carrera_futbolistica},
        {2, "Tu Historia Futbolistica", &mostrar_historia_futbolistica},
        {3, "Resumen General de Carrera", &mostrar_resumen_carrera},
        {0, "Volver", NULL}
    };

    ejecutar_menu("CARRERA FUTBOLISTICA", items, 4);
}
