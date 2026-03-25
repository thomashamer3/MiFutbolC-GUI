/**
 * @file estadisticas_lesiones.h
 * @brief API de análisis estadístico de incidentes médicos deportivos
 *
 * Define interfaz para consultas de agregación lesional utilizando SQL JOINs
 * entre tablas lesion, partido y camiseta, implementando cálculos temporales
 * y análisis correlacional rendimiento-lesión.
 */

#ifndef ESTADISTICAS_LESIONES_H
#define ESTADISTICAS_LESIONES_H

/**
 * @brief Ejecuta análisis estadístico integral de lesiones
 *
 * Coordina múltiples consultas SQL para generar reporte comprehensivo
 * incluyendo métricas demográficas, temporales y de impacto en rendimiento,
 * utilizando funciones de agregación y subconsultas para análisis avanzado.
 */
void mostrar_estadisticas_lesiones();

#endif /* ESTADISTICAS_LESIONES_H */
