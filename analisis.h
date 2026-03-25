/**
 * @file analisis.h
 * @brief API de análisis estadístico de rendimiento deportivo
 *
 * Proporciona interfaz para análisis cuantitativo de datos de partidos,
 * incluyendo métricas comparativas, patrones temporales y evaluación
 * de tendencias de rendimiento utilizando consultas SQL optimizadas
 * sobre base de datos SQLite3.
 */

#ifndef ANALISIS_H
#define ANALISIS_H

/**
 * @brief Ejecuta análisis integral de rendimiento
 *
 * Realiza comparación estadística entre últimos 5 partidos vs promedio general,
 * calcula rachas de victorias/derrotas y genera retroalimentación motivacional
 * basada en algoritmos de comparación de métricas clave.
 */
void mostrar_analisis();

/**
 * @brief Interfaz para análisis temporal de métricas deportivas
 *
 * Implementa análisis longitudinal agrupado por períodos mensuales,
 * permitiendo identificación de patrones estacionales y tendencias
 * de evolución en variables como goles, asistencias y rendimiento general.
 */
void mostrar_evolucion_temporal();

/**
 * @brief Visualiza evolución mensual de estadística de gol
 *
 * Consulta base de datos para agrupar y promediar métricas de gol
 * por períodos mensuales, ordenados cronológicamente descendente.
 */
void evolucion_mensual_goles();

/**
 * @brief Visualiza evolución mensual de estadística de asistencia
 *
 * Consulta base de datos para agrupar y promediar métricas de asistencia
 * por períodos mensuales, ordenados cronológicamente descendente.
 */
void evolucion_mensual_asistencias();

/**
 * @brief Visualiza evolución mensual de rendimiento general
 *
 * Consulta base de datos para agrupar y promediar métricas de rendimiento
 * por períodos mensuales, ordenados cronológicamente descendente.
 */
void evolucion_mensual_rendimiento();

/**
 * @brief Identifica mes histórico de máximo rendimiento
 *
 * Ejecuta consulta SQL con agregación GROUP BY para determinar
 * el mes con mayor promedio de rendimiento_general histórico.
 */
void mejor_mes_historico();

/**
 * @brief Identifica mes histórico de mínimo rendimiento
 *
 * Ejecuta consulta SQL con agregación GROUP BY para determinar
 * el mes con menor promedio de rendimiento_general histórico.
 */
void peor_mes_historico();

/**
 * @brief Compara rendimiento semestral inicio vs fin de año
 *
 * Divide datos en dos períodos semestrales (Ene-Jun vs Jul-Dic)
 * y calcula métricas comparativas para detectar variaciones
 * cíclicas anuales en el rendimiento deportivo.
 */
void inicio_vs_fin_anio();

/**
 * @brief Evalúa impacto de condiciones climáticas en rendimiento
 *
 * Clasifica partidos por temporada climática (fríos: Jun-Sep,
 * cálidos: Dic-Abr) y compara métricas promedio para identificar
 * correlaciones entre variables ambientales y rendimiento atlético.
 */
void meses_frios_vs_calidos();

/**
 * @brief Sintetiza trayectoria completa de desarrollo del jugador
 *
 * Calcula métricas globales de carrera incluyendo total de partidos,
 * promedios históricos y análisis de tendencia mediante comparación
 * de primeros vs últimos partidos para evaluar evolución longitudinal.
 */
void progreso_total_jugador();

#endif
