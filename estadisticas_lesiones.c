#include "estadisticas_lesiones.h"
#include "db.h"
#include "utils.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *NOMBRES_MESES[] =
{
    "", "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
    "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"
};

static const char *mes_a_nombre(const char *mes_str)
{
    int mes = mes_str ? atoi(mes_str) : 0;
    if (mes >= 1 && mes <= 12)
    {
        return NOMBRES_MESES[mes];
    }
    return "Desconocido";
}

static double calcular_julianday(const char *fecha)
{
    sqlite3_stmt *stmt_jd;
    double jd = 0;

    if (!preparar_stmt_export(&stmt_jd, "SELECT julianday(?)"))
    {
        return 0;
    }

    sqlite3_bind_text(stmt_jd, 1, fecha, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt_jd) == SQLITE_ROW)
    {
        jd = sqlite3_column_double(stmt_jd, 0);
    }
    sqlite3_finalize(stmt_jd);
    return jd;
}

static double calcular_promedio_rendimiento(const char *sql)
{
    sqlite3_stmt *stmt;
    double avg = 0;

    if (!preparar_stmt_export(&stmt, sql))
    {
        return 0;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        avg = sqlite3_column_double(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return avg;
}

/**
 * @brief Cuenta total de incidentes medicos registrados
 *
 * Proporciona metrica base para evaluacion de frecuencia de lesiones
 * y contextualizacion de otros indicadores estadisticos.
 */
static void mostrar_total_lesiones()
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, "SELECT COUNT(*) FROM lesion"))
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Total de lesiones: %d\n", sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Clasifica incidentes medicos por categorias diagnosticas
 *
 * Facilita identificacion de patrones de tipos de lesiones mas frecuentes
 * para diseno de programas preventivos especificos.
 */
static void mostrar_lesiones_por_tipo()
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, "SELECT tipo, COUNT(*) FROM lesion GROUP BY tipo ORDER BY COUNT(*) DESC"))
    {
        return;
    }
    printf("Lesiones por tipo:\n");
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("  %s: %d\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Distribuye incidentes medicos por jugadores/camisetas
 *
 * Identifica jugadores con mayor susceptibilidad a lesiones para
 * intervenciones preventivas individualizadas.
 */
static void mostrar_lesiones_por_camiseta()
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt,
                              "SELECT c.numero, c.nombre, COUNT(l.id) FROM camiseta c LEFT JOIN lesion l ON c.id = l.camiseta_id GROUP BY c.id ORDER BY COUNT(l.id) DESC"))
    {
        return;
    }
    printf("Lesiones por camiseta:\n");
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("  Camiseta %d (%s): %d\n", sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 2));
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Analiza distribucion temporal mensual de incidentes
 *
 * Detecta periodos de mayor riesgo estacional para optimizar
 * preparacion fisica y estrategias preventivas.
 */
static void mostrar_lesiones_por_mes()
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, "SELECT strftime('%m', substr(fecha,7,4)||'-'||substr(fecha,4,2)||'-'||substr(fecha,1,2)) as mes, COUNT(*) FROM lesion GROUP BY mes ORDER BY COUNT(*) DESC"))
    {
        return;
    }
    printf("Lesiones por mes:\n");
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *mes_str = (const char *)sqlite3_column_text(stmt, 0);
        printf("%s (%d)\n", mes_a_nombre(mes_str), sqlite3_column_int(stmt, 1));
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Identifica periodo de mayor riesgo lesional
 *
 * Determina mes con mayor incidencia para focalizar medidas
 * preventivas y ajuste de cargas de entrenamiento.
 */
static void mostrar_mes_con_mas_lesiones()
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, "SELECT strftime('%m', substr(fecha,7,4)||'-'||substr(fecha,4,2)||'-'||substr(fecha,1,2)) as mes, COUNT(*) FROM lesion GROUP BY mes ORDER BY COUNT(*) DESC LIMIT 1"))
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *mes_str = (const char *)sqlite3_column_text(stmt, 0);
        printf("Mes Con mas Lesiones:%s\n", mes_a_nombre(mes_str));
    }
    else
    {
        printf("Ninguno\n");
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Evalua frecuencia temporal de recidivas lesionales
 *
 * Calcula intervalos promedio entre lesiones consecutivas para
 * identificar patrones de recuperacion y riesgo de reinjury.
 */
static void mostrar_tiempo_promedio_entre_lesiones()
{
    // Obtener todas las fechas de lesiones ordenadas por camiseta y fecha
    sqlite3_stmt *stmt;
    if (!preparar_stmt_export(&stmt, "SELECT camiseta_id, fecha FROM lesion ORDER BY camiseta_id, fecha"))
    {
        return;
    }

    double total_dias = 0;
    int count = 0;
    int prev_camiseta = -1;
    double prev_jd = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int camiseta = sqlite3_column_int(stmt, 0);
        const char* fecha = (const char*)sqlite3_column_text(stmt, 1);
        double jd = calcular_julianday(fecha);

        if (camiseta == prev_camiseta && prev_jd > 0)
        {
            total_dias += jd - prev_jd;
            count++;
        }
        prev_camiseta = camiseta;
        prev_jd = jd;
    }
    sqlite3_finalize(stmt);

    if (count > 0)
    {
        printf("Tiempo promedio entre lesiones: %.1f dias\n", total_dias / count);
    }
    else
    {
        printf("Tiempo promedio entre lesiones: N/A (menos de 2 lesiones)\n");
    }
}

/**
 * @brief Evalua impacto lesional en rendimiento atletico
 *
 * Compara metricas de produccion (goles + asistencias) antes y despues
 * de incidentes medicos para cuantificar efectos en desempeno deportivo.
 */
static double calcular_rendimiento_promedio_antes()
{
    return calcular_promedio_rendimiento(
               "SELECT AVG(p.goles + p.asistencias) FROM partido p JOIN lesion l ON p.camiseta_id = l.camiseta_id WHERE p.fecha < l.fecha");
}

/**
 * @brief Evalua impacto lesional en rendimiento atletico
 *
 * Compara metricas de produccion (goles + asistencias) antes y despues
 * de incidentes medicos para cuantificar efectos en desempeno deportivo.
 */
static double calcular_rendimiento_promedio_despues()
{
    return calcular_promedio_rendimiento(
               "SELECT AVG(p.goles + p.asistencias) FROM partido p JOIN lesion l ON p.camiseta_id = l.camiseta_id WHERE p.fecha > l.fecha");
}

/**
 * @brief Muestra rendimiento promedio antes y despues de lesiones
 *
 * Proporciona evidencia cuantitativa sobre impacto de lesiones
 * en capacidad productiva del jugador para planificacion de rehabilitacion.
 */
static void mostrar_rendimiento_promedio()
{
    double antes = calcular_rendimiento_promedio_antes();
    double despues = calcular_rendimiento_promedio_despues();
    printf("Rendimiento promedio antes de una lesion: %.2f\n", antes);
    printf("Rendimiento promedio despues de una lesion: %.2f\n", despues);
    printf("Baja el rendimiento previo a una lesion? %s\n", (antes < despues) ? "Si" : "No");
}

/**
 * @brief Ejecuta analisis integral de estadisticas lesionales
 *
 * Coordina multiples consultas estadisticas para proporcionar
 * panorama completo de patrones de lesiones y sus correlaciones
 * con rendimiento deportivo.
 */
void mostrar_estadisticas_lesiones()
{
    clear_screen();
    print_header("ESTADISTICAS DE LESIONES");

    mostrar_total_lesiones();
    printf("\n");
    mostrar_lesiones_por_tipo();
    printf("\n");
    mostrar_lesiones_por_camiseta();
    printf("\n");
    mostrar_lesiones_por_mes();
    printf("\n");
    mostrar_mes_con_mas_lesiones();
    printf("\n");
    mostrar_tiempo_promedio_entre_lesiones();
    printf("\n");
    mostrar_rendimiento_promedio();

    pause_console();
}
