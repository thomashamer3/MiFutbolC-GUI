/**
 * @file export.h
 * @brief Declaraciones de funciones para exportar datos en MiFutbolC
 *
 * Este archivo contiene las declaraciones de las funciones de exportación
 * que permiten guardar los datos del sistema en diferentes formatos:
 * CSV, TXT, JSON y HTML para camisetas, partidos, estadísticas y lesiones.
 */

#ifndef EXPORT_H
#define EXPORT_H

/** @name Funciones utilitarias */
/** @{ */

/**
 * @brief Elimina espacios en blanco al final de una cadena.
 *
 * @param str Cadena a recortar.
 * @return Puntero a la cadena recortada.
 */
char *trim_trailing_spaces(char *str);

/**
 * @brief Convierte el número de resultado a texto
 *
 * @param resultado Número del resultado (1=VICTORIA, 2=EMPATE, 3=DERROTA)
 * @return Cadena de texto correspondiente al resultado
 */
const char *resultado_to_text(int resultado);

/**
 * @brief Convierte el número de clima a texto
 *
 * @param clima Número del clima (1=Despejado, 2=Nublado, 3=Lluvia, 4=Ventoso, 5=Mucho Calor, 6=Mucho Frio)
 * @return Cadena de texto correspondiente al clima
 */
const char *clima_to_text(int clima);

/**
 * @brief Convierte el número de dia a texto
 *
 * @param dia Número del dia (1=Dia, 2=Tarde, 3=Noche)
 * @return Cadena de texto correspondiente al dia
 */
const char *dia_to_text(int dia);

/** @} */

/** @name Funciones de exportación de análisis */
/** @{ */

/**
 * @brief Exporta el análisis de rendimiento a formato CSV
 *
 * Genera un archivo CSV con las estadísticas generales, últimos 5 partidos,
 * rachas y análisis motivacional.
 */
void exportar_analisis_csv();

/**
 * @brief Exporta el análisis de rendimiento a formato TXT
 *
 * Genera un archivo de texto con las estadísticas generales, últimos 5 partidos,
 * rachas y análisis motivacional.
 */
void exportar_analisis_txt();

/**
 * @brief Exporta el análisis de rendimiento a formato JSON
 *
 * Genera un archivo JSON con un objeto conteniendo todas las estadísticas
 * del análisis de rendimiento.
 */
void exportar_analisis_json();

/**
 * @brief Exporta el análisis de rendimiento a formato HTML
 *
 * Genera una página HTML con las estadísticas presentadas en formato web.
 */
void exportar_analisis_html();

/**
 * @brief Exporta un resumen financiero por mes y año a TXT
 */
void exportar_finanzas_resumen_txt();

/**
 * @brief Exporta ranking de canchas por rendimiento y lesiones a TXT
 */
void exportar_ranking_canchas_txt();

/**
 * @brief Exporta partidos agrupados por clima a TXT
 */
void exportar_partidos_por_clima_txt();

/**
 * @brief Exporta distribución de lesiones por tipo y estado a TXT
 */
void exportar_lesiones_por_tipo_estado_txt();

/**
 * @brief Exporta historial de rachas a TXT
 */
void exportar_rachas_historial_txt();

/**
 * @brief Exporta distribución de estado de ánimo y cansancio a TXT
 */
void exportar_estado_animo_cansancio_txt();

/** @} */

/**
 * @brief Construye la ruta completa para un archivo de exportación
 *
 * Combina el directorio de datos con el nombre del archivo proporcionado
 * para crear una ruta completa.
 *
 * @param filename Nombre del archivo a exportar
 * @return Cadena de caracteres con la ruta completa del archivo
 */
char *get_export_path(const char *filename);
/** @} */
#endif
