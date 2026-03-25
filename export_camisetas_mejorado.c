/**
 * @file export_camisetas_mejorado.c
 * @brief Funciones mejoradas para exportar datos de camisetas con analisis avanzado
 *
 * Este archivo contiene funciones mejoradas para exportar datos de camisetas
 * con estadisticas avanzadas, eficiencia y analisis de rendimiento.
 */

#include "export.h"
#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#else
#include "direct.h"
#endif
#include <string.h>

/**
 * @brief Estructura para almacenar datos de camiseta con analisis avanzado
 *
 * Esta estructura centraliza los datos de camiseta con analisis avanzado que se utilizan
 * en todas las funciones de exportacion, evitando la duplicacion de codigo y facilitando
 * el mantenimiento.
 */
typedef struct
{
    int id;
    const char *nombre;
    int total_goles;
    int total_asistencias;
    int total_partidos;
    int victorias;
    int empates;
    int derrotas;
    int total_lesiones;
    double rendimiento_promedio;
    double cansancio_promedio;
    double estado_animo_promedio;
    double eficiencia_goles_por_partido;
    double eficiencia_asistencias_por_partido;
    double relacion_goles_asistencias;
    double porcentaje_victorias;
    double porcentaje_lesiones_por_partido;
} CamisetaDataMejorado;

static CamisetaDataMejorado leer_camiseta_mejorada(sqlite3_stmt *stmt)
{
    CamisetaDataMejorado data;
    data.id = sqlite3_column_int(stmt, 0);
    data.nombre = (const char *)sqlite3_column_text(stmt, 1);
    data.total_goles = sqlite3_column_int(stmt, 2);
    data.total_asistencias = sqlite3_column_int(stmt, 3);
    data.total_partidos = sqlite3_column_int(stmt, 4);
    data.victorias = sqlite3_column_int(stmt, 5);
    data.empates = sqlite3_column_int(stmt, 6);
    data.derrotas = sqlite3_column_int(stmt, 7);
    data.total_lesiones = sqlite3_column_int(stmt, 8);
    data.rendimiento_promedio = sqlite3_column_double(stmt, 9);
    data.cansancio_promedio = sqlite3_column_double(stmt, 10);
    data.estado_animo_promedio = sqlite3_column_double(stmt, 11);
    data.eficiencia_goles_por_partido = sqlite3_column_double(stmt, 12);
    data.eficiencia_asistencias_por_partido = sqlite3_column_double(stmt, 13);
    data.relacion_goles_asistencias = sqlite3_column_double(stmt, 14);
    data.porcentaje_victorias = sqlite3_column_double(stmt, 15);
    data.porcentaje_lesiones_por_partido = sqlite3_column_double(stmt, 16);
    return data;
}

/**
 * @brief Obtiene los datos de camisetas con analisis avanzado de la base de datos
 *
 * Funcion estatica que encapsula la consulta SQL comun utilizada por todas
 * las funciones de exportacion mejorada. Esto evita la duplicacion de codigo y centraliza
 * la logica de acceso a datos con analisis avanzado.
 *
 * @param[out] count Puntero a entero para almacenar el numero de camisetas encontradas
 * @return sqlite3_stmt* Statement preparado para iterar sobre los resultados
 */
static sqlite3_stmt* obtener_datos_camisetas_mejorado(int *count)
{
    sqlite3_stmt *stmt;

    if (!preparar_consulta_con_verificacion(&stmt, "camiseta", "camisetas para exportar",
                                            "SELECT c.id, c.nombre, "
                                            "COALESCE(SUM(p.goles), 0) as total_goles, "
                                            "COALESCE(SUM(p.asistencias), 0) as total_asistencias, "
                                            "COUNT(p.id) as total_partidos, "
                                            "COUNT(CASE WHEN p.resultado = 1 THEN 1 END) as victorias, "
                                            "COUNT(CASE WHEN p.resultado = 2 THEN 1 END) as empates, "
                                            "COUNT(CASE WHEN p.resultado = 3 THEN 1 END) as derrotas, "
                                            "COALESCE((SELECT COUNT(*) FROM lesion l WHERE l.camiseta_id = c.id), 0) as total_lesiones, "
                                            "COALESCE(AVG(p.rendimiento_general), 0) as rendimiento_promedio, "
                                            "COALESCE(AVG(p.cansancio), 0) as cansancio_promedio, "
                                            "COALESCE(AVG(p.estado_animo), 0) as estado_animo_promedio, "
                                            "CASE WHEN COUNT(p.id) > 0 THEN COALESCE(SUM(p.goles), 0) * 1.0 / COUNT(p.id) ELSE 0 END as eficiencia_goles_por_partido, "
                                            "CASE WHEN COUNT(p.id) > 0 THEN COALESCE(SUM(p.asistencias), 0) * 1.0 / COUNT(p.id) ELSE 0 END as eficiencia_asistencias_por_partido, "
                                            "CASE WHEN COALESCE(SUM(p.asistencias), 0) > 0 THEN COALESCE(SUM(p.goles), 0) * 1.0 / COALESCE(SUM(p.asistencias), 0) ELSE 0 END as relacion_goles_asistencias, "
                                            "CASE WHEN COUNT(p.id) > 0 THEN COUNT(CASE WHEN p.resultado = 1 THEN 1 END) * 100.0 / COUNT(p.id) ELSE 0 END as porcentaje_victorias, "
                                            "CASE WHEN COUNT(p.id) > 0 THEN COALESCE((SELECT COUNT(*) FROM lesion l WHERE l.camiseta_id = c.id), 0) * 100.0 / COUNT(p.id) ELSE 0 END as porcentaje_lesiones_por_partido "
                                            "FROM camiseta c "
                                            "LEFT JOIN partido p ON c.id = p.camiseta_id "
                                            "GROUP BY c.id, c.nombre "
                                            "ORDER BY c.id",
                                            count))
    {
        return NULL;
    }

    return stmt;
}

/**
 * @brief Exporta las camisetas con analisis avanzado a un archivo CSV mejorado
 *
 * Utiliza la funcion comun de obtencion de datos para evitar duplicacion de codigo.
 * El formato CSV mejorado es ideal para analisis avanzado en herramientas como Excel,
 * proporcionando metricas de eficiencia y rendimiento que no estan disponibles en la exportacion estandar.
 */
void exportar_camisetas_csv_mejorado()
{
    int count;
    sqlite3_stmt *stmt = obtener_datos_camisetas_mejorado(&count);
    if (!stmt) return;

    FILE *f = abrir_archivo_exportacion("camisetas_mejorado.csv", "Error al crear archivo de camisetas mejorado CSV");
    if (!f)
    {
        sqlite3_finalize(stmt);
        return;
    }

    // Escribir encabezado CSV con metricas avanzadas
    fprintf(f, "id,nombre,total_goles,total_asistencias,total_partidos,victorias,empates,derrotas,total_lesiones,rendimiento_promedio,cansancio_promedio,estado_animo_promedio,eficiencia_goles_por_partido,eficiencia_asistencias_por_partido,relacion_goles_asistencias,porcentaje_victorias,porcentaje_lesiones_por_partido\n");

    // Procesar cada fila de resultados con analisis avanzado
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        CamisetaDataMejorado data = leer_camiseta_mejorada(stmt);
        fprintf(f, "%d,%s,%d,%d,%d,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                data.id,
                data.nombre,
                data.total_goles,
                data.total_asistencias,
                data.total_partidos,
                data.victorias,
                data.empates,
                data.derrotas,
                data.total_lesiones,
                data.rendimiento_promedio,
                data.cansancio_promedio,
                data.estado_animo_promedio,
                data.eficiencia_goles_por_partido,
                data.eficiencia_asistencias_por_partido,
                data.relacion_goles_asistencias,
                data.porcentaje_victorias,
                data.porcentaje_lesiones_por_partido);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("camisetas_mejorado.csv"));
    fclose(f);
}

/**
 * @brief Exporta las camisetas con analisis avanzado a un archivo TXT mejorado
 *
 * Utiliza la funcion comun de obtencion de datos para evitar duplicacion de codigo.
 * El formato TXT mejorado es ideal para documentacion legible con analisis avanzado,
 * proporcionando informacion detallada sobre eficiencia y rendimiento para informes tecnicos.
 */
void exportar_camisetas_txt_mejorado()
{
    int count;
    sqlite3_stmt *stmt = obtener_datos_camisetas_mejorado(&count);
    if (!stmt) return;

    FILE *f = abrir_archivo_exportacion("camisetas_mejorado.txt", "Error al crear archivo de camisetas mejorado TXT");
    if (!f)
    {
        sqlite3_finalize(stmt);
        return;
    }

    // Escribir encabezado del archivo de texto con analisis avanzado
    fprintf(f, "LISTADO DE CAMISETAS CON ESTADISTICAS AVANZADAS\n\n");

    // Procesar cada fila de resultados con formato legible y metricas avanzadas
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        CamisetaDataMejorado data = leer_camiseta_mejorada(stmt);
        fprintf(f, "ID: %d - Nombre: %s\n"
                "  Goles Totales: %d\n"
                "  Asistencias Totales: %d\n"
                "  Partidos Totales: %d\n"
                "  Victorias: %d (%.2f%%)\n"
                "  Empates: %d\n"
                "  Derrotas: %d\n"
                "  Lesiones Totales: %d (%.2f%% por partido)\n"
                "  Rendimiento Promedio: %.2f\n"
                "  Cansancio Promedio: %.2f\n"
                "  Estado de Animo Promedio: %.2f\n"
                "  Eficiencia: %.2f goles/partido, %.2f asistencias/partido\n"
                "  Relacion Goles/Asistencias: %.2f\n\n",
                data.id,
                data.nombre,
                data.total_goles,
                data.total_asistencias,
                data.total_partidos,
                data.victorias,
                data.porcentaje_victorias,
                data.empates,
                data.derrotas,
                data.total_lesiones,
                data.porcentaje_lesiones_por_partido,
                data.rendimiento_promedio,
                data.cansancio_promedio,
                data.estado_animo_promedio,
                data.eficiencia_goles_por_partido,
                data.eficiencia_asistencias_por_partido,
                data.relacion_goles_asistencias);
    }

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("camisetas_mejorado.txt"));
    fclose(f);
}

/**
 * @brief Exporta las camisetas con analisis avanzado a un archivo JSON mejorado
 *
 * Utiliza la funcion comun de obtencion de datos para evitar duplicacion de codigo.
 * El formato JSON mejorado es ideal para integracion con aplicaciones y APIs,
 * proporcionando datos estructurados con analisis avanzado para procesamiento automatizado.
 */
void exportar_camisetas_json_mejorado()
{
    int count;
    sqlite3_stmt *stmt = obtener_datos_camisetas_mejorado(&count);
    if (!stmt) return;

    FILE *f = abrir_archivo_exportacion("camisetas_mejorado.json", "Error al crear archivo de camisetas mejorado JSON");
    if (!f)
    {
        sqlite3_finalize(stmt);
        return;
    }

    // Crear estructura JSON y procesar resultados con metricas avanzadas
    cJSON *root = cJSON_CreateArray();

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        CamisetaDataMejorado data = leer_camiseta_mejorada(stmt);
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", data.id);
        cJSON_AddStringToObject(item, "nombre", data.nombre);
        cJSON_AddNumberToObject(item, "total_goles", data.total_goles);
        cJSON_AddNumberToObject(item, "total_asistencias", data.total_asistencias);
        cJSON_AddNumberToObject(item, "total_partidos", data.total_partidos);
        cJSON_AddNumberToObject(item, "victorias", data.victorias);
        cJSON_AddNumberToObject(item, "empates", data.empates);
        cJSON_AddNumberToObject(item, "derrotas", data.derrotas);
        cJSON_AddNumberToObject(item, "total_lesiones", data.total_lesiones);
        cJSON_AddNumberToObject(item, "rendimiento_promedio", data.rendimiento_promedio);
        cJSON_AddNumberToObject(item, "cansancio_promedio", data.cansancio_promedio);
        cJSON_AddNumberToObject(item, "estado_animo_promedio", data.estado_animo_promedio);
        cJSON_AddNumberToObject(item, "eficiencia_goles_por_partido", data.eficiencia_goles_por_partido);
        cJSON_AddNumberToObject(item, "eficiencia_asistencias_por_partido", data.eficiencia_asistencias_por_partido);
        cJSON_AddNumberToObject(item, "relacion_goles_asistencias", data.relacion_goles_asistencias);
        cJSON_AddNumberToObject(item, "porcentaje_victorias", data.porcentaje_victorias);
        cJSON_AddNumberToObject(item, "porcentaje_lesiones_por_partido", data.porcentaje_lesiones_por_partido);
        cJSON_AddItemToArray(root, item);
    }

    // Escribir JSON al archivo y liberar recursos
    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("camisetas_mejorado.json"));
    fclose(f);
}

/**
 * @brief Exporta las camisetas con analisis avanzado a un archivo HTML mejorado
 *
 * Utiliza la funcion comun de obtencion de datos para evitar duplicacion de codigo.
 * El formato HTML mejorado es ideal para visualizacion interactiva en navegadores web,
 * proporcionando una interfaz de usuario amigable con analisis avanzado para presentacion
 * de datos y reportes visuales.
 */
void exportar_camisetas_html_mejorado()
{
    int count;
    sqlite3_stmt *stmt = obtener_datos_camisetas_mejorado(&count);
    if (!stmt) return;

    FILE *f = abrir_archivo_exportacion("camisetas_mejorado.html", "Error al crear archivo de camisetas mejorado HTML");
    if (!f)
    {
        sqlite3_finalize(stmt);
        return;
    }

    // Escribir encabezado HTML y estructura de tabla con metricas avanzadas
    fprintf(f,
            "<html><body><h1>Camisetas con Estadisticas Avanzadas</h1><table border='1'>"
            "<tr><th>ID</th><th>Nombre</th><th>Goles Totales</th><th>Asistencias Totales</th><th>Partidos Totales</th><th>Victorias</th><th>%% Victorias</th><th>Empates</th><th>Derrotas</th><th>Lesiones Totales</th><th>%% Lesiones</th><th>Rendimiento Promedio</th><th>Cansancio Promedio</th><th>Estado de Animo Promedio</th><th>Eficiencia Goles/P</th><th>Eficiencia Asist/P</th><th>Relacion G/A</th></tr>");

    // Procesar cada fila de resultados y generar filas HTML con metricas avanzadas
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        CamisetaDataMejorado data = leer_camiseta_mejorada(stmt);
        fprintf(f,
                "<tr><td>%d</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%.2f%%</td><td>%d</td><td>%d</td><td>%d</td><td>%.2f%%</td><td>%.2f</td><td>%.2f</td><td>%.2f</td><td>%.2f</td><td>%.2f</td><td>%.2f</td></tr>",
                data.id,
                data.nombre,
                data.total_goles,
                data.total_asistencias,
                data.total_partidos,
                data.victorias,
                data.porcentaje_victorias,
                data.empates,
                data.derrotas,
                data.total_lesiones,
                data.porcentaje_lesiones_por_partido,
                data.rendimiento_promedio,
                data.cansancio_promedio,
                data.estado_animo_promedio,
                data.eficiencia_goles_por_partido,
                data.eficiencia_asistencias_por_partido,
                data.relacion_goles_asistencias);
    }

    // Cerrar estructura HTML
    fprintf(f, "</table></body></html>");
    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path("camisetas_mejorado.html"));
    fclose(f);
}
