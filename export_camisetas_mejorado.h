/**
 * @file export_camisetas_mejorado.h
 * @brief Interfaz de Programación de Aplicaciones (API) para exportación avanzada de camisetas
 *
 * Este archivo define la API pública para el sistema de exportación mejorada de datos de camisetas
 * en MiFutbolC. Proporciona funciones para exportar datos con análisis avanzado y estadísticas mejoradas,
 * incluyendo métricas de eficiencia y rendimiento que no están disponibles en las funciones de exportación estándar.
 *
 * @note Todas las funciones declaradas aquí son parte de la API pública y pueden ser
 *       llamadas desde otros módulos del sistema. La exportación mejorada incluye
 *       análisis avanzado que no está disponible en las funciones de exportación estándar.
 */

#ifndef EXPORT_CAMISETAS_MEJORADO_H
#define EXPORT_CAMISETAS_MEJORADO_H

/**
 * @brief Exportación de datos de camisetas en formato CSV con análisis avanzado
 *
 * Exporta los datos de camisetas a un archivo CSV incluyendo estadísticas avanzadas como:
 * - Eficiencia de goles por partido
 * - Eficiencia de asistencias por partido
 * - Relación goles/asistencias
 * - Porcentaje de victorias
 * - Porcentaje de lesiones por partido
 * - Métricas de rendimiento, cansancio y estado de ánimo
 *
 * @details El formato CSV es ideal para análisis cuantitativo en herramientas como Excel,
 *          proporcionando una estructura tabular que facilita el procesamiento y análisis
 *          de datos con funciones de hoja de cálculo.
 *
 * @see exportar_camisetas_txt_mejorado()
 * @see exportar_camisetas_json_mejorado()
 * @see exportar_camisetas_html_mejorado()
 */
void exportar_camisetas_csv_mejorado();

/**
 * @brief Exportación de datos de camisetas en formato TXT con análisis avanzado
 *
 * Exporta los datos de camisetas a un archivo de texto con formato legible, incluyendo
 * estadísticas avanzadas y análisis de rendimiento. Este formato es ideal para
 * documentación técnica y informes que requieren presentación legible.
 *
 * @details El formato de texto proporciona una presentación estructurada y legible de los datos,
 *          facilitando la revisión manual y la generación de informes técnicos que pueden ser
 *          compartidos con partes interesadas no técnicas.
 *
 * @see exportar_camisetas_csv_mejorado()
 * @see exportar_camisetas_json_mejorado()
 * @see exportar_camisetas_html_mejorado()
 */
void exportar_camisetas_txt_mejorado();

/**
 * @brief Exportación de datos de camisetas en formato JSON con análisis avanzado
 *
 * Exporta los datos de camisetas a un archivo JSON con estructura de datos jerárquica,
 * incluyendo estadísticas avanzadas y análisis de rendimiento. Este formato es ideal para
 * integración con aplicaciones y APIs que requieren datos estructurados.
 *
 * @details El formato JSON es ampliamente utilizado para la integración de sistemas y el desarrollo
 *          de aplicaciones web. Proporciona una estructura flexible y estandarizada que facilita
 *          el procesamiento automatizado y la integración con otras aplicaciones y servicios web.
 *
 * @see exportar_camisetas_csv_mejorado()
 * @see exportar_camisetas_txt_mejorado()
 * @see exportar_camisetas_html_mejorado()
 */
void exportar_camisetas_json_mejorado();

/**
 * @brief Exportación de datos de camisetas en formato HTML con análisis avanzado
 *
 * Exporta los datos de camisetas a un archivo HTML con tabla interactiva, incluyendo
 * estadísticas avanzadas y análisis de rendimiento. Este formato es ideal para
 * visualización en navegadores web y presentación de datos.
 *
 * @details El formato HTML proporciona una interfaz de usuario amigable y visualmente atractiva
 *          para la presentación de datos. Permite la visualización interactiva en navegadores web,
 *          facilitando la exploración y análisis de los datos por parte de los usuarios finales.
 *
 * @see exportar_camisetas_csv_mejorado()
 * @see exportar_camisetas_txt_mejorado()
 * @see exportar_camisetas_json_mejorado()
 */
void exportar_camisetas_html_mejorado();

#endif
