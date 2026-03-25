/**
 * @file import.h
 * @brief Declaraciones de funciones para importar datos en MiFutbolC
 *
 * Este archivo contiene las declaraciones de las funciones de importación
 * que permiten cargar los datos del sistema desde archivos en diferentes formatos.
 * Las funciones están organizadas por tipo de dato y formato de archivo.
 */

#ifndef IMPORT_H
#define IMPORT_H

/** @name Funciones de importación de camisetas */
/** @{ */

/**
 * @brief Importa camisetas desde archivo JSON
 *
 * Lee el archivo JSON de camisetas y las inserta en la base de datos.
 */
void importar_camisetas_json();

/** @} */

/** @name Funciones de importación de partidos */
/** @{ */

/**
 * @brief Importa partidos desde archivo JSON
 *
 * Lee el archivo JSON de partidos y los inserta en la base de datos.
 */
void importar_partidos_json();

/**
 * @brief Importa lesiones desde archivo JSON
 *
 * Lee el archivo JSON de lesiones y las inserta en la base de datos.
 */
void importar_lesiones_json();

/**
 * @brief Importa estadísticas desde archivo JSON
 *
 * Lee el archivo JSON de estadísticas y las procesa.
 */
void importar_estadisticas_json();

/** @} */

/** @name Funciones de importación desde TXT */
/** @{ */

/**
 * @brief Importa camisetas desde archivo TXT
 *
 * Lee el archivo TXT de camisetas y las inserta en la base de datos.
 */
void importar_camisetas_txt();

/**
 * @brief Importa partidos desde archivo TXT
 *
 * Lee el archivo TXT de partidos y los inserta en la base de datos.
 */
void importar_partidos_txt();

/**
 * @brief Importa lesiones desde archivo TXT
 *
 * Lee el archivo TXT de lesiones y las inserta en la base de datos.
 */
void importar_lesiones_txt();

/**
 * @brief Importa estadísticas desde archivo TXT
 *
 * Lee el archivo TXT de estadísticas y las procesa.
 */
void importar_estadisticas_txt();

/** @} */

/** @name Funciones de importación desde CSV */
/** @{ */

/**
 * @brief Importa camisetas desde archivo CSV
 *
 * Lee el archivo CSV de camisetas y las inserta en la base de datos.
 */
void importar_camisetas_csv();

/**
 * @brief Importa partidos desde archivo CSV
 *
 * Lee el archivo CSV de partidos y los inserta en la base de datos.
 */
void importar_partidos_csv();

/**
 * @brief Importa lesiones desde archivo CSV
 *
 * Lee el archivo CSV de lesiones y las inserta en la base de datos.
 */
void importar_lesiones_csv();

/**
 * @brief Importa estadísticas desde archivo CSV
 *
 * Lee el archivo CSV de estadísticas y las procesa.
 */
void importar_estadisticas_csv();

/** @} */

/** @name Funciones de importación desde HTML */
/** @{ */

/**
 * @brief Importa camisetas desde archivo HTML
 *
 * Lee el archivo HTML de camisetas y las inserta en la base de datos.
 */
void importar_camisetas_html();

/**
 * @brief Importa partidos desde archivo HTML
 *
 * Lee el archivo HTML de partidos y los inserta en la base de datos.
 */
void importar_partidos_html();

/**
 * @brief Importa lesiones desde archivo HTML
 *
 * Lee el archivo HTML de lesiones y las inserta en la base de datos.
 */
void importar_lesiones_html();

/**
 * @brief Importa estadísticas desde archivo HTML
 *
 * Lee el archivo HTML de estadísticas y las procesa.
 */
void importar_estadisticas_html();

/**
 * @brief Muestra el menú principal de importación
 *
 * Esta función muestra un menú interactivo que permite al usuario
 * seleccionar qué tipo de datos desea importar y desde qué formato.
 * Proporciona acceso a todas las funciones de importación disponibles.
 */
void menu_importar();

#endif
