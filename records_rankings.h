/**
 * @file records_rankings.h
 * @brief Módulo de récords, rankings y análisis comparativo de desempeño
 *
 * Define interfaz para consultas analíticas que identifican y clasifican
 * registros extremos (máximos y mínimos) en base de datos de partidos,
 * canchas, camisetas y períodos de tiempo. Implementa rankings de
 * desempeño comparativo para análisis competitivo.
 *
 * @author MiFutbolC Development Team
 * @version 1.0
 * @date 28 de enero de 2026
 *
 * @section analisis Tipos de Análisis
 * - **Récords Individuales**: Máximos goles, asistencias en un partido
 * - **Mejores/Peores Combinaciones**: Análisis de cancha-camiseta
 * - **Rankings Temporales**: Mejor/peor temporada por rendimiento
 * - **Extremos de Rendimiento**: Partidos anómalamente buenos/malos
 * - **Factores Contextuales**: Rankings por clima, cansancio
 * - **Correlaciones**: Combinaciones de variables de mayor impacto
 *
 * @note Utiliza agregaciones SQL para cálculos complejos
 * @see estadisticas.h, export_records_rankings.h
 */

#ifndef RECORDS_RANKINGS_H
#define RECORDS_RANKINGS_H

/**
 * @brief Interfaz menú principal para visualización de récords y rankings
 *
 * Presenta menú interactivo con opciones para:
 * - Récords individuales (goles, asistencias)
 * - Mejores/peores combinaciones
 * - Rankings temporales
 * - Análisis de factores contextuales
 * - Exportación de rankings
 *
 * @details Utiliza ejecutar_menu() para navegación delegada
 */
void menu_records_rankings();

/**
 * @brief Identifica y muestra máximo de goles en un único partido
 *
 * Ejecuta consulta agregada SELECT MAX(goles) para obtener y mostrar
 * récord histórico de goles marcados en un partido individual,
 * incluyendo contexto de ese partido (fecha, cancha, camiseta).
 *
 * @details Información mostrada:
 * - Número de goles (valor máximo)
 * - Fecha del partido
 * - Cancha donde ocurrió
 * - Camiseta en ese partido
 * - Contexto (clima, cansancio, rendimiento)
 */
void mostrar_record_goles_partido();

/**
 * @brief Identifica y muestra máximo de asistencias en un partido
 *
 * Similar a record de goles, obtiene máximo de asistencias registradas
 * en un único partido con contexto completo.
 *
 * @details Información mostrada:
 * - Número de asistencias (valor máximo)
 * - Fecha, cancha y camiseta asociadas
 * - Contexto del desempeño en ese partido
 *
 * @see mostrar_record_goles_partido()
 */
void mostrar_record_asistencias_partido();

/**
 * @brief Analiza y muestra mejor combinación cancha-camiseta
 *
 * Ejecuta análisis de correlación entre parejas (cancha, camiseta)
 * para identificar combinación con mayor rendimiento promedio.
 *
 * @details Cálculo:
 * - GROUP BY cancha, camiseta
 * - ORDER BY AVG(rendimiento_general) DESC LIMIT 1
 * - Muestra rendimiento promedio de esa combinación
 *
 * @note Requiere al menos 1 partido en combinación para incluir
 */
void mostrar_mejor_combinacion_cancha_camiseta();

/**
 * @brief Analiza y muestra peor combinación cancha-camiseta
 *
 * Equivalente a mejor combinación pero identifica pareja con
 * menor rendimiento promedio histórico.
 *
 * @see mostrar_mejor_combinacion_cancha_camiseta()
 */
void mostrar_peor_combinacion_cancha_camiseta();

/**
 * @brief Calcula y muestra mejor período temporal (mejor temporada)
 *
 * Divide histórico en períodos temporales (meses, años) y
 * calcula rendimiento promedio por período, mostrando el mejor.
 *
 * @details Análisis:
 * - GROUP BY MONTH/YEAR de fecha
 * - ORDER BY AVG(rendimiento_general) DESC LIMIT 1
 * - Muestra período y métricas del mejor desempeño
 */
void mostrar_mejor_temporada();

/**
 * @brief Calcula y muestra peor período temporal (peor temporada)
 *
 * Inverso a mejor temporada, identifica período con menor
 * rendimiento promedio.
 *
 * @see mostrar_mejor_temporada()
 */
void mostrar_peor_temporada();

/**
 * @brief Muestra el partido con mejor rendimiento_general histórico
 *
 * Obtiene partido con valor máximo en campo rendimiento_general,
 * mostrando desempeño excepcional puntual.
 */
void mostrar_partido_peor_rendimiento_general();

/**
 * @brief Muestra el partido con mejor combinación (goles + asistencias)
 */
void mostrar_partido_mejor_combinacion_goles_asistencias();

/**
 * @brief Muestra los partidos sin goles
 */
void mostrar_partidos_sin_goles();

/**
 * @brief Muestra los partidos sin asistencias
 */
void mostrar_partidos_sin_asistencias();

/**
 * @brief Muestra la mejor racha goleadora
 */
void mostrar_mejor_racha_goleadora();

/**
 * @brief Muestra la peor racha
 */
void mostrar_peor_racha();

/**
 * @brief Muestra los partidos consecutivos anotando
 */
void mostrar_partidos_consecutivos_anotando();

#endif /* RECORDS_RANKINGS_H */
