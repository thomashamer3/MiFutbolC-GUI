/**
 * @file logros.h
 * @brief Declaraciones de funciones para el sistema de logros en MiFutbolC
 *
 * Este archivo contiene las declaraciones de las funciones relacionadas con
 * el sistema de logros y badges basado en estadísticas conseguidas.
 */

#ifndef LOGROS_H
#define LOGROS_H

/**
 * @brief Muestra el menú principal de logros y badges
 *
 * Presenta un menú interactivo con opciones para ver logros completados,
 * en progreso y disponibles. Utiliza la función ejecutar_menu para manejar
 * la navegación del menú y delega las operaciones a las funciones correspondientes.
 */
void menu_logros();

/**
 * @brief Muestra todos los logros disponibles con su estado
 *
 * Lista todos los logros definidos en el sistema, indicando cuáles están
 * completados, cuáles están en progreso y cuáles aún no se han iniciado.
 */
void mostrar_todos_logros();

/**
 * @brief Muestra solo los logros completados
 *
 * Lista únicamente los logros que ya han sido conseguidos por alguna camiseta.
 */
void mostrar_logros_completados();

/**
 * @brief Muestra los logros en progreso
 *
 * Lista los logros que están parcialmente completados pero aún no terminados.
 */
void mostrar_logros_en_progreso();

#endif
