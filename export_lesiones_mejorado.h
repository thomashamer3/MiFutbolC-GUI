/**
 * @file export_lesiones_mejorado.h
 * @brief Declaraciones de funciones para exportar lesiones mejoradas
 *
 * Este archivo contiene las declaraciones de funciones para exportar datos de lesiones
 * con estadísticas avanzadas y análisis de impacto en diferentes formatos.
 */

#ifndef EXPORT_LESIONES_MEJORADO_H
#define EXPORT_LESIONES_MEJORADO_H

/**
 * @brief Exporta datos de lesiones a formato CSV con análisis avanzado
 *
 * Exporta los datos de lesiones incluyendo estadísticas avanzadas como
 * impacto en rendimiento, partidos antes y después de la lesión,
 * y comparación de rendimiento.
 *
 * @pre La base de datos debe contener registros en la tabla 'lesion'
 * @post Crea el archivo 'lesiones_mejorado.csv' en el directorio de exportación
 */
void exportar_lesiones_csv_mejorado();

/**
 * @brief Exporta datos de lesiones a formato TXT con análisis avanzado
 *
 * Exporta los datos de lesiones en formato de texto con estadísticas avanzadas
 * y análisis de impacto en un formato legible.
 *
 * @pre La base de datos debe contener registros en la tabla 'lesion'
 * @post Crea el archivo 'lesiones_mejorado.txt' en el directorio de exportación
 */
void exportar_lesiones_txt_mejorado();

/**
 * @brief Exporta datos de lesiones a formato JSON con análisis avanzado
 *
 * Exporta los datos de lesiones en formato JSON con estadísticas avanzadas
 * y análisis de impacto para fácil procesamiento por otras aplicaciones.
 *
 * Estructura del JSON:
 * @code
 * [
 *   {
 *     "id": N,
 *     "jugador": "Nombre",
 *     "tipo": "Tipo de lesión",
 *     "descripcion": "Descripción",
 *     "fecha": "YYYY-MM-DD",
 *     "camiseta_nombre": "Nombre de camiseta",
 *     "partidos_antes_lesion": N,
 *     "partidos_despues_lesion": N,
 *     "rendimiento_antes": N.N,
 *     "rendimiento_despues": N.N,
 *     "impacto_rendimiento": N.N
 *   },
 *   ...
 * ]
 * @endcode
 *
 * @pre La base de datos debe contener registros en la tabla 'lesion'
 * @post Crea el archivo 'lesiones_mejorado.json' en el directorio de exportación
 */
void exportar_lesiones_json_mejorado();

/**
 * @brief Exporta datos de lesiones a formato HTML con análisis avanzado
 *
 * Exporta los datos de lesiones en formato HTML con estadísticas avanzadas
 * y análisis de impacto para visualización en navegadores web.
 *
 * @pre La base de datos debe contener registros en la tabla 'lesion'
 * @post Crea el archivo 'lesiones_mejorado.html' en el directorio de exportación
 */
void exportar_lesiones_html_mejorado();

#endif /* EXPORT_LESIONES_MEJORADO_H */