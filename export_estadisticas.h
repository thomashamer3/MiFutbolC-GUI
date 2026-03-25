/**
 * @file export_estadisticas.h
 * @brief Funciones para exportar estadísticas básicas
 *
 * Este módulo proporciona funcionalidades para exportar estadísticas de partidos
 * en múltiples formatos (CSV, TXT, JSON, HTML). Las estadísticas incluyen:
 * - Métricas por camiseta (goles, asistencias, partidos jugados)
 * - Resultados (victorias, empates, derrotas)
 *
 * @note Requiere que la base de datos esté inicializada (variable global 'db')
 */

#ifndef EXPORT_ESTADISTICAS_H
#define EXPORT_ESTADISTICAS_H

/**
 * @brief Exporta las estadísticas a formato CSV
 *
 * Genera un archivo CSV con las estadísticas agrupadas por camiseta,
 * incluyendo nombre, suma de goles, suma de asistencias, número de partidos,
 * victorias, empates y derrotas.
 *
 * @pre La base de datos debe contener registros en la tabla 'partido'
 * @post Crea el archivo 'estadisticas.csv' en el directorio de exportación
 */
void exportar_estadisticas_csv();

/**
 * @brief Exporta las estadísticas a formato TXT
 *
 * Genera un archivo de texto formateado con el mismo contenido que la versión CSV,
 * pero con un formato más legible para visualización directa.
 *
 * @pre La base de datos debe contener registros en la tabla 'partido'
 * @post Crea el archivo 'estadisticas.txt' en el directorio de exportación
 */
void exportar_estadisticas_txt();

/**
 * @brief Exporta las estadísticas a formato JSON
 *
 * Genera un archivo JSON estructurado con las estadísticas generales.
 * Útil para procesamiento automatizado e integración con otras aplicaciones.
 *
 * Estructura del JSON:
 * @code
 * [
 *   {
 *     "camiseta": "Nombre",
 *     "goles": N,
 *     "asistencias": N,
 *     "partidos": N,
 *     "victorias": N,
 *     "empates": N,
 *     "derrotas": N
 *   },
 *   ...
 * ]
 * @endcode
 *
 * @pre La base de datos debe contener registros en la tabla 'partido'
 * @post Crea el archivo 'estadisticas.json' en el directorio de exportación
 */
void exportar_estadisticas_json();

/**
 * @brief Exporta las estadísticas a formato HTML
 *
 * Genera una página HTML con una tabla interactiva mostrando las estadísticas.
 * Ideal para visualización web o informes visuales.
 *
 * @pre La base de datos debe contener registros en la tabla 'partido'
 * @post Crea el archivo 'estadisticas.html' en el directorio de exportación
 */
void exportar_estadisticas_html();

#endif /* EXPORT_ESTADISTICAS_H */