/**
 * @file export_all_mejorado.c
 * @brief Funciones mejoradas para exportar todos los datos con analisis avanzado
 *
 * Este archivo contiene funciones mejoradas para exportar todos los datos
 * con estadisticas avanzadas y analisis integrado.
 */

#include "export_all_mejorado.h"
#include "export_camisetas_mejorado.h"
#include "export_lesiones_mejorado.h"
#include "export_camisetas.h"
#include "export_lesiones.h"
#include "export.h"
#include "utils.h"
#include "menu.h"
#include <stdio.h>

static void exportar_camisetas_mejoradas()
{
    exportar_camisetas_csv_mejorado();
    exportar_camisetas_txt_mejorado();
    exportar_camisetas_json_mejorado();
    exportar_camisetas_html_mejorado();
}

static void exportar_lesiones_mejoradas()
{
    exportar_lesiones_csv_mejorado();
    exportar_lesiones_txt_mejorado();
    exportar_lesiones_json_mejorado();
    exportar_lesiones_html_mejorado();
}

static void exportar_camisetas_basicas()
{
    exportar_camisetas_csv();
    exportar_camisetas_txt();
    exportar_camisetas_json();
    exportar_camisetas_html();
}

static void exportar_lesiones_basicas()
{
    exportar_lesiones_csv();
    exportar_lesiones_txt();
    exportar_lesiones_json();
    exportar_lesiones_html();
}

/**
 * @brief Exportacion completa de datos de camisetas con analisis avanzado
 *
 * Centraliza la exportacion de datos de camisetas en todos los formatos mejorados,
 * proporcionando una solucion integral para el analisis de rendimiento. Esto es esencial
 * para equipos y analistas que necesitan evaluar multiples aspectos del desempeno de los jugadores
 * en diferentes formatos para diferentes usos (hojas de calculo, informes, APIs, visualizacion web).
 *
 * @details La exportacion en multiples formatos permite:
 * - CSV: Analisis cuantitativo en herramientas como Excel
 * - TXT: Documentacion legible para informes
 * - JSON: Integracion con aplicaciones y APIs
 * - HTML: Visualizacion interactiva en navegadores
 *
 * @see exportar_camisetas_csv_mejorado()
 * @see exportar_camisetas_txt_mejorado()
 * @see exportar_camisetas_json_mejorado()
 * @see exportar_camisetas_html_mejorado()
 */
void exportar_camisetas_todo_mejorado()
{
    printf("Exportando camisetas con analisis avanzado...\n");
    exportar_camisetas_mejoradas();
    printf("Exportacion de camisetas con analisis avanzado completada.\n");
    pause_console();
}

/**
 * @brief Exportacion completa de datos de lesiones con analisis de impacto
 *
 * Proporciona una exportacion integral de datos de lesiones en todos los formatos mejorados,
 * incluyendo analisis de impacto en el rendimiento. Esto es crucial para equipos medicos y
 * entrenadores que necesitan evaluar como las lesiones afectan el desempeno de los jugadores
 * y planificar estrategias de recuperacion y prevencion.
 *
 * @details El analisis de impacto de lesiones ayuda a:
 * - Evaluar la gravedad y consecuencias de las lesiones
 * - Planificar programas de rehabilitacion efectivos
 * - Prevenir lesiones futuras mediante la identificacion de patrones
 * - Optimizar la gestion del equipo considerando la disponibilidad de jugadores
 *
 * @see exportar_lesiones_csv_mejorado()
 * @see exportar_lesiones_txt_mejorado()
 * @see exportar_lesiones_json_mejorado()
 * @see exportar_lesiones_html_mejorado()
 */
void exportar_lesiones_todo_mejorado()
{
    printf("Exportando lesiones con analisis avanzado...\n");
    exportar_lesiones_mejoradas();
    printf("Exportacion de lesiones con analisis avanzado completada.\n");
    pause_console();
}

void exportar_todo_mejorado()
{
    printf("Exportando todo con analisis avanzado...\n");

    // Exportar datos mejorados
    exportar_camisetas_mejoradas();
    exportar_lesiones_mejoradas();

    // Exportar datos originales para compatibilidad
    exportar_camisetas_basicas();
    exportar_lesiones_basicas();

    printf("Exportacion de todo con analisis avanzado completada.\n");
    pause_console();
}

void menu_exportar_mejorado()
{
    MenuItem items[] =
    {
        {1, "Camisetas con Analisis Avanzado", exportar_camisetas_todo_mejorado},
        {2, "Lesiones con Analisis de Impacto", exportar_lesiones_todo_mejorado},
        {3, "Todo con Analisis Avanzado", exportar_todo_mejorado},
        {0, "Volver", NULL}
    };
    ejecutar_menu_estandar("EXPORTAR DATOS MEJORADOS", items, 4);
}
