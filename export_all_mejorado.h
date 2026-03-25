/**
 * @file export_all_mejorado.h
 * @brief Interfaz de Programación de Aplicaciones (API) para exportación avanzada de datos
 *
 * Este archivo define la API pública para el sistema de exportación mejorada en MiFutbolC.
 * Proporciona funciones para exportar datos con análisis avanzado y estadísticas mejoradas,
 * permitiendo a los usuarios generar informes completos en múltiples formatos.
 *
 * @note Todas las funciones declaradas aquí son parte de la API pública y pueden ser
 *       llamadas desde otros módulos del sistema. La exportación mejorada incluye
 *       análisis avanzado que no está disponible en las funciones de exportación estándar.
 */

#ifndef EXPORT_ALL_MEJORADO_H
#define EXPORT_ALL_MEJORADO_H

/**
 * @brief Interfaz de usuario para exportación de datos mejorados
 *
 * Inicia un menú interactivo que permite a los usuarios seleccionar entre diferentes
 * opciones de exportación mejorada. Este menú sirve como punto de entrada principal
 * para todas las funciones de exportación avanzada del sistema.
 *
 * @details La función implementa un sistema de menú interactivo que presenta opciones
 *          para exportar diferentes categorías de datos con análisis avanzado. No toma
 *          parámetros ni devuelve valores, ya que su propósito es únicamente presentar
 *          opciones y delegar el control a las funciones específicas.
 *
 * @see exportar_camisetas_todo_mejorado()
 * @see exportar_lesiones_todo_mejorado()
 * @see exportar_todo_mejorado()
 */
void menu_exportar_mejorado();

/**
 * @brief Exportación integral de datos de camisetas con análisis avanzado
 *
 * Exporta los datos de camisetas en todos los formatos disponibles (CSV, TXT, JSON, HTML)
 * incluyendo estadísticas avanzadas y análisis de rendimiento. Esta función centraliza
 * la exportación de datos de camisetas para proporcionar una solución completa.
 *
 * @details Las estadísticas avanzadas incluyen:
 * - Eficiencia de goles y asistencias
 * - Porcentaje de victorias y análisis de lesiones
 * - Métricas de rendimiento por partido
 * - Análisis comparativo de diferentes periodos
 *
 * @note Esta función exporta en todos los formatos mejorados, proporcionando
 *       flexibilidad para diferentes usos y aplicaciones.
 */
void exportar_camisetas_todo_mejorado();

/**
 * @brief Exportación integral de datos de lesiones con análisis de impacto
 *
 * Exporta los datos de lesiones en todos los formatos disponibles (CSV, TXT, JSON, HTML)
 * incluyendo estadísticas avanzadas y análisis de impacto en el rendimiento. Esta función
 * es esencial para el análisis médico y de rendimiento relacionado con lesiones.
 *
 * @details El análisis de impacto incluye:
 * - Evaluación de la gravedad de las lesiones
 * - Comparación de rendimiento antes y después de lesiones
 * - Identificación de patrones de lesiones
 * - Métricas de recuperación y rehabilitación
 *
 * @note Esta función es particularmente útil para equipos médicos y entrenadores
 *       que necesitan evaluar el impacto de las lesiones en el desempeño.
 */
void exportar_lesiones_todo_mejorado();

/**
 * @brief Exportación completa del sistema con análisis avanzado
 *
 * Realiza una exportación exhaustiva de todos los datos del sistema (camisetas y lesiones)
 * en todos los formatos disponibles, incluyendo tanto las versiones mejoradas con análisis
 * avanzado como las versiones originales para mantener compatibilidad.
 *
 * @details Esta función maestra:
 * - Exporta datos mejorados con análisis avanzado para toma de decisiones
 * - Exporta datos originales para compatibilidad con versiones anteriores
 * - Genera archivos en múltiples formatos (CSV, TXT, JSON, HTML)
 * - Proporciona una solución integral para migraciones y análisis completos
 *
 * @warning Esta es la función más completa pero también la más lenta, ya que
 *          exporta todos los datos en todos los formatos disponibles. Se recomienda
 *          usarla solo cuando se necesiten todos los datos y formatos.
 *
 * @see exportar_camisetas_todo_mejorado()
 * @see exportar_lesiones_todo_mejorado()
 */
void exportar_todo_mejorado();

#endif
