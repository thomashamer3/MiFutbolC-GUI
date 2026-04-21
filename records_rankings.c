/**
 * @file records_rankings.c
 * @brief Implementacion de records y rankings en MiFutbolC
 */

#include "records_rankings.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef UNIT_TEST
#include "estadisticas_gui_capture.h"
#endif

#define ARRAY_COUNT(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))
#define RECORDS_ITEM(numero, texto, accion) {(numero), (texto), (accion), MENU_CATEGORY_ANALISIS}
#define RECORDS_BACK_ITEM {0, "Volver", NULL, MENU_CATEGORY_ADMIN}

static void mostrar_partido_rendimiento(const char *titulo, const char *order_clause);
static void mostrar_record_simple_gui(const char *titulo, const char *sql);
static void mostrar_no_hay_registros_gui(const char *entidad);

static void mostrar_top5_mejores_partidos(void);
static void mostrar_ranking_camisetas(void);

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, NULL) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

static void mostrar_no_hay_registros_gui(const char *entidad)
{
    if (!entidad || entidad[0] == '\0')
    {
        printf("No hay registros disponibles.\n");
        return;
    }

    size_t len = safe_strnlen(entidad, SIZE_MAX);
    if (len == 0)
    {
        printf("No hay registros disponibles.\n");
        return;
    }

    printf("No hay %s registrad%s.\n", entidad,
           (entidad[len - 1] == 'a' || entidad[len - 1] == 'o') ? "o" : "os");
}

static void mostrar_record_simple_gui(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt;

    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Valor: %d\n", sqlite3_column_int(stmt, 0));
        if (sqlite3_column_count(stmt) > 1)
        {
            printf("Camiseta: %s\n", sqlite3_column_text(stmt, 1));
        }
        if (sqlite3_column_count(stmt) > 2)
        {
            printf("Fecha: %s\n", sqlite3_column_text(stmt, 2));
        }
    }
    else
    {
        mostrar_no_hay_registros_gui("datos disponibles");
    }

    sqlite3_finalize(stmt);
}

/**
 * Muestra resultados de combinaciones cancha-camiseta.
 * Necesario para mantener formato consistente en consultas complejas de rendimiento.
 */
static void mostrar_combinacion(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            printf("Cancha: %s\n", sqlite3_column_text(stmt, 0));
            printf("Camiseta: %s\n", sqlite3_column_text(stmt, 1));
            printf("Rendimiento Promedio: %.2f\n", sqlite3_column_double(stmt, 2));
            printf("Partidos Jugados: %d\n", sqlite3_column_int(stmt, 3));
        }
        else
        {
            mostrar_no_hay_registros_gui("datos disponibles");
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Funcion auxiliar para mostrar temporadas
 */
static void mostrar_temporada(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char* year = (const char*)sqlite3_column_text(stmt, 0);
            if (year)
            {
                printf("Anio: %s\n", year);
            }
            else
            {
                printf("Anio: Desconocido\n");
            }
            printf("Rendimiento Promedio: %.2f\n", sqlite3_column_double(stmt, 1));
            printf("Partidos Jugados: %d\n", sqlite3_column_int(stmt, 2));
        }
        else
        {
            mostrar_no_hay_registros_gui("datos disponibles");
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Muestra el record de goles en un partido
 */
void mostrar_record_goles_partido()
{
    clear_screen();
    print_header("RECORD DE GOLES EN UN PARTIDO");

    mostrar_record_simple_gui("Record de Goles en un Partido",
                              "SELECT p.goles, c.nombre, p.fecha_hora "
                              "FROM partido p "
                              "JOIN camiseta c ON p.camiseta_id = c.id "
                              "ORDER BY p.goles DESC LIMIT 1");

    pause_console();
}

/**
 * @brief Muestra el record de asistencias en un partido
 */
void mostrar_record_asistencias_partido()
{
    clear_screen();
    print_header("RECORD DE ASISTENCIAS EN UN PARTIDO");

    mostrar_record_simple_gui("Record de Asistencias en un Partido",
                              "SELECT p.asistencias, c.nombre, p.fecha_hora "
                              "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
                              "ORDER BY p.asistencias DESC, p.fecha_hora DESC LIMIT 1");

    pause_console();
}

/**
 * @brief Muestra la mejor combinacion cancha + camiseta
 */
void mostrar_mejor_combinacion_cancha_camiseta()
{
    clear_screen();
    print_header("MEJOR COMBINACION CANCHA + CAMISETA");

    mostrar_combinacion("Mejor Combinacion Cancha + Camiseta",
                        "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) "
                        "FROM partido p "
                        "JOIN cancha ca ON p.cancha_id = ca.id "
                        "JOIN camiseta c ON p.camiseta_id = c.id "
                        "GROUP BY p.cancha_id, p.camiseta_id "
                        "ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1");

    pause_console();
}

/**
 * @brief Muestra la peor combinacion cancha + camiseta
 */
void mostrar_peor_combinacion_cancha_camiseta()
{
    clear_screen();
    print_header("PEOR COMBINACION CANCHA + CAMISETA");

    mostrar_combinacion("Peor Combinacion Cancha + Camiseta",
                        "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) "
                        "FROM partido p "
                        "JOIN cancha ca ON p.cancha_id = ca.id "
                        "JOIN camiseta c ON p.camiseta_id = c.id "
                        "GROUP BY p.cancha_id, p.camiseta_id "
                        "ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1");

    pause_console();
}

/**
 * @brief Muestra la mejor temporada
 */
void mostrar_mejor_temporada()
{
    clear_screen();
    print_header("MEJOR TEMPORADA");

    mostrar_temporada("Mejor Temporada",
                      "SELECT substr(p.fecha_hora, instr(p.fecha_hora, '/') + 4, 4), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p WHERE p.fecha_hora IS NOT NULL GROUP BY substr(p.fecha_hora, instr(p.fecha_hora, '/') + 4, 4) ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1");

    pause_console();
}

/**
 * @brief Muestra la peor temporada
 */
void mostrar_peor_temporada()
{
    clear_screen();
    print_header("PEOR TEMPORADA");

    mostrar_temporada("Peor Temporada",
                      "SELECT substr(p.fecha_hora, instr(p.fecha_hora, '/') + 4, 4), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p WHERE p.fecha_hora IS NOT NULL GROUP BY substr(p.fecha_hora, instr(p.fecha_hora, '/') + 4, 4) ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1");

    pause_console();
}

/**
 * @brief Muestra el partido con mejor rendimiento_general
 */
void mostrar_partido_mejor_rendimiento_general()
{
    clear_screen();
    print_header("PARTIDO CON MEJOR RENDIMIENTO GENERAL");

    mostrar_partido_rendimiento("Partido con Mejor Rendimiento General",
                                "ORDER BY p.rendimiento_general DESC LIMIT 1");
}

/**
 * @brief Muestra el partido con peor rendimiento_general
 */
void mostrar_partido_peor_rendimiento_general()
{
    clear_screen();
    print_header("PARTIDO CON PEOR RENDIMIENTO GENERAL");

    mostrar_partido_rendimiento("Partido con Peor Rendimiento General",
                                "ORDER BY p.rendimiento_general ASC LIMIT 1");
}

static void mostrar_partido_rendimiento(const char *titulo, const char *order_clause)
{
    sqlite3_stmt *stmt;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT p.id, p.fecha_hora, c.nombre, p.rendimiento_general "
             "FROM partido p "
             "JOIN camiseta c ON p.camiseta_id = c.id "
             "%s",
             order_clause);

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            printf("ID: %d\n", sqlite3_column_int(stmt, 0));
            printf("Fecha: %s\n", sqlite3_column_text(stmt, 1));
            printf("Camiseta: %s\n", sqlite3_column_text(stmt, 2));
            printf("Rendimiento General: %.2f\n", sqlite3_column_double(stmt, 3));
        }
        else
        {
            mostrar_no_hay_registros_gui("datos disponibles");
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

/**
 * @brief Muestra el partido con mejor combinacion (goles + asistencias)
 */
void mostrar_partido_mejor_combinacion_goles_asistencias()
{
    clear_screen();
    print_header("PARTIDO CON MEJOR COMBINACION GOLES+ASISTENCIAS");

    sqlite3_stmt *stmt;

    printf("\nPartido con Mejor Combinacion Goles+Asistencias\n");
    printf("----------------------------------------\n");

    if (preparar_stmt(
                "SELECT p.id, p.fecha_hora, c.nombre, p.goles, p.asistencias, (p.goles + p.asistencias) AS combinacion "
                "FROM partido p "
                "JOIN camiseta c ON p.camiseta_id = c.id "
                "ORDER BY combinacion DESC LIMIT 1",
                &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            printf("ID: %d\n", sqlite3_column_int(stmt, 0));
            printf("Fecha: %s\n", sqlite3_column_text(stmt, 1));
            printf("Camiseta: %s\n", sqlite3_column_text(stmt, 2));
            printf("Goles: %d\n", sqlite3_column_int(stmt, 3));
            printf("Asistencias: %d\n", sqlite3_column_int(stmt, 4));
            printf("Combinacion: %d\n", sqlite3_column_int(stmt, 5));
        }
        else
        {
            mostrar_no_hay_registros_gui("datos disponibles");
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

/**
 * Muestra lista de partidos que cumplen una condicion especifica.
 * Reutilizable para diferentes criterios de filtrado de partidos.
 */
static void mostrar_lista_partidos(const char *header, const char *titulo, const char *condicion, const char *mensaje_vacio)
{
    clear_screen();
    print_header(header);

    sqlite3_stmt *stmt;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT p.id, p.fecha_hora, c.nombre, p.goles, p.asistencias "
             "FROM partido p "
             "JOIN camiseta c ON p.camiseta_id = c.id "
             "WHERE %s "
             "ORDER BY p.fecha_hora DESC",
             condicion);

    if (preparar_stmt(sql, &stmt))
    {
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            printf("ID: %d | Fecha: %s | Camiseta: %s | Goles: %d | Asistencias: %d\n",
                   sqlite3_column_int(stmt, 0),
                   sqlite3_column_text(stmt, 1),
                   sqlite3_column_text(stmt, 2),
                   sqlite3_column_int(stmt, 3),
                   sqlite3_column_int(stmt, 4));
            count++;
        }

        if (count == 0)
        {
            printf("%s\n", mensaje_vacio);
        }
        else
        {
            printf("\nTotal: %d partidos\n", count);
        }

        sqlite3_finalize(stmt);
    }

    pause_console();
}

/**
 * @brief Muestra los partidos sin goles
 */
void mostrar_partidos_sin_goles()
{
    mostrar_lista_partidos("PARTIDOS SIN GOLES", "Partidos sin Goles",
                           "p.goles = 0", "No hay partidos sin goles.");
}

/**
 * @brief Muestra los partidos sin asistencias
 */
void mostrar_partidos_sin_asistencias()
{
    mostrar_lista_partidos("PARTIDOS SIN ASISTENCIAS", "Partidos sin Asistencias",
                           "p.asistencias = 0", "No hay partidos sin asistencias.");
}

/**
 * Estructura para almacenar informacion de racha.
 * Necesaria para devolver multiples valores de la funcion de calculo de rachas.
 */
typedef struct
{
    int mejor_racha;
    int inicio_racha;
    int fin_racha;
} RachaInfo;

/**
 * Calcula la mejor racha consecutiva basada en condicion especifica.
 * Separado del display para permitir reutilizacion y testing independiente.
 */
static RachaInfo calcular_mejor_racha(int tipo_racha)
{
    sqlite3_stmt *stmt;
    RachaInfo resultado = {0, -1, -1};
    int racha_actual = 0;
    int temp_inicio = -1;

    if (!preparar_stmt("SELECT id, goles FROM partido ORDER BY fecha_hora ASC", &stmt))
    {
        return resultado;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        int goles = sqlite3_column_int(stmt, 1);
        int condicion = (tipo_racha == 1) ? (goles > 0) : (goles == 0);

        if (condicion)
        {
            if (racha_actual == 0)
            {
                temp_inicio = id;
            }
            racha_actual++;
            if (racha_actual > resultado.mejor_racha)
            {
                resultado.mejor_racha = racha_actual;
                resultado.inicio_racha = temp_inicio;
                resultado.fin_racha = id;
            }
        }
        else
        {
            racha_actual = 0;
        }
    }

    sqlite3_finalize(stmt);
    return resultado;
}

/**
 * Muestra informacion de racha en pantalla.
 * Separado del calculo para claridad de responsabilidades.
 */
static void mostrar_racha_info(const char *titulo, RachaInfo racha)
{
    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (racha.mejor_racha > 0)
    {
        printf("Mejor Racha: %d partidos\n", racha.mejor_racha);
        printf("Desde partido ID %d hasta ID %d\n", racha.inicio_racha, racha.fin_racha);
    }
    else
    {
        mostrar_no_hay_registros_gui("rachas disponibles");
    }
}

/**
 * Funcion principal para mostrar rachas.
 * Coordina calculo y display de rachas consecutivas.
 */
static void mostrar_racha(const char *titulo, int tipo_racha)
{
    RachaInfo racha = calcular_mejor_racha(tipo_racha);
    mostrar_racha_info(titulo, racha);
}

/**
 * @brief Muestra la mejor racha goleadora
 */
void mostrar_mejor_racha_goleadora()
{
    clear_screen();
    print_header("MEJOR RACHA GOLEADORA");

    mostrar_racha("Mejor Racha Goleadora (partidos consecutivos con goles)", 1);

    pause_console();
}

/**
 * @brief Muestra la peor racha
 */
void mostrar_peor_racha()
{
    clear_screen();
    print_header("PEOR RACHA");

    mostrar_racha("Peor Racha (partidos consecutivos sin goles)", 0);

    pause_console();
}

/**
 * @brief Muestra los partidos consecutivos anotando
 */
void mostrar_partidos_consecutivos_anotando()
{
    clear_screen();
    print_header("PARTIDOS CONSECUTIVOS ANOTANDO");

    mostrar_racha("Partidos Consecutivos Anotando", 1);

    pause_console();
}

static void mostrar_top5_mejores_partidos(void)
{
    clear_screen();
    print_header("TOP 5 MEJORES PARTIDOS");

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT fecha_hora, goles, asistencias, rendimiento_general "
        "FROM partido "
        "ORDER BY rendimiento_general DESC, (goles + asistencias) DESC, fecha_hora DESC "
        "LIMIT 5";

    printf("\nTop 5 mejores partidos\n");
    printf("----------------------------------------\n");

    if (!preparar_stmt(sql, &stmt))
    {
        pause_console();
        return;
    }

    int pos = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha = (const char *)sqlite3_column_text(stmt, 0);
        int goles = sqlite3_column_int(stmt, 1);
        int asistencias = sqlite3_column_int(stmt, 2);
        int rendimiento = sqlite3_column_int(stmt, 3);

        printf("%d. %s    %d goles - %d asistencias    rendimiento %d\n",
               pos,
               fecha ? fecha : "N/A",
               goles,
               asistencias,
               rendimiento);
        pos++;
    }

    if (pos == 1)
    {
        mostrar_no_hay_registros_gui("partidos disponibles");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void mostrar_ranking_camisetas(void)
{
    clear_screen();
    print_header("RANKING DE TUS CAMISETAS");

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT "
        "  c.nombre, "
        "  COUNT(p.id) AS partidos, "
        "  SUM(CASE WHEN p.resultado = 1 THEN 1 ELSE 0 END) AS victorias, "
        "  SUM(CASE WHEN p.resultado = 2 THEN 1 ELSE 0 END) AS empates, "
        "  SUM(CASE WHEN p.resultado = 3 THEN 1 ELSE 0 END) AS derrotas, "
        "  CASE WHEN COUNT(p.id) > 0 "
        "       THEN (SUM(CASE WHEN p.resultado = 1 THEN 1 ELSE 0 END) * 100.0) / COUNT(p.id) "
        "       ELSE 0 END AS winrate "
        "FROM camiseta c "
        "LEFT JOIN partido p ON p.camiseta_id = c.id "
        "GROUP BY c.id, c.nombre "
        "HAVING COUNT(p.id) > 0 "
        "ORDER BY winrate DESC, victorias DESC, partidos DESC";

    printf("\nRanking de tus camisetas\n");
    printf("----------------------------------------\n");

    if (!preparar_stmt(sql, &stmt))
    {
        pause_console();
        return;
    }

    int pos = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
        int partidos = sqlite3_column_int(stmt, 1);
        int victorias = sqlite3_column_int(stmt, 2);
        int empates = sqlite3_column_int(stmt, 3);
        int derrotas = sqlite3_column_int(stmt, 4);
        double winrate = sqlite3_column_double(stmt, 5);

        printf("%d) %s\n", pos, nombre ? nombre : "Camiseta sin nombre");
        printf("   Partidos: %d  Victorias: %d  Empates: %d  Derrotas: %d\n",
               partidos, victorias, empates, derrotas);
        printf("   Winrate: %.0f%%\n\n", winrate);
        pos++;
    }

    if (pos == 1)
    {
        mostrar_no_hay_registros_gui("camisetas con partidos");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * Construye array de opciones del menu de records y rankings.
 * Centralizado aqui para mantener consistencia y facilitar mantenimiento.
 */
static const MenuItem *construir_menu_records(int *cantidad)
{
    static const MenuItem items[] =
    {
        RECORDS_ITEM(1, "Record de Goles en un Partido", mostrar_record_goles_partido),
        RECORDS_ITEM(2, "Record de Asistencias", mostrar_record_asistencias_partido),
        RECORDS_ITEM(3, "Mejor Combinacion Cancha + Camiseta", mostrar_mejor_combinacion_cancha_camiseta),
        RECORDS_ITEM(4, "Peor Combinacion Cancha + Camiseta", mostrar_peor_combinacion_cancha_camiseta),
        RECORDS_ITEM(5, "Mejor Temporada", mostrar_mejor_temporada),
        RECORDS_ITEM(6, "Peor Temporada", mostrar_peor_temporada),
        RECORDS_ITEM(7, "Partido con Mejor Rendimiento General", mostrar_partido_mejor_rendimiento_general),
        RECORDS_ITEM(8, "Partido con Peor Rendimiento General", mostrar_partido_peor_rendimiento_general),
        RECORDS_ITEM(9, "Partido con Mejor Combinacion Goles+Asistencias", mostrar_partido_mejor_combinacion_goles_asistencias),
        RECORDS_ITEM(10, "Partidos sin Goles", mostrar_partidos_sin_goles),
        RECORDS_ITEM(11, "Partidos sin Asistencias", mostrar_partidos_sin_asistencias),
        RECORDS_ITEM(12, "Mejor Racha Goleadora", mostrar_mejor_racha_goleadora),
        RECORDS_ITEM(13, "Peor Racha", mostrar_peor_racha),
        RECORDS_ITEM(14, "Partidos Consecutivos Anotando", mostrar_partidos_consecutivos_anotando),
        RECORDS_ITEM(15, "Top 5 Mejores Partidos", mostrar_top5_mejores_partidos),
        RECORDS_ITEM(16, "Ranking de Tus Camisetas", mostrar_ranking_camisetas),
        RECORDS_BACK_ITEM
    };

    if (cantidad)
    {
        *cantidad = ARRAY_COUNT(items);
    }

    return items;
}

/**
 * @brief Muestra el menu de records y rankings
 */
void menu_records_rankings()
{
    int cantidad = 0;
    const MenuItem *items = construir_menu_records(&cantidad);
    ejecutar_menu_estandar("RECORDS & RANKINGS", items, cantidad);
}
