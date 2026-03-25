/**
 * @file export.c
 * @brief Funciones para exportar datos de la base de datos a diferentes formatos
 *
 * Este archivo contiene funciones para exportar datos de lesiones, partidos,
 * estadisticas y analisis a formatos CSV, TXT, JSON y HTML.
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

#ifdef _WIN32
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

/**
 * @brief Construye la ruta completa para un archivo de exportacion
 *
 * Combina el directorio de datos con el nombre del archivo proporcionado
 * para crear una ruta completa.
 *
 * @param filename Nombre del archivo a exportar
 * @return Cadena de caracteres con la ruta completa del archivo
 */
char *get_export_path(const char *filename)
{
    static char path[1024];
    const char *export_dir = get_export_dir();
    if (!export_dir)
    {
        // Fallback to current directory if export dir is not available
        strcpy_s(path, sizeof(path), filename);
    }
    else
    {
        strcpy_s(path, sizeof(path), export_dir);
        strcat_s(path, sizeof(path), PATH_SEP);
        strcat_s(path, sizeof(path), filename);
    }
    return path;
}



/* ===================== HELPER FUNCTIONS (STATIC) ===================== */

/* Forward declarations of static functions */
static void calcular_estadisticas_generales(Estadisticas *stats);
static void calcular_estadisticas_ultimos5(Estadisticas *stats);
static void calcular_rachas(int *mejor_racha_victorias, int *peor_racha_derrotas);
static int has_partido_records();

/**
 * Verifica si hay registros de partidos en la base de datos.
 * Centraliza la logica de verificacion para evitar duplicacion de codigo.
 * Retorna 1 si hay registros, 0 si no hay.
 */
static int has_partido_records()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    if (!preparar_stmt_export(&check_stmt, "SELECT COUNT(*) FROM partido"))
    {
        return 0;
    }
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    return count > 0;
}

/**
 * Calcula todas las estadisticas necesarias para el analisis.
 * Centraliza la logica de calculo para evitar duplicacion de codigo.
 */
static void calcular_todas_estadisticas(Estadisticas *generales, Estadisticas *ultimos5, int *mejor_racha_v, int *peor_racha_d)
{
    calcular_estadisticas_generales(generales);
    calcular_estadisticas_ultimos5(ultimos5);
    calcular_rachas(mejor_racha_v, peor_racha_d);
}

/* ===================== ANALISIS ===================== */

/**
 * Calcula estadisticas generales de todos los partidos.
 * Esta funcion es utilizada por el analisis de rendimiento para obtener metricas globales.
 */
static void calcular_estadisticas_generales(Estadisticas *stats)
{
    calcular_estadisticas(stats,
                          "SELECT COUNT(*), AVG(goles), AVG(asistencias), AVG(rendimiento_general), AVG(cansancio), AVG(estado_animo) "
                          "FROM partido");
}

/**
 * @brief Calcula estadisticas de los ultimos 5 partidos
 *
 * @param stats Puntero a la estructura donde almacenar las estadisticas
 */
static void calcular_estadisticas_ultimos5(Estadisticas *stats)
{
    calcular_estadisticas(stats,
                          "SELECT COUNT(*), AVG(goles), AVG(asistencias), AVG(rendimiento_general), AVG(cansancio), AVG(estado_animo) "
                          "FROM (SELECT * FROM partido ORDER BY fecha_hora DESC LIMIT 5)");
}

/**
 * @brief Calcula la racha mas larga de victorias y derrotas
 *
 * @param mejor_racha_victorias Puntero donde almacenar la mejor racha de victorias
 * @param peor_racha_derrotas Puntero donde almacenar la peor racha de derrotas
 */
static void calcular_rachas(int *mejor_racha_victorias, int *peor_racha_derrotas)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt,
                              "SELECT resultado FROM partido ORDER BY fecha_hora"))
    {
        *mejor_racha_victorias = 0;
        *peor_racha_derrotas = 0;
        return;
    }

    int racha_actual_v = 0;
    int max_racha_v = 0;
    int racha_actual_d = 0;
    int max_racha_d = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int resultado = sqlite3_column_int(stmt, 0);
        actualizar_rachas(resultado, &racha_actual_v, &max_racha_v,
                          &racha_actual_d, &max_racha_d);
    }

    *mejor_racha_victorias = max_racha_v;
    *peor_racha_derrotas = max_racha_d;
    sqlite3_finalize(stmt);
}

/* ===================== EXPORTACIONES TXT ADICIONALES ===================== */

void exportar_finanzas_resumen_txt()
{
    FILE *f = abrir_archivo_exportacion("finanzas_resumen.txt", "Error al crear archivo de finanzas resumen TXT");
    if (!f)
        return;

    fprintf(f, "RESUMEN FINANCIERO\n\n");

    if (!has_records("financiamiento"))
    {
        fprintf(f, "No hay transacciones financieras registradas.\n");
        fclose(f);
        return;
    }

    sqlite3_stmt *stmt;

    fprintf(f, "RESUMEN POR MES\n");
    fprintf(f, "----------------\n");
    if (preparar_stmt_export(&stmt,
                             "SELECT strftime('%Y-%m', fecha) as periodo, "
                             "SUM(CASE WHEN tipo = 0 THEN monto ELSE 0 END) ingresos, "
                             "SUM(CASE WHEN tipo = 1 THEN monto ELSE 0 END) gastos "
                             "FROM financiamiento GROUP BY periodo ORDER BY periodo"))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *periodo = (const char *)sqlite3_column_text(stmt, 0);
            double ingresos = sqlite3_column_double(stmt, 1);
            double gastos = sqlite3_column_double(stmt, 2);
            fprintf(f, "Mes: %s | Ingresos: %.2f | Gastos: %.2f | Balance: %.2f\n",
                    periodo ? periodo : "-", ingresos, gastos, ingresos - gastos);
        }
        sqlite3_finalize(stmt);
    }

    fprintf(f, "\nRESUMEN POR ANIO\n");
    fprintf(f, "----------------\n");
    if (preparar_stmt_export(&stmt,
                             "SELECT strftime('%Y', fecha) as anio, "
                             "SUM(CASE WHEN tipo = 0 THEN monto ELSE 0 END) ingresos, "
                             "SUM(CASE WHEN tipo = 1 THEN monto ELSE 0 END) gastos "
                             "FROM financiamiento GROUP BY anio ORDER BY anio"))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *anio = (const char *)sqlite3_column_text(stmt, 0);
            double ingresos = sqlite3_column_double(stmt, 1);
            double gastos = sqlite3_column_double(stmt, 2);
            fprintf(f, "Anio: %s | Ingresos: %.2f | Gastos: %.2f | Balance: %.2f\n",
                    anio ? anio : "-", ingresos, gastos, ingresos - gastos);
        }
        sqlite3_finalize(stmt);
    }

    fclose(f);
}

void exportar_ranking_canchas_txt()
{
    FILE *f = abrir_archivo_exportacion("ranking_canchas.txt", "Error al crear archivo de ranking de canchas TXT");
    if (!f)
        return;

    fprintf(f, "RANKING DE CANCHAS (RENDIMIENTO Y LESIONES)\n\n");

    if (!has_records("cancha"))
    {
        fprintf(f, "No hay canchas registradas.\n");
        fclose(f);
        return;
    }

    sqlite3_stmt *stmt;
    if (preparar_stmt_export(&stmt,
                             "SELECT can.nombre, "
                             "COUNT(DISTINCT p.id) as partidos, "
                             "COALESCE(AVG(p.rendimiento_general), 0), "
                             "COUNT(l.id) as lesiones "
                             "FROM cancha can "
                             "LEFT JOIN partido p ON p.cancha_id = can.id "
                             "LEFT JOIN lesion l ON l.partido_id = p.id "
                             "GROUP BY can.id "
                             "ORDER BY COALESCE(AVG(p.rendimiento_general), 0) DESC, COUNT(l.id) ASC"))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
            int partidos = sqlite3_column_int(stmt, 1);
            double rendimiento = sqlite3_column_double(stmt, 2);
            int lesiones = sqlite3_column_int(stmt, 3);
            fprintf(f, "Cancha: %s | Partidos: %d | Rendimiento Promedio: %.2f | Lesiones: %d\n",
                    nombre ? nombre : "-", partidos, rendimiento, lesiones);
        }
        sqlite3_finalize(stmt);
    }

    fclose(f);
}

void exportar_partidos_por_clima_txt()
{
    FILE *f = abrir_archivo_exportacion("partidos_por_clima.txt", "Error al crear archivo de partidos por clima TXT");
    if (!f)
        return;

    fprintf(f, "PARTIDOS POR CLIMA\n\n");

    if (!has_records("partido"))
    {
        fprintf(f, "No hay partidos registrados.\n");
        fclose(f);
        return;
    }

    sqlite3_stmt *stmt;
    if (preparar_stmt_export(&stmt,
                             "SELECT clima, COUNT(*), AVG(goles), AVG(asistencias) "
                             "FROM partido GROUP BY clima ORDER BY clima"))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int clima = sqlite3_column_int(stmt, 0);
            int count = sqlite3_column_int(stmt, 1);
            double avg_goles = sqlite3_column_double(stmt, 2);
            double avg_asist = sqlite3_column_double(stmt, 3);
            fprintf(f, "Clima: %s | Partidos: %d | Prom. Goles: %.2f | Prom. Asistencias: %.2f\n",
                    clima_to_text(clima), count, avg_goles, avg_asist);
        }
        sqlite3_finalize(stmt);
    }

    fclose(f);
}

void exportar_lesiones_por_tipo_estado_txt()
{
    FILE *f = abrir_archivo_exportacion("lesiones_por_tipo_estado.txt", "Error al crear archivo de lesiones por tipo y estado TXT");
    if (!f)
        return;

    fprintf(f, "DISTRIBUCION DE LESIONES POR TIPO Y ESTADO\n\n");

    if (!has_records("lesion"))
    {
        fprintf(f, "No hay lesiones registradas.\n");
        fclose(f);
        return;
    }

    sqlite3_stmt *stmt;
    if (preparar_stmt_export(&stmt,
                             "SELECT tipo, estado, COUNT(*) FROM lesion GROUP BY tipo, estado ORDER BY tipo, estado"))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *tipo = (const char *)sqlite3_column_text(stmt, 0);
            const char *estado = (const char *)sqlite3_column_text(stmt, 1);
            int count = sqlite3_column_int(stmt, 2);
            fprintf(f, "Tipo: %s | Estado: %s | Cantidad: %d\n",
                    tipo ? tipo : "-", estado ? estado : "-", count);
        }
        sqlite3_finalize(stmt);
    }

    fclose(f);
}

void exportar_rachas_historial_txt()
{
    FILE *f = abrir_archivo_exportacion("rachas_historial.txt", "Error al crear archivo de rachas TXT");
    if (!f)
        return;

    fprintf(f, "HISTORIAL DE RACHAS\n\n");

    if (!has_records("partido"))
    {
        fprintf(f, "No hay partidos registrados.\n");
        fclose(f);
        return;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt,
                              "SELECT resultado, fecha_hora FROM partido ORDER BY fecha_hora"))
    {
        fclose(f);
        return;
    }

    int racha_resultado = -1;
    int racha_count = 0;
    char fecha_inicio[32] = "";
    char fecha_fin[32] = "";

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int resultado = sqlite3_column_int(stmt, 0);
        const char *fecha_raw = (const char *)sqlite3_column_text(stmt, 1);
        char fecha_formateada[32];
        if (fecha_raw)
        {
            format_date_for_display(fecha_raw, fecha_formateada, sizeof(fecha_formateada));
        }
        else
        {
            strcpy_s(fecha_formateada, sizeof(fecha_formateada), "-");
        }

        if (racha_resultado == -1)
        {
            racha_resultado = resultado;
            racha_count = 1;
            strcpy_s(fecha_inicio, sizeof(fecha_inicio), fecha_formateada);
            strcpy_s(fecha_fin, sizeof(fecha_fin), fecha_formateada);
            continue;
        }

        if (resultado == racha_resultado)
        {
            racha_count++;
            strcpy_s(fecha_fin, sizeof(fecha_fin), fecha_formateada);
        }
        else
        {
            fprintf(f, "Racha %s: %d partido(s) | Desde %s hasta %s\n",
                    resultado_to_text(racha_resultado), racha_count, fecha_inicio, fecha_fin);
            racha_resultado = resultado;
            racha_count = 1;
            strcpy_s(fecha_inicio, sizeof(fecha_inicio), fecha_formateada);
            strcpy_s(fecha_fin, sizeof(fecha_fin), fecha_formateada);
        }
    }

    if (racha_resultado != -1)
    {
        fprintf(f, "Racha %s: %d partido(s) | Desde %s hasta %s\n",
                resultado_to_text(racha_resultado), racha_count, fecha_inicio, fecha_fin);
    }

    sqlite3_finalize(stmt);
    fclose(f);
}

void exportar_estado_animo_cansancio_txt()
{
    FILE *f = abrir_archivo_exportacion("estado_animo_cansancio.txt", "Error al crear archivo de estado de animo y cansancio TXT");
    if (!f)
        return;

    fprintf(f, "DISTRIBUCION DE ESTADO DE ANIMO Y CANSANCIO\n\n");

    if (!has_records("partido"))
    {
        fprintf(f, "No hay partidos registrados.\n");
        fclose(f);
        return;
    }

    sqlite3_stmt *stmt;

    fprintf(f, "ESTADO DE ANIMO\n");
    fprintf(f, "---------------\n");
    if (preparar_stmt_export(&stmt,
                             "SELECT estado_animo, COUNT(*) FROM partido GROUP BY estado_animo ORDER BY estado_animo"))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int valor = sqlite3_column_int(stmt, 0);
            int count = sqlite3_column_int(stmt, 1);
            fprintf(f, "Estado de animo %d: %d partido(s)\n", valor, count);
        }
        sqlite3_finalize(stmt);
    }

    fprintf(f, "\nCANSANCIO\n");
    fprintf(f, "---------\n");
    if (preparar_stmt_export(&stmt,
                             "SELECT cansancio, COUNT(*) FROM partido GROUP BY cansancio ORDER BY cansancio"))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int valor = sqlite3_column_int(stmt, 0);
            int count = sqlite3_column_int(stmt, 1);
            fprintf(f, "Cansancio %d: %d partido(s)\n", valor, count);
        }
        sqlite3_finalize(stmt);
    }

    fclose(f);
}

/**
 * @brief Genera un mensaje motivacional basado en el rendimiento
 *
 * @param ultimos Puntero a estadisticas de ultimos 5 partidos
 * @param generales Puntero a estadisticas generales
 * @return Cadena de texto con el mensaje motivacional
 */
static const char *mensaje_motivacional(const Estadisticas *ultimos, const Estadisticas *generales)
{
    double diff_goles = ultimos->avg_goles - generales->avg_goles;
    double diff_rendimiento = ultimos->avg_rendimiento - generales->avg_rendimiento;

    if (diff_goles > 0.5 && diff_rendimiento > 0.5)
    {
        return "Excelente. Estas en racha ascendente. Sigue asi, tu esfuerzo esta dando frutos. Mantien la consistencia y continua trabajando duro en los entrenamientos.";
    }
    else if (diff_goles < -0.5 || diff_rendimiento < -0.5)
    {
        return "No te desanimes. Todos tenemos dias dificiles. Analiza que puedes mejorar: Revisa tu preparacion fisica y tecnica. Habla con tu entrenador sobre estrategias. Recuerda: el futbol es un deporte de perseverancia.";
    }
    else
    {
        return "Buen trabajo manteniendo el nivel. La consistencia es clave en el futbol. Sigue entrenando y manten la motivacion alta. Cada partido es una oportunidad!";
    }
}

/**
 * Exporta el analisis de rendimiento a un archivo CSV.
 * Usa funciones auxiliares para mantener el codigo conciso y dentro del limite de lineas.
 */
void exportar_analisis_csv()
{
    if (!has_partido_records())
    {
        mostrar_no_hay_registros("registros de partidos para exportar analisis");
        return;
    }

    FILE *f = abrir_archivo_exportacion("analisis.csv", "Error al crear archivo de analisis CSV");
    if (!f)
        return;

    fprintf(f, "Tipo,Promedio_Goles,Promedio_Asistencias,Promedio_Rendimiento,Promedio_Cansancio,Promedio_Animo,Total_Partidos\n");

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v;
    int peor_racha_d;

    calcular_todas_estadisticas(&generales, &ultimos5, &mejor_racha_v, &peor_racha_d);

    fprintf(f, "Generales,%.2f,%.2f,%.2f,%.2f,%.2f,%d\n",
            generales.avg_goles, generales.avg_asistencias, generales.avg_rendimiento,
            generales.avg_cansancio, generales.avg_animo, generales.total_partidos);

    fprintf(f, "Ultimos5,%.2f,%.2f,%.2f,%.2f,%.2f,%d\n",
            ultimos5.avg_goles, ultimos5.avg_asistencias, ultimos5.avg_rendimiento,
            ultimos5.avg_cansancio, ultimos5.avg_animo, ultimos5.total_partidos);

    fprintf(f, "Rachas,%d,%d\n", mejor_racha_v, peor_racha_d);

    const char *msg = mensaje_motivacional(&ultimos5, &generales);
    fprintf(f, "Mensaje,%s\n", msg);

    printf("Archivo exportado a: %s\n", get_export_path("analisis.csv"));
    fclose(f);
}

/**
 * Exporta el analisis de rendimiento a un archivo de texto plano.
 * Usa funciones auxiliares para mantener el codigo conciso y dentro del limite de lineas.
 */
void exportar_analisis_txt()
{
    if (!has_partido_records())
    {
        mostrar_no_hay_registros("registros de partidos para exportar analisis");
        return;
    }

    FILE *f = abrir_archivo_exportacion("analisis.txt", "Error al crear archivo de analisis TXT");
    if (!f)
        return;

    fprintf(f, "ANALISIS DE RENDIMIENTO\n\n");

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v;
    int peor_racha_d;

    calcular_todas_estadisticas(&generales, &ultimos5, &mejor_racha_v, &peor_racha_d);

    fprintf(f, "ESTADISTICAS GENERALES:\n");
    fprintf(f, "Total Partidos: %d\n", generales.total_partidos);
    fprintf(f, "Promedio Goles: %.2f\n", generales.avg_goles);
    fprintf(f, "Promedio Asistencias: %.2f\n", generales.avg_asistencias);
    fprintf(f, "Promedio Rendimiento: %.2f\n", generales.avg_rendimiento);
    fprintf(f, "Promedio Cansancio: %.2f\n", generales.avg_cansancio);
    fprintf(f, "Promedio Estado Animo: %.2f\n\n", generales.avg_animo);

    fprintf(f, "ULTIMOS 5 PARTIDOS:\n");
    fprintf(f, "Total Partidos: %d\n", ultimos5.total_partidos);
    fprintf(f, "Promedio Goles: %.2f\n", ultimos5.avg_goles);
    fprintf(f, "Promedio Asistencias: %.2f\n", ultimos5.avg_asistencias);
    fprintf(f, "Promedio Rendimiento: %.2f\n", ultimos5.avg_rendimiento);
    fprintf(f, "Promedio Cansancio: %.2f\n", ultimos5.avg_cansancio);
    fprintf(f, "Promedio Estado Animo: %.2f\n\n", ultimos5.avg_animo);

    fprintf(f, "RACHAS:\n");
    fprintf(f, "Mejor racha de victorias: %d partidos\n", mejor_racha_v);
    fprintf(f, "Peor racha de derrotas: %d partidos\n\n", peor_racha_d);

    const char *msg = mensaje_motivacional(&ultimos5, &generales);
    fprintf(f, "ANALISIS MOTIVACIONAL:\n%s\n", msg);

    printf("Archivo exportado a: %s\n", get_export_path("analisis.txt"));
    fclose(f);
}

/**
 * Exporta el analisis de rendimiento a un archivo JSON.
 * Usa funciones auxiliares para mantener el codigo conciso y dentro del limite de lineas.
 */
void exportar_analisis_json()
{
    if (!has_partido_records())
    {
        mostrar_no_hay_registros("registros de partidos para exportar analisis");
        return;
    }

    FILE *f = abrir_archivo_exportacion("analisis.json", "Error al crear archivo de analisis JSON");
    if (!f)
        return;

    cJSON *root = cJSON_CreateObject();

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v;
    int peor_racha_d;

    calcular_todas_estadisticas(&generales, &ultimos5, &mejor_racha_v, &peor_racha_d);

    cJSON *generales_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(generales_obj, "total_partidos", generales.total_partidos);
    cJSON_AddNumberToObject(generales_obj, "avg_goles", generales.avg_goles);
    cJSON_AddNumberToObject(generales_obj, "avg_asistencias", generales.avg_asistencias);
    cJSON_AddNumberToObject(generales_obj, "avg_rendimiento", generales.avg_rendimiento);
    cJSON_AddNumberToObject(generales_obj, "avg_cansancio", generales.avg_cansancio);
    cJSON_AddNumberToObject(generales_obj, "avg_animo", generales.avg_animo);
    cJSON_AddItemToObject(root, "generales", generales_obj);

    cJSON *ultimos5_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(ultimos5_obj, "total_partidos", ultimos5.total_partidos);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_goles", ultimos5.avg_goles);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_asistencias", ultimos5.avg_asistencias);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_rendimiento", ultimos5.avg_rendimiento);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_cansancio", ultimos5.avg_cansancio);
    cJSON_AddNumberToObject(ultimos5_obj, "avg_animo", ultimos5.avg_animo);
    cJSON_AddItemToObject(root, "ultimos5", ultimos5_obj);

    cJSON *rachas_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(rachas_obj, "mejor_racha_victorias", mejor_racha_v);
    cJSON_AddNumberToObject(rachas_obj, "peor_racha_derrotas", peor_racha_d);
    cJSON_AddItemToObject(root, "rachas", rachas_obj);

    const char *msg = mensaje_motivacional(&ultimos5, &generales);
    cJSON_AddStringToObject(root, "mensaje_motivacional", msg);

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);

    free(json_string);
    cJSON_Delete(root);
    printf("Archivo exportado a: %s\n", get_export_path("analisis.json"));
    fclose(f);
}

/**
 * Exporta el analisis de rendimiento a un archivo HTML.
 * Usa funciones auxiliares para mantener el codigo conciso y dentro del limite de lineas.
 */
void exportar_analisis_html()
{
    if (!has_partido_records())
    {
        mostrar_no_hay_registros("registros de partidos para exportar analisis");
        return;
    }

    FILE *f = abrir_archivo_exportacion("analisis.html", "Error al crear archivo de analisis HTML");
    if (!f)
        return;

    fprintf(f, "<html><body><h1>Analisis de Rendimiento</h1>");

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v;
    int peor_racha_d;

    calcular_todas_estadisticas(&generales, &ultimos5, &mejor_racha_v, &peor_racha_d);

    fprintf(f, "<h2>Estadisticas Generales</h2>");
    fprintf(f, "<table border='1'>");
    fprintf(f, "<tr><th>Total Partidos</th><td>%d</td></tr>", generales.total_partidos);
    fprintf(f, "<tr><th>Promedio Goles</th><td>%.2f</td></tr>", generales.avg_goles);
    fprintf(f, "<tr><th>Promedio Asistencias</th><td>%.2f</td></tr>", generales.avg_asistencias);
    fprintf(f, "<tr><th>Promedio Rendimiento</th><td>%.2f</td></tr>", generales.avg_rendimiento);
    fprintf(f, "<tr><th>Promedio Cansancio</th><td>%.2f</td></tr>", generales.avg_cansancio);
    fprintf(f, "<tr><th>Promedio Estado Animo</th><td>%.2f</td></tr>", generales.avg_animo);
    fprintf(f, "</table>");

    fprintf(f, "<h2>Ultimos 5 Partidos</h2>");
    fprintf(f, "<table border='1'>");
    fprintf(f, "<tr><th>Total Partidos</th><td>%d</td></tr>", ultimos5.total_partidos);
    fprintf(f, "<tr><th>Promedio Goles</th><td>%.2f</td></tr>", ultimos5.avg_goles);
    fprintf(f, "<tr><th>Promedio Asistencias</th><td>%.2f</td></tr>", ultimos5.avg_asistencias);
    fprintf(f, "<tr><th>Promedio Rendimiento</th><td>%.2f</td></tr>", ultimos5.avg_rendimiento);
    fprintf(f, "<tr><th>Promedio Cansancio</th><td>%.2f</td></tr>", ultimos5.avg_cansancio);
    fprintf(f, "<tr><th>Promedio Estado Animo</th><td>%.2f</td></tr>", ultimos5.avg_animo);
    fprintf(f, "</table>");

    fprintf(f, "<h2>Rachas</h2>");
    fprintf(f, "<table border='1'>");
    fprintf(f, "<tr><th>Mejor Racha Victorias</th><td>%d partidos</td></tr>", mejor_racha_v);
    fprintf(f, "<tr><th>Peor Racha Derrotas</th><td>%d partidos</td></tr>", peor_racha_d);
    fprintf(f, "</table>");

    const char *msg = mensaje_motivacional(&ultimos5, &generales);
    fprintf(f, "<h2>Analisis Motivacional</h2>");
    fprintf(f, "<p>%s</p>", msg);

    fprintf(f, "</body></html>");
    printf("Archivo exportado a: %s\n", get_export_path("analisis.html"));
    fclose(f);
}
