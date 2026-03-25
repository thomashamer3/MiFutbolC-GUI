/**
 * @file export_camisetas.c
 * @brief Funciones para exportar datos de camisetas a diferentes formatos
 *
 * Este archivo contiene funciones para exportar datos de camisetas
 * a formatos CSV, TXT, JSON y HTML.
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
 * @brief Obtiene los datos de camisetas de la base de datos
 *
 * Funcion estatica que encapsula la consulta SQL comun utilizada por todas
 * las funciones de exportacion. Esto evita la duplicacion de codigo y centraliza
 * la logica de acceso a datos.
 *
 * @param[out] count Puntero a entero para almacenar el numero de camisetas encontradas
 * @return sqlite3_stmt* Statement preparado para iterar sobre los resultados
 */
static sqlite3_stmt* obtener_datos_camisetas(int *count)
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
                                            "COALESCE((SELECT COUNT(*) FROM lesion l INNER JOIN partido p2 ON l.partido_id = p2.id WHERE p2.camiseta_id = c.id), 0) as total_lesiones, "
                                            "COALESCE(AVG(p.rendimiento_general), 0) as rendimiento_promedio, "
                                            "COALESCE(AVG(p.cansancio), 0) as cansancio_promedio, "
                                            "COALESCE(AVG(p.estado_animo), 0) as estado_animo_promedio "
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
 * @brief Configuracion para exportacion generica
 *
 * Estructura que define los parametros para una exportacion generica,
 * permitiendo reutilizar el codigo comun para diferentes formatos.
 */
typedef struct
{
    const char *filename;           /**< Nombre del archivo de salida */
    void *context;                  /**< Contexto adicional (ej. cJSON root para JSON) */
    void (*write_header)(FILE *f, void *context); /**< Funcion para escribir encabezado */
    void (*write_row)(FILE *f, sqlite3_stmt *stmt, void *context); /**< Funcion para escribir fila */
    void (*write_footer)(FILE *f, void *context); /**< Funcion para escribir pie */
} ExportConfig;

/**
 * @brief Abre un archivo de exportacion con manejo de errores
 *
 * Funcion auxiliar que encapsula la apertura de archivos de exportacion
 * y el manejo de errores, evitando duplicacion de codigo.
 *
 * @param filename Nombre del archivo a abrir
 * @param stmt Statement de SQLite para liberar en caso de error
 * @return FILE* Puntero al archivo abierto, o NULL si falla
 */
static FILE* open_export_file(const char *filename, sqlite3_stmt *stmt)
{
    FILE *f;
    errno_t err = fopen_s(&f, get_export_path(filename), "w");
    if (err != 0 || f == NULL)
    {
        sqlite3_finalize(stmt);
        return NULL;
    }
    return f;
}

/**
 * @brief Funcion generica para exportar camisetas
 *
 * Implementa el flujo comun de exportacion utilizando una configuracion
 * que define como escribir encabezado, filas y pie para cada formato.
 *
 * @param config Configuracion de exportacion
 */
static void export_camisetas_generic(ExportConfig *config)
{
    int count;
    sqlite3_stmt *stmt = obtener_datos_camisetas(&count);
    if (!stmt) return;

    FILE *f = open_export_file(config->filename, stmt);
    if (!f) return;

    // Escribir encabezado
    if (config->write_header)
        config->write_header(f, config->context);

    // Procesar filas
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        config->write_row(f, stmt, config->context);
    }

    // Escribir pie
    if (config->write_footer)
        config->write_footer(f, config->context);

    sqlite3_finalize(stmt);
    printf("Archivo exportado a: %s\n", get_export_path(config->filename));
    fclose(f);
}

/** @name Funciones auxiliares para exportacion */
/** @{ */

/**
 * @brief Escribe el encabezado CSV
 */
static void write_csv_header(FILE *f, void *)
{
    fprintf(f, "id,nombre,total_goles,total_asistencias,total_partidos,victorias,empates,derrotas,total_lesiones,rendimiento_promedio,cansancio_promedio,estado_animo_promedio\n");
}

/**
 * @brief Escribe una fila CSV
 */
static void write_csv_row(FILE *f, sqlite3_stmt *stmt, void *)
{
    fprintf(f, "%d,%s,%d,%d,%d,%d,%d,%d,%d,%.2f,%.2f,%.2f\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_int(stmt, 4),
            sqlite3_column_int(stmt, 5),
            sqlite3_column_int(stmt, 6),
            sqlite3_column_int(stmt, 7),
            sqlite3_column_int(stmt, 8),
            sqlite3_column_double(stmt, 9),
            sqlite3_column_double(stmt, 10),
            sqlite3_column_double(stmt, 11));
}

/**
 * @brief Escribe el encabezado TXT
 */
static void write_txt_header(FILE *f, void *)
{
    fprintf(f, "LISTADO DE CAMISETAS CON ESTADISTICAS\n\n");
}

/**
 * @brief Escribe una fila TXT
 */
static void write_txt_row(FILE *f, sqlite3_stmt *stmt, void *)
{
    fprintf(f, "ID: %d - Nombre: %s\n"
            "  Goles Totales: %d\n"
            "  Asistencias Totales: %d\n"
            "  Partidos Totales: %d\n"
            "  Victorias: %d\n"
            "  Empates: %d\n"
            "  Derrotas: %d\n"
            "  Lesiones Totales: %d\n"
            "  Rendimiento Promedio: %.2f\n"
            "  Cansancio Promedio: %.2f\n"
            "  Estado de Animo Promedio: %.2f\n\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_int(stmt, 4),
            sqlite3_column_int(stmt, 5),
            sqlite3_column_int(stmt, 6),
            sqlite3_column_int(stmt, 7),
            sqlite3_column_int(stmt, 8),
            sqlite3_column_double(stmt, 9),
            sqlite3_column_double(stmt, 10),
            sqlite3_column_double(stmt, 11));
}

/**
 * @brief Escribe una fila JSON (agrega objeto al array)
 */
static void write_json_row(FILE *, sqlite3_stmt *stmt, void *context)
{
    cJSON *root = (cJSON *)context;
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
    cJSON_AddStringToObject(item, "nombre", (const char *)sqlite3_column_text(stmt, 1));
    cJSON_AddNumberToObject(item, "total_goles", sqlite3_column_int(stmt, 2));
    cJSON_AddNumberToObject(item, "total_asistencias", sqlite3_column_int(stmt, 3));
    cJSON_AddNumberToObject(item, "total_partidos", sqlite3_column_int(stmt, 4));
    cJSON_AddNumberToObject(item, "victorias", sqlite3_column_int(stmt, 5));
    cJSON_AddNumberToObject(item, "empates", sqlite3_column_int(stmt, 6));
    cJSON_AddNumberToObject(item, "derrotas", sqlite3_column_int(stmt, 7));
    cJSON_AddNumberToObject(item, "total_lesiones", sqlite3_column_int(stmt, 8));
    cJSON_AddNumberToObject(item, "rendimiento_promedio", sqlite3_column_double(stmt, 9));
    cJSON_AddNumberToObject(item, "cansancio_promedio", sqlite3_column_double(stmt, 10));
    cJSON_AddNumberToObject(item, "estado_animo_promedio", sqlite3_column_double(stmt, 11));
    cJSON_AddItemToArray(root, item);
}

/**
 * @brief Escribe el pie JSON (imprime el JSON al archivo)
 */
static void write_json_footer(FILE *f, void *context)
{
    cJSON *root = (cJSON *)context;
    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);
}

/**
 * @brief Escribe el encabezado HTML
 */
static void write_html_header(FILE *f, void *)
{
    fprintf(f,
            "<html><body><h1>Camisetas con Estadisticas</h1><table border='1'>"
            "<tr><th>ID</th><th>Nombre</th><th>Goles Totales</th><th>Asistencias Totales</th><th>Partidos Totales</th><th>Victorias</th><th>Empates</th><th>Derrotas</th><th>Lesiones Totales</th><th>Rendimiento Promedio</th><th>Cansancio Promedio</th><th>Estado de Animo Promedio</th></tr>");
}

/**
 * @brief Escribe una fila HTML
 */
static void write_html_row(FILE *f, sqlite3_stmt *stmt, void *)
{
    fprintf(f,
            "<tr><td>%d</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%.2f</td><td>%.2f</td><td>%.2f</td></tr>",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_int(stmt, 4),
            sqlite3_column_int(stmt, 5),
            sqlite3_column_int(stmt, 6),
            sqlite3_column_int(stmt, 7),
            sqlite3_column_int(stmt, 8),
            sqlite3_column_double(stmt, 9),
            sqlite3_column_double(stmt, 10),
            sqlite3_column_double(stmt, 11));
}

/**
 * @brief Escribe el pie HTML
 */
static void write_html_footer(FILE *f, void *)
{
    fprintf(f, "</table></body></html>");
}

/** @} */

/** @name Funciones de exportacion de camisetas */
/** @{ */

/**
 * @brief Exporta las camisetas a un archivo CSV
 *
 * Utiliza la funcion generica de exportacion para evitar duplicacion de codigo.
 * El formato CSV es ideal para importacion en hojas de calculo y analisis de datos.
 */
void exportar_camisetas_csv()
{
    ExportConfig config =
    {
        .filename = "camisetas.csv",
        .context = NULL,
        .write_header = write_csv_header,
        .write_row = write_csv_row,
        .write_footer = NULL
    };
    export_camisetas_generic(&config);
}

/**
 * @brief Exporta las camisetas a un archivo de texto plano
 *
 * Utiliza la funcion generica de exportacion para evitar duplicacion de codigo.
 * El formato TXT es ideal para visualizacion humana y documentacion impresa.
 */
void exportar_camisetas_txt()
{
    ExportConfig config =
    {
        .filename = "camisetas.txt",
        .context = NULL,
        .write_header = write_txt_header,
        .write_row = write_txt_row,
        .write_footer = NULL
    };
    export_camisetas_generic(&config);
}

/**
 * @brief Exporta las camisetas a un archivo JSON
 *
 * Utiliza la funcion generica de exportacion para evitar duplicacion de codigo.
 * El formato JSON es ideal para APIs y aplicaciones web que necesitan datos estructurados.
 */
void exportar_camisetas_json()
{
    cJSON *root = cJSON_CreateArray();
    ExportConfig config =
    {
        .filename = "camisetas.json",
        .context = root,
        .write_header = NULL,  // No header needed for JSON
        .write_row = write_json_row,
        .write_footer = write_json_footer
    };
    export_camisetas_generic(&config);
}

/**
 * @brief Exporta las camisetas a un archivo HTML
 *
 * Utiliza la funcion generica de exportacion para evitar duplicacion de codigo.
 * El formato HTML es ideal para visualizacion en navegadores web y reportes interactivos.
 */
void exportar_camisetas_html()
{
    ExportConfig config =
    {
        .filename = "camisetas.html",
        .context = NULL,
        .write_header = write_html_header,
        .write_row = write_html_row,
        .write_footer = write_html_footer
    };
    export_camisetas_generic(&config);
}

/** @} */ /* End of Doxygen group */
