/**
 * @file estadisticas_meta.c
 * @brief Modulo para estadisticas avanzadas y meta-analisis en partidos de futbol.
 *
 * Este archivo contiene funciones para analizar consistencia, partidos atipicos,
 * dependencia del contexto, impacto del cansancio y estado de animo.
 */

#include "estadisticas_meta.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "estadisticas_gui_capture.h"

static const char *resultado_a_texto(int resultado)
{
    switch (resultado)
    {
    case 1:
        return "Victoria";
    case 2:
        return "Empate";
    case 3:
        return "Derrota";
    default:
        return "Desconocido";
    }
}

// Static helper to execute SQL queries and display results in a formatted way,
// ensuring consistent output format across different statistical analyses without code duplication.
static void query(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt;
    char nombre[200];

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (!preparar_stmt_export(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        return;
    }

    int num_cols = sqlite3_column_count(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (num_cols == 1)
        {
            if (sqlite3_column_type(stmt, 0) == SQLITE_FLOAT)
            {
                printf("%-30s : %.4f\n", titulo, sqlite3_column_double(stmt, 0));
            }
            else if (sqlite3_column_type(stmt, 0) == SQLITE_INTEGER)
            {
                printf("%-30s : %d\n", titulo, sqlite3_column_int(stmt, 0));
            }
            else if (sqlite3_column_type(stmt, 0) == SQLITE_NULL)
            {
                printf("%-30s : N/A\n", titulo);
            }
            else
            {
                printf("%-30s : %s\n", titulo,
                       (const char *)sqlite3_column_text(stmt, 0));
            }
        }
        else
        {
            const char* text = (const char*)sqlite3_column_text(stmt, 0);
            snprintf(nombre, sizeof(nombre), "%s", text ? text : "Desconocido");

            if (sqlite3_column_type(stmt, 1) == SQLITE_FLOAT)
            {
                printf("%-30s : %.2f\n", nombre, sqlite3_column_double(stmt, 1));
            }
            else if (sqlite3_column_type(stmt, 1) == SQLITE_INTEGER)
            {
                printf("%-30s : %d\n", nombre, sqlite3_column_int(stmt, 1));
            }
            else
            {
                printf("%-30s : %d\n", nombre, sqlite3_column_int(stmt, 1));
            }
        }
    }

    sqlite3_finalize(stmt);
}

/**
 * @brief Muestra la consistencia del rendimiento (variabilidad)
 *
 * Analiza la desviacion estandar y coeficiente de variacion del rendimiento general
 * para evaluar la consistencia del jugador/equipo.
 */
void mostrar_consistencia_rendimiento()
{
    clear_screen();
    print_header("CONSISTENCIA DEL RENDIMIENTO");

    // Calcular estadisticas basicas
    query("Promedio de Rendimiento General",
          "SELECT ROUND(AVG(rendimiento_general), 2) FROM partido");

    // Calcular desviacion estandar
    query("Desviacion Estandar del Rendimiento",
          "SELECT ROUND(SQRT(AVG(rendimiento_general * rendimiento_general) - AVG(rendimiento_general) * AVG(rendimiento_general)), 2) FROM partido");

    // Calcular coeficiente de variacion
    query("Coeficiente de Variacion (%)",
          "SELECT ROUND((SQRT(AVG(rendimiento_general * rendimiento_general) - AVG(rendimiento_general) * AVG(rendimiento_general)) / AVG(rendimiento_general) * 100), 2) FROM partido");

    // Mostrar rango de rendimiento
    query("Rango de Rendimiento (Minimo)",
          "SELECT MIN(rendimiento_general) FROM partido");
    query("Rango de Rendimiento (Maximo)",
          "SELECT MAX(rendimiento_general) FROM partido");

    pause_console();
}

/**
 * @brief Funcion generica para mostrar partidos con ciertos criterios SQL
 */
static void mostrar_partidos_con_sql(const char *titulo, const char *descripcion, const char *sql)
{
    clear_screen();
    print_header(titulo);

    printf("\n%s\n", descripcion);
    printf("----------------------------------------\n");

    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *resultado_str = resultado_a_texto(sqlite3_column_int(stmt, 6));

        printf("ID: %d, Fecha: %s, Cansancio: %d, Rendimiento: %d, Goles: %d, Asistencias: %d, Resultado: %s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_int(stmt, 2),
               sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4),
               sqlite3_column_int(stmt, 5),
               resultado_str);
        hay = 1;
    }
    if (!hay) printf("No se encontraron partidos que cumplan el criterio.\n");

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Muestra los partidos atipicos (muy buenos/muy malos)
 *
 * Identifica partidos con rendimiento significativamente diferente al promedio.
 */
void mostrar_partidos_outliers()
{
    clear_screen();
    print_header("PARTIDOS ATIPICOS");

    printf("\nPartidos con rendimiento excepcionalmente alto:\n");
    printf("----------------------------------------\n");

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT id, fecha_hora, rendimiento_general, goles, asistencias "
        "FROM partido "
        "WHERE rendimiento_general > ("
        "  SELECT AVG(rendimiento_general) + 1.5 * ("
        "    (SELECT rendimiento_general FROM partido ORDER BY rendimiento_general ASC "
        "     LIMIT 1 OFFSET (SELECT COUNT(*)*3/4 FROM partido)) - "
        "    (SELECT rendimiento_general FROM partido ORDER BY rendimiento_general ASC "
        "     LIMIT 1 OFFSET (SELECT COUNT(*)/4 FROM partido))"
        "  ) FROM partido"
        ") ORDER BY rendimiento_general DESC";

    int hay = 0;
    if (!preparar_stmt_export(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Partido ID: %d, Fecha: %s, Rendimiento: %d, Goles: %d, Asistencias: %d\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_int(stmt, 2),
               sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4));
        hay = 1;
    }
    if (!hay) printf("No se encontraron partidos atipicos altos.\n");
    sqlite3_finalize(stmt);

    printf("\nPartidos con rendimiento excepcionalmente bajo:\n");
    printf("----------------------------------------\n");

    const char *sql2 =
        "SELECT id, fecha_hora, rendimiento_general, goles, asistencias "
        "FROM partido "
        "WHERE rendimiento_general < ("
        "  SELECT AVG(rendimiento_general) - 1.5 * ("
        "    (SELECT rendimiento_general FROM partido ORDER BY rendimiento_general ASC "
        "     LIMIT 1 OFFSET (SELECT COUNT(*)*3/4 FROM partido)) - "
        "    (SELECT rendimiento_general FROM partido ORDER BY rendimiento_general ASC "
        "     LIMIT 1 OFFSET (SELECT COUNT(*)/4 FROM partido))"
        "  ) FROM partido"
        ") ORDER BY rendimiento_general ASC";

    hay = 0;
    if (!preparar_stmt_export(&stmt, sql2))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Partido ID: %d, Fecha: %s, Rendimiento: %d, Goles: %d, Asistencias: %d\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1),
               sqlite3_column_int(stmt, 2),
               sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4));
        hay = 1;
    }
    if (!hay) printf("No se encontraron partidos atipicos bajos.\n");
    sqlite3_finalize(stmt);

    pause_console();
}

/**
 * @brief Muestra la dependencia del contexto
 *
 * Analiza como el rendimiento varia segun diferentes contextos (clima, dia, etc.)
 */
void mostrar_dependencia_contexto()
{
    clear_screen();
    print_header("DEPENDENCIA DEL CONTEXTO");

    printf("\nRendimiento por contexto:\n");
    printf("----------------------------------------\n");

    // Rendimiento por clima
    query("Rendimiento por Clima",
          "SELECT clima, ROUND(AVG(rendimiento_general), 2), COUNT(*) FROM partido GROUP BY clima ORDER BY AVG(rendimiento_general) DESC");

    // Rendimiento por dia de semana
    query("Rendimiento por Dia de Semana",
          "SELECT CASE strftime('%w', fecha_hora) WHEN '0' THEN 'Domingo' WHEN '1' THEN 'Lunes' WHEN '2' THEN 'Martes' WHEN '3' THEN 'Miercoles' WHEN '4' THEN 'Jueves' WHEN '5' THEN 'Viernes' WHEN '6' THEN 'Sabado' ELSE 'Desconocido' END AS dia, ROUND(AVG(rendimiento_general), 2), COUNT(*) FROM partido GROUP BY dia ORDER BY AVG(rendimiento_general) DESC");

    // Rendimiento por resultado
    query("Rendimiento por Resultado",
          "SELECT CASE resultado WHEN 1 THEN 'Victoria' WHEN 2 THEN 'Empate' WHEN 3 THEN 'Derrota' ELSE 'Desconocido' END AS resultado, ROUND(AVG(rendimiento_general), 2), COUNT(*) FROM partido GROUP BY resultado ORDER BY AVG(rendimiento_general) DESC");

    pause_console();
}

/**
 * @brief Muestra el impacto real del cansancio
 *
 * Analiza la correlacion entre cansancio y rendimiento
 */
void mostrar_impacto_real_cansancio()
{
    clear_screen();
    print_header("IMPACTO REAL DEL CANSANCIO");

    // Correlacion entre cansancio y rendimiento
    query("Correlacion Cansancio-Rendimiento",
          "SELECT ROUND((COUNT(*) * SUM(cansancio * rendimiento_general) - SUM(cansancio) * SUM(rendimiento_general)) / "
          "(SQRT((COUNT(*) * SUM(cansancio * cansancio) - SUM(cansancio) * SUM(cansancio)) * "
          "(COUNT(*) * SUM(rendimiento_general * rendimiento_general) - SUM(rendimiento_general) * SUM(rendimiento_general)))), 4) "
          "FROM partido");

    // Rendimiento por nivel de cansancio
    query("Rendimiento por Nivel de Cansancio",
          "SELECT CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END AS nivel_cansancio, "
          "ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio, "
          "ROUND(AVG(goles), 2) AS goles_promedio, "
          "ROUND(AVG(asistencias), 2) AS asistencias_promedio, "
          "COUNT(*) AS partidos "
          "FROM partido GROUP BY CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END "
          "ORDER BY rendimiento_promedio DESC");

    // Impacto en resultados
    query("Resultados por Nivel de Cansancio",
          "SELECT CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END AS nivel_cansancio, "
          "SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END) AS victorias, "
          "SUM(CASE WHEN resultado = 2 THEN 1 ELSE 0 END) AS empates, "
          "SUM(CASE WHEN resultado = 3 THEN 1 ELSE 0 END) AS derrotas, "
          "COUNT(*) AS total "
          "FROM partido GROUP BY CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END");

    pause_console();
}

/**
 * @brief Muestra el impacto real del estado de animo
 *
 * Analiza la correlacion entre estado de animo y rendimiento
 */
void mostrar_impacto_real_estado_animo()
{
    clear_screen();
    print_header("IMPACTO REAL DEL ESTADO DE aNIMO");

    // Correlacion entre estado de animo y rendimiento
    query("Correlacion Estado de Animo-Rendimiento",
          "SELECT ROUND((COUNT(*) * SUM(estado_animo * rendimiento_general) - SUM(estado_animo) * SUM(rendimiento_general)) / "
          "(SQRT((COUNT(*) * SUM(estado_animo * estado_animo) - SUM(estado_animo) * SUM(estado_animo)) * "
          "(COUNT(*) * SUM(rendimiento_general * rendimiento_general) - SUM(rendimiento_general) * SUM(rendimiento_general)))), 4) "
          "FROM partido");

    // Rendimiento por nivel de estado de animo
    query("Rendimiento por Nivel de Estado de Animo",
          "SELECT CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END AS nivel_animo, "
          "ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio, "
          "ROUND(AVG(goles), 2) AS goles_promedio, "
          "ROUND(AVG(asistencias), 2) AS asistencias_promedio, "
          "COUNT(*) AS partidos "
          "FROM partido GROUP BY CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END "
          "ORDER BY rendimiento_promedio DESC");

    // Impacto en resultados
    query("Resultados por Nivel de Estado de Animo",
          "SELECT CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END AS nivel_animo, "
          "SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END) AS victorias, "
          "SUM(CASE WHEN resultado = 2 THEN 1 ELSE 0 END) AS empates, "
          "SUM(CASE WHEN resultado = 3 THEN 1 ELSE 0 END) AS derrotas, "
          "COUNT(*) AS total "
          "FROM partido GROUP BY CASE WHEN estado_animo <= 3 THEN 'Bajo (1-3)' WHEN estado_animo <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END");

    pause_console();
}

/**
 * @brief Muestra la eficiencia: goles por partido vs rendimiento
 *
 * Analiza la relacion entre produccion de goles y rendimiento general
 */
void mostrar_eficiencia_goles_vs_rendimiento()
{
    clear_screen();
    print_header("EFICIENCIA: GOLES POR PARTIDO VS RENDIMIENTO");

    // Correlacion entre goles y rendimiento
    query("Correlacion Goles-Rendimiento",
          "SELECT ROUND((COUNT(*) * SUM(goles * rendimiento_general) - SUM(goles) * SUM(rendimiento_general)) / "
          "(SQRT((COUNT(*) * SUM(goles * goles) - SUM(goles) * SUM(goles)) * "
          "(COUNT(*) * SUM(rendimiento_general * rendimiento_general) - SUM(rendimiento_general) * SUM(rendimiento_general)))), 4) "
          "FROM partido");

    // Eficiencia por rango de goles
    query("Eficiencia por Rango de Goles",
          "SELECT CASE WHEN goles = 0 THEN '0 goles' WHEN goles <= 2 THEN '1-2 goles' WHEN goles <= 4 THEN '3-4 goles' ELSE '5+ goles' END AS rango_goles, "
          "ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio, "
          "COUNT(*) AS partidos "
          "FROM partido GROUP BY CASE WHEN goles = 0 THEN '0 goles' WHEN goles <= 2 THEN '1-2 goles' WHEN goles <= 4 THEN '3-4 goles' ELSE '5+ goles' END "
          "ORDER BY rendimiento_promedio DESC");

    // Rendimiento por gol (eficiencia)
    query("Rendimiento por Gol (Eficiencia)",
          "SELECT ROUND(AVG(rendimiento_general) / NULLIF(AVG(goles), 0), 2) AS rendimiento_por_gol "
          "FROM partido WHERE goles > 0");

    pause_console();
}

/**
 * @brief Muestra la eficiencia: asistencias por partido vs cansancio
 *
 * Analiza como el cansancio afecta la capacidad de asistir
 */
void mostrar_eficiencia_asistencias_vs_cansancio()
{
    clear_screen();
    print_header("EFICIENCIA: ASISTENCIAS VS CANSANCIO");

    // Correlacion entre asistencias y cansancio
    query("Correlacion Asistencias-Cansancio",
          "SELECT ROUND((COUNT(*) * SUM(asistencias * cansancio) - SUM(asistencias) * SUM(cansancio)) / "
          "(SQRT((COUNT(*) * SUM(asistencias * asistencias) - SUM(asistencias) * SUM(asistencias)) * "
          "(COUNT(*) * SUM(cansancio * cansancio) - SUM(cansancio) * SUM(cansancio)))), 4) "
          "FROM partido");

    // Asistencias por nivel de cansancio
    query("Asistencias por Nivel de Cansancio",
          "SELECT CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END AS nivel_cansancio, "
          "ROUND(AVG(asistencias), 2) AS asistencias_promedio, "
          "ROUND(AVG(asistencias) / NULLIF(AVG(cansancio), 0), 2) AS asistencias_por_unidad_cansancio, "
          "COUNT(*) AS partidos "
          "FROM partido GROUP BY CASE WHEN cansancio <= 3 THEN 'Bajo (1-3)' WHEN cansancio <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END "
          "ORDER BY asistencias_promedio DESC");

    pause_console();
}

/**
 * @brief Muestra el rendimiento obtenido por esfuerzo
 *
 * Analiza la relacion entre esfuerzo (cansancio) y resultados
 */
void mostrar_rendimiento_por_esfuerzo()
{
    clear_screen();
    print_header("RENDIMIENTO OBTENIDO POR ESFUERZO");

    // Rendimiento por unidad de cansancio
    query("Rendimiento por Unidad de Cansancio",
          "SELECT ROUND(AVG(rendimiento_general) / NULLIF(AVG(cansancio), 0), 2) AS rendimiento_por_cansancio "
          "FROM partido WHERE cansancio > 0");

    // Eficiencia por nivel de esfuerzo
    query("Eficiencia por Nivel de Esfuerzo",
          "SELECT CASE WHEN cansancio <= 3 THEN 'Bajo esfuerzo (1-3)' WHEN cansancio <= 7 THEN 'Esfuerzo medio (4-7)' ELSE 'Alto esfuerzo (8-10)' END AS nivel_esfuerzo, "
          "ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio, "
          "ROUND(AVG(rendimiento_general) / NULLIF(AVG(cansancio), 0), 2) AS rendimiento_por_unidad_esfuerzo, "
          "COUNT(*) AS partidos "
          "FROM partido GROUP BY CASE WHEN cansancio <= 3 THEN 'Bajo esfuerzo (1-3)' WHEN cansancio <= 7 THEN 'Esfuerzo medio (4-7)' ELSE 'Alto esfuerzo (8-10)' END "
          "ORDER BY rendimiento_por_unidad_esfuerzo DESC");

    pause_console();
}

/**
 * @brief Muestra partidos exigentes bien rendidos
 *
 * Identifica partidos dificiles con buen rendimiento
 */
void mostrar_partidos_exigentes_bien_rendidos()
{
    mostrar_partidos_con_sql(
        "PARTIDOS EXIGENTES BIEN RENDIDOS",
        "Partidos con alto cansancio y buen rendimiento:",
        "SELECT id, fecha_hora, cansancio, rendimiento_general, goles, asistencias, resultado "
        "FROM partido "
        "WHERE cansancio > 7 AND rendimiento_general > (SELECT AVG(rendimiento_general) FROM partido) "
        "ORDER BY rendimiento_general DESC, cansancio DESC"
    );
}

/**
 * @brief Muestra partidos faciles mal rendidos
 *
 * Identifica partidos faciles con bajo rendimiento
 */
void mostrar_partidos_faciles_mal_rendidos()
{
    mostrar_partidos_con_sql(
        "PARTIDOS FACILES MAL RENDIDOS",
        "Partidos con bajo cansancio y bajo rendimiento:",
        "SELECT id, fecha_hora, cansancio, rendimiento_general, goles, asistencias, resultado "
        "FROM partido "
        "WHERE cansancio <= 3 AND rendimiento_general < (SELECT AVG(rendimiento_general) FROM partido) "
        "ORDER BY rendimiento_general ASC, cansancio ASC"
    );
}
