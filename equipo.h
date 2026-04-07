/**
 * @file equipo.h
 * @brief API de gestión de entidades de equipos deportivos
 *
 * Define interfaz para operaciones CRUD sobre entidades de equipos y jugadores,
 * implementando validaciones de integridad y conversión de enumeraciones
 * para interfaz de usuario en sistema de gestión deportiva.
 */

#ifndef EQUIPO_H
#define EQUIPO_H

/**
 * @brief Posiciones disponibles para los jugadores en el campo de juego
 *
 * Enumeración que define las posiciones estándar en fútbol donde pueden jugar los jugadores.
 */
typedef enum
{
    ARQUERO,        /**< Portero o arquero - defiende la portería */
    DEFENSOR,       /**< Defensa - protege la zona defensiva */
    MEDIOCAMPISTA,  /**< Mediocampista - conecta defensa con ataque */
    DELANTERO       /**< Delantero - ataca y busca marcar goles */
} Posicion;

/**
 * @brief Tipos de equipos disponibles en el sistema
 *
 * Define si un equipo es permanente (fijo) o temporal (momentáneo).
 */
typedef enum
{
    FIJO,        /**< Equipo permanente guardado en base de datos */
    MOMENTANEO   /**< Equipo temporal que no se guarda */
} TipoEquipo;

/**
 * @brief Tipos de fútbol disponibles según cantidad de jugadores
 *
 * Define las modalidades de fútbol soportadas por el sistema.
 */
typedef enum
{
    FUTBOL_5,  /**< Fútbol sala con 5 jugadores por equipo */
    FUTBOL_7,  /**< Fútbol reducido con 7 jugadores por equipo */
    FUTBOL_8,  /**< Fútbol con 8 jugadores por equipo */
    FUTBOL_11  /**< Fútbol tradicional con 11 jugadores por equipo */
} TipoFutbol;

/**
 * @brief Estructura que representa a un jugador de fútbol
 *
 * Contiene toda la información básica necesaria para representar
 * a un jugador dentro de un equipo.
 */
typedef struct
{
    char nombre[50];    /**< Nombre completo del jugador */
    int numero;         /**< Número de camiseta del jugador */
    Posicion posicion;  /**< Posición en la que juega el jugador */
    int es_capitan;     /**< Indicador si el jugador es capitán (1) o no (0) */
} Jugador;

/**
 * @brief Estructura que representa a un equipo de fútbol
 *
 * Contiene toda la información necesaria para representar
 * a un equipo completo con sus jugadores.
 */
typedef struct
{
    int id;                        /**< Identificador único del equipo en base de datos */
    char nombre[50];               /**< Nombre del equipo */
    TipoEquipo tipo;               /**< Tipo de equipo (fijo o momentáneo) */
    TipoFutbol tipo_futbol;        /**< Modalidad de fútbol que juega */
    Jugador jugadores[11];         /**< Array de jugadores (máximo 11 para fútbol 11) */
    int num_jugadores;             /**< Número actual de jugadores en el equipo */
    int partido_id;                /**< ID del partido al que está asignado (-1 si no asignado) */
} Equipo;

/**
 * @brief Interfaz de menú para operaciones CRUD de equipos
 *
 * Implementa patrón de menú delegando a funciones especializadas
 * para cada operación sobre entidades de equipo, utilizando
 * estructura MenuItem para navegación controlada.
 */
void menu_equipos();

/**
 * @brief Punto de entrada para creación de entidades de equipo
 *
 * Implementa menú de selección entre modos fijo (persistente) y momentáneo,
 * delegando a funciones especializadas según el tipo seleccionado por usuario.
 */
void crear_equipo();

/**
 * @brief Muestra un listado de todos los equipos registrados
 *
 * Consulta la base de datos y muestra en pantalla todos los equipos
 * con sus respectivos datos.
 */
void listar_equipos();

/**
 * @brief Permite modificar los datos de un equipo existente
 *
 * Muestra la lista de equipos disponibles, solicita el ID a modificar,
 * verifica que exista y permite cambiar los campos del equipo.
 */
void modificar_equipo();

/**
 * @brief Permite eliminar un equipo existente
 *
 * Muestra la lista de equipos disponibles, solicita el ID a eliminar,
 * verifica que exista y solicita confirmación antes de proceder con
 * la eliminación del registro de la base de datos.
 */
void eliminar_equipo();


/**
 * @brief Muestra la información de un equipo
 *
 * Muestra todos los detalles de un equipo, incluyendo sus jugadores.
 *
 * @param equipo Puntero al equipo a mostrar
 */
void mostrar_equipo(const Equipo *equipo);

/**
 * @brief Obtiene el nombre de una posición
 *
 * Convierte el enum de posición a una cadena legible.
 *
 * @param posicion La posición a convertir
 * @return Cadena con el nombre de la posición
 */
const char *get_nombre_posicion(Posicion posicion);

/**
 * @brief Obtiene el nombre de un tipo de fútbol
 *
 * Convierte el enum de tipo de fútbol a una cadena legible.
 *
 * @param tipo El tipo de fútbol a convertir
 * @return Cadena con el nombre del tipo de fútbol
 */
const char *get_nombre_tipo_futbol(TipoFutbol tipo);

/**
 * @brief Dibuja el estado de la cancha durante simulaciones de partido.
 */
void mostrar_cancha_animada(int minuto, int evento_tipo);

#endif
