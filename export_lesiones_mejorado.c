/**
 * @file export_lesiones_mejorado.c
 * @brief Funciones mejoradas para exportar datos de lesiones con analisis avanzado
 *
 * Este archivo contiene funciones mejoradas para exportar datos de lesiones
 * con estadisticas avanzadas y analisis de impacto.
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

/* ============================================================================
 * CONSULTAS SQL ESTaTICAS - Centralizadas para mantenimiento
 * ============================================================================ */

static const char *SQL_LESIONES_AVANZADO =
    "SELECT l.id, l.jugador, l.tipo, l.descripcion, l.fecha, c.nombre as camiseta_nombre, "
    "(SELECT COUNT(*) FROM partido p WHERE p.camiseta_id = l.camiseta_id AND p.fecha_hora < l.fecha) as partidos_antes_lesion, "
    "(SELECT COUNT(*) FROM partido p WHERE p.camiseta_id = l.camiseta_id AND p.fecha_hora > l.fecha) as partidos_despues_lesion, "
    "(SELECT AVG(p.rendimiento_general) FROM partido p WHERE p.camiseta_id = l.camiseta_id AND p.fecha_hora < l.fecha) as rendimiento_antes, "
    "(SELECT AVG(p.rendimiento_general) FROM partido p WHERE p.camiseta_id = l.camiseta_id AND p.fecha_hora > l.fecha) as rendimiento_despues, "
    "CASE WHEN (SELECT AVG(p.rendimiento_general) FROM partido p WHERE p.camiseta_id = l.camiseta_id AND p.fecha_hora < l.fecha) > 0 "
    "THEN ((SELECT AVG(p.rendimiento_general) FROM partido p WHERE p.camiseta_id = l.camiseta_id AND p.fecha_hora > l.fecha) - "
    "(SELECT AVG(p.rendimiento_general) FROM partido p WHERE p.camiseta_id = l.camiseta_id AND p.fecha_hora < l.fecha)) * 100.0 / "
    "(SELECT AVG(p.rendimiento_general) FROM partido p WHERE p.camiseta_id = l.camiseta_id AND p.fecha_hora < l.fecha) "
    "ELSE 0 END as impacto_rendimiento "
    "FROM lesion l "
    "LEFT JOIN camiseta c ON l.camiseta_id = c.id";

/* ============================================================================
 * HELPER ESTaTICOS
 * ============================================================================ */

/** @brief Escribe lesiones en formato CSV */
static void write_lesiones_csv(FILE *file)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_LESIONES_AVANZADO))
    {
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%d,%s,%s,%s,%s,%s,%d,%d,%.2f,%.2f,%.2f\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4),
                sqlite3_column_text(stmt, 5),
                sqlite3_column_int(stmt, 6),
                sqlite3_column_int(stmt, 7),
                sqlite3_column_double(stmt, 8),
                sqlite3_column_double(stmt, 9),
                sqlite3_column_double(stmt, 10));
    }

    sqlite3_finalize(stmt);
}

/** @brief Escribe lesiones en formato TXT */
static void write_lesiones_txt(FILE *file)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_LESIONES_AVANZADO))
    {
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "ID: %d - Jugador: %s\n"
                "  Tipo: %s\n"
                "  Descripcion: %s\n"
                "  Fecha: %s\n"
                "  Camiseta: %s\n"
                "  Partidos antes de lesion: %d\n"
                "  Partidos despues de lesion: %d\n"
                "  Rendimiento antes: %.2f\n"
                "  Rendimiento despues: %.2f\n"
                "  Impacto en rendimiento: %.2f%%\n\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4),
                sqlite3_column_text(stmt, 5),
                sqlite3_column_int(stmt, 6),
                sqlite3_column_int(stmt, 7),
                sqlite3_column_double(stmt, 8),
                sqlite3_column_double(stmt, 9),
                sqlite3_column_double(stmt, 10));
    }

    sqlite3_finalize(stmt);
}

/** @brief Escribe lesiones en formato HTML */
static void write_lesiones_html(FILE *file)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_LESIONES_AVANZADO))
    {
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file,
                "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%.2f</td><td>%.2f</td><td>%.2f%%</td></tr>",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4),
                sqlite3_column_text(stmt, 5),
                sqlite3_column_int(stmt, 6),
                sqlite3_column_int(stmt, 7),
                sqlite3_column_double(stmt, 8),
                sqlite3_column_double(stmt, 9),
                sqlite3_column_double(stmt, 10));
    }

    sqlite3_finalize(stmt);
}

/** @brief Escribe lesiones en formato JSON */
static void write_lesiones_json(FILE *file)
{
    cJSON *root = cJSON_CreateArray();
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, SQL_LESIONES_AVANZADO))
    {
        cJSON_Delete(root);
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddStringToObject(item, "jugador", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddStringToObject(item, "tipo", (const char *)sqlite3_column_text(stmt, 2));
        cJSON_AddStringToObject(item, "descripcion", (const char *)sqlite3_column_text(stmt, 3));
        cJSON_AddStringToObject(item, "fecha", (const char *)sqlite3_column_text(stmt, 4));
        cJSON_AddStringToObject(item, "camiseta_nombre", (const char *)sqlite3_column_text(stmt, 5));
        cJSON_AddNumberToObject(item, "partidos_antes_lesion", sqlite3_column_int(stmt, 6));
        cJSON_AddNumberToObject(item, "partidos_despues_lesion", sqlite3_column_int(stmt, 7));
        cJSON_AddNumberToObject(item, "rendimiento_antes", sqlite3_column_double(stmt, 8));
        cJSON_AddNumberToObject(item, "rendimiento_despues", sqlite3_column_double(stmt, 9));
        cJSON_AddNumberToObject(item, "impacto_rendimiento", sqlite3_column_double(stmt, 10));
        cJSON_AddItemToArray(root, item);
    }

    char *json_string = cJSON_Print(root);
    fprintf(file, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
}

/* ============================================================================
 * EXPORTACIoN LESIONES MEJORADO (4 formatos)
 * ============================================================================ */

/**
 * @brief Exporta las lesiones con analisis avanzado a un archivo CSV mejorado
 *
 * Crea un archivo CSV con estadisticas avanzadas incluyendo impacto en rendimiento.
 *
 * Esta funcion exporta los datos de lesiones con metricas avanzadas como:
 * - Partidos antes y despues de la lesion
 * - Rendimiento antes y despues de la lesion
 * - Impacto en rendimiento (porcentaje de cambio)
 * - Informacion detallada del jugador y tipo de lesion
 *
 * @see exportar_lesiones_txt_mejorado()
 * @see exportar_lesiones_json_mejorado()
 * @see exportar_lesiones_html_mejorado()
 */
void exportar_lesiones_csv_mejorado()
{
    if (!has_records("lesion"))
    {
        mostrar_no_hay_registros("lesiones para exportar");
        return;
    }

    FILE *f = abrir_archivo_exportacion("lesiones_mejorado.csv", "Error al crear el archivo CSV");
    if (!f)
    {
        return;
    }

    fprintf(f, "id,jugador,tipo,descripcion,fecha,camiseta_nombre,partidos_antes_lesion,partidos_despues_lesion,rendimiento_antes,rendimiento_despues,impacto_rendimiento\n");
    write_lesiones_csv(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("lesiones_mejorado.csv"));
}

/**
 * @brief Exporta las lesiones con analisis avanzado a un archivo TXT mejorado
 *
 * Crea un archivo de texto con estadisticas avanzadas y analisis de impacto.
 *
 * Esta funcion exporta los datos de lesiones en formato de texto legible con metricas avanzadas como:
 * - Partidos antes y despues de la lesion
 * - Rendimiento antes y despues de la lesion
 * - Impacto en rendimiento (porcentaje de cambio)
 * - Informacion detallada del jugador y tipo de lesion
 *
 * @see exportar_lesiones_csv_mejorado()
 * @see exportar_lesiones_json_mejorado()
 * @see exportar_lesiones_html_mejorado()
 */
void exportar_lesiones_txt_mejorado()
{
    if (!has_records("lesion"))
    {
        mostrar_no_hay_registros("lesiones para exportar");
        return;
    }

    FILE *f = abrir_archivo_exportacion("lesiones_mejorado.txt", "Error al crear el archivo TXT");
    if (!f)
    {
        return;
    }

    fprintf(f, "LISTADO DE LESIONES CON ANALISIS DE IMPACTO\n\n");
    write_lesiones_txt(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("lesiones_mejorado.txt"));
}

/**
 * @brief Exporta las lesiones con analisis avanzado a un archivo JSON mejorado
 *
 * Crea un archivo JSON con estadisticas avanzadas y analisis de impacto.
 *
 * Esta funcion exporta los datos de lesiones en formato JSON con metricas avanzadas como:
 * - Partidos antes y despues de la lesion
 * - Rendimiento antes y despues de la lesion
 * - Impacto en rendimiento (porcentaje de cambio)
 * - Informacion detallada del jugador y tipo de lesion
 *
 * @see exportar_lesiones_csv_mejorado()
 * @see exportar_lesiones_txt_mejorado()
 * @see exportar_lesiones_html_mejorado()
 */
void exportar_lesiones_json_mejorado()
{
    if (!has_records("lesion"))
    {
        mostrar_no_hay_registros("lesiones para exportar");
        return;
    }

    FILE *f = abrir_archivo_exportacion("lesiones_mejorado.json", "Error al crear el archivo JSON");
    if (!f)
    {
        return;
    }

    write_lesiones_json(f);

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("lesiones_mejorado.json"));
}

/**
 * @brief Exporta las lesiones con analisis avanzado a un archivo HTML mejorado
 *
 * Crea un archivo HTML con estadisticas avanzadas y analisis de impacto.
 *
 * Esta funcion exporta los datos de lesiones en formato HTML con metricas avanzadas como:
 * - Partidos antes y despues de la lesion
 * - Rendimiento antes y despues de la lesion
 * - Impacto en rendimiento (porcentaje de cambio)
 * - Informacion detallada del jugador y tipo de lesion
 *
 * @see exportar_lesiones_csv_mejorado()
 * @see exportar_lesiones_txt_mejorado()
 * @see exportar_lesiones_json_mejorado()
 */
void exportar_lesiones_html_mejorado()
{
    if (!has_records("lesion"))
    {
        mostrar_no_hay_registros("lesiones para exportar");
        return;
    }

    FILE *f = abrir_archivo_exportacion("lesiones_mejorado.html", "Error al crear el archivo HTML");
    if (!f)
    {
        return;
    }

    fprintf(f,
            "<html><body><h1>Lesiones con Analisis de Impacto</h1><table border='1'>"
            "<tr><th>ID</th><th>Jugador</th><th>Tipo</th><th>Descripcion</th><th>Fecha</th><th>Camiseta</th><th>Partidos Antes</th><th>Partidos Despues</th><th>Rendimiento Antes</th><th>Rendimiento Despues</th><th>Impacto %%</th></tr>");

    write_lesiones_html(f);

    fprintf(f, "</table></body></html>");

    fclose(f);
    printf("Archivo exportado a: %s\n", get_export_path("lesiones_mejorado.html"));
}
