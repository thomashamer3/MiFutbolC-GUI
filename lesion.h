/**
 * @file lesion.h
 * @brief Declaraciones de funciones para la gestión de lesiones en MiFutbolC
 *
 * Este archivo contiene las declaraciones de las funciones relacionadas con
 * la gestión de lesiones de jugadores, incluyendo creación, listado, edición
 * y eliminación de registros de lesiones.
 */

#ifndef LESION_H
#define LESION_H

/**
 * @brief Crea una nueva lesión en la base de datos
 *
 * Solicita al usuario los datos de la lesión (jugador, tipo, fecha, duración)
 * y la inserta en la tabla 'lesion'. Utiliza el ID más pequeño disponible
 * para reutilizar IDs eliminados.
 */
void crear_lesion();

/**
 * @brief Muestra un listado de todas las lesiones registradas
 *
 * Consulta la base de datos y muestra en pantalla todas las lesiones
 * con sus respectivos datos: ID, jugador, tipo, fecha y duración.
 * Si no hay lesiones, muestra un mensaje informativo.
 */
void listar_lesiones();

/**
 * @brief Permite editar los datos de una lesión existente
 *
 * Muestra la lista de lesiones disponibles, solicita el ID a editar,
 * verifica que exista y permite cambiar todos los campos de la lesión.
 */
void modificar_lesion();

/**
 * @brief Elimina una lesión de la base de datos
 *
 * Muestra la lista de lesiones disponibles, solicita el ID a eliminar,
 * verifica que exista y solicita confirmación antes de eliminar.
 */
void eliminar_lesion();

/**
 * @brief Muestra las estadísticas de lesiones
 *
 * Calcula y muestra estadísticas como total de lesiones, lesiones por tipo,
 * por camiseta, por mes, mes con más lesiones, tiempo promedio entre lesiones,
 * rendimiento promedio antes y después de lesiones, y si baja el rendimiento previo.
 */
void mostrar_estadisticas_lesiones();

/**
 * @brief Muestra diferencias de días entre lesiones consecutivas
 */
void mostrar_diferencias_lesiones();

/**
 * @brief Pregunta al usuario si desea actualizar el estado de las lesiones activas
 */
void actualizar_estados_lesiones();

/**
 * @brief Muestra el menú principal de gestión de lesiones
 *
 * Presenta un menú interactivo con opciones para crear, listar, editar
 * y eliminar lesiones. Utiliza la función ejecutar_menu para manejar
 * la navegación del menú y delega las operaciones a las funciones correspondientes.
 */
void menu_lesiones();

#endif
