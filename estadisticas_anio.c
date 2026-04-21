/**
 * @file estadisticas_anio.c
 * @brief Modulo de analisis estadistico temporal anual
 *
 * Implementa consultas SQL para agregacion de metricas deportivas por ano,
 * permitiendo evaluacion de rendimiento longitudinal y tendencias historicas.
 */

#include "estadisticas_anio.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

#include "estadisticas_gui_capture.h"

/**
 * @brief Visualiza evolucion anual del rendimiento deportivo
 *
 * Agrega metricas por ano para identificar patrones estacionales,
 * comparar rendimiento interanual y evaluar progreso a largo plazo.
 */
void mostrar_estadisticas_por_anio()
{
    clear_screen();
    print_header("ESTADISTICAS POR ANIO");
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt,
                              "SELECT substr(fecha_hora, 7, 4) AS anio, c.nombre, COUNT(*) AS partidos, SUM(goles) AS total_goles, SUM(asistencias) AS total_asistencias, ROUND(AVG(goles), 2) AS avg_goles, ROUND(AVG(asistencias), 2) AS avg_asistencias "
                              "FROM partido p "
                              "JOIN camiseta c ON p.camiseta_id = c.id "
                              "GROUP BY anio, c.id "
                              "ORDER BY anio DESC, total_goles DESC"))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    char current_anio[5] = "";
    int hay = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        EstadisticaAnio stats;
        extraer_estadistica_anio(stmt, &stats);

        if (strcmp(current_anio, stats.anio) != 0)
        {
            if (hay) printf("\n");
            printf("Anio: %s\n", stats.anio);
            printf("----------------------------------------\n");
            strcpy_s(current_anio, sizeof(current_anio), stats.anio);
        }

        printf("%-30s | PJ: %d | G: %d | A: %d | G/P: %.2f | A/P: %.2f\n",
               stats.camiseta, stats.partidos, stats.total_goles, stats.total_asistencias, stats.avg_goles, stats.avg_asistencias);
        hay = 1;
    }

    if (!hay)
        printf("No hay estadisticas registradas.\n");

    sqlite3_finalize(stmt);
    pause_console();
}
