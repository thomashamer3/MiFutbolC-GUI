/**
 * @file estadisticas_generales.c
 * @brief Modulo de analisis estadistico global de rendimiento deportivo
 *
 * Implementa consultas SQL para metricas agregadas de partidos,
 * incluyendo analisis por clima, dia de semana, cansancio y estado animico,
 * utilizando funciones de agrupamiento y JOINs para insights comprehensivos.
 */

#include "estadisticas_generales.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "estadisticas_gui_capture.h"

/**
 * @brief Funcion auxiliar para generar el CASE WHEN para clima
 * @return Cadena SQL con el CASE WHEN para convertir clima numerico a texto
 */
static const char* get_clima_case_sql()
{
    return "CASE WHEN clima = 1 THEN 'Despejado' WHEN clima = 2 THEN 'Nublado' WHEN clima = 3 THEN 'Lluvia' WHEN clima = 4 THEN 'Ventoso' WHEN clima = 5 THEN 'Mucho Calor' WHEN clima = 6 THEN 'Mucho Frio' END";
}

/**
 * @brief Funcion auxiliar para generar el CASE WHEN para niveles (estado_animo/cansancio)
 * @param columna Nombre de la columna ('estado_animo' o 'cansancio')
 * @return Cadena SQL con el CASE WHEN para convertir rangos numericos a texto
 */
static const char* get_nivel_case_sql(const char* columna)
{
    static char sql[256];
    snprintf(sql, sizeof(sql),
             "CASE WHEN %s <= 3 THEN 'Bajo (1-3)' WHEN %s <= 7 THEN 'Medio (4-7)' ELSE 'Alto (8-10)' END",
             columna, columna);
    return sql;
}

/**
 * @brief Funcion generica para mostrar estadisticas por dia de la semana
 * @param titulo Titulo a mostrar
 * @param columna Columna a promediar ('rendimiento_general', 'goles', 'asistencias')
 * @param order_by Orden ('DESC' o 'ASC')
 * @param limit Limite de resultados (0 para sin limite)
 */
static void mostrar_por_dia_semana(const char* titulo, const char* columna, const char* order_by, int limit)
{
    clear_screen();
    print_header(titulo);

    // Usar la funcion para remover tildes de los textos
    printf("\n%s\n", remover_tildes(titulo));
    printf("----------------------------------------\n");

    sqlite3_stmt *stmt;
    char sql[1024];
    const char* limit_clause = (limit > 0) ? " LIMIT 1" : "";

    int order_by_columna = (strcmp(order_by, "DESC") != 0 && strcmp(order_by, "ASC") != 0);

    snprintf(sql, sizeof(sql),
             "WITH dias_semana AS ("
             "SELECT 0 AS dia_num, 'Domingo' AS dia_nombre UNION ALL "
             "SELECT 1, 'Lunes' UNION ALL "
             "SELECT 2, 'Martes' UNION ALL "
             "SELECT 3, 'Miercoles' UNION ALL "
             "SELECT 4, 'Jueves' UNION ALL "
             "SELECT 5, 'Viernes' UNION ALL "
             "SELECT 6, 'Sabado'"
             ") "
             "SELECT ds.dia_nombre, "
             "ROUND(COALESCE(AVG(p.%s), 0), 2) AS promedio "
             "FROM dias_semana ds "
             "LEFT JOIN partido p ON CAST(strftime('%%w', substr(p.fecha_hora, 7, 4) || '-' || substr(p.fecha_hora, 4, 2) || '-' || substr(p.fecha_hora, 1, 2)) AS INTEGER) = ds.dia_num "
             "AND p.fecha_hora IS NOT NULL AND p.fecha_hora != '' "
             "GROUP BY ds.dia_num, ds.dia_nombre "
             "ORDER BY %s%s%s",
             columna,
             order_by_columna ? "" : "promedio ",
             order_by,
             limit_clause);

    if (!preparar_stmt_export(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *dia = (const char *)sqlite3_column_text(stmt, 0);
        double promedio = sqlite3_column_double(stmt, 1);

        printf("%-30s : %.2f\n", remover_tildes(dia), promedio);
    }

    sqlite3_finalize(stmt);
    pause_console();
}

// Array de dias de la semana en espanol
const char* dias[] = {"Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"};

/**
 * @brief Funcion auxiliar para ejecutar consultas SQL y mostrar resultados.
 *
 * Esta funcion ejecuta una consulta SQL preparada y muestra los resultados
 * en formato de tabla con el titulo proporcionado.
 *
 * @param titulo El titulo a mostrar antes de los resultados.
 * @param sql La consulta SQL a ejecutar.
 */
static void query(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt;
    char nombre[200];
    int num_cols;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (!preparar_stmt_export(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        return;
    }
    num_cols = sqlite3_column_count(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (num_cols == 1)
        {
            if (sqlite3_column_type(stmt, 0) == SQLITE_INTEGER)
            {
                printf("%d\n", sqlite3_column_int(stmt, 0));
            }
            else if (sqlite3_column_type(stmt, 0) == SQLITE_FLOAT)
            {
                printf("%.2f\n", sqlite3_column_double(stmt, 0));
            }
            else
            {
                snprintf(nombre, sizeof(nombre), "%s", sqlite3_column_text(stmt, 0));
                printf("%s\n", nombre);
            }
        }
        else
        {
            snprintf(nombre, sizeof(nombre), "%s", sqlite3_column_text(stmt, 0));

            // Check if the second column is integer or real
            if (sqlite3_column_type(stmt, 1) == SQLITE_INTEGER)
            {
                printf("%-30s : %d\n",
                       nombre,
                       sqlite3_column_int(stmt, 1));
            }
            else if (sqlite3_column_type(stmt, 1) == SQLITE_FLOAT)
            {
                printf("%-30s : %.2f\n",
                       nombre,
                       sqlite3_column_double(stmt, 1));
            }
            else
            {
                // Fallback to int
                printf("%-30s : %d\n",
                       nombre,
                       sqlite3_column_int(stmt, 1));
            }
        }
    }

    sqlite3_finalize(stmt);
}

static void mostrar_query_simple(const char *header, const char *titulo, const char *sql)
{
    clear_screen();
    print_header(header);
    query(titulo, sql);
    pause_console();
}

#define SQL_CAMISETA_AGREGADA(expr, orden) \
    "SELECT c.nombre, " expr " " \
    "FROM partido p " \
    "JOIN camiseta c ON p.camiseta_id=c.id " \
    "GROUP BY c.id " \
    "ORDER BY 2 " orden " LIMIT 1"

#define SQL_CAMISETA_POR_RESULTADO(resultado) \
    "SELECT c.nombre, COUNT(*) " \
    "FROM partido p " \
    "JOIN camiseta c ON p.camiseta_id=c.id " \
    "WHERE p.resultado = " resultado " " \
    "GROUP BY c.id " \
    "ORDER BY 2 DESC LIMIT 1"

/**
 * @brief Muestra las estadisticas principales de las camisetas.
 *
 * Esta funcion imprime un encabezado y ejecuta varias consultas para mostrar
 * estadisticas como la camiseta con mas goles, asistencias, partidos jugados
 * y la suma de goles mas asistencias. Al final, pausa la consola.
 */
void mostrar_estadisticas_generales()
{
    clear_screen();
    print_header("ESTADISTICAS");
    typedef struct
    {
        const char *titulo;
        const char *sql;
    } StatQuery;

    StatQuery queries[] =
    {
        {
            "Camiseta con mas Goles",
            SQL_CAMISETA_AGREGADA("IFNULL(SUM(p.goles),0)", "DESC")
        },
        {
            "Camiseta con mas Asistencias",
            SQL_CAMISETA_AGREGADA("IFNULL(SUM(p.asistencias),0)", "DESC")
        },
        {
            "Camiseta con mas Partidos",
            SQL_CAMISETA_AGREGADA("COUNT(*)", "DESC")
        },
        {
            "Camiseta con mas Goles + Asistencias",
            SQL_CAMISETA_AGREGADA("IFNULL(SUM(p.goles+p.asistencias),0)", "DESC")
        },
        {
            "Camiseta con mejor Rendimiento General promedio",
            SQL_CAMISETA_AGREGADA("IFNULL(ROUND(AVG(p.rendimiento_general), 2), 0.00)", "DESC")
        },
        {
            "Camiseta con mejor Estado de Animo promedio",
            SQL_CAMISETA_AGREGADA("IFNULL(ROUND(AVG(p.estado_animo), 2), 0.00)", "DESC")
        },
        {
            "Camiseta con menos Cansancio promedio",
            SQL_CAMISETA_AGREGADA("IFNULL(ROUND(AVG(p.cansancio), 2), 0.00)", "ASC")
        },
        {
            "Camiseta con mas Victorias",
            SQL_CAMISETA_POR_RESULTADO("1")
        },
        {
            "Camiseta con mas Empates",
            SQL_CAMISETA_POR_RESULTADO("2")
        },
        {
            "Camiseta con mas Derrotas",
            SQL_CAMISETA_POR_RESULTADO("3")
        },
        {
            "Camiseta mas Sorteada",
            "SELECT c.nombre, c.sorteada "
            "FROM camiseta c "
            "ORDER BY c.sorteada DESC LIMIT 1"
        }
    };

    size_t total = sizeof(queries) / sizeof(queries[0]);
    for (size_t i = 0; i < total; i++)
    {
        query(queries[i].titulo, queries[i].sql);
    }

    pause_console();
}

#undef SQL_CAMISETA_POR_RESULTADO
#undef SQL_CAMISETA_AGREGADA

/**
 * @brief Muestra el total de partidos jugados
 */
void mostrar_total_partidos_jugados()
{
    mostrar_query_simple("TOTAL DE PARTIDOS JUGADOS",
                         "Total de Partidos Jugados",
                         "SELECT COUNT(*) FROM partido");
}

/**
 * @brief Muestra el promedio de goles por partido
 */
void mostrar_promedio_goles_por_partido()
{
    mostrar_query_simple("PROMEDIO DE GOLES POR PARTIDO",
                         "Promedio de Goles por Partido",
                         "SELECT ROUND(AVG(goles), 2) FROM partido");
}

/**
 * @brief Muestra el promedio de asistencias por partido
 */
void mostrar_promedio_asistencias_por_partido()
{
    mostrar_query_simple("PROMEDIO DE ASISTENCIAS POR PARTIDO",
                         "Promedio de Asistencias por Partido",
                         "SELECT ROUND(AVG(asistencias), 2) FROM partido");
}

/**
 * @brief Muestra el promedio de rendimiento_general
 */
void mostrar_promedio_rendimiento_general()
{
    mostrar_query_simple("PROMEDIO DE RENDIMIENTO_GENERAL",
                         "Promedio de Rendimiento General",
                         "SELECT ROUND(AVG(rendimiento_general), 2) FROM partido");
}

/**
 * @brief Muestra el rendimiento promedio por clima
 */
void mostrar_rendimiento_promedio_por_clima()
{
    char sql[1024];
    int written = snprintf(sql, sizeof(sql),
                           "SELECT %s AS clima_texto, ROUND(AVG(rendimiento_general), 2) FROM partido GROUP BY clima ORDER BY clima",
                           get_clima_case_sql());
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("RENDIMIENTO PROMEDIO POR CLIMA",
                         "Rendimiento Promedio por Clima",
                         sql);
}

/**
 * @brief Muestra los goles por clima
 */
void mostrar_goles_por_clima()
{
    char sql[1024];
    int written = snprintf(sql, sizeof(sql),
                           "SELECT %s AS clima_texto, SUM(goles) FROM partido GROUP BY clima ORDER BY clima",
                           get_clima_case_sql());
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("GOLES POR CLIMA",
                         "Goles por Clima",
                         sql);
}

/**
 * @brief Muestra las asistencias por clima
 */
void mostrar_asistencias_por_clima()
{
    char sql[1024];
    int written = snprintf(sql, sizeof(sql),
                           "SELECT %s AS clima_texto, SUM(asistencias) FROM partido GROUP BY clima ORDER BY clima",
                           get_clima_case_sql());
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("ASISTENCIAS POR CLIMA",
                         "Asistencias por Clima",
                         sql);
}

/**
 * @brief Muestra el clima donde se rinde mejor
 */
void mostrar_clima_mejor_rendimiento()
{
    char sql[1024];
    int written = snprintf(sql, sizeof(sql),
                           "SELECT %s AS clima_texto, ROUND(AVG(rendimiento_general), 2) FROM partido GROUP BY clima ORDER BY AVG(rendimiento_general) DESC LIMIT 1",
                           get_clima_case_sql());
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("CLIMA DONDE SE RINDE MEJOR",
                         "Clima con Mejor Rendimiento Promedio",
                         sql);
}

/**
 * @brief Muestra el clima donde se rinde peor
 */
void mostrar_clima_peor_rendimiento()
{
    char sql[1024];
    int written = snprintf(sql, sizeof(sql),
                           "SELECT %s AS clima_texto, ROUND(AVG(rendimiento_general), 2) FROM partido GROUP BY clima ORDER BY AVG(rendimiento_general) ASC LIMIT 1",
                           get_clima_case_sql());
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("CLIMA DONDE SE RINDE PEOR",
                         "Clima con Peor Rendimiento Promedio",
                         sql);
}

/**
 * @brief Muestra el mejor dia de la semana
 */
void mostrar_mejor_dia_semana()
{
    mostrar_por_dia_semana("MEJOR DIA DE LA SEMANA", "rendimiento_general", "DESC", 1);
}

/**
 * @brief Muestra el peor dia de la semana
 */
void mostrar_peor_dia_semana()
{
    mostrar_por_dia_semana("PEOR DIA DE LA SEMANA", "rendimiento_general", "ASC", 1);
}

/**
 * @brief Muestra los goles promedio por dia
 */
void mostrar_goles_promedio_por_dia()
{
    mostrar_por_dia_semana("GOLES PROMEDIO POR DIA", "goles", "ds.dia_num", 0);
}

/**
 * @brief Muestra las asistencias promedio por dia
 */
void mostrar_asistencias_promedio_por_dia()
{
    mostrar_por_dia_semana("ASISTENCIAS PROMEDIO POR DIA", "asistencias", "ds.dia_num", 0);
}

/**
 * @brief Muestra el rendimiento promedio por dia
 */
void mostrar_rendimiento_promedio_por_dia()
{
    mostrar_por_dia_semana("RENDIMIENTO PROMEDIO POR DIA", "rendimiento_general", "ds.dia_num", 0);
}

/**
 * @brief Muestra el rendimiento segun nivel de cansancio
 */
void mostrar_rendimiento_por_nivel_cansancio()
{
    char sql[1024];
    int written = snprintf(sql, sizeof(sql),
                           "SELECT %s AS nivel_cansancio, ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio, COUNT(*) AS partidos FROM partido GROUP BY %s ORDER BY rendimiento_promedio DESC",
                           get_nivel_case_sql("cansancio"), get_nivel_case_sql("cansancio"));
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("RENDIMIENTO POR NIVEL DE CANSANCIO",
                         "Rendimiento por Nivel de Cansancio",
                         sql);
}

/**
 * @brief Muestra los goles con cansancio alto vs bajo
 */
void mostrar_goles_cansancio_alto_vs_bajo()
{
    clear_screen();
    print_header("GOLES CON CANSANCIO ALTO VS BAJO");

    // Usar la funcion para remover tildes de los textos
    printf("\n%s\n", remover_tildes("Goles con Cansancio Alto vs Bajo"));
    printf("----------------------------------------\n");

    // Query modificada para mostrar el formato esperado: Alto: 1, Bajo: 0
    sqlite3_stmt *stmt;
    int num_cols;

    // Consulta para cansancio alto (>7) vs bajo (<=7)
    const char *sql = "SELECT CASE WHEN cansancio > 7 THEN 'Alto' ELSE 'Bajo' END AS nivel_cansancio, "
                      "SUM(goles) AS total_goles, ROUND(AVG(goles), 2) AS promedio_goles, COUNT(*) AS partidos "
                      "FROM partido GROUP BY CASE WHEN cansancio > 7 THEN 'Alto' ELSE 'Bajo' END";

    if (!preparar_stmt_export(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }
    num_cols = sqlite3_column_count(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (num_cols >= 2)
        {
            const char *nivel = (const char *)sqlite3_column_text(stmt, 0);
            int total_goles = sqlite3_column_int(stmt, 1);
            double promedio_goles = sqlite3_column_double(stmt, 2);

            // Mostrar en el formato especificado en la tarea
            printf("%-30s : %d", remover_tildes(nivel), total_goles);

            // Agregar nota para cansancio bajo si hay caida de rendimiento
            if (strcmp(nivel, "Bajo") == 0 && promedio_goles < 1.0)
            {
                printf(", Caida de Rendimiento por Cansancio Acumulado");
            }
            printf("\n");
        }
    }

    sqlite3_finalize(stmt);

    pause_console();
}

/**
 * @brief Muestra los partidos jugados con cansancio alto
 */
void mostrar_partidos_cansancio_alto()
{
    mostrar_query_simple("PARTIDOS JUGADOS CON CANSANCIO ALTO",
                         "Partidos con Cansancio Alto (>7)",
                         "SELECT COUNT(*) AS partidos_cansancio_alto FROM partido WHERE cansancio > 7");
}

/**
 * @brief Muestra la caida de rendimiento por cansancio acumulado
 */
void mostrar_caida_rendimiento_cansancio_acumulado()
{
    clear_screen();
    print_header("CAIDA DE RENDIMIENTO POR CANSANCIO ACUMULADO");

    // Usar la funcion para remover tildes de los textos
    printf("\n%s\n", remover_tildes("Caida de Rendimiento por Cansancio Acumulado"));
    printf("----------------------------------------\n");

    // Comparar rendimiento en partidos recientes vs antiguos con alto cansancio
    sqlite3_stmt *stmt;
    const char *sql = "SELECT 'Recientes (ultimos 5)' AS periodo, ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio FROM (SELECT rendimiento_general FROM partido WHERE cansancio > 7 ORDER BY fecha_hora DESC LIMIT 5) UNION ALL SELECT 'Antiguos (primeros 5)' AS periodo, ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio FROM (SELECT rendimiento_general FROM partido WHERE cansancio > 7 ORDER BY fecha_hora ASC LIMIT 5)";

    if (!preparar_stmt_export(&stmt, sql))
    {
        printf("Error al consultar la base de datos.\n");
        pause_console();
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *periodo = (const char *)sqlite3_column_text(stmt, 0);
        double rendimiento = sqlite3_column_double(stmt, 1);

        printf("%-30s : %.2f", remover_tildes(periodo), rendimiento);
        printf("\n");
    }

    sqlite3_finalize(stmt);

    pause_console();
}

/**
 * @brief Muestra el rendimiento segun estado de animo
 */
void mostrar_rendimiento_por_estado_animo()
{
    char sql[1024];
    int written = snprintf(sql, sizeof(sql),
                           "SELECT %s AS nivel_animo, ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio, COUNT(*) AS partidos FROM partido GROUP BY %s ORDER BY rendimiento_promedio DESC",
                           get_nivel_case_sql("estado_animo"), get_nivel_case_sql("estado_animo"));
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("RENDIMIENTO POR ESTADO DE ANIMO",
                         "Rendimiento por Estado de Animo",
                         sql);
}

/**
 * @brief Muestra los goles segun estado de animo
 */
void mostrar_goles_por_estado_animo()
{
    char sql[1024];
    int written = snprintf(sql, sizeof(sql),
                           "SELECT %s AS nivel_animo, SUM(goles) AS total_goles, ROUND(AVG(goles), 2) AS promedio_goles, COUNT(*) AS partidos FROM partido GROUP BY %s ORDER BY promedio_goles DESC",
                           get_nivel_case_sql("estado_animo"), get_nivel_case_sql("estado_animo"));
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("GOLES POR ESTADO DE ANIMO",
                         "Goles por Estado de Animo",
                         sql);
}

/**
 * @brief Muestra las asistencias segun estado de animo
 */
void mostrar_asistencias_por_estado_animo()
{
    char sql[1024];
    int written = snprintf(sql, sizeof(sql),
                           "SELECT %s AS nivel_animo, SUM(asistencias) AS total_asistencias, ROUND(AVG(asistencias), 2) AS promedio_asistencias, COUNT(*) AS partidos FROM partido GROUP BY %s ORDER BY promedio_asistencias DESC",
                           get_nivel_case_sql("estado_animo"), get_nivel_case_sql("estado_animo"));
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("ASISTENCIAS POR ESTADO DE ANIMO",
                         "Asistencias por Estado de Animo",
                         sql);
}

/**
 * @brief Muestra el estado de animo ideal para jugar
 */
void mostrar_estado_animo_ideal()
{
    char sql[1024];
    int written = snprintf(sql, sizeof(sql),
                           "SELECT %s AS nivel_animo, ROUND(AVG(rendimiento_general), 2) AS rendimiento_promedio FROM partido GROUP BY %s ORDER BY rendimiento_promedio DESC LIMIT 1",
                           get_nivel_case_sql("estado_animo"), get_nivel_case_sql("estado_animo"));
    if (written < 0 || (size_t)written >= sizeof(sql))
    {
        printf("Error: no se pudo construir la consulta SQL completa.\n");
        pause_console();
        return;
    }
    mostrar_query_simple("ESTADO DE ANIMO IDEAL PARA JUGAR",
                         "Estado de Animo Ideal",
                         sql);
}
/**
 * @brief Obtiene el dia de la semana para una fecha dada
 * @param dia Dia del mes (1-31)
 * @param mes Mes del ano (1-12)
 * @param anio Ano (ej. 2023)
 * @return Nombre del dia de la semana en espanol
 */
const char* obtener_dia_semana(int dia, int mes, int anio)
{
    struct tm fecha = {0};
    fecha.tm_mday = dia;
    fecha.tm_mon  = mes - 1;      // Meses: 0-11
    fecha.tm_year = anio - 2023;  // Anos desde 1900

    mktime(&fecha); // Calcula el dia de la semana

    return dias[fecha.tm_wday];
}
