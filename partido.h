/**
 * @file partido.h
 * @brief Declaraciones de funciones para la gestión de partidos en MiFutbolC
 *
 * Este archivo contiene las declaraciones de las funciones relacionadas con
 * la gestión de partidos de fútbol, incluyendo creación, listado, modificación
 * y eliminación de registros de partidos.
 */

#ifndef PARTIDO_H
#define PARTIDO_H

/**
 * @brief Muestra el menú principal de gestión de partidos
 *
 * Presenta un menú interactivo con opciones para crear, listar, modificar
 * y eliminar partidos. Utiliza la función ejecutar_menu para manejar
 * la navegación del menú y delega las operaciones a las funciones correspondientes.
 */
void menu_partidos();

/**
 * @brief Crea un nuevo partido en la base de datos
 *
 * Solicita al usuario los datos del partido (cancha, goles, asistencias, camiseta)
 * y lo inserta en la tabla 'partido'. Obtiene la fecha y hora actual automáticamente.
 * Verifica que la camiseta seleccionada exista antes de insertar.
 * Utiliza el ID más pequeño disponible para reutilizar IDs eliminados.
 */
void crear_partido();

/**
 * @brief Muestra un listado de todos los partidos registrados
 *
 * Consulta la base de datos y muestra en pantalla todos los partidos
 * con sus respectivos datos: ID, cancha, fecha/hora, goles, asistencias
 * y nombre de la camiseta utilizada. Realiza un JOIN con la tabla camiseta
 * para obtener el nombre de la camiseta.
 *
 * @note Si no hay partidos registrados, muestra un mensaje informativo
 */
void listar_partidos();

/**
 * @brief Permite eliminar un partido existente
 *
 * Muestra la lista de partidos disponibles, solicita el ID a eliminar,
 * verifica que exista y solicita confirmación antes de proceder con
 * la eliminación del registro de la base de datos.
 */
void eliminar_partido();

/**
 * @brief Permite modificar los datos de un partido existente
 *
 * Muestra la lista de partidos disponibles, solicita el ID a modificar,
 * verifica que exista y permite cambiar todos los campos del partido:
 * cancha, goles, asistencias y camiseta utilizada.
 * Verifica que la camiseta especificada exista antes de actualizar.
 */
void modificar_partido();

/**
 * @brief Permite buscar partidos según diferentes criterios
 *
 * Presenta un submenú con opciones para buscar partidos por:
 * - Camiseta utilizada
 * - Número de goles
 * - Número de asistencias
 * - Cancha donde se jugó
 */
void buscar_partidos();

/**
 * @brief Menú de análisis táctico con diagramas
 *
 * Permite crear, ver y guardar diagramas tácticos asociados a partidos.
 */
void menu_tacticas_partido();

#endif
