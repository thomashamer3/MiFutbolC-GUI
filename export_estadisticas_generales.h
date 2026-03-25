/**
 * @file export_estadisticas_generales.h
 * @brief Funciones para exportar estadísticas generales, por mes y por año
 *
 * Este módulo proporciona funcionalidades para exportar estadísticas de partidos
 * en múltiples formatos (CSV, TXT, JSON, HTML). Las estadísticas incluyen:
 * - Métricas generales (goles, asistencias, partidos jugados)
 * - Rendimiento (rendimiento general, estado de ánimo, cansancio)
 * - Resultados (victorias, empates, derrotas)
 *
 * @note Requiere que la base de datos esté inicializada (variable global 'db')
 */

#ifndef EXPORT_ESTADISTICAS_GENERALES_H
#define EXPORT_ESTADISTICAS_GENERALES_H

/**
 * @brief Exporta las estadísticas generales a formato CSV
 *
 * Genera un archivo CSV con las siguientes categorías:
 * - Camiseta con más goles
 * - Camiseta con más asistencias
 * - Camiseta con más partidos
 * - Camiseta con mayor suma de goles+asistencias
 * - Mejor rendimiento general promedio
 * - Mejor estado de ánimo promedio
 * - Menor cansancio promedio
 * - Más victorias, empates, derrotas
 * - Camiseta más sorteada
 *
 * @pre La base de datos debe contener registros en la tabla 'partido'
 * @post Crea el archivo 'estadisticas_generales.csv' en el directorio de exportación
 */
void exportar_estadisticas_generales_csv();

/**
 * @brief Exporta las estadísticas generales a formato TXT
 *
 * Genera un archivo de texto formateado con el mismo contenido que la versión CSV,
 * pero con un formato más legible para visualización directa.
 *
 * @pre La base de datos debe contener registros en la tabla 'partido'
 * @post Crea el archivo 'estadisticas_generales.txt' en el directorio de exportación
 */
void exportar_estadisticas_generales_txt();

/**
 * @brief Exporta las estadísticas generales a formato JSON
 *
 * Genera un archivo JSON estructurado con las estadísticas generales.
 * Útil para procesamiento automatizado e integración con otras aplicaciones.
 *
 * Estructura del JSON:
 * @code
 * {
 *   "estadisticas_generales": {
 *     "mas_goles": {"camiseta": "...", "valor": N},
 *     "mas_asistencias": {...},
 *     ...
 *   }
 * }
 * @endcode
 *
 * @pre La base de datos debe contener registros en la tabla 'partido'
 * @post Crea el archivo 'estadisticas_generales.json' en el directorio de exportación
 */
void exportar_estadisticas_generales_json();

/**
 * @brief Exporta las estadísticas generales a formato HTML
 *
 * Genera una página HTML con una tabla interactiva mostrando las estadísticas.
 * Ideal para visualización web o informes visuales.
 *
 * @pre La base de datos debe contener registros en la tabla 'partido'
 * @post Crea el archivo 'estadisticas_generales.html' en el directorio de exportación
 */
void exportar_estadisticas_generales_html();

/**
 * @brief Exporta las estadísticas mensuales a formato CSV
 *
 * Agrupa las estadísticas por mes-año (formato MM-AAAA) y camiseta.
 * Incluye: partidos jugados, total de goles/asistencias y promedios.
 *
 * @pre La base de datos debe contener registros en la tabla 'partido'
 * @post Crea el archivo 'estadisticas_por_mes.csv' en el directorio de exportación
 */
void exportar_estadisticas_por_mes_csv();

/**
 * @brief Exporta las estadísticas mensuales a formato TXT
 *
 * Archivo de texto formateado con estadísticas agrupadas por mes-año.
 *
 * @pre La base de datos debe contener registros en la tabla 'partido'
 * @post Crea el archivo 'estadisticas_por_mes.txt' en el directorio de exportación
 */
void exportar_estadisticas_por_mes_txt();

/**
 * @brief Exporta las estadísticas mensuales a formato JSON
 *
 * JSON anidado por mes-año, conteniendo estadísticas de cada camiseta.
 *
 * @pre La base de datos debe contener registros en la tabla 'partido'
 * @post Crea el archivo 'estadisticas_por_mes.json' en el directorio de exportación
 */
void exportar_estadisticas_por_mes_json();

/**
 * @brief Exporta las estadísticas mensuales a formato HTML
 *
 * Página HTML con tablas por cada mes-año, mostrando estadísticas mensuales.
 *
 * @pre La base de datos debe contener registros en la tabla 'partido'
 * @post Crea el archivo 'estadisticas_por_mes.html' en el directorio de exportación
 */
void exportar_estadisticas_por_mes_html();

/**
 * @brief Exporta las estadísticas anuales a formato CSV
 *
 * Agrupa las estadísticas por año y camiseta.
 * Incluye: partidos, totales/promedios de goles/asistencias y récord V-E-D.
 *
 * @pre La base de datos debe contener registros en la tabla 'partido'
 * @post Crea el archivo 'estadisticas_por_anio.csv' en el directorio de exportación
 */
void exportar_estadisticas_por_anio_csv();

/**
 * @brief Exporta las estadísticas anuales a formato TXT
 *
 * Archivo de texto formateado con estadísticas agrupadas por año.
 *
 * @pre La base de datos debe contener registros en la tabla 'partido'
 * @post Crea el archivo 'estadisticas_por_anio.txt' en el directorio de exportación
 */
void exportar_estadisticas_por_anio_txt();

/**
 * @brief Exporta las estadísticas anuales a formato JSON
 *
 * JSON anidado por año, conteniendo estadísticas detalladas de cada camiseta.
 *
 * @pre La base de datos debe contener registros en la tabla 'partido'
 * @post Crea el archivo 'estadisticas_por_anio.json' en el directorio de exportación
 */
void exportar_estadisticas_por_anio_json();

/**
 * @brief Exporta las estadísticas anuales a formato HTML
 *
 * Página HTML con tablas por cada año, mostrando estadísticas anuales completas.
 *
 * @pre La base de datos debe contener registros en la tabla 'partido'
 * @post Crea el archivo 'estadisticas_por_anio.html' en el directorio de exportación
 */
void exportar_estadisticas_por_anio_html();

#endif /* EXPORT_ESTADISTICAS_GENERALES_H */