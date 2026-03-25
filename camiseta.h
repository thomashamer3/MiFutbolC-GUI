/**
 * @file camiseta.h
 * @brief API pública para gestión de camisetas en MiFutbolC
 *
 * Define la interfaz para realizar operaciones CRUD (Crear, Leer, Actualizar, Borrar)
 * sobre la entidad camiseta, además de funcionalidades especiales como sorteos
 * para la asignación de indumentaria deportiva.
 */

#ifndef CAMISETA_H
#define CAMISETA_H

/**
 * @brief Muestra el menú interactivo de gestión de camisetas
 *
 * Presenta opciones para crear, listar, editar, eliminar camisetas y
 * realizar sorteos. Gestiona el flujo de navegación del usuario.
 */
void menu_camisetas();

/**
 * @brief Crea una nueva camiseta en la base de datos
 *
 * Solicita al usuario el nombre o identificador de la camiseta y
 * la persiste en el sistema utilizando un ID autoincremental optimizado.
 */
void crear_camiseta();

/**
 * @brief Lista todas las camisetas registradas
 *
 * Recupera de la base de datos el listado completo de camisetas y
 * las muestra en formato de tabla en la consola.
 */
void listar_camisetas();

/**
 * @brief Edita el nombre de una camiseta existente
 *
 * Permite al usuario seleccionar una camiseta por su ID y modificar
 * su descripción o nombre actual.
 */
void editar_camiseta();

/**
 * @brief Elimina una camiseta de la base de datos
 *
 * Solicita el ID de la camiseta a eliminar y, tras confirmar con el usuario,
 * remueve el registro permanentemente de la base de datos.
 */
void eliminar_camiseta();

/**
 * @brief Realiza un sorteo aleatorio entre camisetas disponibles
 *
 * Selecciona al azar una de las camisetas registradas en el sistema.
 * Útil para decidir qué indumentaria usar en un partido.
 */
void sortear_camiseta();

/**
 * @brief Carga una imagen asociada a una camiseta
 *
 * Permite seleccionar una imagen del sistema, copiarla a la carpeta
 * Imagenes de la app y guardar su ruta relativa en la base de datos.
 */
void cargar_imagen_camiseta();

/**
 * @brief Abre la imagen asociada a una camiseta
 *
 * Recupera la ruta de imagen registrada en DB y la abre con el visor
 * predeterminado del sistema operativo.
 */
void ver_imagen_camiseta();

#endif

