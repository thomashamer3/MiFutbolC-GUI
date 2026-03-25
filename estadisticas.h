/**
 * @file estadisticas.h
 * @brief Interfaz de Programación de Aplicaciones (API) para el módulo de estadísticas
 *
 * Este archivo define la API pública para el sistema de visualización de estadísticas
 * en MiFutbolC. Proporciona funciones para navegar a través de diferentes categorías
 * de análisis estadístico, organizadas en una estructura jerárquica de menús.
 *
 * @note Todas las funciones declaradas aquí son parte de la API pública y pueden ser
 *       llamadas desde otros módulos del sistema. No se debe modificar la firma de
 *       estas funciones sin actualizar la documentación correspondiente.
 */

#ifndef ESTADISTICAS_H
#define ESTADISTICAS_H

/**
 * @brief Punto de entrada principal para el módulo de estadísticas
 *
 * Esta función inicia el menú principal de estadísticas, que sirve como
 * centro de navegación para todas las categorías de análisis disponibles.
 * Implementa un sistema de menú interactivo que permite a los usuarios
 * seleccionar entre diferentes tipos de estadísticas.
 *
 * @details La función no toma parámetros ni devuelve valores, ya que su
 *          propósito es únicamente presentar opciones de menú y delegar
 *          el control a las funciones específicas de cada categoría.
 *
 * @see menu_estadisticas_generales()
 * @see menu_estadisticas_partidos()
 * @see menu_estadisticas_goles()
 * @see menu_estadisticas_asistencias()
 * @see menu_estadisticas_rendimiento()
 */
void menu_estadisticas();

/**
 * @brief Menú de estadísticas agregadas y análisis temporal
 *
 * Proporciona acceso a estadísticas generales y análisis por periodos de tiempo.
 * Este sub-menú permite a los usuarios examinar tendencias y patrones a lo largo
 * de diferentes escalas temporales (generales, mensuales, anuales).
 *
 * @details Las estadísticas temporales son esenciales para identificar patrones
 *          de rendimiento y tomar decisiones informadas sobre estrategias de
 *          entrenamiento y planificación a largo plazo.
 *
 * @see mostrar_estadisticas_generales()
 * @see mostrar_estadisticas_por_mes()
 * @see mostrar_estadisticas_por_anio()
 * @see menu_records_rankings()
 */
void menu_estadisticas_generales();

/**
 * @brief Menú de análisis de partidos jugados
 *
 * Ofrece acceso a métricas detalladas sobre los partidos registrados en el sistema.
 * Este sub-menú se enfoca en el análisis de patrones de rendimiento bajo diferentes
 * condiciones de partido, incluyendo factores como cansancio y dificultad.
 *
 * @details El análisis de partidos es crucial para entender cómo factores contextuales
 *          afectan el desempeño, permitiendo ajustar estrategias para futuros encuentros
 *          y optimizar la preparación del equipo.
 *
 * @see mostrar_total_partidos_jugados()
 * @see mostrar_partidos_cansancio_alto()
 * @see mostrar_partidos_outliers()
 * @see mostrar_partidos_exigentes_bien_rendidos()
 * @see mostrar_partidos_faciles_mal_rendidos()
 */
void menu_estadisticas_partidos();

/**
 * @brief Menú de análisis de productividad ofensiva
 *
 * Proporciona acceso a estadísticas relacionadas con la anotación de goles y
 * la efectividad ofensiva. Este sub-menú permite analizar la productividad
 * de anotación en correlación con diversos factores contextuales.
 *
 * @details El análisis de goles es fundamental para evaluar la efectividad del juego
 *          ofensivo y entender cómo diferentes variables (clima, cansancio, estado de ánimo)
 *          afectan la capacidad de anotación del equipo.
 *
 * @see mostrar_promedio_goles_por_partido()
 * @see mostrar_goles_por_clima()
 * @see mostrar_goles_promedio_por_dia()
 * @see mostrar_goles_cansancio_alto_vs_bajo()
 * @see mostrar_goles_por_estado_animo()
 * @see mostrar_eficiencia_goles_vs_rendimiento()
 */
void menu_estadisticas_goles();

/**
 * @brief Menú de análisis de juego en equipo
 *
 * Ofrece acceso a estadísticas relacionadas con las asistencias y la creación
 * de oportunidades de gol. Este sub-menú se enfoca en métricas que evalúan
 * la calidad del juego colectivo y la efectividad del trabajo en equipo.
 *
 * @details Las asistencias son un indicador clave del juego en equipo y la capacidad
 *          de crear oportunidades de gol. Este análisis ayuda a evaluar la efectividad
 *          del juego ofensivo colectivo bajo diferentes condiciones.
 *
 * @see mostrar_promedio_asistencias_por_partido()
 * @see mostrar_asistencias_por_clima()
 * @see mostrar_asistencias_promedio_por_dia()
 * @see mostrar_asistencias_por_estado_animo()
 * @see mostrar_eficiencia_asistencias_vs_cansancio()
 */
void menu_estadisticas_asistencias();

/**
 * @brief Menú de análisis de rendimiento global
 *
 * Proporciona acceso a un análisis exhaustivo del rendimiento bajo diversas condiciones.
 * Este es el sub-menú más completo, ya que el rendimiento es la métrica más importante
 * para evaluar el desempeño global del equipo.
 *
 * @details El análisis de rendimiento ofrece información detallada sobre cómo factores
 *          externos e internos afectan el desempeño, permitiendo optimizar el entrenamiento
 *          y la preparación para maximizar el rendimiento en diferentes contextos.
 *
 * @see mostrar_promedio_rendimiento_general()
 * @see mostrar_rendimiento_promedio_por_clima()
 * @see mostrar_clima_mejor_rendimiento()
 * @see mostrar_clima_peor_rendimiento()
 * @see mostrar_mejor_dia_semana()
 * @see mostrar_peor_dia_semana()
 * @see mostrar_rendimiento_promedio_por_dia()
 * @see mostrar_rendimiento_por_nivel_cansancio()
 * @see mostrar_caida_rendimiento_cansancio_acumulado()
 * @see mostrar_rendimiento_por_estado_animo()
 * @see mostrar_estado_animo_ideal()
 * @see mostrar_consistencia_rendimiento()
 * @see mostrar_dependencia_contexto()
 * @see mostrar_impacto_real_cansancio()
 * @see mostrar_impacto_real_estado_animo()
 * @see mostrar_rendimiento_por_esfuerzo()
 */
void menu_estadisticas_rendimiento();

#endif
