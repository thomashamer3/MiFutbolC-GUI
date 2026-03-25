/**
 * @file export_pdf.h
 * @brief Generación de informes en PDF.
 */

#ifndef EXPORT_PDF_H
#define EXPORT_PDF_H

/**
 * @brief Genera un informe total en PDF con todos los datos del usuario.
 * @return 1 si se generó correctamente, 0 en caso de error.
 */
int generar_informe_total_pdf(void);

/**
 * @brief Genera un informe personal mensual en PDF.
 * @param mes_yyyy_mm Mes en formato YYYY-MM
 * @return 1 si se generó el informe, 0 si falló.
 */
int generar_informe_personal_mensual_pdf(const char *mes_yyyy_mm);

#endif
