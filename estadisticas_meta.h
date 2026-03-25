/**
 * @file estadisticas_meta.h
 * @brief Declaraciones de funciones para estadísticas avanzadas y meta-análisis
 *
 * Este archivo contiene las declaraciones de las funciones relacionadas con
 * el análisis avanzado de estadísticas de fútbol.
 */

#ifndef ESTADISTICAS_META_H
#define ESTADISTICAS_META_H

/**
 * @brief Muestra la consistencia del rendimiento (variabilidad)
 */
void mostrar_consistencia_rendimiento();

/**
 * @brief Muestra los partidos atípicos (muy buenos/muy malos)
 */
void mostrar_partidos_outliers();

/**
 * @brief Muestra la dependencia del contexto
 */
void mostrar_dependencia_contexto();

/**
 * @brief Muestra el impacto real del cansancio
 */
void mostrar_impacto_real_cansancio();

/**
 * @brief Muestra el impacto real del estado de ánimo
 */
void mostrar_impacto_real_estado_animo();

/**
 * @brief Muestra la eficiencia: goles por partido vs rendimiento
 */
void mostrar_eficiencia_goles_vs_rendimiento();

/**
 * @brief Muestra la eficiencia: asistencias por partido vs cansancio
 */
void mostrar_eficiencia_asistencias_vs_cansancio();

/**
 * @brief Muestra el rendimiento obtenido por esfuerzo
 */
void mostrar_rendimiento_por_esfuerzo();

/**
 * @brief Muestra partidos exigentes bien rendidos
 */
void mostrar_partidos_exigentes_bien_rendidos();

/**
 * @brief Muestra partidos fáciles mal rendidos
 */
void mostrar_partidos_faciles_mal_rendidos();

#endif
