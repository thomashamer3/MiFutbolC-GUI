/**
 * @brief Puntero a función que representa una acción de menú
 *
 * Define el tipo de función que puede ser ejecutada como acción en un elemento de menú.
 * No recibe parámetros y no retorna valor.
 */

#ifndef MODELS_H
#define MODELS_H

typedef void (*MenuAction)(void);

/**
 * @struct MenuItem
 * @brief Estructura que representa un elemento de menú
 *
 * Esta estructura define los componentes de un elemento en un menú interactivo,
 * incluyendo la opción numérica, el texto descriptivo y la acción asociada.
 */
typedef struct
{
    int opcion;            /**< Número de opción del menú */
    const char *texto;     /**< Texto descriptivo de la opción */
    MenuAction accion;     /**< Función a ejecutar cuando se selecciona la opción */
} MenuItem;

#endif

