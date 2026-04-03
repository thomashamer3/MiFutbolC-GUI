/**
 * @file menu.h
 * @brief Sistema de menús interactivos para la aplicación MiFutbolC
 *
 * Define estructuras y funciones para crear y gestionar menús interactivos
 * en consola, incluyendo inicialización de la aplicación y gestión de usuario.
 */

#ifndef MENU_H
#define MENU_H

typedef enum
{
    MENU_CATEGORY_ALL = 0,
    MENU_CATEGORY_GESTION,
    MENU_CATEGORY_COMPETENCIA,
    MENU_CATEGORY_ANALISIS,
    MENU_CATEGORY_ADMIN
} MenuCategory;

/**
 * @brief Estructura que representa un elemento de menú
 *
 * Define un ítem individual dentro de un menú, incluyendo su número de opción,
 * texto descriptivo y la función a ejecutar cuando se selecciona.
 */
typedef struct
{
    int opcion;            /**< Número de opción del menú (usado para selección) */
    const char *texto;     /**< Texto descriptivo mostrado al usuario */
    void (*accion)();      /**< Puntero a función que se ejecuta al seleccionar */
    MenuCategory categoria;/**< Categoria para filtros en GUI */
} MenuItem;

/**
 * @brief Ejecuta un menú interactivo en la consola
 *
 * Esta función muestra un menú con el título proporcionado y una lista de opciones.
 * Permite al usuario seleccionar una opción y ejecuta la acción correspondiente.
 * Si la acción es NULL, sale del menú.
 *
 * @param titulo El título del menú a mostrar
 * @param items Arreglo de elementos del menú
 * @param cantidad Número de elementos en el arreglo
 */
void ejecutar_menu(const char *titulo, const MenuItem *items, int cantidad);

/**
 * @brief Ejecuta un menu estandarizado con log de ingreso.
 *
 * Esta funcion registra en el log la entrada al menu y luego delega
 * la navegacion a ejecutar_menu().
 *
 * @param titulo El titulo del menu a mostrar
 * @param items Arreglo de elementos del menu
 * @param cantidad Numero de elementos en el arreglo
 */
void ejecutar_menu_estandar(const char *titulo, const MenuItem *items, int cantidad);

/**
 * @brief Inicializa la aplicación completa
 *
 * Configura la consola, establece el locale para caracteres especiales,
 * inicializa la base de datos y carga la configuración del sistema.
 */
void initialize_application(void);

/**
 * @brief Maneja la verificación y creación del nombre de usuario
 *
 * Verifica si existe un nombre de usuario configurado y, si no existe,
 * solicita al usuario que ingrese uno para personalizar la aplicación.
 */
void handle_user_name(void);

/**
 * @brief Crea el menú filtrado dinámicamente
 *
 * Genera un menú personalizado basado en la configuración del usuario
 * y el modo de operación actual (Simple, Avanzado, Personalizado).
 *
 * @param count Puntero donde se almacenará el número de ítems del menú
 * @return Puntero a array dinámico de MenuItem (debe ser liberado)
 */
MenuItem *create_filtered_menu(int *count);

/**
 * @brief Ejecuta el menú principal y libera recursos
 *
 * Muestra el menú principal de la aplicación y gestiona la navegación
 * del usuario. Al finalizar, libera los recursos asignados.
 *
 * @param filtered_items Array de ítems del menú a mostrar
 * @param count Número de ítems en el array
 */
void run_menu(MenuItem *filtered_items, int count);

#ifdef UNIT_TEST
typedef struct
{
    const char *titulo;
    int cantidad;
    MenuItem last_item;
} MenuTestCapture;

int menu_get_item_count(void);
const MenuItem *menu_get_items(void);
const MenuItem *menu_buscar_item(const MenuItem *items, int cantidad, int opcion);
void menu_test_set_capture(MenuTestCapture *capture);
#endif

#endif
