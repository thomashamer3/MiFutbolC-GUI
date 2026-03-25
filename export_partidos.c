#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include "export_partidos_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#else
#include "direct.h"
#endif
#include <string.h>

/* ===================== HELPER FUNCTIONS (STATIC) ===================== */

/**
 * Checks if there are any partido records in the database.
 * Returns 1 if records exist, 0 if no records found.
 * This avoids duplicating the count check in every export function.
 */
int has_partido_records()
{
    return check_partido_records();
}

/**
 * Executes the standard partido query and returns the statement.
 * This centralizes the common SQL query used by most export functions.
 */
sqlite3_stmt* execute_partido_query(const char* order_by_clause)
{
    return prepare_partido_query(order_by_clause);
}

/**
 * Writes partido data in CSV format to the given file.
 * Handles the common CSV formatting logic.
 */
static void write_partido_csv(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,Rendimiento_General,Cansancio,Estado_Animo,Comentario_Personal\n");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        write_partido_csv_row(f, stmt);
    }
}

/**
 * Writes partido data in TXT format to the given file.
 * Handles the common TXT formatting logic.
 */
static void write_partido_txt(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f, "PARTIDOS\n\n");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        write_partido_txt_row(f, stmt);
    }
}

/**
 * Writes partido data in JSON format to the given file.
 * Handles the common JSON formatting logic.
 */
static void write_partido_json(FILE *f, sqlite3_stmt *stmt)
{
    cJSON *root = cJSON_CreateArray();

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        cJSON *item = cJSON_CreateObject();
        write_partido_json_object(item, stmt);
        cJSON_AddItemToArray(root, item);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);
}

/**
 * Writes partido data in HTML format to the given file.
 * Handles the common HTML formatting logic.
 */
static void write_partido_html(FILE *f, sqlite3_stmt *stmt)
{
    fprintf(f,
            "<html><body><h1>PARTIDOS</h1><table border='1'>"
            "<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</th><th>Camiseta</th><th>Resultado</th><th>Clima</th><th>Dia</th><th>Rendimiento General</th><th>Cansancio</th><th>Estado Animo</th><th>Comentario Personal</th></tr>");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        write_partido_html_row(f, stmt);
    }

    fprintf(f, "</table></body></html>");
}

/* ===================== PARTIDOS ===================== */

/**
 * Export all partidos to CSV format.
 * Uses helper functions to keep the main function concise.
 */
void exportar_partidos_csv()
{
    export_partidos_generic("partidos.csv", write_partido_csv);
}

/**
 * Export all partidos to TXT format.
 * Uses helper functions to keep the main function concise.
 */
void exportar_partidos_txt()
{
    export_partidos_generic("partidos.txt", write_partido_txt);
}

/**
 * Export all partidos to JSON format.
 * Uses helper functions to keep the main function concise.
 */
void exportar_partidos_json()
{
    export_partidos_generic("partidos.json", write_partido_json);
}

/**
 * Export all partidos to HTML format.
 * Uses helper functions to keep the main function concise.
 */
void exportar_partidos_html()
{
    export_partidos_generic("partidos.html", write_partido_html);
}

/* ===================== PARTIDOS ESPECIFICOS ===================== */

/**
 * @brief Exporta el partido con mas goles a un archivo CSV
 */
void exportar_partido_mas_goles_csv()
{
    exportar_partido_especifico_csv("ORDER BY p.goles DESC, p.fecha_hora DESC LIMIT 1", "partido_mas_goles.csv");
}

/**
 * @brief Exporta el partido con mas goles a un archivo de texto plano
 */
void exportar_partido_mas_goles_txt()
{
    exportar_partido_especifico_txt("ORDER BY p.goles DESC, p.fecha_hora DESC LIMIT 1", "partido_mas_goles.txt", "PARTIDO CON MAS GOLES");
}

/**
 * @brief Exporta el partido con mas goles a un archivo JSON
 */
void exportar_partido_mas_goles_json()
{
    export_partido_especifico_generic("ORDER BY p.goles DESC, p.fecha_hora DESC LIMIT 1", "partido_mas_goles.json", write_partido_json);
}

/**
 * @brief Exporta el partido con mas goles a un archivo HTML
 */
void exportar_partido_mas_goles_html()
{
    export_partido_especifico_generic("ORDER BY p.goles DESC, p.fecha_hora DESC LIMIT 1", "partido_mas_goles.html", write_partido_html);
}

/**
 * @brief Exporta el partido con mas asistencias a un archivo CSV
 */
void exportar_partido_mas_asistencias_csv()
{
    exportar_partido_especifico_csv("ORDER BY p.asistencias DESC, p.fecha_hora DESC LIMIT 1", "partido_mas_asistencias.csv");
}

/**
 * @brief Exporta el partido con mas asistencias a un archivo de texto plano
 */
void exportar_partido_mas_asistencias_txt()
{
    exportar_partido_especifico_txt("ORDER BY p.asistencias DESC, p.fecha_hora DESC LIMIT 1", "partido_mas_asistencias.txt", "PARTIDO CON MAS ASISTENCIAS");
}

/**
 * @brief Exporta el partido con mas asistencias a un archivo JSON
 */
void exportar_partido_mas_asistencias_json()
{
    export_partido_especifico_generic("ORDER BY p.asistencias DESC, p.fecha_hora DESC LIMIT 1", "partido_mas_asistencias.json", write_partido_json);
}

/**
 * @brief Exporta el partido con mas asistencias a un archivo HTML
 */
void exportar_partido_mas_asistencias_html()
{
    export_partido_especifico_generic("ORDER BY p.asistencias DESC, p.fecha_hora DESC LIMIT 1", "partido_mas_asistencias.html", write_partido_html);
}

/**
 * @brief Exporta el partido mas reciente con menos goles a un archivo CSV
 */
void exportar_partido_menos_goles_reciente_csv()
{
    exportar_partido_especifico_csv("ORDER BY p.goles ASC, p.fecha_hora DESC LIMIT 1", "partido_menos_goles_reciente.csv");
}

/**
 * @brief Exporta el partido mas reciente con menos goles a un archivo de texto plano
 */
void exportar_partido_menos_goles_reciente_txt()
{
    exportar_partido_especifico_txt("ORDER BY p.goles ASC, p.fecha_hora DESC LIMIT 1", "partido_menos_goles_reciente.txt", "PARTIDO MAS RECIENTE CON MENOS GOLES");
}

/**
 * @brief Exporta el partido mas reciente con menos goles a un archivo JSON
 */
void exportar_partido_menos_goles_reciente_json()
{
    export_partido_especifico_generic("ORDER BY p.goles ASC, p.fecha_hora DESC LIMIT 1", "partido_menos_goles_reciente.json", write_partido_json);
}

/**
 * @brief Exporta el partido mas reciente con menos goles a un archivo HTML
 */
void exportar_partido_menos_goles_reciente_html()
{
    export_partido_especifico_generic("ORDER BY p.goles ASC, p.fecha_hora DESC LIMIT 1", "partido_menos_goles_reciente.html", write_partido_html);
}

/**
 * @brief Exporta el partido mas reciente con menos asistencias a un archivo CSV
 */
void exportar_partido_menos_asistencias_reciente_csv()
{
    exportar_partido_especifico_csv("ORDER BY p.asistencias ASC, p.fecha_hora DESC LIMIT 1", "partido_menos_asistencias_reciente.csv");
}

/**
 * @brief Exporta el partido mas reciente con menos asistencias a un archivo de texto plano
 */
void exportar_partido_menos_asistencias_reciente_txt()
{
    exportar_partido_especifico_txt("ORDER BY p.asistencias ASC, p.fecha_hora DESC LIMIT 1", "partido_menos_asistencias_reciente.txt", "PARTIDO MAS RECIENTE CON MENOS ASISTENCIAS");
}

/**
 * @brief Exporta el partido mas reciente con menos asistencias a un archivo JSON
 */
void exportar_partido_menos_asistencias_reciente_json()
{
    export_partido_especifico_generic("ORDER BY p.asistencias ASC, p.fecha_hora DESC LIMIT 1", "partido_menos_asistencias_reciente.json", write_partido_json);
}

/**
 * @brief Exporta el partido mas reciente con menos asistencias a un archivo HTML
 */
void exportar_partido_menos_asistencias_reciente_html()
{
    export_partido_especifico_generic("ORDER BY p.asistencias ASC, p.fecha_hora DESC LIMIT 1", "partido_menos_asistencias_reciente.html", write_partido_html);
}