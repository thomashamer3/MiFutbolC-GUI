/**
 * @file cancha.h
 * @brief API de gestión CRUD para entidades de canchas deportivas
 *
 * Define interfaz para operaciones de base de datos sobre tabla 'cancha',
 * implementando patrón de reutilización de IDs mediante consultas SQL
 * recursivas y validaciones de integridad referencial.
 */

#ifndef CANCHA_H
#define CANCHA_H

/**
 * @brief Interfaz de menú para operaciones de gestión de canchas
 *
 * Implementa patrón de menú interactivo delegando a funciones especializadas
 * para cada operación CRUD, utilizando estructura MenuItem para navegación.
 */
void menu_canchas();

/**
 * @brief Inserta nueva entidad cancha en base de datos
 *
 * Ejecuta algoritmo de asignación de ID secuencial mediante consulta
 * recursiva SQL, insertando registro con binding de parámetros
 * para prevenir inyección SQL.
 */
void crear_cancha();

/**
 * @brief Recupera y visualiza conjunto completo de canchas
 *
 * Realiza consulta SELECT ordenada por ID, iterando sobre resultados
 * para presentación tabular en interfaz de consola.
 */
void listar_canchas();

/**
 * @brief Elimina entidad cancha con validaciones de integridad
 *
 * Implementa protocolo de eliminación segura con confirmación de usuario,
 * validación de existencia previa y manejo de restricciones referenciales
 * mediante operaciones DELETE condicionales.
 */
void eliminar_cancha();

/**
 * @brief Actualiza atributo nombre de entidad cancha existente
 *
 * Ejecuta UPDATE con validación de existencia y binding de parámetros,
 * permitiendo correcciones sin requerir eliminación/recreación del registro.
 */
void modificar_cancha();

#endif
