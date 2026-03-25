/**
 * @file export_records_rankings.c
 * @brief Implementacion de exportacion de records y rankings en MiFutbolC
 */

#include "export_records_rankings.h"
#include "db.h"
#include "utils.h"
#include "export.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static sqlite3_stmt *execute_records_query(const char *sql);
static int get_record_data(sqlite3_stmt *stmt, int *valor, const char **camiseta, const char **fecha);
static int get_combinacion_data(sqlite3_stmt *stmt, const char **cancha, const char **camiseta, double *rendimiento, int *partidos);
static int get_temporada_data(sqlite3_stmt *stmt, const char **anio, double *rendimiento, int *partidos);

static void write_record_json(FILE *file, const char *key, const char *sql)
{
    sqlite3_stmt *stmt = execute_records_query(sql);
    int valor;
    const char *camiseta;
    const char *fecha;

    fprintf(file, "    \"%s\": ", key);
    if (stmt && get_record_data(stmt, &valor, &camiseta, &fecha))
    {
        fprintf(file, "{\"valor\": %d, \"camiseta\": \"%s\", \"fecha\": \"%s\"}", valor, camiseta, fecha);
    }
    else
    {
        fprintf(file, "null");
    }
    if (stmt)
        sqlite3_finalize(stmt);
}

static void write_combinacion_json(FILE *file, const char *key, const char *sql)
{
    sqlite3_stmt *stmt = execute_records_query(sql);
    const char *cancha;
    const char *camiseta;
    double rendimiento;
    int partidos;

    fprintf(file, "    \"%s\": ", key);
    if (stmt && get_combinacion_data(stmt, &cancha, &camiseta, &rendimiento, &partidos))
    {
        fprintf(file, "{\"cancha\": \"%s\", \"camiseta\": \"%s\", \"rendimiento_promedio\": %.2f, \"partidos\": %d}",
                cancha, camiseta, rendimiento, partidos);
    }
    else
    {
        fprintf(file, "null");
    }
    if (stmt)
        sqlite3_finalize(stmt);
}

static void write_temporada_json(FILE *file, const char *key, const char *sql)
{
    sqlite3_stmt *stmt = execute_records_query(sql);
    const char *anio;
    double rendimiento;
    int partidos;

    fprintf(file, "    \"%s\": ", key);
    if (stmt && get_temporada_data(stmt, &anio, &rendimiento, &partidos))
    {
        fprintf(file, "{\"anio\": \"%s\", \"rendimiento_promedio\": %.2f, \"partidos\": %d}",
                anio, rendimiento, partidos);
    }
    else
    {
        fprintf(file, "null");
    }
    if (stmt)
        sqlite3_finalize(stmt);
}

static void write_record_section_html(FILE *file, const char *title, const char *sql)
{
    fprintf(file, "<h2>%s</h2>\n", title);
    sqlite3_stmt *stmt = execute_records_query(sql);
    int valor;
    const char *camiseta;
    const char *fecha;

    if (stmt && get_record_data(stmt, &valor, &camiseta, &fecha))
    {
        fprintf(file, "<p><strong>%d</strong> (Camiseta: %s, Fecha: %s)</p>\n", valor, camiseta, fecha);
    }
    else
    {
        fprintf(file, "<p>No hay registros disponibles</p>\n");
    }
    if (stmt)
        sqlite3_finalize(stmt);
}

static void write_combinacion_section_html(FILE *file, const char *title, const char *sql)
{
    fprintf(file, "<h2>%s</h2>\n", title);
    sqlite3_stmt *stmt = execute_records_query(sql);
    const char *cancha;
    const char *camiseta;
    double rendimiento;
    int partidos;

    if (stmt && get_combinacion_data(stmt, &cancha, &camiseta, &rendimiento, &partidos))
    {
        fprintf(file,
                "<p>Cancha: <strong>%s</strong>, Camiseta: <strong>%s</strong>, Rendimiento Promedio: <strong>%.2f</strong>, Partidos: <strong>%d</strong></p>\n",
                cancha, camiseta, rendimiento, partidos);
    }
    else
    {
        fprintf(file, "<p>No hay registros disponibles</p>\n");
    }
    if (stmt)
        sqlite3_finalize(stmt);
}

static void write_temporada_section_html(FILE *file, const char *title, const char *sql)
{
    fprintf(file, "<h2>%s</h2>\n", title);
    sqlite3_stmt *stmt = execute_records_query(sql);
    const char *anio;
    double rendimiento;
    int partidos;

    if (stmt && get_temporada_data(stmt, &anio, &rendimiento, &partidos))
    {
        fprintf(file, "<p>Anio: <strong>%s</strong>, Rendimiento Promedio: <strong>%.2f</strong>, Partidos: <strong>%d</strong></p>\n",
                anio, rendimiento, partidos);
    }
    else
    {
        fprintf(file, "<p>No hay registros disponibles</p>\n");
    }
    if (stmt)
        sqlite3_finalize(stmt);
}

/**
 * Ejecuta una consulta SQL y devuelve el statement.
 * Centraliza la ejecucion de consultas para evitar duplicacion de codigo.
 */
static sqlite3_stmt* execute_records_query(const char* sql)
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return NULL;
    }
    return stmt;
}

/**
 * Funcion auxiliar para exportar records a CSV.
 * Centraliza la logica comun de exportacion CSV para records individuales.
 */
static void exportar_record_csv(const char *titulo, const char *sql, const char *filepath)
{
    exportar_record_simple_csv(titulo, sql, filepath);
}

/**
 * Funcion auxiliar para exportar combinaciones a CSV.
 * Centraliza la logica comun de exportacion CSV para combinaciones cancha-camiseta.
 */
static void exportar_combinacion_csv(const char *titulo, const char *sql, const char *filepath)
{
    sqlite3_stmt *stmt;
    FILE *file;
    errno_t err = fopen_s(&file, filepath, "w");

    if (err != 0 || !file)
    {
        printf("Error al crear el archivo\n");
        return;
    }

    fprintf(file, "%s\n", titulo);
    fprintf(file, "Cancha,Camiseta,Rendimiento_Promedio,Partidos_Jugados\n");

    stmt = execute_records_query(sql);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%s,%s,%.2f,%d\n",
                sqlite3_column_text(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_double(stmt, 2),
                sqlite3_column_int(stmt, 3));
    }

    if (stmt) sqlite3_finalize(stmt);
    fclose(file);
    printf("Exportado a %s\n", filepath);
}

/**
 * Funcion auxiliar para exportar temporadas a CSV.
 * Centraliza la logica comun de exportacion CSV para temporadas.
 */
static void exportar_temporada_csv(const char *titulo, const char *sql, const char *filepath)
{
    sqlite3_stmt *stmt;
    FILE *file;
    errno_t err = fopen_s(&file, filepath, "w");

    if (err != 0 || !file)
    {
        printf("Error al crear el archivo\n");
        return;
    }

    fprintf(file, "%s\n", titulo);
    fprintf(file, "Anio,Rendimiento_Promedio,Partidos_Jugados\n");

    stmt = execute_records_query(sql);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%d,%.2f,%d\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_double(stmt, 1),
                sqlite3_column_int(stmt, 2));
    }

    if (stmt) sqlite3_finalize(stmt);
    fclose(file);
    printf("Exportado a %s\n", filepath);
}

/**
 * Funcion auxiliar para obtener datos de record.
 * Centraliza la logica de consulta y formateo para records individuales.
 * Retorna 1 si encontro datos, 0 si no encontro.
 */
static int get_record_data(sqlite3_stmt *stmt, int *valor, const char **camiseta, const char **fecha)
{
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        *valor = sqlite3_column_int(stmt, 0);
        *camiseta = (const char *)sqlite3_column_text(stmt, 1);
        *fecha = (const char *)sqlite3_column_text(stmt, 2);
        return 1;
    }
    return 0;
}

/**
 * Funcion auxiliar para obtener datos de combinacion.
 * Centraliza la logica de consulta y formateo para combinaciones cancha-camiseta.
 * Retorna 1 si encontro datos, 0 si no encontro.
 */
static int get_combinacion_data(sqlite3_stmt *stmt, const char **cancha, const char **camiseta, double *rendimiento, int *partidos)
{
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        *cancha = (const char *)sqlite3_column_text(stmt, 0);
        *camiseta = (const char *)sqlite3_column_text(stmt, 1);
        *rendimiento = sqlite3_column_double(stmt, 2);
        *partidos = sqlite3_column_int(stmt, 3);
        return 1;
    }
    return 0;
}

/**
 * Funcion auxiliar para obtener datos de temporada.
 * Centraliza la logica de consulta y formateo para temporadas.
 * Retorna 1 si encontro datos, 0 si no encontro.
 */
static int get_temporada_data(sqlite3_stmt *stmt, const char **anio, double *rendimiento, int *partidos)
{
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        *anio = (const char *)sqlite3_column_text(stmt, 0);
        *rendimiento = sqlite3_column_double(stmt, 1);
        *partidos = sqlite3_column_int(stmt, 2);
        return 1;
    }
    return 0;
}

/**
 * @brief Exporta el record de goles en un partido a CSV
 */
void exportar_record_goles_partido_csv()
{
    exportar_record_csv("Record de Goles en un Partido",
                        "SELECT p.goles, c.nombre, p.fecha_hora "
                        "FROM partido p "
                        "JOIN camiseta c ON p.camiseta_id = c.id "
                        "ORDER BY p.goles DESC LIMIT 1",
                        get_export_path("record_goles_partido.csv"));
}

/**
 * @brief Exporta el record de asistencias en un partido a CSV
 */
void exportar_record_asistencias_partido_csv()
{
    exportar_record_csv("Record de Asistencias en un Partido",
                        "SELECT p.asistencias, c.nombre, p.fecha_hora "
                        "FROM partido p "
                        "JOIN camiseta c ON p.camiseta_id = c.id "
                        "ORDER BY p.asistencias DESC LIMIT 1",
                        get_export_path("record_asistencias_partido.csv"));
}

/**
 * @brief Exporta la mejor combinacion cancha + camiseta a CSV
 */
void exportar_mejor_combinacion_cancha_camiseta_csv()
{
    exportar_combinacion_csv("Mejor Combinacion Cancha + Camiseta",
                             "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) "
                             "FROM partido p "
                             "JOIN cancha ca ON p.cancha_id = ca.id "
                             "JOIN camiseta c ON p.camiseta_id = c.id "
                             "GROUP BY p.cancha_id, p.camiseta_id "
                             "ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1",
                             get_export_path("mejor_combinacion_cancha_camiseta.csv"));
}

/**
 * @brief Exporta la peor combinacion cancha + camiseta a CSV
 */
void exportar_peor_combinacion_cancha_camiseta_csv()
{
    exportar_combinacion_csv("Peor Combinacion Cancha + Camiseta",
                             "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) "
                             "FROM partido p "
                             "JOIN cancha ca ON p.cancha_id = ca.id "
                             "JOIN camiseta c ON p.camiseta_id = c.id "
                             "GROUP BY p.cancha_id, p.camiseta_id "
                             "ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1",
                             get_export_path("peor_combinacion_cancha_camiseta.csv"));
}

/**
 * @brief Exporta la mejor temporada a CSV
 */
void exportar_mejor_temporada_csv()
{
    exportar_temporada_csv("Mejor Temporada",
                           "SELECT substr(p.fecha_hora, 7, 4), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) "
                           "FROM partido p "
                           "WHERE p.fecha_hora IS NOT NULL AND p.fecha_hora != '' "
                           "GROUP BY substr(p.fecha_hora, 7, 4) "
                           "ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1",
                           get_export_path("mejor_temporada.csv"));
}

/**
 * @brief Exporta la peor temporada a CSV
 */
void exportar_peor_temporada_csv()
{
    exportar_temporada_csv("Peor Temporada",
                           "SELECT substr(p.fecha_hora, 7, 4), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) "
                           "FROM partido p "
                           "WHERE p.fecha_hora IS NOT NULL AND p.fecha_hora != '' "
                           "GROUP BY substr(p.fecha_hora, 7, 4) "
                           "ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1",
                           get_export_path("peor_temporada.csv"));
}

/**
 * Exporta records y rankings a TXT.
 * Usa funciones auxiliares para mantener el codigo conciso y dentro del limite de lineas.
 */
void exportar_records_rankings_txt()
{
    FILE *file = abrir_archivo_exportacion("records_rankings.txt", "Error al crear el archivo");
    if (!file)
        return;

    fprintf(file, "RECORDS & RANKINGS\n");
    fprintf(file, "==================\n\n");

    sqlite3_stmt *stmt;
    int valor;
    const char *camiseta;
    const char *fecha;
    const char *cancha;
    double rendimiento;
    int partidos;
    const char *anio;

    // Record de goles
    stmt = execute_records_query("SELECT p.goles, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.goles DESC LIMIT 1");
    if (stmt && get_record_data(stmt, &valor, &camiseta, &fecha))
    {
        fprintf(file, "Record de Goles en un Partido: %d (Camiseta: %s, Fecha: %s)\n", valor, camiseta, fecha);
    }
    if (stmt) sqlite3_finalize(stmt);

    // Record de asistencias
    stmt = execute_records_query("SELECT p.asistencias, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.asistencias DESC LIMIT 1");
    if (stmt && get_record_data(stmt, &valor, &camiseta, &fecha))
    {
        fprintf(file, "Record de Asistencias en un Partido: %d (Camiseta: %s, Fecha: %s)\n", valor, camiseta, fecha);
    }
    if (stmt) sqlite3_finalize(stmt);

    // Mejor combinacion cancha + camiseta
    stmt = execute_records_query("SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1");
    if (stmt && get_combinacion_data(stmt, &cancha, &camiseta, &rendimiento, &partidos))
    {
        fprintf(file, "Mejor Combinacion Cancha + Camiseta: Cancha: %s, Camiseta: %s, Rendimiento Promedio: %.2f, Partidos: %d\n", cancha, camiseta, rendimiento, partidos);
    }
    if (stmt) sqlite3_finalize(stmt);

    // Peor combinacion cancha + camiseta
    stmt = execute_records_query("SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1");
    if (stmt && get_combinacion_data(stmt, &cancha, &camiseta, &rendimiento, &partidos))
    {
        fprintf(file, "Peor Combinacion Cancha + Camiseta: Cancha: %s, Camiseta: %s, Rendimiento Promedio: %.2f, Partidos: %d\n", cancha, camiseta, rendimiento, partidos);
    }
    if (stmt) sqlite3_finalize(stmt);

    // Mejor temporada
    stmt = execute_records_query("SELECT substr(p.fecha_hora, 7, 4), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p WHERE p.fecha_hora IS NOT NULL AND p.fecha_hora != '' GROUP BY substr(p.fecha_hora, 7, 4) ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1");
    if (stmt && get_temporada_data(stmt, &anio, &rendimiento, &partidos))
    {
        fprintf(file, "Mejor Temporada: Anio: %s, Rendimiento Promedio: %.2f, Partidos: %d\n", anio, rendimiento, partidos);
    }
    if (stmt) sqlite3_finalize(stmt);

    // Peor temporada
    stmt = execute_records_query("SELECT substr(p.fecha_hora, 7, 4), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p WHERE p.fecha_hora IS NOT NULL AND p.fecha_hora != '' GROUP BY substr(p.fecha_hora, 7, 4) ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1");
    if (stmt && get_temporada_data(stmt, &anio, &rendimiento, &partidos))
    {
        fprintf(file, "Peor Temporada: Anio: %s, Rendimiento Promedio: %.2f, Partidos: %d\n", anio, rendimiento, partidos);
    }
    if (stmt) sqlite3_finalize(stmt);

    fclose(file);
    printf("Exportado a %s\n", get_export_path("records_rankings.txt"));
}

/**
 * Exporta records y rankings a JSON.
 * Usa funciones auxiliares para mantener el codigo conciso y dentro del limite de lineas.
 */
void exportar_records_rankings_json()
{
    FILE *file = abrir_archivo_exportacion("records_rankings.json", "Error al crear el archivo");
    if (!file)
        return;

    fprintf(file, "{\n");
    fprintf(file, "  \"records_rankings\": {\n");

    // Write all records
    write_record_json(file, "record_goles", "SELECT p.goles, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.goles DESC LIMIT 1");
    fprintf(file, ",\n");
    write_record_json(file, "record_asistencias", "SELECT p.asistencias, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.asistencias DESC LIMIT 1");
    fprintf(file, ",\n");
    write_combinacion_json(file, "mejor_combinacion", "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1");
    fprintf(file, ",\n");
    write_combinacion_json(file, "peor_combinacion", "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1");
    fprintf(file, ",\n");
    write_temporada_json(file, "mejor_temporada", "SELECT substr(p.fecha_hora, 7, 4), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p WHERE p.fecha_hora IS NOT NULL AND p.fecha_hora != '' GROUP BY substr(p.fecha_hora, 7, 4) ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1");
    fprintf(file, ",\n");
    write_temporada_json(file, "peor_temporada", "SELECT substr(p.fecha_hora, 7, 4), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p WHERE p.fecha_hora IS NOT NULL AND p.fecha_hora != '' GROUP BY substr(p.fecha_hora, 7, 4) ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1");

    fprintf(file, "\n  }\n");
    fprintf(file, "}\n");

    fclose(file);
    printf("Exportado a %s\n", get_export_path("records_rankings.json"));
}

/**
 * Exporta records y rankings a HTML.
 * Usa funciones auxiliares para mantener el codigo conciso y dentro del limite de lineas.
 */
void exportar_records_rankings_html()
{
    FILE *file = abrir_archivo_exportacion("records_rankings.html", "Error al crear el archivo");
    if (!file)
        return;

    fprintf(file, "<!DOCTYPE html>\n");
    fprintf(file, "<html>\n");
    fprintf(file, "<head><title>Records & Rankings</title></head>\n");
    fprintf(file, "<body>\n");
    fprintf(file, "<h1>RECORDS & RANKINGS</h1>\n");

    // Write all sections
    write_record_section_html(file, "Record de Goles en un Partido", "SELECT p.goles, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.goles DESC LIMIT 1");
    write_record_section_html(file, "Record de Asistencias en un Partido", "SELECT p.asistencias, c.nombre, p.fecha_hora FROM partido p JOIN camiseta c ON p.camiseta_id = c.id ORDER BY p.asistencias DESC LIMIT 1");
    write_combinacion_section_html(file, "Mejor Combinacion Cancha + Camiseta", "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1");
    write_combinacion_section_html(file, "Peor Combinacion Cancha + Camiseta", "SELECT ca.nombre, c.nombre, ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p JOIN cancha ca ON p.cancha_id = ca.id JOIN camiseta c ON p.camiseta_id = c.id GROUP BY p.cancha_id, p.camiseta_id ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1");
    write_temporada_section_html(file, "Mejor Temporada", "SELECT substr(p.fecha_hora, 7, 4), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p WHERE p.fecha_hora IS NOT NULL AND p.fecha_hora != '' GROUP BY substr(p.fecha_hora, 7, 4) ORDER BY AVG(p.rendimiento_general) DESC LIMIT 1");
    write_temporada_section_html(file, "Peor Temporada", "SELECT substr(p.fecha_hora, 7, 4), ROUND(AVG(p.rendimiento_general), 2), COUNT(*) FROM partido p WHERE p.fecha_hora IS NOT NULL AND p.fecha_hora != '' GROUP BY substr(p.fecha_hora, 7, 4) ORDER BY AVG(p.rendimiento_general) ASC LIMIT 1");

    fprintf(file, "</body>\n");
    fprintf(file, "</html>\n");

    fclose(file);
    printf("Exportado a %s\n", get_export_path("records_rankings.html"));
}
