/**
 * @file export_lesiones.h
 * @brief Declaraciones de funciones para exportar datos de lesiones
 *
 * Este archivo contiene las declaraciones de las funciones de exportación
 * para lesiones en diferentes formatos: CSV, TXT, JSON y HTML.
 */

#ifndef EXPORT_LESIONES_H
#define EXPORT_LESIONES_H

/** @name Funciones de exportación de lesiones */
/** @{ */

/**
 * @brief Exporta la lista de lesiones a formato CSV
 *
 * Genera un archivo CSV con todos los registros de lesiones,
 * incluyendo ID, jugador, tipo, descripción y fecha.
 *
 * @pre La base de datos debe contener registros en la tabla 'lesion'
 * @post Crea el archivo 'lesiones.csv' en el directorio de exportación
 */
void exportar_lesiones_csv();

/**
 * @brief Exporta la lista de lesiones a formato TXT
 *
 * Genera un archivo de texto plano con la lista formateada de lesiones.
 *
 * @pre La base de datos debe contener registros en la tabla 'lesion'
 * @post Crea el archivo 'lesiones.txt' en el directorio de exportación
 */
void exportar_lesiones_txt();

/**
 * @brief Exporta la lista de lesiones a formato JSON
 *
 * Genera un archivo JSON con un array de objetos representando las lesiones.
 *
 * Estructura del JSON:
 * @code
 * [
 *   {
 *     "id": N,
 *     "jugador": "Nombre",
 *     "tipo": "Tipo de lesión",
 *     "descripcion": "Descripción",
 *     "fecha": "YYYY-MM-DD"
 *   },
 *   ...
 * ]
 * @endcode
 *
 * @pre La base de datos debe contener registros en la tabla 'lesion'
 * @post Crea el archivo 'lesiones.json' en el directorio de exportación
 */
void exportar_lesiones_json();

/**
 * @brief Exporta la lista de lesiones a formato HTML
 *
 * Genera una página HTML con una tabla que muestra todas las lesiones.
 *
 * @pre La base de datos debe contener registros en la tabla 'lesion'
 * @post Crea el archivo 'lesiones.html' en el directorio de exportación
 */
void exportar_lesiones_html();

/** @} */ /* End of Doxygen group */

#endif /* EXPORT_LESIONES_H */