/**
 * @file logros.c
 * @brief Sistema de logros y badges para MiFutbolC
 *
 * Este archivo implementa el sistema de logros basado en estadisticas
 * conseguidas por las camisetas en partidos de futbol.
 */

#include "logros.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UNIT_TEST
#include "estadisticas_gui_capture.h"
#endif

#define ARRAY_COUNT(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))
#define LOGROS_ITEM(numero, texto, accion) {(numero), (texto), (accion), MENU_CATEGORY_ANALISIS}
#define LOGROS_BACK_ITEM {0, "Volver", NULL, MENU_CATEGORY_ADMIN}


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

static void mostrar_no_hay_registros_logros(const char *entidad)
{
    if (!entidad || entidad[0] == '\0')
    {
        printf("No hay registros disponibles.\n");
        return;
    }

    printf("No hay %s registrados.\n", entidad);
}

/**
 * @struct Logro
 * @brief Estructura que representa un logro/badge
 */
typedef struct
{
    const char *nombre;
    const char *descripcion;
    int objetivo;
    const char *tipo; // tipos disponibles: "goles", "asistencias", "partidos", "goles+asistencias", "victorias", "empates", "derrotas", "rendimiento_general", "estado_animo", "canchas_distintas", "hat_tricks", "poker_asistencias", "rendimiento_perfecto", "animo_perfecto", "goles_victorias", "asistencias_victorias", "rendimiento_victorias", "animo_victorias", "goles_derrotas", "asistencias_derrotas", "rendimiento_empates", "animo_empates", y muchos mas (ver LOGRO_QUERIES)
} Logro;

/**
 * @brief Estructura para mapear tipos de logros con sus consultas SQL
 */
typedef struct
{
    const char *tipo;
    const char *sql;
} LogroQuery;

typedef struct
{
    int id;
    char nombre[128];
} CamisetaRow;

typedef struct
{
    const Logro *logro;
    int estado;
    int progreso;
} LogroVistaRow;

/**
 * @brief Array de consultas SQL predefinidas para cada tipo de logro
 */
static const LogroQuery LOGRO_QUERIES[] =
{
    {"goles", "SELECT IFNULL(SUM(goles), 0) FROM partido WHERE camiseta_id = ?"},
    {"asistencias", "SELECT IFNULL(SUM(asistencias), 0) FROM partido WHERE camiseta_id = ?"},
    {"partidos", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ?"},
    {"goles+asistencias", "SELECT IFNULL(SUM(goles + asistencias), 0) FROM partido WHERE camiseta_id = ?"},
    {"victorias", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND resultado = 1"},
    {"empates", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND resultado = 2"},
    {"derrotas", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND resultado = 3"},
    {"rendimiento_general", "SELECT IFNULL(SUM(rendimiento_general), 0) FROM partido WHERE camiseta_id = ?"},
    {"estado_animo", "SELECT IFNULL(SUM(estado_animo), 0) FROM partido WHERE camiseta_id = ?"},
    {"canchas_distintas", "SELECT COUNT(DISTINCT cancha_id) FROM partido WHERE camiseta_id = ?"},
    {"hat_tricks", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND goles >= 3"},
    {"poker_asistencias", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND asistencias >= 4"},
    {"rendimiento_perfecto", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND rendimiento_general = 10"},
    {"animo_perfecto", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND estado_animo = 10"},
    {"goles_victorias", "SELECT IFNULL(SUM(goles), 0) FROM partido WHERE camiseta_id = ? AND resultado = 1"},
    {"asistencias_victorias", "SELECT IFNULL(SUM(asistencias), 0) FROM partido WHERE camiseta_id = ? AND resultado = 1"},
    {"rendimiento_victorias", "SELECT IFNULL(SUM(rendimiento_general), 0) FROM partido WHERE camiseta_id = ? AND resultado = 1"},
    {"animo_victorias", "SELECT IFNULL(SUM(estado_animo), 0) FROM partido WHERE camiseta_id = ? AND resultado = 1"},
    {"goles_derrotas", "SELECT IFNULL(SUM(goles), 0) FROM partido WHERE camiseta_id = ? AND resultado = 3"},
    {"asistencias_derrotas", "SELECT IFNULL(SUM(asistencias), 0) FROM partido WHERE camiseta_id = ? AND resultado = 3"},
    {"rendimiento_empates", "SELECT IFNULL(SUM(rendimiento_general), 0) FROM partido WHERE camiseta_id = ? AND resultado = 2"},
    {"animo_empates", "SELECT IFNULL(SUM(estado_animo), 0) FROM partido WHERE camiseta_id = ? AND resultado = 2"},
    // Nuevos tipos de logros
    {"goles_empates", "SELECT IFNULL(SUM(goles), 0) FROM partido WHERE camiseta_id = ? AND resultado = 2"},
    {"asistencias_empates", "SELECT IFNULL(SUM(asistencias), 0) FROM partido WHERE camiseta_id = ? AND resultado = 2"},
    {"rendimiento_derrotas", "SELECT IFNULL(SUM(rendimiento_general), 0) FROM partido WHERE camiseta_id = ? AND resultado = 3"},
    {"animo_derrotas", "SELECT IFNULL(SUM(estado_animo), 0) FROM partido WHERE camiseta_id = ? AND resultado = 3"},
    {"partidos_sin_goles", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND goles = 0"},
    {"partidos_sin_asistencias", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND asistencias = 0"},
    {"partidos_con_goles", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND goles > 0"},
    {"partidos_con_asistencias", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND asistencias > 0"},
    {"partidos_con_contribucion", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND (goles > 0 OR asistencias > 0)"},
    {"hat_tricks_dobles", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND goles >= 4"},
    {"asistencias_dobles", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND asistencias >= 5"},
    {"rendimiento_alto", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND rendimiento_general >= 8"},
    {"animo_alto", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND estado_animo >= 8"},
    {"rendimiento_bajo", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND rendimiento_general <= 3"},
    {"animo_bajo", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND estado_animo <= 3"},
    {"goles_por_partido_promedio", "SELECT ROUND(IFNULL(AVG(goles), 0) * 10) FROM partido WHERE camiseta_id = ?"},
    {"asistencias_por_partido_promedio", "SELECT ROUND(IFNULL(AVG(asistencias), 0) * 10) FROM partido WHERE camiseta_id = ?"},
    {"rendimiento_promedio", "SELECT ROUND(IFNULL(AVG(rendimiento_general), 0) * 10) FROM partido WHERE camiseta_id = ?"},
    {"animo_promedio", "SELECT ROUND(IFNULL(AVG(estado_animo), 0) * 10) FROM partido WHERE camiseta_id = ?"},
    {"partidos_con_rendimiento_alto", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND rendimiento_general >= 9"},
    {"partidos_con_animo_alto", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND estado_animo >= 9"},
    {"partidos_con_rendimiento_perfecto_y_animo", "SELECT COUNT(*) FROM partido WHERE camiseta_id = ? AND rendimiento_general = 10 AND estado_animo = 10"},
    {"goles_en_primer_tiempo", "SELECT IFNULL(SUM(goles), 0) FROM partido WHERE camiseta_id = ?"},
    {"asistencias_en_segundo_tiempo", "SELECT IFNULL(SUM(asistencias), 0) FROM partido WHERE camiseta_id = ?"},
    {"victorias_consecutivas_max", "SELECT COUNT(*) FROM (SELECT resultado, ROW_NUMBER() OVER (ORDER BY id) - ROW_NUMBER() OVER (PARTITION BY resultado ORDER BY id) as grp FROM partido WHERE camiseta_id = ?) WHERE resultado = 1 GROUP BY grp ORDER BY COUNT(*) DESC LIMIT 1"},
    {"derrotas_consecutivas_max", "SELECT COUNT(*) FROM (SELECT resultado, ROW_NUMBER() OVER (ORDER BY id) - ROW_NUMBER() OVER (PARTITION BY resultado ORDER BY id) as grp FROM partido WHERE camiseta_id = ?) WHERE resultado = 3 GROUP BY grp ORDER BY COUNT(*) DESC LIMIT 1"},
    {"empates_consecutivos_max", "SELECT COUNT(*) FROM (SELECT resultado, ROW_NUMBER() OVER (ORDER BY id) - ROW_NUMBER() OVER (PARTITION BY resultado ORDER BY id) as grp FROM partido WHERE camiseta_id = ?) WHERE resultado = 2 GROUP BY grp ORDER BY COUNT(*) DESC LIMIT 1"},
    {"goles_en_ultimo_partido", "SELECT IFNULL(goles, 0) FROM partido WHERE camiseta_id = ? ORDER BY id DESC LIMIT 1"},
    {"asistencias_en_ultimo_partido", "SELECT IFNULL(asistencias, 0) FROM partido WHERE camiseta_id = ? ORDER BY id DESC LIMIT 1"},
    {"rendimiento_en_ultimo_partido", "SELECT IFNULL(rendimiento_general, 0) FROM partido WHERE camiseta_id = ? ORDER BY id DESC LIMIT 1"},
    {"animo_en_ultimo_partido", "SELECT IFNULL(estado_animo, 0) FROM partido WHERE camiseta_id = ? ORDER BY id DESC LIMIT 1"}
};

#define NUM_QUERIES (sizeof(LOGRO_QUERIES) / sizeof(LogroQuery))

/**
 * @brief Array de logros disponibles en el sistema
 */
static const Logro LOGROS[] =
{
    {"Primer Gol", "Anotar tu primer gol", 1, "goles"},
    {"Goleador Novato", "Anotar 5 goles", 5, "goles"},
    {"Goleador Promedio", "Anotar 10 goles", 10, "goles"},
    {"Goleador Experto", "Anotar 25 goles", 25, "goles"},
    {"Goleador Maestro", "Anotar 50 goles", 50, "goles"},
    {"Goleador Leyenda", "Anotar 100 goles", 100, "goles"},
    {"Primera Asistencia", "Dar tu primera asistencia", 1, "asistencias"},
    {"Asistente Novato", "Dar 5 asistencias", 5, "asistencias"},
    {"Asistente Promedio", "Dar 10 asistencias", 10, "asistencias"},
    {"Asistente Experto", "Dar 25 asistencias", 25, "asistencias"},
    {"Asistente Maestro", "Dar 50 asistencias", 50, "asistencias"},
    {"Asistente Leyenda", "Dar 100 asistencias", 100, "asistencias"},
    {"Debutante", "Jugar tu primer partido", 1, "partidos"},
    {"Jugador Regular", "Jugar 5 partidos", 5, "partidos"},
    {"Jugador Estrella", "Jugar 10 partidos", 10, "partidos"},
    {"Jugador Veterano", "Jugar 25 partidos", 25, "partidos"},
    {"Jugador Maestro", "Jugar 50 partidos", 50, "partidos"},
    {"Jugador Leyenda", "Jugar 100 partidos", 100, "partidos"},
    {"Contribuidor Novato", "Acumular 10 puntos (goles + asistencias)", 10, "goles+asistencias"},
    {"Contribuidor Promedio", "Acumular 25 puntos (goles + asistencias)", 25, "goles+asistencias"},
    {"Contribuidor Experto", "Acumular 50 puntos (goles + asistencias)", 50, "goles+asistencias"},
    {"Contribuidor Maestro", "Acumular 100 puntos (goles + asistencias)", 100, "goles+asistencias"},
    {"Contribuidor Leyenda", "Acumular 250 puntos (goles + asistencias)", 250, "goles+asistencias"},
    // Victories
    {"Primera Victoria", "Ganar tu primer partido", 1, "victorias"},
    {"Ganador Novato", "Ganar 5 partidos", 5, "victorias"},
    {"Ganador Promedio", "Ganar 10 partidos", 10, "victorias"},
    {"Ganador Experto", "Ganar 25 partidos", 25, "victorias"},
    {"Ganador Maestro", "Ganar 50 partidos", 50, "victorias"},
    {"Ganador Leyenda", "Ganar 100 partidos", 100, "victorias"},
    // Draws
    {"Primer Empate", "Empatar tu primer partido", 1, "empates"},
    {"Empatador Novato", "Empatar 5 partidos", 5, "empates"},
    {"Empatador Promedio", "Empatar 10 partidos", 10, "empates"},
    {"Empatador Experto", "Empatar 25 partidos", 25, "empates"},
    {"Empatador Maestro", "Empatar 50 partidos", 50, "empates"},
    {"Empatador Leyenda", "Empatar 100 partidos", 100, "empates"},
    // Losses
    {"Primera Derrota", "Perder tu primer partido", 1, "derrotas"},
    {"Perdedor Novato", "Perder 5 partidos", 5, "derrotas"},
    {"Perdedor Promedio", "Perder 10 partidos", 10, "derrotas"},
    {"Perdedor Experto", "Perder 25 partidos", 25, "derrotas"},
    {"Perdedor Maestro", "Perder 50 partidos", 50, "derrotas"},
    {"Perdedor Leyenda", "Perder 100 partidos", 100, "derrotas"},
    // General Performance
    {"Rendimiento Inicial", "Acumular 10 puntos de rendimiento general", 10, "rendimiento_general"},
    {"Rendimiento Novato", "Acumular 50 puntos de rendimiento general", 50, "rendimiento_general"},
    {"Rendimiento Promedio", "Acumular 100 puntos de rendimiento general", 100, "rendimiento_general"},
    {"Rendimiento Experto", "Acumular 250 puntos de rendimiento general", 250, "rendimiento_general"},
    {"Rendimiento Maestro", "Acumular 500 puntos de rendimiento general", 500, "rendimiento_general"},
    {"Rendimiento Leyenda", "Acumular 1000 puntos de rendimiento general", 1000, "rendimiento_general"},
    // Mood
    {"Animo Inicial", "Acumular 10 puntos de estado de Animo", 10, "estado_animo"},
    {"Animo Novato", "Acumular 50 puntos de estado de Animo", 50, "estado_animo"},
    {"Animo Promedio", "Acumular 100 puntos de estado de Animo", 100, "estado_animo"},
    {"Animo Experto", "Acumular 250 puntos de estado de Animo", 250, "estado_animo"},
    {"Animo Maestro", "Acumular 500 puntos de estado de Animo", 500, "estado_animo"},
    {"Animo Leyenda", "Acumular 1000 puntos de estado de Animo", 1000, "estado_animo"},
    // Distinct Pitches
    {"Explorador de Canchas", "Jugar en 1 cancha distinta", 1, "canchas_distintas"},
    {"Viajero Novato", "Jugar en 5 canchas distintas", 5, "canchas_distintas"},
    {"Viajero Promedio", "Jugar en 10 canchas distintas", 10, "canchas_distintas"},
    {"Viajero Experto", "Jugar en 25 canchas distintas", 25, "canchas_distintas"},
    {"Viajero Maestro", "Jugar en 50 canchas distintas", 50, "canchas_distintas"},
    // Hat-Tricks
    {"Primer Hat-Trick", "Anotar 3 o mas goles en un partido", 1, "hat_tricks"},
    {"Hat-Tricker Novato", "Anotar 3 o mas goles en 5 partidos", 5, "hat_tricks"},
    {"Hat-Tricker Promedio", "Anotar 3 o mas goles en 10 partidos", 10, "hat_tricks"},
    {"Hat-Tricker Experto", "Anotar 3 o mas goles en 25 partidos", 25, "hat_tricks"},
    // Poker Assists
    {"Primer Poker de Asistencias", "Dar 4 o mas asistencias en un partido", 1, "poker_asistencias"},
    {"Poker Asistente Novato", "Dar 4 o mas asistencias en 5 partidos", 5, "poker_asistencias"},
    {"Poker Asistente Promedio", "Dar 4 o mas asistencias en 10 partidos", 10, "poker_asistencias"},
    // Perfect Performance
    {"Primer Rendimiento Perfecto", "Obtener rendimiento perfecto (10) en un partido", 1, "rendimiento_perfecto"},
    {"Rendimiento Perfecto Novato", "Obtener rendimiento perfecto en 5 partidos", 5, "rendimiento_perfecto"},
    {"Rendimiento Perfecto Promedio", "Obtener rendimiento perfecto en 10 partidos", 10, "rendimiento_perfecto"},
    {"Rendimiento Perfecto Experto", "Obtener rendimiento perfecto en 25 partidos", 25, "rendimiento_perfecto"},
    // Perfect Mood
    {"Primer Animo Perfecto", "Obtener animo perfecto (10) en un partido", 1, "animo_perfecto"},
    {"Animo Perfecto Novato", "Obtener animo perfecto en 5 partidos", 5, "animo_perfecto"},
    {"Animo Perfecto Promedio", "Obtener animo perfecto en 10 partidos", 10, "animo_perfecto"},
    {"Animo Perfecto Experto", "Obtener animo perfecto en 25 partidos", 25, "animo_perfecto"},
    // Victory Achievements
    {"Goleador Victorioso", "Anotar 10 goles en partidos ganados", 10, "goles_victorias"},
    {"Asistente Victorioso", "Dar 10 asistencias en partidos ganados", 10, "asistencias_victorias"},
    {"Rendimiento Victorioso", "Acumular 50 puntos de rendimiento en victorias", 50, "rendimiento_victorias"},
    {"Animo Victorioso", "Acumular 50 puntos de animo en victorias", 50, "animo_victorias"},
    // Loss Achievements
    {"Goleador en Derrotas", "Anotar 5 goles en partidos perdidos", 5, "goles_derrotas"},
    {"Asistente en Derrotas", "Dar 5 asistencias en partidos perdidos", 5, "asistencias_derrotas"},
    // Draw Achievements
    {"Rendimiento en Empates", "Acumular 25 puntos de rendimiento en empates", 25, "rendimiento_empates"},
    {"Animo en Empates", "Acumular 25 puntos de animo en empates", 25, "animo_empates"},
    // Additional Achievements
    {"Gol en Victoria", "Anotar en 5 partidos ganados", 5, "goles_victorias"},
    {"Asistencia Clave", "Asistir en 5 partidos ganados", 5, "asistencias_victorias"},
    {"Presente en la Derrota", "Anotar en 5 partidos perdidos", 5, "goles_derrotas"},
    {"Asistencia en Derrota", "Asistir en 5 partidos perdidos", 5, "asistencias_derrotas"},
    // New Achievements for Draws
    {"Primer Gol en Empate", "Anotar tu primer gol en un empate", 1, "goles_empates"},
    {"Goleador en Empates", "Anotar 5 goles en empates", 5, "goles_empates"},
    {"Asistente en Empates", "Dar 5 asistencias en empates", 5, "asistencias_empates"},
    {"Contribuidor en Empates", "Acumular 10 puntos en empates", 10, "goles_empates"},
    // New Achievements for Losses
    {"Rendimiento en Derrotas", "Acumular 50 puntos de rendimiento en derrotas", 50, "rendimiento_derrotas"},
    {"Animo en Derrotas", "Acumular 50 puntos de animo en derrotas", 50, "animo_derrotas"},
    // No Contribution Achievements
    {"Primer Partido Sin Goles", "Jugar un partido sin anotar", 1, "partidos_sin_goles"},
    {"5 Partidos Sin Goles", "Jugar 5 partidos sin anotar", 5, "partidos_sin_goles"},
    {"Primer Partido Sin Asistencias", "Jugar un partido sin asistir", 1, "partidos_sin_asistencias"},
    {"5 Partidos Sin Asistencias", "Jugar 5 partidos sin asistir", 5, "partidos_sin_asistencias"},
    // Contribution Achievements
    {"Primer Gol Anotado", "Anotar en un partido", 1, "partidos_con_goles"},
    {"5 Partidos con Goles", "Anotar en 5 partidos", 5, "partidos_con_goles"},
    {"Primer Asistencia Dada", "Asistir en un partido", 1, "partidos_con_asistencias"},
    {"5 Partidos con Asistencias", "Asistir en 5 partidos", 5, "partidos_con_asistencias"},
    {"Contribuidor Inicial", "Contribuir en un partido", 1, "partidos_con_contribucion"},
    {"Contribuidor Regular", "Contribuir en 10 partidos", 10, "partidos_con_contribucion"},
    // Advanced Scoring
    {"Primer Hat-Trick Doble", "Anotar 4 o mas goles en un partido", 1, "hat_tricks_dobles"},
    {"Hat-Tricker Doble Novato", "Anotar 4 o mas goles en 3 partidos", 3, "hat_tricks_dobles"},
    {"Primer Poker de Asistencias Doble", "Dar 5 o mas asistencias en un partido", 1, "asistencias_dobles"},
    {"Poker Asistente Doble Novato", "Dar 5 o mas asistencias en 3 partidos", 3, "asistencias_dobles"},
    // High Performance
    {"Rendimiento Alto Inicial", "Obtener rendimiento >=8 en un partido", 1, "rendimiento_alto"},
    {"Rendimiento Alto Regular", "Obtener rendimiento >=8 en 10 partidos", 10, "rendimiento_alto"},
    {"Animo Alto Inicial", "Obtener animo >=8 en un partido", 1, "animo_alto"},
    {"Animo Alto Regular", "Obtener animo >=8 en 10 partidos", 10, "animo_alto"},
    // Low Performance
    {"Rendimiento Bajo", "Obtener rendimiento <=3 en un partido", 1, "rendimiento_bajo"},
    {"Animo Bajo", "Obtener animo <=3 en un partido", 1, "animo_bajo"},
    // Average Achievements (Note: These use multiplied values, so objectives are *10)
    {"Promedio Goleador", "Mantener promedio de 0.5 goles por partido", 5, "goles_por_partido_promedio"},
    {"Promedio Asistente", "Mantener promedio de 0.5 asistencias por partido", 5, "asistencias_por_partido_promedio"},
    {"Promedio Rendimiento Alto", "Mantener promedio de rendimiento >=7", 70, "rendimiento_promedio"},
    {"Promedio Animo Alto", "Mantener promedio de animo >=7", 70, "animo_promedio"},
    // Near Perfect
    {"Rendimiento Cercano a Perfecto", "Obtener rendimiento >=9 en un partido", 1, "partidos_con_rendimiento_alto"},
    {"Animo Cercano a Perfecto", "Obtener animo >=9 en un partido", 1, "partidos_con_animo_alto"},
    {"Dia Perfecto", "Obtener rendimiento y animo perfectos en un partido", 1, "partidos_con_rendimiento_perfecto_y_animo"},
    // Placeholder for time-based (since no time columns)
    {"Goleador en Primer Tiempo", "Anotar 10 goles (simulado)", 10, "goles_en_primer_tiempo"},
    {"Asistente en Segundo Tiempo", "Dar 10 asistencias (simulado)", 10, "asistencias_en_segundo_tiempo"},
    // Streak Achievements (May not work with standard SQLite)
    {"Racha de Victorias", "Ganar 3 partidos consecutivos", 3, "victorias_consecutivas_max"},
    {"Racha de Derrotas", "Perder 3 partidos consecutivos", 3, "derrotas_consecutivas_max"},
    {"Racha de Empates", "Empatar 3 partidos consecutivos", 3, "empates_consecutivos_max"},
    // Last Match Achievements
    {"Ultimo Gol", "Anotar en el ultimo partido", 1, "goles_en_ultimo_partido"},
    {"ultima Asistencia", "Asistir en el ultimo partido", 1, "asistencias_en_ultimo_partido"},
    {"Ultimo Rendimiento Perfecto", "Rendimiento perfecto en el ultimo partido", 10, "rendimiento_en_ultimo_partido"},
    {"Ultimo Animo Perfecto", "Animo perfecto en el ultimo partido", 10, "animo_en_ultimo_partido"},
    // More Tiered Achievements
    {"Goleador en Empates Experto", "Anotar 10 goles en empates", 10, "goles_empates"},
    {"Asistente en Empates Experto", "Dar 10 asistencias en empates", 10, "asistencias_empates"},
    {"Rendimiento en Derrotas Experto", "Acumular 100 puntos de rendimiento en derrotas", 100, "rendimiento_derrotas"},
    {"Animo en Derrotas Experto", "Acumular 100 puntos de animo en derrotas", 100, "animo_derrotas"},
    {"10 Partidos Sin Goles", "Jugar 10 partidos sin anotar", 10, "partidos_sin_goles"},
    {"10 Partidos Sin Asistencias", "Jugar 10 partidos sin asistir", 10, "partidos_sin_asistencias"},
    {"10 Partidos con Goles", "Anotar en 10 partidos", 10, "partidos_con_goles"},
    {"10 Partidos con Asistencias", "Asistir en 10 partidos", 10, "partidos_con_asistencias"},
    {"Contribuidor Avanzado", "Contribuir en 25 partidos", 25, "partidos_con_contribucion"},
    {"Hat-Tricker Doble Experto", "Anotar 4 o mas goles en 10 partidos", 10, "hat_tricks_dobles"},
    {"Poker Asistente Doble Experto", "Dar 5 o mas asistencias en 10 partidos", 10, "asistencias_dobles"},
    {"Rendimiento Alto Experto", "Obtener rendimiento >=8 en 25 partidos", 25, "rendimiento_alto"},
    {"Animo Alto Experto", "Obtener animo >=8 en 25 partidos", 25, "animo_alto"},
    {"Rendimiento Bajo Experto", "Obtener rendimiento <=3 en 5 partidos", 5, "rendimiento_bajo"},
    {"Animo Bajo Experto", "Obtener animo <=3 en 5 partidos", 5, "animo_bajo"},
    {"Promedio Goleador Experto", "Mantener promedio de 1 gol por partido", 10, "goles_por_partido_promedio"},
    {"Promedio Asistente Experto", "Mantener promedio de 1 asistencia por partido", 10, "asistencias_por_partido_promedio"},
    {"Rendimiento Cercano a Perfecto Experto", "Obtener rendimiento >=9 en 10 partidos", 10, "partidos_con_rendimiento_alto"},
    {"Animo Cercano a Perfecto Experto", "Obtener animo >=9 en 10 partidos", 10, "partidos_con_animo_alto"},
    {"Dia Perfecto Experto", "Obtener rendimiento y animo perfectos en 5 partidos", 5, "partidos_con_rendimiento_perfecto_y_animo"},
    {"Racha de Victorias Experta", "Ganar 5 partidos consecutivos", 5, "victorias_consecutivas_max"},
    {"Racha de Derrotas Experta", "Perder 5 partidos consecutivos", 5, "derrotas_consecutivas_max"},
    {"Racha de Empates Experta", "Empatar 5 partidos consecutivos", 5, "empates_consecutivos_max"}
};

#define NUM_LOGROS (sizeof(LOGROS) / sizeof(Logro))



/**
 * @brief Obtiene el progreso actual de una camiseta para un logro especifico (version optimizada)
 *
 * @param camiseta_id ID de la camiseta
 * @param tipo Tipo de estadistica
 * @return Valor actual de la estadistica
 */
static int obtener_progreso_logro(int camiseta_id, const char *tipo)
{
    // Buscar la consulta correspondiente en el array
    for (size_t i = 0; i < NUM_QUERIES; i++)
    {
        if (strcmp(tipo, LOGRO_QUERIES[i].tipo) != 0)
        {
            continue;
        }

        sqlite3_stmt *stmt;
        int progreso = 0;

        if (!preparar_stmt(LOGRO_QUERIES[i].sql, &stmt))
        {
            return 0;
        }

        sqlite3_bind_int(stmt, 1, camiseta_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            progreso = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);

        return progreso;
    }

    // Tipo no encontrado
    return 0;
}

/**
 * @brief Determina el estado de un logro para una camiseta especifica
 *
 * @param camiseta_id ID de la camiseta
 * @param logro Puntero al logro
 * @param progreso Puntero donde se almacenara el progreso actual
 * @return 0: No iniciado, 1: En progreso, 2: Completado
 */
static int obtener_estado_logro(int camiseta_id, const Logro *logro, int *progreso)
{
    *progreso = obtener_progreso_logro(camiseta_id, logro->tipo);

    if (*progreso >= logro->objetivo)
    {
        return 2; // Completado
    }
    else if (*progreso > 0)
    {
        return 1; // En progreso
    }
    else
    {
        return 0; // No iniciado
    }
}

/**
 * @brief Obtiene el nombre de una camiseta desde la base de datos
 *
 * Para mostrar informacion contextual al usuario, recupera el nombre asociado al ID.
 *
 * @param camiseta_id ID de la camiseta
 * @param nombre Buffer para almacenar el nombre
 */
static void obtener_nombre_camiseta(int camiseta_id, char *nombre)
{
    obtener_nombre_entidad("camiseta", camiseta_id, nombre, 256);
}

/**
 * @brief Muestra un logro individual con su estado y progreso
 *
 * Para mantener consistencia visual, centraliza la logica de impresion de cada logro.
 *
 * @param logro Puntero al logro
 * @param estado Estado del logro (0,1,2)
 * @param progreso Progreso actual
 */
static void mostrar_logro_individual(const Logro *logro, int estado, int progreso)
{
    const char *estado_texto;

    switch (estado)
    {
    case 0:
        estado_texto = "[NO INICIADO]";
        break;
    case 1:
        estado_texto = "[EN PROGRESO]";
        break;
    case 2:
        estado_texto = "[COMPLETADO]";
        break;
    default:
        estado_texto = "[DESCONOCIDO]";
        break;
    }

    printf("%s %s\n", logro->nombre, estado_texto);
    printf("%s\n", logro->descripcion);
    printf("Progreso: %d/%d\n\n", progreso, logro->objetivo);
}

static int logro_estado_cumple_filtro(int estado, int filtro)
{
    if (filtro == 1 && estado != 2)
    {
        return 0;
    }
    if (filtro == 2 && estado != 1)
    {
        return 0;
    }
    if (filtro == 3 && estado != 0)
    {
        return 0;
    }
    return 1;
}

static Color logro_color_por_estado(int estado)
{
    if (estado == 2)
    {
        return (Color){56, 176, 92, 255};
    }
    if (estado == 1)
    {
        return (Color){232, 193, 72, 255};
    }
    return (Color){201, 75, 75, 255};
}

static const char *logro_etiqueta_estado(int estado)
{
    if (estado == 2) return "COMPLETADO";
    if (estado == 1) return "EN PROGRESO";
    return "NO COMPLETADO";
}

static int cargar_logros_vista(int camiseta_id,
                               int filtro,
                               LogroVistaRow **rows_out,
                               int *count_out,
                               int *completados_out,
                               int *progreso_out,
                               int *no_completados_out)
{
    LogroVistaRow *rows = NULL;
    int count = 0;
    int comp = 0;
    int prog = 0;
    int no_comp = 0;

    if (!rows_out || !count_out || !completados_out || !progreso_out || !no_completados_out)
    {
        return 0;
    }

    *rows_out = NULL;
    *count_out = 0;
    *completados_out = 0;
    *progreso_out = 0;
    *no_completados_out = 0;

    rows = (LogroVistaRow *)malloc(NUM_LOGROS * sizeof(LogroVistaRow));
    if (!rows)
    {
        return 0;
    }

    for (size_t i = 0; i < NUM_LOGROS; i++)
    {
        int progreso = 0;
        int estado = obtener_estado_logro(camiseta_id, &LOGROS[i], &progreso);

        if (estado == 2)
        {
            comp++;
        }
        else if (estado == 1)
        {
            prog++;
        }
        else
        {
            no_comp++;
        }

        if (!logro_estado_cumple_filtro(estado, filtro))
        {
            continue;
        }

        rows[count].logro = &LOGROS[i];
        rows[count].estado = estado;
        rows[count].progreso = progreso;
        count++;
    }

    *rows_out = rows;
    *count_out = count;
    *completados_out = comp;
    *progreso_out = prog;
    *no_completados_out = no_comp;
    return 1;
}

#ifndef UNIT_TEST
static void dibujar_logro_row_visual(const LogroVistaRow *row, Rectangle rect)
{
    Color estado_color = logro_color_por_estado(row->estado);
    Rectangle progress_bg = {rect.x + 14.0f, rect.y + rect.height - 17.0f, rect.width - 28.0f, 8.0f};
    Rectangle progress_fill = progress_bg;
    float ratio = 0.0f;
    char progreso_txt[64];

    if (row->logro->objetivo > 0)
    {
        ratio = (float)row->progreso / (float)row->logro->objetivo;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
    }
    progress_fill.width = progress_bg.width * ratio;

    DrawRectangleRounded(rect, 0.10f, 8, ColorAlpha(estado_color, 0.18f));
    DrawRectangleRoundedLines(rect, 0.10f, 8, estado_color);

    gui_text_truncated(row->logro->nombre, rect.x + 14.0f, rect.y + 8.0f, 18.0f, rect.width - 190.0f, RAYWHITE);
    gui_text(logro_etiqueta_estado(row->estado), rect.x + rect.width - 166.0f, rect.y + 8.0f, 16.0f, estado_color);

    gui_text_truncated(row->logro->descripcion,
                       rect.x + 14.0f,
                       rect.y + 34.0f,
                       15.0f,
                       rect.width - 28.0f,
                       (Color){218, 232, 222, 255});

    DrawRectangleRec(progress_bg, ColorAlpha(BLACK, 0.40f));
    if (progress_fill.width > 0.0f)
    {
        DrawRectangleRec(progress_fill, estado_color);
    }

    (void)snprintf(progreso_txt, sizeof(progreso_txt),
                   "%d/%d", row->progreso, row->logro->objetivo);
    gui_text(progreso_txt,
             progress_bg.x + progress_bg.width - 72.0f,
             progress_bg.y - 17.0f,
             13.0f,
             (Color){228, 236, 232, 255});
}

static void mostrar_logros_camiseta_visual_gui(int camiseta_id, const char *titulo, int filtro)
{
    char nombre_camiseta[128] = {0};
    LogroVistaRow *rows = NULL;
    int count = 0;
    int total_comp = 0;
    int total_prog = 0;
    int total_no_comp = 0;
    int scroll = 0;
    const int row_h = 88;

    obtener_nombre_camiseta(camiseta_id, nombre_camiseta);
    if (safe_strnlen(nombre_camiseta, sizeof(nombre_camiseta)) == 0)
    {
        return;
    }

    if (!cargar_logros_vista(camiseta_id,
                             filtro,
                             &rows,
                             &count,
                             &total_comp,
                             &total_prog,
                             &total_no_comp))
    {
        return;
    }

    while (!WindowShouldClose())
    {
        const int sw = GetScreenWidth();
        const int sh = GetScreenHeight();
        const int panel_x = 40;
        const int panel_y = 102;
        const int panel_w = sw - 80;
        const int panel_h = sh - 148;
        const int list_y = panel_y + 110;
        const int list_h = panel_h - 146;
        int visible_rows = list_h / row_h;
        int max_scroll = 0;
        char subtitle[192];
        char resumen[128];

        if (visible_rows < 1)
        {
            visible_rows = 1;
        }
        max_scroll = (count > visible_rows) ? (count - visible_rows) : 0;

        if (GetMouseWheelMove() > 0.01f) scroll -= 2;
        if (GetMouseWheelMove() < -0.01f) scroll += 2;
        if (IsKeyPressed(KEY_UP)) scroll--;
        if (IsKeyPressed(KEY_DOWN)) scroll++;
        if (IsKeyPressed(KEY_PAGE_UP)) scroll -= visible_rows;
        if (IsKeyPressed(KEY_PAGE_DOWN)) scroll += visible_rows;
        if (IsKeyPressed(KEY_HOME)) scroll = 0;
        if (IsKeyPressed(KEY_END)) scroll = max_scroll;
        if (scroll < 0) scroll = 0;
        if (scroll > max_scroll) scroll = max_scroll;

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header(titulo, sw);

        DrawRectangleRounded((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                             0.020f, 8, (Color){20, 42, 31, 255});
        DrawRectangleRoundedLines((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                                  0.020f, 8, (Color){53, 112, 81, 255});

        (void)snprintf(subtitle, sizeof(subtitle), "Camiseta: %s", nombre_camiseta);
        gui_text(subtitle, (float)(panel_x + 14), (float)(panel_y + 12), 22.0f, (Color){233, 247, 236, 255});

        (void)snprintf(resumen, sizeof(resumen), "Verde: %d | Amarillo: %d | Rojo: %d", total_comp, total_prog, total_no_comp);
        gui_text(resumen, (float)(panel_x + 14), (float)(panel_y + 42), 17.0f, (Color){196, 224, 210, 255});

        DrawCircle(panel_x + 20, panel_y + 76, 6.0f, logro_color_por_estado(2));
        gui_text("Completado", (float)(panel_x + 32), (float)(panel_y + 68), 15.0f, (Color){228, 240, 232, 255});

        DrawCircle(panel_x + 162, panel_y + 76, 6.0f, logro_color_por_estado(1));
        gui_text("En progreso", (float)(panel_x + 174), (float)(panel_y + 68), 15.0f, (Color){228, 240, 232, 255});

        DrawCircle(panel_x + 304, panel_y + 76, 6.0f, logro_color_por_estado(0));
        gui_text("No completado", (float)(panel_x + 316), (float)(panel_y + 68), 15.0f, (Color){228, 240, 232, 255});

        BeginScissorMode(panel_x + 10, list_y, panel_w - 20, list_h);
        if (count == 0)
        {
            gui_text("No hay logros para este filtro.", (float)(panel_x + 18), (float)(list_y + 16), 20.0f, (Color){225, 122, 122, 255});
        }
        else
        {
            for (int i = scroll; i < count; i++)
            {
                int row = i - scroll;
                float y = (float)(list_y + row * row_h);
                Rectangle row_rect;

                if (row >= visible_rows)
                {
                    break;
                }

                row_rect = (Rectangle){(float)(panel_x + 12), y, (float)(panel_w - 24), (float)(row_h - 6)};
                dibujar_logro_row_visual(&rows[i], row_rect);
            }
        }
        EndScissorMode();

        gui_draw_footer_hint("Rueda/Flechas: desplazarte | ESC/Enter: volver", (float)panel_x, sh);
        EndDrawing();

        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        {
            break;
        }
    }

    free(rows);
}
#endif

/**
 * @brief Muestra los logros de una camiseta especifica
 *
 * Para proporcionar feedback al usuario, lista logros filtrados segun el criterio seleccionado.
 *
 * @param camiseta_id ID de la camiseta
 * @param filtro 0: Todos, 1: Solo completados, 2: Solo en progreso
 */
static void mostrar_logros_camiseta(int camiseta_id, int filtro)
{
    char nombre_camiseta[100];
    obtener_nombre_camiseta(camiseta_id, nombre_camiseta);

    if (safe_strnlen(nombre_camiseta, sizeof(nombre_camiseta)) == 0) return; // Error already printed

    printf("LOGROS DE: %s\n", nombre_camiseta);
    printf("========================================\n\n");

    int mostrados = 0;

    for (size_t i = 0; i < NUM_LOGROS; i++)
    {
        int progreso = 0;
        int estado = obtener_estado_logro(camiseta_id, &LOGROS[i], &progreso);

        if (!logro_estado_cumple_filtro(estado, filtro))
        {
            continue;
        }

        mostrados++;
        mostrar_logro_individual(&LOGROS[i], estado, progreso);
    }

    if (mostrados == 0)
    {
        mostrar_no_hay_registros_logros("logros que mostrar con el filtro seleccionado");
    }
}

/**
 * @brief Carga camisetas con partidos para selector GUI
 *
 * Para evitar seleccion invalida, filtra solo camisetas que tienen al menos un partido.
 */
static int cargar_camisetas_con_partidos(CamisetaRow **rows_out, int *count_out)
{
    CamisetaRow *rows = NULL;
    int count = 0;
    int cap = 16;
    sqlite3_stmt *stmt;

    if (!rows_out || !count_out)
    {
        return 0;
    }

    *rows_out = NULL;
    *count_out = 0;

    rows = (CamisetaRow *)malloc((size_t)cap * sizeof(CamisetaRow));
    if (!rows)
    {
        return 0;
    }

    if (!preparar_stmt("SELECT DISTINCT c.id, c.nombre FROM camiseta c INNER JOIN partido p ON c.id = p.camiseta_id ORDER BY c.id", &stmt))
    {
        free(rows);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (count >= cap)
        {
            int new_cap = cap * 2;
            CamisetaRow *tmp = (CamisetaRow *)realloc(rows, (size_t)new_cap * sizeof(CamisetaRow));
            if (!tmp)
            {
                sqlite3_finalize(stmt);
                free(rows);
                return 0;
            }
            rows = tmp;
            cap = new_cap;
        }

        rows[count].id = sqlite3_column_int(stmt, 0);
        (void)snprintf(rows[count].nombre,
                       sizeof(rows[count].nombre),
                       "%s",
                       sqlite3_column_text(stmt, 1) ? (const char *)sqlite3_column_text(stmt, 1) : "(sin nombre)");
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

/**
 * @brief Selector GUI de camiseta para ver logros
 *
 * Usa lista clickeable para eliminar input CLI en el flujo de logros.
 *
 * @return 1 si se selecciono, 0 si se cancelo o fallo
 */
static int seleccionar_camiseta_gui(const char *titulo, int *id_out)
{
#ifdef UNIT_TEST
    int camiseta_id = input_int("ID de la camiseta,(0 para Cancelar): ");
    if (camiseta_id == 0)
    {
        if (id_out) *id_out = 0;
        return 0;
    }
    if (!existe_id("camiseta", camiseta_id))
    {
        printf("La camiseta no existe.\n");
        if (id_out) *id_out = 0;
        return 0;
    }
    if (id_out) *id_out = camiseta_id;
    return 1;
#else
    CamisetaRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 34;

    if (!id_out)
    {
        return 0;
    }
    *id_out = 0;

    if (!cargar_camisetas_con_partidos(&rows, &count))
    {
        return 0;
    }

    if (count == 0)
    {
        free(rows);
        clear_screen();
        print_header(titulo);
        mostrar_no_hay_registros_logros("camisetas con partidos");
        pause_console();
        return 0;
    }

    while (!WindowShouldClose())
    {
        const int sw = GetScreenWidth();
        const int sh = GetScreenHeight();
        const int panel_x = 60;
        const int panel_y = 110;
        const int panel_w = sw - 120;
        const int panel_h = sh - 180;
        const int content_y = panel_y + 32;
        const int content_h = panel_h - 32;
        int visible_rows = content_h / row_h;
        int max_scroll = 0;
        int clicked_id = 0;

        if (visible_rows < 1)
        {
            visible_rows = 1;
        }
        max_scroll = (count > visible_rows) ? (count - visible_rows) : 0;

        if (GetMouseWheelMove() > 0.01f) scroll -= 3;
        if (GetMouseWheelMove() < -0.01f) scroll += 3;
        if (IsKeyPressed(KEY_UP)) scroll--;
        if (IsKeyPressed(KEY_DOWN)) scroll++;
        if (IsKeyPressed(KEY_PAGE_UP)) scroll -= visible_rows;
        if (IsKeyPressed(KEY_PAGE_DOWN)) scroll += visible_rows;
        if (IsKeyPressed(KEY_HOME)) scroll = 0;
        if (IsKeyPressed(KEY_END)) scroll = max_scroll;

        if (scroll < 0) scroll = 0;
        if (scroll > max_scroll) scroll = max_scroll;

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header(titulo, sw);

        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                            "ID", 12.0f,
                            "CAMISETA", 80.0f);

        BeginScissorMode(panel_x, content_y, panel_w, content_h);
        for (int i = scroll; i < count; i++)
        {
            const int row = i - scroll;
            const int y = content_y + row * row_h;
            Rectangle fila;

            if (row >= visible_rows)
            {
                break;
            }

            fila = (Rectangle){(float)(panel_x + 6), (float)y, (float)(panel_w - 12), (float)(row_h - 2)};
            gui_draw_list_row_bg(fila, row, CheckCollisionPointRec(GetMousePosition(), fila));
            if (CheckCollisionPointRec(GetMousePosition(), fila) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                clicked_id = rows[i].id;
            }

            gui_text(TextFormat("%3d", rows[i].id), (float)(panel_x + 12), (float)(y + 7), 18.0f, (Color){220, 238, 225, 255});
            gui_text(rows[i].nombre, (float)(panel_x + 80), (float)(y + 7), 18.0f, (Color){233, 247, 236, 255});
        }
        EndScissorMode();

        gui_draw_footer_hint("Click para seleccionar | Rueda/Flechas: desplazarte | ESC/Enter: volver", (float)panel_x, sh);
        EndDrawing();

        if (clicked_id > 0)
        {
            *id_out = clicked_id;
            free(rows);
            return 1;
        }

        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        {
            break;
        }
    }

    free(rows);
    return 0;
#endif
}

/**
 * @brief Muestra logros con un filtro especifico
 *
 * Centraliza la logica comun de mostrar logros para diferentes vistas,
 * reduciendo duplicacion de codigo y mejorando mantenibilidad.
 *
 * @param titulo Titulo de la vista
 * @param filtro Tipo de filtro (0=todos, 1=completados, 2=en progreso)
 */
static void mostrar_logros_con_filtro(const char *titulo, int filtro)
{
    int camiseta_id = 0;

    if (!seleccionar_camiseta_gui("SELECCIONAR CAMISETA", &camiseta_id))
    {
        return;
    }

#ifndef UNIT_TEST
    mostrar_logros_camiseta_visual_gui(camiseta_id, titulo, filtro);
    return;
#endif

    clear_screen();
    print_header(titulo);
    mostrar_logros_camiseta(camiseta_id, filtro);
    pause_console();
}

/**
 * @brief Muestra todos los logros disponibles con su estado
 */
void mostrar_todos_logros()
{
    mostrar_logros_con_filtro("TODOS LOS LOGROS", 0);
}

/**
 * @brief Muestra solo los logros completados
 */
void mostrar_logros_completados()
{
    mostrar_logros_con_filtro("LOGROS COMPLETADOS", 1);
}

/**
 * @brief Muestra los logros en progreso
 */
void mostrar_logros_en_progreso()
{
    mostrar_logros_con_filtro("LOGROS EN PROGRESO", 2);
}

/**
 * @brief Muestra los logros no completados
 */
void mostrar_logros_no_completados()
{
    mostrar_logros_con_filtro("LOGROS NO COMPLETADOS", 3);
}

/**
 * @brief Muestra el menu principal de logros y badges
 */
void menu_logros()
{
    static const MenuItem items[] =
    {
        LOGROS_ITEM(1, "Ver Todos los Logros", mostrar_todos_logros),
        LOGROS_ITEM(2, "Logros Completados", mostrar_logros_completados),
        LOGROS_ITEM(3, "Logros en Progreso", mostrar_logros_en_progreso),
        LOGROS_ITEM(4, "Logros No Completados", mostrar_logros_no_completados),
        LOGROS_BACK_ITEM
    };

    ejecutar_menu_estandar("LOGROS", items, ARRAY_COUNT(items));
}
