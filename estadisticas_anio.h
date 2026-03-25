/**
 * @file estadisticas_anio.h
 * @brief API de análisis estadístico anual de rendimiento deportivo
 *
 * Define interfaz para consultas de agregación temporal por año,
 * implementando JOINs SQL y funciones de agrupamiento para métricas
 * longitudinales de partidos y camisetas.
 */

#ifndef ESTADISTICAS_ANIO_H
#define ESTADISTICAS_ANIO_H

/**
 * @brief Ejecuta consulta de estadísticas agrupadas por año
 *
 * Realiza agregación SQL con funciones SUM, COUNT y AVG sobre tabla partido,
 * unida con camiseta para presentación ordenada por año descendente y
 * goles totales descendentes dentro de cada año.
 */
void mostrar_estadisticas_por_anio();

#endif /* ESTADISTICAS_ANIO_H */
