/**
 * @file torneo.h
 * @brief Declaraciones de funciones para la gestión de torneos en MiFutbolC
 *
 * Este archivo contiene las declaraciones de las funciones relacionadas con
 * la gestión de torneos de fútbol, incluyendo creación, listado, modificación
 * y eliminación de registros de torneos.
 */

#ifndef TORNEO_H
#define TORNEO_H

/**
 * @brief Tipos de torneo según la estructura de partidos
 *
 * Define las diferentes formas en que se pueden organizar los partidos
 * en un torneo, desde ida y vuelta hasta eliminación directa.
 */
typedef enum
{
    IDA_Y_VUELTA,          /**< Los equipos juegan entre sí en partidos de ida y vuelta */
    SOLO_IDA,              /**< Los equipos juegan entre sí solo en un partido */
    ELIMINACION_DIRECTA,   /**< Formato de copa donde los perdedores quedan eliminados */
    GRUPOS_Y_ELIMINACION  /**< Combinación de grupos iniciales con eliminación posterior */
} TipoTorneos;

/**
 * @brief Formatos específicos disponibles para diferentes cantidades de equipos
 *
 * Define los formatos de torneo disponibles según el número de equipos participantes,
 * desde round-robin hasta eliminación directa por fases.
 */
typedef enum
{
    ROUND_ROBIN,           /**< Sistema de liga donde todos juegan contra todos */
    MINI_GRUPO_CON_FINAL,  /**< Grupos pequeños con una final al final */
    LIGA_SIMPLE,           /**< Liga simple con partidos de ida */
    LIGA_DOBLE,            /**< Liga con partidos de ida y vuelta */
    GRUPOS_CON_FINAL,      /**< Grupos seguidos de una fase final */
    COPA_SIMPLE,           /**< Copa básica de eliminación directa */
    GRUPOS_ELIMINACION,    /**< Grupos con eliminación directa posterior */
    COPA_REPECHAJE,        /**< Copa con sistema de repechaje */
    LIGA_GRANDE,           /**< Liga grande con muchos equipos */
    MULTIPLES_GRUPOS,      /**< Múltiples grupos de clasificación */
    ELIMINACION_FASES      /**< Eliminación directa organizada por fases */
} FormatoTorneos;

/**
 * @brief Estructura que representa un torneo de fútbol
 *
 * Contiene toda la información necesaria para representar
 * un torneo completo con sus características y configuración.
 */
typedef struct
{
    int id;                        /**< Identificador único del torneo en base de datos */
    char nombre[50];               /**< Nombre del torneo */
    int tiene_equipo_fijo;         /**< Indicador si el torneo tiene un equipo fijo (1) o no (0) */
    int equipo_fijo_id;            /**< ID del equipo fijo asignado al torneo (-1 si no tiene) */
    int cantidad_equipos;          /**< Número total de equipos participantes */
    TipoTorneos tipo_torneo;       /**< Tipo de estructura del torneo */
    FormatoTorneos formato_torneo; /**< Formato específico del torneo */
} Torneo;

/**
 * @brief Muestra el menú principal de gestión de torneos
 *
 * Presenta un menú interactivo con opciones para crear, listar, modificar
 * y eliminar torneos. Utiliza la función ejecutar_menu para manejar
 * la navegación del menú y delega las operaciones a las funciones correspondientes.
 */
void menu_torneos();

/**
 * @brief Crea un nuevo torneo
 *
 * Solicita al usuario los datos del torneo y lo guarda en la base de datos.
 */
void crear_torneo();

/**
 * @brief Muestra un listado de todos los torneos registrados
 *
 * Consulta la base de datos y muestra en pantalla todos los torneos
 * con sus respectivos datos.
 */
void listar_torneos();

/**
 * @brief Permite modificar los datos de un torneo existente
 *
 * Muestra la lista de torneos disponibles, solicita el ID a modificar,
 * verifica que exista y permite cambiar los campos del torneo.
 */
void modificar_torneo();

/**
 * @brief Permite eliminar un torneo existente
 *
 * Muestra la lista de torneos disponibles, solicita el ID a eliminar,
 * verifica que exista y solicita confirmación antes de proceder con
 * la eliminación del registro de la base de datos.
 */
void eliminar_torneo();

/**
 * @brief Muestra la información de un torneo
 *
 * Muestra todos los detalles de un torneo.
 *
 * @param torneo Puntero al torneo a mostrar
 */
void mostrar_torneo(Torneo *torneo);

/**
 * @brief Muestra el menú de administración de torneos
 *
 * Permite administrar aspectos avanzados de un torneo como fixture, resultados y tablas de posiciones.
 */
void administrar_torneo();

/**
 * @brief Muestra el fixture de un torneo
 *
 * Muestra los partidos programados para un torneo.
 *
 * @param torneo_id ID del torneo
 */
void mostrar_fixture(int torneo_id);

/**
 * @brief Permite ingresar resultados de partidos
 *
 * Permite registrar los resultados de los partidos de un torneo.
 *
 * @param torneo_id ID del torneo
 */
void ingresar_resultado(int torneo_id);

/**
 * @brief Muestra la tabla de posiciones de un torneo
 *
 * Muestra la clasificación actual de los equipos en un torneo.
 *
 * @param torneo_id ID del torneo
 */
void ver_tabla_posiciones(int torneo_id);

/**
 * @brief Muestra el estado de los equipos en un torneo
 *
 * Muestra información sobre el estado de los equipos (liga/copa).
 *
 * @param torneo_id ID del torneo
 */
void estado_equipos(int torneo_id);

/**
 * @brief Obtiene el nombre de un tipo de torneo
 *
 * Convierte el enum de tipo de torneo a una cadena legible.
 *
 * @param tipo El tipo de torneo a convertir
 * @return Cadena con el nombre del tipo de torneo
 */
const char* get_nombre_tipo_torneo(TipoTorneos tipo);

/**
 * @brief Obtiene el nombre de un formato de torneo
 *
 * Convierte el enum de formato de torneo a una cadena legible.
 *
 * @param formato El formato de torneo a convertir
 * @return Cadena con el nombre del formato de torneo
 */
const char* get_nombre_formato_torneo(FormatoTorneos formato);

/**
 * @brief Asocia equipos a un torneo
 *
 * Permite asociar equipos existentes a un torneo.
 *
 * @param torneo_id ID del torneo al que se asociarán los equipos
 */
void asociar_equipos_torneo(int torneo_id);

/**
 * @brief Actualiza la tabla de posiciones después de un partido
 *
 * @param torneo_id ID del torneo
 * @param equipo1_id ID del primer equipo
 * @param equipo2_id ID del segundo equipo
 * @param goles1 Goles del primer equipo
 * @param goles2 Goles del segundo equipo
 */
void actualizar_tabla_posiciones(int torneo_id, int equipo1_id, int equipo2_id, int goles1, int goles2);

/**
 * @brief Actualiza las estadísticas de los jugadores después de un partido
 *
 * @param torneo_id ID del torneo
 * @param equipo1_id ID del primer equipo
 * @param equipo2_id ID del segundo equipo
 * @param goles1 Goles del primer equipo
 * @param goles2 Goles del segundo equipo
 */
void actualizar_estadisticas_jugadores(int torneo_id, int equipo1_id, int equipo2_id, int goles1, int goles2);

/**
 * @brief Actualiza la fase del torneo para torneos de eliminación
 *
 * @param torneo_id ID del torneo
 * @param equipo1_id ID del primer equipo
 * @param equipo2_id ID del segundo equipo
 * @param goles1 Goles del primer equipo
 * @param goles2 Goles del segundo equipo
 */
void actualizar_fase_torneo(int torneo_id, int equipo1_id, int equipo2_id, int goles1, int goles2);

/**
 * @brief Muestra estadísticas de jugadores de un torneo
 *
 * @param torneo_id ID del torneo
 * @param equipo_id ID del equipo (0 para mejores goleadores del torneo)
 */
void mostrar_estadisticas_jugador(int torneo_id, int equipo_id);

/**
 * @brief Muestra el historial de un equipo en torneos anteriores
 *
 * @param equipo_id ID del equipo
 */
void mostrar_historial_equipo(int equipo_id);

/**
 * @brief Finaliza un torneo y guarda el historial de todos los equipos
 *
 * @param torneo_id ID del torneo
 */
void finalizar_torneo(int torneo_id);

/**
 * @brief Obtiene el nombre de un equipo por su ID
 *
 * @param equipo_id ID del equipo
 * @return Nombre del equipo o "Equipo Desconocido" si no existe
 */
const char* get_equipo_nombre(int equipo_id);

/**
 * @brief Muestra un dashboard con información en tiempo real del torneo
 *
 * Muestra posición actual, próximos partidos, últimos resultados, goleadores, etc.
 *
 * @param torneo_id ID del torneo
 * @param equipo_id ID del equipo (opcional, 0 para vista general)
 */
void mostrar_dashboard_torneo(int torneo_id, int equipo_id);

/**
 * @brief Muestra los próximos partidos de un equipo
 *
 * @param torneo_id ID del torneo
 * @param equipo_id ID del equipo
 */
void mostrar_proximos_partidos(int torneo_id, int equipo_id);

/**
 * @brief Exporta la tabla de posiciones a un archivo
 *
 * @param torneo_id ID del torneo
 */
void exportar_tabla_posiciones(int torneo_id);

/**
 * @brief Exporta estadísticas de jugadores a un archivo
 *
 * @param torneo_id ID del torneo
 * @param equipo_id ID del equipo (0 para todos los equipos)
 */
void exportar_estadisticas_jugadores(int torneo_id, int equipo_id);

/**
 * @brief Genera un reporte completo del torneo
 *
 * Incluye tabla de posiciones, estadísticas de jugadores, y resumen general
 *
 * @param torneo_id ID del torneo
 */
void generar_reporte_torneo(int torneo_id);

/**
 * @brief Muestra el menú para gestionar tablas de goleadores y asistidores
 *
 * Permite agregar, eliminar, modificar y listar registros en las tablas de goleadores y asistidores
 */
void gestionar_tablas_goleadores_asistidores();

/**
 * @brief Lista las tablas de goleadores y asistidores de un torneo
 *
 * @param torneo_id ID del torneo
 */
void listar_tablas_goleadores_asistidores(int torneo_id);

/**
 * @brief Agrega un registro a las tablas de goleadores y asistidores
 *
 * @param torneo_id ID del torneo
 */
void agregar_registro_goleador_asistidor(int torneo_id);

/**
 * @brief Elimina un registro de las tablas de goleadores y asistidores
 *
 * @param torneo_id ID del torneo
 */
void eliminar_registro_goleador_asistidor(int torneo_id);

/**
 * @brief Modifica un registro de las tablas de goleadores y asistidores
 *
 * @param torneo_id ID del torneo
 */
void modificar_registro_goleador_asistidor(int torneo_id);

#endif
