/**
* @file equipo.c
* @brief Implementacion de funciones para la gestion de equipos en MiFutbolC
*/

#include "equipo.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "partido.h"
#include "ascii_art.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include "compat_windows.h"
#endif
#include "sqlite3.h"
#include <ctype.h>
#include <limits.h>
#ifdef _WIN32
#include <process.h>
#include <io.h>
#else
#include "process.h"
#include <strings.h>
#endif


static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, 0) == SQLITE_OK;
}

static int abrir_imagen_en_sistema(const char *ruta)
{
    if (!ruta || ruta[0] == '\0')
    {
        return 0;
    }

    char cmd[1400];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", ruta);
#else
    snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" >/dev/null 2>&1", ruta);
#endif
    return system(cmd) == 0;
}

static int construir_ruta_absoluta_imagen_equipo_por_id(int id, char *ruta_absoluta, size_t size)
{
    if (!ruta_absoluta || size == 0)
    {
        return 0;
    }

    char ruta_db[300] = {0};
    if (!db_get_image_path_by_id("equipo", id, ruta_db, sizeof(ruta_db)))
    {
        return 0;
    }

    return db_resolve_image_absolute_path(ruta_db, ruta_absoluta, size);
}

static int cargar_imagen_para_equipo_id(int id)
{
    return app_cargar_imagen_entidad(id, "equipo", "mifutbol_imagen_sel_equipo.txt");
}

void cargar_imagen_equipo()
{
    clear_screen();
    print_header("CARGAR IMAGEN DE EQUIPO");

    if (!hay_registros("equipo"))
    {
        mostrar_no_hay_registros("equipos");
        pause_console();
        return;
    }

    listar_equipos();
    int id = input_int("\nID de equipo (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("equipo", id))
    {
        printf("ID inexistente.\n");
        pause_console();
        return;
    }

    if (!cargar_imagen_para_equipo_id(id))
    {
        printf("No se pudo completar la carga de imagen.\n");
    }

    pause_console();
}

void ver_imagen_equipo()
{
    clear_screen();
    print_header("VER IMAGEN DE EQUIPO");

    if (!hay_registros("equipo"))
    {
        mostrar_no_hay_registros("equipos");
        pause_console();
        return;
    }

    listar_equipos();
    int id = input_int("\nID de equipo (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("equipo", id))
    {
        printf("ID inexistente.\n");
        pause_console();
        return;
    }

    char ruta_absoluta[1200] = {0};
    if (!construir_ruta_absoluta_imagen_equipo_por_id(id, ruta_absoluta, sizeof(ruta_absoluta)))
    {
        printf("No se encontro imagen cargada para ese equipo.\n");
        pause_console();
        return;
    }

    if (!abrir_imagen_en_sistema(ruta_absoluta))
    {
        printf("No se pudo abrir la imagen en el sistema.\n");
        pause_console();
        return;
    }

    printf("Abriendo imagen...\n");
    pause_console();
}

static int ejecutar_update_text(const char *sql, const char *value, int id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, sql))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, value, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, id);

    int result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

static int ejecutar_update_int(const char *sql, int value, int id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, sql))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, value);
    sqlite3_bind_int(stmt, 2, id);

    int result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

static int ejecutar_update_id(const char *sql, int id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, sql))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, id);

    int result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

static void solicitar_nombre_equipo(const char *prompt, char *buffer, int size)
{
    while (1)
    {
        input_string(prompt, buffer, size);
        trim_whitespace(buffer);

        if (buffer[0] != '\0')
            return;

        printf("El nombre no puede estar vacio.\n");
    }
}

static int numero_repetido(const int *numeros, int count, int numero, int ignore_idx)
{
    for (int i = 0; i < count; i++)
    {
        if (i == ignore_idx)
        {
            continue;
        }
        if (numeros[i] == numero)
        {
            return 1;
        }
    }
    return 0;
}

static int ya_hay_arquero(const int *posiciones, int count, int ignore_idx)
{
    for (int i = 0; i < count; i++)
    {
        if (i == ignore_idx)
        {
            continue;
        }
        if (posiciones[i] == ARQUERO)
        {
            return 1;
        }
    }
    return 0;
}

static int numero_repetido_en_equipo(const Equipo *equipo, int count, int numero, int ignore_idx)
{
    for (int i = 0; i < count; i++)
    {
        if (i == ignore_idx)
        {
            continue;
        }
        if (equipo->jugadores[i].numero == numero)
        {
            return 1;
        }
    }
    return 0;
}

static int ya_hay_arquero_en_equipo(const Equipo *equipo, int count, int ignore_idx)
{
    for (int i = 0; i < count; i++)
    {
        if (i == ignore_idx)
        {
            continue;
        }
        if (equipo->jugadores[i].posicion == ARQUERO)
        {
            return 1;
        }
    }
    return 0;
}

static void mostrar_progreso_creacion_equipo(const Equipo *equipo, int cargados, const char *prefix)
{
    if (cargados <= 0)
    {
        return;
    }

    printf("\n%sJugadores cargados hasta ahora:\n", prefix);
    for (int i = 0; i < cargados; i++)
    {
        printf("- %s (N:%d, %s)%s\n",
               equipo->jugadores[i].nombre,
               equipo->jugadores[i].numero,
               get_nombre_posicion(equipo->jugadores[i].posicion),
               equipo->jugadores[i].es_capitan ? " [CAPITAN]" : "");
    }
}

// Data structures to reduce parameter count in functions
typedef struct
{
    int goles_local;
    int goles_visitante;
    int goles_jugadores_local[11];
    int asistencias_jugadores_local[11];
    int goles_jugadores_visitante[11];
    int asistencias_jugadores_visitante[11];
} PartidoStats;

typedef struct
{
    int jugadores_ids[11];
    char jugadores_nombres[11][50];
    int jugadores_numeros[11];
    int jugadores_posiciones[11];
    int jugadores_capitanes[11];
    int jugador_count;
} EquipoPlayerInfo;

// Function prototypes
void crear_un_equipo_momentaneo();
void gestionar_equipo_momentaneo(Equipo *equipo);
void modificar_jugador_momentaneo(Equipo *equipo);
void agregar_jugador_momentaneo(Equipo *equipo);
void eliminar_jugador_momentaneo(Equipo *equipo);
void cambiar_capitan_momentaneo(Equipo *equipo);
void crear_dos_equipos_momentaneos();
void gestionar_dos_equipos_momentaneos(Equipo *equipo_local, Equipo *equipo_visitante);
void gestionar_equipo_individual(Equipo *equipo, const char *tipo_equipo);
void simular_partido(const Equipo *equipo_local, const Equipo *equipo_visitante);

void insert_jugadores_for_equipo(int equipo_id, const Equipo *equipo);

// Prototipo anadido para evitar declaracion implicita
void modificar_jugador_existente(const int *jugadores_ids, char jugadores_nombres[][50],
                                 const int *jugadores_numeros, const int *jugadores_posiciones, const int *jugadores_capitanes, int jugador_count);

// Prototipo para seleccion de posicion (evita declaracion implicita al usarla antes)
Posicion select_posicion();

/**
 * @brief Actualiza el nombre de un jugador en la base de datos
 *
 * @param player_id ID del jugador
 * @param new_name Nuevo nombre del jugador
 * @return 1 si se actualizo exitosamente, 0 si hubo error
 */
int update_player_name(int player_id, const char *new_name)
{
    const char *sql = "UPDATE jugador SET nombre = ? WHERE id = ?;";
    return ejecutar_update_text(sql, new_name, player_id);
}

/**
 * @brief Actualiza el numero de un jugador en la base de datos
 *
 * @param player_id ID del jugador
 * @param new_number Nuevo numero del jugador
 * @return 1 si se actualizo exitosamente, 0 si hubo error
 */
int update_player_number(int player_id, int new_number)
{
    const char *sql = "UPDATE jugador SET numero = ? WHERE id = ?;";
    return ejecutar_update_int(sql, new_number, player_id);
}

/**
 * @brief Actualiza la posicion de un jugador en la base de datos
 *
 * @param player_id ID del jugador
 * @param new_position Nueva posicion del jugador
 * @return 1 si se actualizo exitosamente, 0 si hubo error
 */
int update_player_position(int player_id, Posicion new_position)
{
    const char *sql = "UPDATE jugador SET posicion = ? WHERE id = ?;";
    return ejecutar_update_int(sql, new_position, player_id);
}

/**
 * @brief Actualiza el estado de capitan de un jugador en la base de datos
 *
 * @param player_id ID del jugador
 * @param is_captain Nuevo estado de capitan (1=capitan, 0=no capitan)
 * @return 1 si se actualizo exitosamente, 0 si hubo error
 */
int update_player_captain_status(int player_id, int is_captain)
{
    const char *sql = "UPDATE jugador SET es_capitan = ? WHERE id = ?;";
    return ejecutar_update_int(sql, is_captain, player_id);
}

void agregar_nuevo_jugador(int equipo_id, int jugador_count, const int *jugadores_numeros, const int *jugadores_posiciones);
void eliminar_jugador_existente(const int *jugadores_ids, const int *jugadores_numeros, int jugador_count);
/**
 * @brief Cambia el capitan de un equipo en la base de datos
 *
 * Funcion helper que maneja el cambio de capitan,
 * reduciendo la complejidad de anidamiento en modificar_equipo().
 *
 * @param equipo_id ID del equipo
 * @param info Puntero a estructura con informacion de los jugadores
 */
void cambiar_capitan_equipo(int equipo_id, EquipoPlayerInfo *info)
{
    printf("\nSeleccione el nuevo capitan:\n");
    for (int i = 0; i < info->jugador_count; i++)
    {
        printf("%d. %s (Actual: %s)\n", info->jugadores_numeros[i], info->jugadores_nombres[i],
               info->jugadores_capitanes[i] ? "CAPITAN" : "No");
    }

    int nuevo_capitan_num = input_int("Ingrese el numero del nuevo capitan: ");

    // Buscar el jugador
    int nuevo_capitan_idx = -1;
    for (int i = 0; i < info->jugador_count; i++)
    {
        if (info->jugadores_numeros[i] == nuevo_capitan_num)
        {
            nuevo_capitan_idx = i;
            break;
        }
    }

    if (nuevo_capitan_idx == -1)
    {
        printf("Numero de jugador no encontrado.\n");
        pause_console();
        return;
    }

    // Primero, quitar el capitan actual
    const char *sql_update = "UPDATE jugador SET es_capitan = 0 WHERE equipo_id = ?;";
    ejecutar_update_id(sql_update, equipo_id);

    // Luego, establecer el nuevo capitan
    sql_update = "UPDATE jugador SET es_capitan = 1 WHERE id = ?;";
    if (ejecutar_update_id(sql_update, info->jugadores_ids[nuevo_capitan_idx]))
    {
        printf("Capitan cambiado exitosamente.\n");
    }
    else
    {
        printf("Error al cambiar el capitan: %s\n", sqlite3_errmsg(db));
    }

    pause_console();
}

/**
 * @brief Asigna un equipo a un partido existente
 *
 * Funcion helper que maneja la logica de asignacion de un equipo a un partido,
 * reduciendo la complejidad de anidamiento en handle_modify_team_assignment().
 *
 * @param equipo_id ID del equipo a asignar
 */
void assign_team_to_party(int equipo_id)
{
    listar_partidos();
    int partido_id = input_int("Ingrese el ID del partido (0 para cancelar): ");
    if (partido_id <= 0 || !existe_id("partido", partido_id))
        return;

    const char *sql_update = "UPDATE equipo SET partido_id = ? WHERE id = ?;";
    if (ejecutar_update_int(sql_update, partido_id, equipo_id))
    {
        printf("Equipo asignado al partido exitosamente.\n");
    }
    else
    {
        printf("Error al asignar equipo a partido: %s\n", sqlite3_errmsg(db));
    }
}

/**
 * @brief Remueve la asignacion de partido de un equipo
 *
 * Funcion helper que maneja la logica de remover la asignacion de partido,
 * reduciendo la complejidad de anidamiento en handle_modify_team_assignment().
 *
 * @param equipo_id ID del equipo a desasignar
 */
void remove_team_from_party(int equipo_id)
{
    const char *sql_update = "UPDATE equipo SET partido_id = -1 WHERE id = ?;";
    if (ejecutar_update_id(sql_update, equipo_id))
    {
        printf("Asignacion de partido removida exitosamente.\n");
    }
    else
    {
        printf("Error al remover asignacion de partido: %s\n", sqlite3_errmsg(db));
    }
}

// Helper functions for reducing complexity in modificar_equipo
void show_available_teams_for_modification();
void handle_modify_team_name(int equipo_id);
void handle_modify_team_type(int equipo_id);
void handle_modify_team_assignment(int equipo_id);
void handle_modify_players(int equipo_id);

/**
 * @brief Obtiene el ID de un equipo para modificar desde entrada del usuario
 * @return ID del equipo seleccionado o 0 si se cancela
 */
int get_equipo_id_to_modify()
{
    int equipo_id = input_int("\nIngrese el ID del equipo a modificar (0 para cancelar): ");

    if (equipo_id == 0)
        return 0;

    if (!existe_id("equipo", equipo_id))
    {
        printf("ID de equipo invalido.\n");
        pause_console();
        return 0;
    }

    return equipo_id;
}

// Helper functions for reducing complexity in modificar_jugador_existente
void handle_modify_player_name(int player_id);
void handle_modify_player_number(int player_id, const int *all_numbers, int count);
void handle_modify_player_position(int player_id, const int *all_positions, int count, int current_index);
void handle_toggle_player_captain(int player_id);

/**
 * @brief Muestra la lista de equipos disponibles para modificacion
 *
 * Funcion helper que extrae la logica de mostrar equipos disponibles,
 * reduciendo la complejidad de anidamiento en modificar_equipo().
 */
void show_available_teams_for_modification()
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre FROM equipo ORDER BY id;";

    if (preparar_stmt(&stmt, sql))
    {
        printf("\n=== EQUIPOS DISPONIBLES ===\n\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int id = sqlite3_column_int(stmt, 0);
            const char *nombre = (const char*)sqlite3_column_text(stmt, 1);
            printf("%d. %s\n", id, nombre);
        }

        if (!found)
        {
            mostrar_no_hay_registros("equipos registrados para modificar");
            sqlite3_finalize(stmt);
            pause_console();
            return;
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Maneja la modificacion del nombre del equipo
 *
 * Funcion helper que extrae la logica de modificacion del nombre del equipo,
 * reduciendo la complejidad del switch en modificar_equipo().
 *
 * @param equipo_id ID del equipo a modificar
 */
void handle_modify_team_name(int equipo_id)
{
    char nuevo_nombre[50];
    solicitar_nombre_equipo("Ingrese el nuevo nombre: ", nuevo_nombre, sizeof(nuevo_nombre));

    const char *sql_update = "UPDATE equipo SET nombre = ? WHERE id = ?;";
    if (ejecutar_update_text(sql_update, nuevo_nombre, equipo_id))
    {
        printf("Nombre actualizado exitosamente.\n");
    }
    else
    {
        printf("Error al actualizar el nombre: %s\n", sqlite3_errmsg(db));
    }
}

/**
 * @brief Maneja la modificacion del tipo de futbol del equipo
 *
 * Funcion helper que extrae la logica de modificacion del tipo de futbol,
 * reduciendo la complejidad del switch en modificar_equipo().
 *
 * @param equipo_id ID del equipo a modificar
 */
void handle_modify_team_type(int equipo_id)
{
    printf("\nSeleccione el nuevo tipo de futbol:\n");
    printf("1. Futbol 5\n");
    printf("2. Futbol 7\n");
    printf("3. Futbol 8\n");
    printf("4. Futbol 11\n");

    int nuevo_tipo = input_int(">");
    TipoFutbol tipo_futbol;
    switch (nuevo_tipo)
    {
    case 1:
        tipo_futbol = FUTBOL_5;
        break;
    case 2:
        tipo_futbol = FUTBOL_7;
        break;
    case 3:
        tipo_futbol = FUTBOL_8;
        break;
    case 4:
        tipo_futbol = FUTBOL_11;
        break;
    default:
        printf("Opcion invalida.\n");
        pause_console();
        return;
    }

    const char *sql_update = "UPDATE equipo SET tipo_futbol = ? WHERE id = ?;";
    if (ejecutar_update_int(sql_update, tipo_futbol, equipo_id))
    {
        printf("Tipo de futbol actualizado exitosamente.\n");
    }
    else
    {
        printf("Error al actualizar el tipo de futbol: %s\n", sqlite3_errmsg(db));
    }
}

/**
 * @brief Maneja la modificacion de la asignacion a partido del equipo
 *
 * Funcion helper que extrae la logica de modificacion de asignacion a partido,
 * reduciendo la complejidad del switch en modificar_equipo().
 *
 * @param equipo_id ID del equipo a modificar
 */
void handle_modify_team_assignment(int equipo_id)
{
    if (confirmar("Desea asignar este equipo a un partido?"))
    {
        assign_team_to_party(equipo_id);
    }
    else
    {
        remove_team_from_party(equipo_id);
    }
}

/**
 * @brief Maneja el menu de modificacion de jugadores del equipo
 *
 * Funcion helper que extrae la logica completa de modificacion de jugadores,
 * reduciendo significativamente la complejidad del switch en modificar_equipo().
 *
 * @param equipo_id ID del equipo cuyos jugadores se van a modificar
 */
void handle_modify_players(int equipo_id)
{
    printf("\n=== MODIFICAR JUGADORES ===\n");

    // Obtener jugadores actuales del equipo
    sqlite3_stmt *stmt_jugadores;
    const char *sql_jugadores = "SELECT id, nombre, numero, posicion, es_capitan FROM jugador WHERE equipo_id = ? ORDER BY numero;";

    if (preparar_stmt(&stmt_jugadores, sql_jugadores))
    {
        sqlite3_bind_int(stmt_jugadores, 1, equipo_id);

        printf("\nJugadores actuales:\n");
        int jugador_count = 0;
        int jugadores_ids[11]; // Maximo para futbol 11
        char jugadores_nombres[11][50];
        int jugadores_numeros[11];
        int jugadores_posiciones[11];
        int jugadores_capitanes[11];

        while (sqlite3_step(stmt_jugadores) == SQLITE_ROW)
        {
            jugadores_ids[jugador_count] = sqlite3_column_int(stmt_jugadores, 0);
            snprintf(jugadores_nombres[jugador_count], sizeof(jugadores_nombres[jugador_count]), "%s", (const char*)sqlite3_column_text(stmt_jugadores, 1));
            jugadores_numeros[jugador_count] = sqlite3_column_int(stmt_jugadores, 2);
            jugadores_posiciones[jugador_count] = sqlite3_column_int(stmt_jugadores, 3);
            jugadores_capitanes[jugador_count] = sqlite3_column_int(stmt_jugadores, 4);

            printf("%d. %s (Numero: %d, Posicion: %s)%s\n",
                   jugadores_numeros[jugador_count],
                   jugadores_nombres[jugador_count],
                   jugadores_numeros[jugador_count],
                   get_nombre_posicion(jugadores_posiciones[jugador_count]),
                   jugadores_capitanes[jugador_count] ? " [CAPITAN]" : "");

            jugador_count++;
        }
        sqlite3_finalize(stmt_jugadores);

        if (jugador_count == 0)
        {
            mostrar_no_hay_registros("jugadores registrados para este equipo");
            pause_console();
            return;
        }

        // Mostrar opciones de modificacion
        printf("\nSeleccione que desea hacer:\n");
        printf("1. Modificar un jugador existente\n");
        printf("2. Agregar un nuevo jugador\n");
        printf("3. Eliminar un jugador\n");
        printf("4. Cambiar capitan\n");
        printf("5. Volver\n");

        int opcion_jugador = input_int(">");

        switch (opcion_jugador)
        {
        case 1:
            modificar_jugador_existente(jugadores_ids, jugadores_nombres,
                                        jugadores_numeros, jugadores_posiciones, jugadores_capitanes, jugador_count);
            break;
        case 2:
            agregar_nuevo_jugador(equipo_id, jugador_count, jugadores_numeros, jugadores_posiciones);
            break;
        case 3:
            eliminar_jugador_existente(jugadores_ids, jugadores_numeros, jugador_count);
            break;
        case 4:
        {
            EquipoPlayerInfo info =
            {
                .jugador_count = jugador_count
            };
            memcpy(info.jugadores_ids, jugadores_ids, sizeof(int) * jugador_count);
            memcpy(info.jugadores_nombres, jugadores_nombres, sizeof(jugadores_nombres));
            memcpy(info.jugadores_numeros, jugadores_numeros, sizeof(int) * jugador_count);
            memcpy(info.jugadores_posiciones, jugadores_posiciones, sizeof(int) * jugador_count);
            memcpy(info.jugadores_capitanes, jugadores_capitanes, sizeof(int) * jugador_count);

            cambiar_capitan_equipo(equipo_id, &info);
            break;
        }
        case 5:
            break;
        default:
            printf("Opcion invalida.\n");
            pause_console();
        }
    }
}

/**
 * @brief Maneja la modificacion del nombre de un jugador
 *
 * Funcion helper que extrae la logica de modificacion del nombre,
 * reduciendo la complejidad del switch en modificar_jugador_existente().
 *
 * @param player_id ID del jugador a modificar
 */
void handle_modify_player_name(int player_id)
{
    char nuevo_nombre[50];
    input_string("Ingrese el nuevo nombre: ", nuevo_nombre, sizeof(nuevo_nombre));

    if (update_player_name(player_id, nuevo_nombre))
    {
        printf("Nombre del jugador actualizado exitosamente.\n");
    }
    else
    {
        printf("Error al actualizar el nombre: %s\n", sqlite3_errmsg(db));
    }
}

/**
 * @brief Maneja la modificacion del numero de un jugador
 *
 * Funcion helper que extrae la logica de modificacion del numero,
 * incluyendo validacion de duplicados, reduciendo la complejidad del switch.
 *
 * @param player_id ID del jugador a modificar
 * @param all_numbers Array con todos los numeros de jugadores del equipo
 * @param count Numero total de jugadores en el equipo
 */
void handle_modify_player_number(int player_id, const int *all_numbers, int count)
{
    int nuevo_numero = input_int("Ingrese el nuevo numero: ");

    if (numero_repetido(all_numbers, count, nuevo_numero, -1))
    {
        printf("El numero ya esta en uso por otro jugador.\n");
        pause_console();
        return;
    }

    if (update_player_number(player_id, nuevo_numero))
    {
        printf("Numero del jugador actualizado exitosamente.\n");
    }
    else
    {
        printf("Error al actualizar el numero: %s\n", sqlite3_errmsg(db));
    }
}

/**
 * @brief Maneja la modificacion de la posicion de un jugador
 *
 * Funcion helper que extrae la logica de seleccion y actualizacion de posicion,
 * reduciendo la complejidad del switch en modificar_jugador_existente().
 *
 * @param player_id ID del jugador a modificar
 */
void handle_modify_player_position(int player_id, const int *all_positions, int count, int current_index)
{
    printf("Seleccione la nueva posicion:\n");
    printf("1. Arquero\n");
    printf("2. Defensor\n");
    printf("3. Mediocampista\n");
    printf("4. Delantero\n");

    int nueva_posicion = input_int(">");
    Posicion posicion;
    switch (nueva_posicion)
    {
    case 1:
        posicion = ARQUERO;
        break;
    case 2:
        posicion = DEFENSOR;
        break;
    case 3:
        posicion = MEDIOCAMPISTA;
        break;
    case 4:
        posicion = DELANTERO;
        break;
    default:
        printf("Posicion invalida.\n");
        pause_console();
        return;
    }

    if (posicion == ARQUERO && ya_hay_arquero(all_positions, count, current_index))
    {
        printf("Ya hay un arquero en este equipo. Solo se permite uno.\n");
        pause_console();
        return;
    }

    if (update_player_position(player_id, posicion))
    {
        printf("Posicion del jugador actualizada exitosamente.\n");
    }
    else
    {
        printf("Error al actualizar la posicion: %s\n", sqlite3_errmsg(db));
    }
}

/**
 * @brief Obtiene el estado actual de capitan de un jugador
 *
 * Funcion helper que consulta la base de datos para obtener el estado
 * de capitan actual de un jugador especifico.
 *
 * @param player_id ID del jugador
 * @return 1 si es capitan, 0 si no lo es, -1 en caso de error
 */
int get_player_captain_status(int player_id)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT es_capitan FROM jugador WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
        return -1;

    sqlite3_bind_int(stmt, 1, player_id);

    int status = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        status = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return status;
}

/**
 * @brief Maneja el cambio del estado de capitan de un jugador
 *
 * Funcion helper que extrae la logica de toggle del estado de capitan,
 * reduciendo la complejidad del switch en modificar_jugador_existente().
 *
 * @param player_id ID del jugador a modificar
 */
void handle_toggle_player_captain(int player_id)
{
    // Cambiar estado de capitan
    int current_status = get_player_captain_status(player_id);
    if (current_status == -1)
    {
        printf("Error al obtener el estado actual de capitan: %s\n", sqlite3_errmsg(db));
        return;
    }

    int nuevo_capitan = !current_status;

    if (update_player_captain_status(player_id, nuevo_capitan))
    {
        printf("Estado de capitan actualizado exitosamente.\n");
    }
    else
    {
        printf("Error al actualizar el estado de capitan: %s\n", sqlite3_errmsg(db));
    }
}

/**
 * @brief Inserta un registro de equipo en la base de datos
 *
 * Funcion helper que maneja la insercion del registro del equipo,
 * retornando el ID del equipo insertado o -1 en caso de error.
 *
 * @param equipo Puntero al equipo a insertar
 * @return ID del equipo insertado, o -1 si hay error
 */
int insert_equipo_record(const Equipo *equipo)
{
    sqlite3_stmt *stmt;
    const char *sql_next_id =
        "SELECT CASE "
        "WHEN NOT EXISTS (SELECT 1 FROM equipo WHERE id = 1) THEN 1 "
        "ELSE ("
        "  SELECT MIN(e1.id + 1) "
        "  FROM equipo e1 "
        "  LEFT JOIN equipo e2 ON e2.id = e1.id + 1 "
        "  WHERE e2.id IS NULL"
        ") END;";
    const char *sql = "INSERT INTO equipo (id, nombre, tipo, tipo_futbol, num_jugadores, partido_id) VALUES (?, ?, ?, ?, ?, ?);";
    int next_id = -1;

    if (sqlite3_prepare_v2(db, sql_next_id, -1, &stmt, 0) != SQLITE_OK)
    {
        printf("Error al preparar consulta de ID para equipo: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        next_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (next_id <= 0)
    {
        printf("No se pudo determinar un ID valido para el equipo.\n");
        return -1;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, next_id);
        sqlite3_bind_text(stmt, 2, equipo->nombre, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, equipo->tipo);
        sqlite3_bind_int(stmt, 4, equipo->tipo_futbol);
        sqlite3_bind_int(stmt, 5, equipo->num_jugadores);
        sqlite3_bind_int(stmt, 6, equipo->partido_id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            sqlite3_finalize(stmt);
            return next_id;
        }
        else
        {
            printf("Error al guardar el equipo: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return -1;
        }
    }
    else
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return -1;
    }
}

/**
 * @brief Inserta todos los jugadores de un equipo en la base de datos
 *
 * Funcion helper que maneja la insercion de todos los jugadores
 * asociados a un equipo especifico.
 *
 * @param equipo_id ID del equipo al que pertenecen los jugadores
 * @param equipo Puntero al equipo que contiene los jugadores
 */
void insert_jugadores_for_equipo(int equipo_id, const Equipo *equipo)
{
    sqlite3_stmt *stmt_jugador;
    const char *sql_jugador = "INSERT INTO jugador (equipo_id, nombre, numero, posicion, es_capitan) VALUES (?, ?, ?, ?, ?);";

    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        Jugador const *jugador = &equipo->jugadores[i];

        if (sqlite3_prepare_v2(db, sql_jugador, -1, &stmt_jugador, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt_jugador, 1, equipo_id);
            sqlite3_bind_text(stmt_jugador, 2, jugador->nombre, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt_jugador, 3, jugador->numero);
            sqlite3_bind_int(stmt_jugador, 4, jugador->posicion);
            sqlite3_bind_int(stmt_jugador, 5, jugador->es_capitan);

            sqlite3_step(stmt_jugador);
            sqlite3_finalize(stmt_jugador);
        }
    }
}

/**
 * @brief Maneja la asignacion opcional de un equipo a un partido
 *
 * Funcion helper que pregunta al usuario si desea asignar el equipo
 * a un partido existente y maneja la logica correspondiente.
 *
 * @param equipo_id ID del equipo a asignar
 */
void handle_party_assignment(int equipo_id)
{
    if (!confirmar("Desea asignar este equipo a un partido existente?"))
        return;

    listar_partidos();
    int partido_id = input_int("Ingrese el ID del partido (0 para cancelar): ");
    if (partido_id <= 0 || !existe_id("partido", partido_id))
        return;

    sqlite3_stmt *stmt;
    const char *sql_update = "UPDATE equipo SET partido_id = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, 0) != SQLITE_OK)
        return;

    sqlite3_bind_int(stmt, 1, partido_id);
    sqlite3_bind_int(stmt, 2, equipo_id);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        printf("Error al asignar equipo a partido: %s\n", sqlite3_errmsg(db));
    }
    else
    {
        printf("Equipo asignado al partido %d exitosamente.\n", partido_id);
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Genera cancha de futbol animada con balon en movimiento
 */
void mostrar_cancha_animada(int minuto, int evento_tipo)
{
    // Posiciones del balon basadas en el minuto y tipo de evento
    char *posiciones[12] =
    {
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n          |  CENTRO  |                 \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n         O|  CENTRO  |                 \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n          ============                \n          |  CENTRO  |O                \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |O AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n          |  CENTRO  |                 \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL O|               \n         +-------------+               \n                                       \n          ============                \n          |  CENTRO  |                 \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n         O============                \n          |  CENTRO  |                 \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n          |O CENTRO  |                 \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n          |  CENTRO O|                 \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n          |  CENTRO  |                 \n          =========O==                \n                                       \n         +-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n          |  CENTRO  |                 \n          ============                \n                                       \n        O+-------------+               \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n          |  CENTRO  |                 \n          ============                \n                                       \n         +-------------+O              \n         | AREA VISITANTE |            \n         +-------------+               ",
        "         +-------------+               \n         |  AREA LOCAL  |               \n         +-------------+               \n                                       \n          ============                \n          |  CENTRO  |                 \n          ============                \n                                       \n         +-------------+               \n         | AREA VISITANTE |O           \n         +-------------+               "
    };

    printf("=======================================\n");
    printf("           CANCHA DE FUTBOL            \n");
    printf("=======================================\n");

    // Mostrar la posicion del balon basada en el minuto y tipo de evento
    /*
     * El indice de posicion se calcula de forma ciclica (modulo 12) utilizando el minuto
     * actual y el tipo de evento para variar la visualizacion dinamicamente cada vez
     * que la funcion es llamada durante la simulacion.
     */
    int posicion_index = (minuto + evento_tipo) % 12;
    printf("%s\n", posiciones[posicion_index]);

    printf("=======================================\n");
}

/**
 * @brief Convierte una posicion enumerada a su nombre textual
 *
 * Proporciona representacion legible de posiciones para interfaz de usuario,
 * facilitando la comprension y seleccion de roles de jugadores.
 *
 * @param posicion El valor enumerado de la posicion
 * @return Cadena constante con el nombre de la posicion, o "Desconocido" si no es valida
 */
const char* get_nombre_posicion(Posicion posicion)
{
    switch (posicion)
    {
    case ARQUERO:
        return "Arquero";
    case DEFENSOR:
        return "Defensor";
    case MEDIOCAMPISTA:
        return "Mediocampista";
    case DELANTERO:
        return "Delantero";
    default:
        return "Desconocido";
    }
}

/**
 * @brief Convierte un tipo de futbol enumerado a su nombre textual
 *
 * Facilita la presentacion de modalidades deportivas en interfaz de usuario,
 * permitiendo seleccion y visualizacion clara de formatos de juego disponibles.
 *
 * @param tipo El valor enumerado del tipo de futbol
 * @return Cadena constante con el nombre del tipo de futbol, o "Desconocido" si no es valido
 */
const char* get_nombre_tipo_futbol(TipoFutbol tipo)
{
    switch (tipo)
    {
    case FUTBOL_5:
        return "Futbol 5";
    case FUTBOL_7:
        return "Futbol 7";
    case FUTBOL_8:
        return "Futbol 8";
    case FUTBOL_11:
        return "Futbol 11";
    default:
        return "Desconocido";
    }
}

/**
 * @brief Solicita y valida la entrada de datos para un jugador
 *
 * Esta funcion helper centraliza la logica de entrada de datos para jugadores,
 * reduciendo la complejidad cognitiva en funciones mas grandes al extraer
 * la logica repetitiva de validacion de nombre y seleccion de posicion.
 *
 * @param jugador Puntero al jugador que se va a modificar
 * @param numero_auto Si es verdadero, asigna numeros automaticamente
 */
void input_jugador_data(Jugador *jugador, int numero_auto)
{
    // Nombre del jugador
    char nombre_temp[50];
    do
    {
        input_string("Nombre: ", nombre_temp, sizeof(nombre_temp));
        if (safe_strnlen(nombre_temp, sizeof(nombre_temp)) == 0)
        {
            printf("El nombre no puede estar vacio. Intente nuevamente.\n");
        }
    }
    while (safe_strnlen(nombre_temp, sizeof(nombre_temp)) == 0);
    snprintf(jugador->nombre, sizeof(jugador->nombre), "%s", nombre_temp);

    // Numero del jugador
    if (numero_auto)
    {
        // Auto-asignar numeros secuenciales (se asignara despues)
        jugador->numero = 0;
    }
    else
    {
        jugador->numero = input_int("Numero: ");
    }

    // Posicion del jugador
    jugador->posicion = select_posicion();
    jugador->es_capitan = 0;
}

/**
 * @brief Muestra menu de seleccion de posicion y retorna la posicion elegida
 *
 * Funcion helper que extrae la logica de seleccion de posicion, reduciendo
 * duplicacion de codigo y mejorando la mantenibilidad.
 *
 * @return La posicion seleccionada por el usuario
 */
Posicion select_posicion()
{
    printf("Posicion:\n");
    printf("1. Arquero\n");
    printf("2. Defensor\n");
    printf("3. Mediocampista\n");
    printf("4. Delantero\n");

    int opcion_posicion = input_int(">");
    switch (opcion_posicion)
    {
    case 1:
        return ARQUERO;
    case 2:
        return DEFENSOR;
    case 3:
        return MEDIOCAMPISTA;
    case 4:
        return DELANTERO;
    default:
        printf("Posicion invalida. Se asignara como Delantero.\n");
        return DELANTERO;
    }
}

/**
 * @brief Muestra lista de jugadores y permite seleccionar capitan
 *
 * Funcion helper que centraliza la logica de seleccion de capitan,
 * reduciendo complejidad en funciones que crean equipos.
 *
 * @param equipo Puntero al equipo
 * @return indice del capitan seleccionado, o -1 si no se selecciona
 */
int select_capitan(Equipo *equipo)
{
    printf("\nSeleccione el capitan del equipo (1-%d):\n", equipo->num_jugadores);
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        printf("%d. %s\n", i + 1, equipo->jugadores[i].nombre);
    }

    int capitan_idx = input_int(">") - 1;
    if (capitan_idx >= 0 && capitan_idx < equipo->num_jugadores)
    {
        equipo->jugadores[capitan_idx].es_capitan = 1;
        return capitan_idx;
    }
    else
    {
        printf("Seleccion invalida. No se asignara capitan.\n");
        return -1;
    }
}

/**
 * @brief Guarda un equipo completo en la base de datos
 *
 * Funcion helper que encapsula toda la logica de guardado de equipo
 * y jugadores en la base de datos, reduciendo complejidad en crear_equipo_fijo.
 * Utiliza funciones helper para mantener bajo nivel de complejidad cognitiva.
 *
 * @param equipo Puntero al equipo a guardar
 */
void save_equipo_to_db(const Equipo *equipo)
{
    int equipo_id = insert_equipo_record(equipo);

    if (equipo_id != -1)
    {
        insert_jugadores_for_equipo(equipo_id, equipo);
        printf("Equipo guardado exitosamente con ID: %d\n", equipo_id);
        if (confirmar("Desea cargar imagen para este equipo ahora?") &&
                !cargar_imagen_para_equipo_id(equipo_id))
        {
            printf("No se pudo cargar la imagen en este momento.\n");
        }
        handle_party_assignment(equipo_id);
    }
}

/**
 * @brief Inicializa los datos basicos de un equipo
 *
 * Funcion helper que configura nombre, tipo de futbol y numero de jugadores,
 * reduciendo duplicacion de codigo entre diferentes funciones de creacion de equipos.
 *
 * @param equipo Puntero al equipo a inicializar
 * @param tipo_futbol Tipo de futbol seleccionado
 * @param num_jugadores Numero de jugadores calculado
 */
void input_equipo_basico(Equipo *equipo, TipoFutbol tipo_futbol, int num_jugadores)
{
    equipo->tipo_futbol = tipo_futbol;
    equipo->num_jugadores = num_jugadores;

    // Solicitar nombre del equipo
    solicitar_nombre_equipo("Ingrese el nombre del equipo: ", equipo->nombre, sizeof(equipo->nombre));
}

/**
 * @brief Crea los jugadores para un equipo solicitando datos al usuario
 *
 * Funcion helper que maneja la creacion de todos los jugadores de un equipo,
 * reduciendo la complejidad de anidamiento en funciones de creacion de equipos.
 *
 * @param equipo Puntero al equipo cuyos jugadores se van a crear
 * @param auto_numero Si es verdadero, asigna numeros automaticamente secuenciales
 * @param prefix Prefijo para los mensajes de jugador (ej. "EQUIPO LOCAL - ")
 */
void crear_jugadores_equipo(Equipo *equipo, int auto_numero, const char *prefix)
{
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        int jugador_valido = 0;
        while (!jugador_valido)
        {
            clear_screen();
            printf("\n%sJugador %d de %d:\n", prefix, i + 1, equipo->num_jugadores);
            mostrar_progreso_creacion_equipo(equipo, i, prefix);

            input_jugador_data(&equipo->jugadores[i], auto_numero);

            if (auto_numero)
            {
                equipo->jugadores[i].numero = i + 1;
            }

            if (!auto_numero && numero_repetido_en_equipo(equipo, i, equipo->jugadores[i].numero, -1))
            {
                printf("El numero ya esta en uso. Intente con otro.\n");
                pause_console();
                continue;
            }

            if (equipo->jugadores[i].posicion == ARQUERO && ya_hay_arquero_en_equipo(equipo, i, -1))
            {
                printf("Ya hay un arquero cargado. Solo se permite uno por equipo.\n");
                pause_console();
                continue;
            }

            jugador_valido = 1;
        }
    }
}

/**
 * @brief Muestra menu para seleccionar tipo de futbol y retorna la seleccion
 *
 * Funcion helper que extrae la logica de seleccion de tipo de futbol,
 * reduciendo duplicacion de codigo y mejorando mantenibilidad.
 *
 * @return El tipo de futbol seleccionado, o -1 si el usuario cancela
 */
TipoFutbol seleccionar_tipo_futbol()
{
    printf("\nSeleccione el tipo de futbol:\n");
    printf("1. Futbol 5\n");
    printf("2. Futbol 7\n");
    printf("3. Futbol 8\n");
    printf("4. Futbol 11\n");
    printf("5. Volver\n");

    int opcion_tipo = input_int(">");
    switch (opcion_tipo)
    {
    case 1:
        return FUTBOL_5;
    case 2:
        return FUTBOL_7;
    case 3:
        return FUTBOL_8;
    case 4:
        return FUTBOL_11;
    case 5:
        return -1; // Cancelar
    default:
        printf("Opcion invalida. Volviendo al menu principal.\n");
        pause_console();
        return -1;
    }
}

/**
 * @brief Retorna el numero de jugadores correspondiente a un tipo de futbol
 *
 * Funcion helper que encapsula la logica de conversion de tipo de futbol
 * a numero de jugadores, centralizando esta regla de negocio.
 *
 * @param tipo_futbol El tipo de futbol
 * @return Numero de jugadores para ese tipo de futbol
 */
int get_num_jugadores_por_tipo(TipoFutbol tipo_futbol)
{
    switch (tipo_futbol)
    {
    case FUTBOL_5:
        return 5;
    case FUTBOL_7:
        return 7;
    case FUTBOL_8:
        return 8;
    case FUTBOL_11:
        return 11;
    default:
        return 5; // Default a futbol 5
    }
}

/**
 * @brief Modifica un jugador existente en la base de datos
 *
 * Funcion helper que maneja la modificacion de campos especificos de un jugador,
 * reduciendo la complejidad de anidamiento en modificar_equipo().
 *
 * @param jugadores_ids Array con los IDs de los jugadores
 * @param jugadores_nombres Array con los nombres de los jugadores
 * @param jugadores_numeros Array con los numeros de los jugadores
 * @param jugadores_capitanes Array con el estado de capitan de los jugadores
 * @param jugador_count Numero total de jugadores
 */
void modificar_jugador_existente(const int *jugadores_ids, char jugadores_nombres[][50],
                                 const int *jugadores_numeros, const int *jugadores_posiciones, const int *jugadores_capitanes, int jugador_count)
{
    int jugador_num = input_int("Ingrese el numero del jugador a modificar: ");

    // Buscar el jugador
    int jugador_idx = -1;
    for (int i = 0; i < jugador_count; i++)
    {
        if (jugadores_numeros[i] == jugador_num)
        {
            jugador_idx = i;
            break;
        }
    }

    if (jugador_idx == -1)
    {
        printf("Numero de jugador no encontrado.\n");
        pause_console();
        return;
    }

    // Mostrar informacion actual
    printf("\nModificando jugador: %s\n", jugadores_nombres[jugador_idx]);
    printf("1. Nombre: %s\n", jugadores_nombres[jugador_idx]);
    printf("2. Numero: %d\n", jugadores_numeros[jugador_idx]);
    printf("3. Posicion: %s\n", get_nombre_posicion(jugadores_posiciones[jugador_idx]));
    printf("4. Capitan: %s\n", jugadores_capitanes[jugador_idx] ? "Si" : "No");
    printf("5. Volver\n");

    int campo_modificar = input_int("Seleccione el campo a modificar: ");

    switch (campo_modificar)
    {
    case 1:
        handle_modify_player_name(jugadores_ids[jugador_idx]);
        break;
    case 2:
        handle_modify_player_number(jugadores_ids[jugador_idx], jugadores_numeros, jugador_count);
        break;
    case 3:
        handle_modify_player_position(jugadores_ids[jugador_idx], jugadores_posiciones, jugador_count, jugador_idx);
        break;
    case 4:
        handle_toggle_player_captain(jugadores_ids[jugador_idx]);
        break;
    case 5:
        break;
    default:
        printf("Opcion invalida.\n");
    }

    pause_console();
}

/**
 * @brief Agrega un nuevo jugador a un equipo en la base de datos
 *
 * Funcion helper que maneja la logica de agregar un jugador,
 * reduciendo la complejidad de anidamiento en modificar_equipo().
 *
 * @param equipo_id ID del equipo
 * @param jugador_count Numero actual de jugadores
 * @param jugadores_numeros Array con los numeros de los jugadores existentes
 */
void agregar_nuevo_jugador(int equipo_id, int jugador_count, const int *jugadores_numeros, const int *jugadores_posiciones)
{
    if (jugador_count >= 11)
    {
        printf("El equipo ya tiene el maximo de jugadores (11).\n");
        pause_console();
        return;
    }

    Jugador nuevo_jugador;

    // Nombre del jugador
    char nombre_temp[50];
    do
    {
        input_string("Nombre: ", nombre_temp, sizeof(nombre_temp));
        if (safe_strnlen(nombre_temp, sizeof(nombre_temp)) == 0)
        {
            printf("El nombre no puede estar vacio. Intente nuevamente.\n");
        }
    }
    while (safe_strnlen(nombre_temp, sizeof(nombre_temp)) == 0);
    snprintf(nuevo_jugador.nombre, sizeof(nuevo_jugador.nombre), "%s", nombre_temp);

    // Numero del jugador
    int numero_valido = 0;
    while (!numero_valido)
    {
        nuevo_jugador.numero = input_int("Numero: ");

        if (numero_repetido(jugadores_numeros, jugador_count, nuevo_jugador.numero, -1))
        {
            printf("El numero ya esta en uso. Por favor, elija otro numero.\n");
        }
        else
        {
            numero_valido = 1;
        }
    }

    // Posicion del jugador
    printf("Posicion:\n");
    printf("1. Arquero\n");
    printf("2. Defensor\n");
    printf("3. Mediocampista\n");
    printf("4. Delantero\n");

    int opcion_posicion = input_int(">");
    switch (opcion_posicion)
    {
    case 1:
        if (ya_hay_arquero(jugadores_posiciones, jugador_count, -1))
        {
            printf("Ya hay un arquero en este equipo. Solo se permite uno.\n");
            pause_console();
            return;
        }
        nuevo_jugador.posicion = ARQUERO;
        break;
    case 2:
        nuevo_jugador.posicion = DEFENSOR;
        break;
    case 3:
        nuevo_jugador.posicion = MEDIOCAMPISTA;
        break;
    case 4:
        nuevo_jugador.posicion = DELANTERO;
        break;
    default:
        printf("Posicion invalida. Se asignara como Delantero.\n");
        nuevo_jugador.posicion = DELANTERO;
    }

    nuevo_jugador.es_capitan = 0;

    // Insertar nuevo jugador
    sqlite3_stmt *stmt;
    const char *sql_insert = "INSERT INTO jugador (equipo_id, nombre, numero, posicion, es_capitan) VALUES (?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, equipo_id);
        sqlite3_bind_text(stmt, 2, nuevo_jugador.nombre, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, nuevo_jugador.numero);
        sqlite3_bind_int(stmt, 4, nuevo_jugador.posicion);
        sqlite3_bind_int(stmt, 5, nuevo_jugador.es_capitan);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Jugador agregado exitosamente.\n");
        }
        else
        {
            printf("Error al agregar el jugador: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

/**
 * @brief Agrega un jugador nuevo a un equipo momentaneo (si hay espacio)
 */
void agregar_jugador_momentaneo(Equipo *equipo)
{
    int max_jugadores = 0;
    switch (equipo->tipo_futbol)
    {
    case FUTBOL_5:
        max_jugadores = 5;
        break;
    case FUTBOL_7:
        max_jugadores = 7;
        break;
    case FUTBOL_8:
        max_jugadores = 8;
        break;
    case FUTBOL_11:
        max_jugadores = 11;
        break;
    }

    if (equipo->num_jugadores >= max_jugadores)
    {
        printf("El equipo ya tiene el maximo de jugadores (%d).\n", max_jugadores);
        pause_console();
        return;
    }

    Jugador *nuevo_jugador = &equipo->jugadores[equipo->num_jugadores];

    // Nombre del jugador
    char nombre_temp[50];
    do
    {
        input_string("Nombre: ", nombre_temp, sizeof(nombre_temp));
        if (safe_strnlen(nombre_temp, sizeof(nombre_temp)) == 0)
        {
            printf("El nombre no puede estar vacio. Intente nuevamente.\n");
        }
    }
    while (safe_strnlen(nombre_temp, sizeof(nombre_temp)) == 0);
    snprintf(nuevo_jugador->nombre, sizeof(nuevo_jugador->nombre), "%s", nombre_temp);

    // Numero del jugador
    int numero_valido = 0;
    while (!numero_valido)
    {
        nuevo_jugador->numero = input_int("Numero: ");

        // Verificar si el numero ya existe
        int numero_existe = 0;
        for (int i = 0; i < equipo->num_jugadores; i++)
        {
            if (equipo->jugadores[i].numero == nuevo_jugador->numero)
            {
                numero_existe = 1;
                break;
            }
        }

        if (numero_existe)
        {
            printf("El numero ya esta en uso. Por favor, elija otro numero.\n");
        }
        else
        {
            numero_valido = 1;
        }
    }

    // Posicion del jugador
    printf("Posicion:\n");
    printf("1. Arquero\n");
    printf("2. Defensor\n");
    printf("3. Mediocampista\n");
    printf("4. Delantero\n");

    int opcion_posicion = input_int(">");
    switch (opcion_posicion)
    {
    case 1:
        for (int i = 0; i < equipo->num_jugadores; i++)
        {
            if (equipo->jugadores[i].posicion == ARQUERO)
            {
                printf("Ya hay un arquero en este equipo. Solo se permite uno.\n");
                pause_console();
                return;
            }
        }
        nuevo_jugador->posicion = ARQUERO;
        break;
    case 2:
        nuevo_jugador->posicion = DEFENSOR;
        break;
    case 3:
        nuevo_jugador->posicion = MEDIOCAMPISTA;
        break;
    case 4:
        nuevo_jugador->posicion = DELANTERO;
        break;
    default:
        printf("Posicion invalida. Se asignara como Delantero.\n");
        nuevo_jugador->posicion = DELANTERO;
    }

    nuevo_jugador->es_capitan = 0;
    equipo->num_jugadores++;

    printf("Jugador agregado exitosamente.\n");
    pause_console();
}

/**
 * @brief Elimina un jugador existente de la base de datos
 *
 * Funcion helper que maneja la eliminacion de un jugador,
 * reduciendo la complejidad de anidamiento en modificar_equipo().
 *
 * @param jugadores_ids Array con los IDs de los jugadores
 * @param jugadores_numeros Array con los numeros de los jugadores
 * @param jugador_count Numero total de jugadores
 */
void eliminar_jugador_existente(const int *jugadores_ids, const int *jugadores_numeros, int jugador_count)
{
    int jugador_num = input_int("Ingrese el numero del jugador a eliminar: ");

    // Buscar el jugador
    int jugador_idx = -1;
    for (int i = 0; i < jugador_count; i++)
    {
        if (jugadores_numeros[i] == jugador_num)
        {
            jugador_idx = i;
            break;
        }
    }

    if (jugador_idx == -1)
    {
        printf("Numero de jugador no encontrado.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    const char *sql_delete = "DELETE FROM jugador WHERE id = ?;";
    if (confirmar("Esta seguro que desea eliminar este jugador?") &&
            sqlite3_prepare_v2(db, sql_delete, -1, &stmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, jugadores_ids[jugador_idx]);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Jugador eliminado exitosamente.\n");
        }
        else
        {
            printf("Error al eliminar el jugador: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

/**
 * @brief Muestra los jugadores de un equipo especifico
 *
 * Funcion helper que extrae la logica de mostrar jugadores de un equipo,
 * reduciendo la complejidad de anidamiento en listar_equipos().
 *
 * @param equipo_id ID del equipo cuyos jugadores se van a mostrar
 */
void mostrar_jugadores_equipo(int equipo_id)
{
    sqlite3_stmt *stmt_jugadores;
    const char *sql_jugadores = "SELECT nombre, numero, posicion, es_capitan FROM jugador WHERE equipo_id = ? ORDER BY numero;";

    if (sqlite3_prepare_v2(db, sql_jugadores, -1, &stmt_jugadores, 0) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt_jugadores, 1, equipo_id);

        int has_jugadores = 0;
        while (sqlite3_step(stmt_jugadores) == SQLITE_ROW)
        {
            has_jugadores = 1;
            const char *jugador_nombre = (const char*)sqlite3_column_text(stmt_jugadores, 0);
            int jugador_numero = sqlite3_column_int(stmt_jugadores, 1);
            int jugador_posicion = sqlite3_column_int(stmt_jugadores, 2);
            int es_capitan = sqlite3_column_int(stmt_jugadores, 3);

            printf("%d. %s (Numero: %d, Posicion: %s)%s\n",
                   jugador_numero,
                   jugador_nombre,
                   jugador_numero,
                   get_nombre_posicion(jugador_posicion),
                   es_capitan ? " [CAPITAN]" : "");
        }

        if (!has_jugadores)
        {
            mostrar_no_hay_registros("jugadores registrados para este equipo");
        }

        sqlite3_finalize(stmt_jugadores);
    }
    else
    {
        printf("Error al obtener jugadores: %s\n", sqlite3_errmsg(db));
    }
}

/**
 * @brief Muestra por pantalla toda la informacion detallada de un equipo
 *
 * Esta funcion presenta en consola la informacion completa de un equipo,
 * incluyendo sus datos basicos, tipo de futbol, numero de jugadores y
 * la lista completa de jugadores con sus posiciones y roles.
 *
 * @param equipo Puntero al equipo cuya informacion se va a mostrar
 */
void mostrar_equipo(const Equipo *equipo)
{
    printf("\n=== INFORMACION DEL EQUIPO ===\n");
    printf("Nombre: %s\n", equipo->nombre);
    printf("Tipo: %s\n", equipo->tipo == FIJO ? "Fijo" : "Momentaneo");
    printf("Tipo de Futbol: %s\n", get_nombre_tipo_futbol(equipo->tipo_futbol));
    printf("Numero de Jugadores: %d\n", equipo->num_jugadores);
    printf("Asignado a Partido: %s\n", equipo->partido_id == -1 ? "No" : "Si");

    printf("\n=== JUGADORES ===\n");
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        const Jugador *jugador = &equipo->jugadores[i];
        printf("%d. %s (Numero: %d, Posicion: %s)%s\n",
               i + 1,
               jugador->nombre,
               jugador->numero,
               get_nombre_posicion(jugador->posicion),
               jugador->es_capitan ? " [CAPITAN]" : "");
    }
    printf("\n");
}

/**
 * @brief Crea un nuevo equipo fijo que se guarda permanentemente en la base de datos
 *
 * Esta funcion guia al usuario a traves del proceso completo de creacion de un equipo
 * permanente. Solicita el nombre, tipo de futbol, informacion de cada jugador y
 * seleccion de capitan. El equipo y sus jugadores se guardan en la base de datos
 * y opcionalmente se puede asignar a un partido existente.
 */
void crear_equipo_fijo()
{
    Equipo equipo;
    equipo.tipo = FIJO;
    equipo.partido_id = -1;

    // Determinar tipo de futbol y numero de jugadores
    TipoFutbol tipo_futbol = seleccionar_tipo_futbol();
    if (tipo_futbol == (TipoFutbol)-1) return; // Usuario cancelo

    int num_jugadores = get_num_jugadores_por_tipo(tipo_futbol);
    input_equipo_basico(&equipo, tipo_futbol, num_jugadores);

    // Solicitar informacion de cada jugador
    crear_jugadores_equipo(&equipo, 0, "");

    // Seleccionar capitan
    select_capitan(&equipo);

    // Mostrar equipo creado y guardar
    clear_screen();
    mostrar_equipo(&equipo);
    save_equipo_to_db(&equipo);
}

/**
 * @brief Crea un nuevo equipo momentaneo que no se guarda en la base de datos
 *
 * Esta funcion guia al usuario a traves del proceso de creacion de un equipo
 * temporal. Solicita el nombre, tipo de futbol, informacion de cada jugador y
 * seleccion de capitan. El equipo se crea en memoria pero no se persiste,
 * siendo util para partidos puntuales o simulaciones.
 */
void crear_equipo_momentaneo()
{
    clear_screen();
    print_header("CREAR EQUIPO MOMENTANEO");

    // Preguntar si quiere crear 1 o 2 equipos
    printf("Seleccione cuantos equipos momentaneos desea crear:\n");
    printf("1. Un solo equipo\n");
    printf("2. Dos equipos (Local y Visitante)\n");
    printf("3. Volver\n");

    int opcion_equipos = input_int(">");

    switch (opcion_equipos)
    {
    case 1:
        crear_un_equipo_momentaneo();
        break;
    case 2:
        crear_dos_equipos_momentaneos();
        break;
    case 3:
        return;
    default:
        printf("Opcion invalida. Volviendo al menu principal.\n");
        pause_console();
        return;
    }
}

/**
 * @brief Crea un solo equipo momentaneo
 */
void crear_un_equipo_momentaneo()
{
    Equipo equipo;
    equipo.tipo = MOMENTANEO;
    equipo.partido_id = -1;

    // Determinar tipo de futbol y numero de jugadores
    TipoFutbol tipo_futbol = seleccionar_tipo_futbol();
    if (tipo_futbol == (TipoFutbol)-1) return; // Usuario cancelo

    int num_jugadores = get_num_jugadores_por_tipo(tipo_futbol);
    input_equipo_basico(&equipo, tipo_futbol, num_jugadores);

    // Solicitar informacion de cada jugador con numeros auto-asignados
    crear_jugadores_equipo(&equipo, 1, "");

    // Seleccionar capitan
    select_capitan(&equipo);

    // Mostrar equipo creado y ofrecer opciones de gestion
    gestionar_equipo_momentaneo(&equipo);
}

/**
 * @brief Muestra el equipo y ofrece opciones de gestion de jugadores
 */
void gestionar_equipo_momentaneo(Equipo *equipo)
{
    int salir = 0;

    while (!salir)
    {
        clear_screen();
        mostrar_equipo(equipo);

        printf("Opciones de gestion:\n");
        printf("1. Modificar un jugador\n");
        printf("2. Agregar un jugador nuevo (si hay espacio)\n");
        printf("3. Eliminar un jugador\n");
        printf("4. Cambiar capitan\n");
        printf("5. Finalizar\n");

        int opcion = input_int("Seleccione una opcion: ");

        switch (opcion)
        {
        case 1:
            modificar_jugador_momentaneo(equipo);
            break;
        case 2:
            agregar_jugador_momentaneo(equipo);
            break;
        case 3:
            eliminar_jugador_momentaneo(equipo);
            break;
        case 4:
            cambiar_capitan_momentaneo(equipo);
            break;
        case 5:
            salir = 1;
            break;
        default:
            printf("Opcion invalida.\n");
            pause_console();
        }
    }

    printf("Este equipo es momentaneo y no se guardara.\n");
    pause_console();
}

/**
 * @brief Modifica un jugador existente en un equipo momentaneo
 */
void modificar_jugador_momentaneo(Equipo *equipo)
{
    printf("\nSeleccione el jugador a modificar (1-%d):\n", equipo->num_jugadores);
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        printf("%d. %s\n", i + 1, equipo->jugadores[i].nombre);
    }

    int jugador_idx = input_int(">") - 1;
    if (jugador_idx < 0 || jugador_idx >= equipo->num_jugadores)
    {
        printf("Seleccion invalida.\n");
        pause_console();
        return;
    }

    Jugador *jugador = &equipo->jugadores[jugador_idx];

    printf("\nModificando jugador: %s\n", jugador->nombre);
    printf("1. Nombre: %s\n", jugador->nombre);
    printf("2. Numero: %d\n", jugador->numero);
    printf("3. Posicion: %s\n", get_nombre_posicion(jugador->posicion));
    printf("4. Volver\n");

    int campo = input_int("Seleccione el campo a modificar: ");

    switch (campo)
    {
    case 1:
    {
        char nuevo_nombre[50];
        input_string("Nuevo nombre: ", nuevo_nombre, sizeof(nuevo_nombre));
        snprintf(jugador->nombre, sizeof(jugador->nombre), "%s", nuevo_nombre);
        printf("Nombre actualizado.\n");
        break;
    }
    case 2:
    {
        int nuevo_numero = input_int("Nuevo numero: ");

        // Verificar si el numero ya existe
        int numero_existe = 0;
        for (int i = 0; i < equipo->num_jugadores; i++)
        {
            if (i != jugador_idx && equipo->jugadores[i].numero == nuevo_numero)
            {
                numero_existe = 1;
                break;
            }
        }

        if (numero_existe)
        {
            printf("El numero ya esta en uso.\n");
        }
        else
        {
            jugador->numero = nuevo_numero;
            printf("Numero actualizado.\n");
        }
        break;
    }
    case 3:
    {
        printf("Seleccione nueva posicion:\n");
        printf("1. Arquero\n");
        printf("2. Defensor\n");
        printf("3. Mediocampista\n");
        printf("4. Delantero\n");

        int opcion_posicion = input_int(">");
        switch (opcion_posicion)
        {
        case 1:
            if (ya_hay_arquero_en_equipo(equipo, equipo->num_jugadores, jugador_idx))
            {
                printf("Ya hay un arquero en este equipo. Solo se permite uno.\n");
                pause_console();
                return;
            }
            jugador->posicion = ARQUERO;
            break;
        case 2:
            jugador->posicion = DEFENSOR;
            break;
        case 3:
            jugador->posicion = MEDIOCAMPISTA;
            break;
        case 4:
            jugador->posicion = DELANTERO;
            break;
        default:
            printf("Posicion invalida.\n");
        }
        printf("Posicion actualizada.\n");
        break;
    }
    case 4:
        return;
    default:
        printf("Opcion invalida.\n");
    }

    pause_console();
}

/**
 * @brief Elimina un jugador de un equipo momentaneo
 */
void eliminar_jugador_momentaneo(Equipo *equipo)
{
    printf("\nSeleccione el jugador a eliminar (1-%d):\n", equipo->num_jugadores);
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        printf("%d. %s\n", i + 1, equipo->jugadores[i].nombre);
    }

    int jugador_idx = input_int(">") - 1;
    if (jugador_idx < 0 || jugador_idx >= equipo->num_jugadores)
    {
        printf("Seleccion invalida.\n");
        pause_console();
        return;
    }

    if (confirmar("Esta seguro que desea eliminar este jugador?"))
    {
        equipo->jugadores[jugador_idx].es_capitan = 0;

        // Mover los jugadores restantes para llenar el espacio
        for (int i = jugador_idx; i < equipo->num_jugadores - 1; i++)
        {
            equipo->jugadores[i] = equipo->jugadores[i + 1];
        }

        equipo->num_jugadores--;
        printf("Jugador eliminado exitosamente.\n");
    }

    pause_console();
}

/**
 * @brief Cambia el capitan de un equipo momentaneo
 */
void cambiar_capitan_momentaneo(Equipo *equipo)
{
    printf("\nSeleccione el nuevo capitan (1-%d):\n", equipo->num_jugadores);
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        printf("%d. %s %s\n", i + 1, equipo->jugadores[i].nombre,
               equipo->jugadores[i].es_capitan ? "[CAPITAN ACTUAL]" : "");
    }

    int capitan_idx = input_int(">") - 1;
    if (capitan_idx < 0 || capitan_idx >= equipo->num_jugadores)
    {
        printf("Seleccion invalida.\n");
        pause_console();
        return;
    }

    // Quitar capitan actual
    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        equipo->jugadores[i].es_capitan = 0;
    }

    // Asignar nuevo capitan
    equipo->jugadores[capitan_idx].es_capitan = 1;
    printf("Capitan cambiado exitosamente.\n");

    pause_console();
}

/**
 * @brief Crea dos equipos momentaneos (Local y Visitante)
 */
void crear_dos_equipos_momentaneos()
{
    Equipo equipo_local;
    Equipo equipo_visitante;

    // Configurar equipos basicos
    equipo_local.tipo = MOMENTANEO;
    equipo_local.partido_id = -1;
    equipo_visitante.tipo = MOMENTANEO;
    equipo_visitante.partido_id = -1;

    // Solicitar tipo de futbol (comun para ambos equipos)
    printf("\nSeleccione el tipo de futbol:\n");
    printf("1. Futbol 5\n");
    printf("2. Futbol 7\n");
    printf("3. Futbol 8\n");
    printf("4. Futbol 11\n");
    printf("5. Volver\n");

    int opcion_tipo = input_int(">");
    TipoFutbol tipo_futbol;
    int num_jugadores = 0;

    switch (opcion_tipo)
    {
    case 1:
        tipo_futbol = FUTBOL_5;
        num_jugadores = 5;
        break;
    case 2:
        tipo_futbol = FUTBOL_7;
        num_jugadores = 7;
        break;
    case 3:
        tipo_futbol = FUTBOL_8;
        num_jugadores = 8;
        break;
    case 4:
        tipo_futbol = FUTBOL_11;
        num_jugadores = 11;
        break;
    case 5:
        return;
    default:
        printf("Opcion invalida. Volviendo al menu principal.\n");
        pause_console();
        return;
    }

    equipo_local.tipo_futbol = tipo_futbol;
    equipo_local.num_jugadores = num_jugadores;
    equipo_visitante.tipo_futbol = tipo_futbol;
    equipo_visitante.num_jugadores = num_jugadores;

    // Solicitar nombre del equipo local
    solicitar_nombre_equipo("Ingrese el nombre del equipo LOCAL: ", equipo_local.nombre, sizeof(equipo_local.nombre));

    // Solicitar informacion de jugadores para equipo local
    crear_jugadores_equipo(&equipo_local, 1, "EQUIPO LOCAL - ");

    // Seleccionar capitan para equipo local
    printf("\nSeleccione el capitan del equipo LOCAL (1-%d):\n", num_jugadores);
    for (int i = 0; i < num_jugadores; i++)
    {
        printf("%d. %s\n", i + 1, equipo_local.jugadores[i].nombre);
    }

    int capitan_idx = input_int(">") - 1;
    if (capitan_idx >= 0 && capitan_idx < num_jugadores)
    {
        equipo_local.jugadores[capitan_idx].es_capitan = 1;
    }
    else
    {
        printf("Seleccion invalida. No se asignara capitan.\n");
    }

    // Solicitar nombre del equipo visitante
    solicitar_nombre_equipo("Ingrese el nombre del equipo VISITANTE: ", equipo_visitante.nombre, sizeof(equipo_visitante.nombre));

    // Solicitar informacion de jugadores para equipo visitante
    crear_jugadores_equipo(&equipo_visitante, 1, "EQUIPO VISITANTE - ");

    // Seleccionar capitan para equipo visitante
    printf("\nSeleccione el capitan del equipo VISITANTE (1-%d):\n", num_jugadores);
    for (int i = 0; i < num_jugadores; i++)
    {
        printf("%d. %s\n", i + 1, equipo_visitante.jugadores[i].nombre);
    }

    capitan_idx = input_int(">") - 1;
    if (capitan_idx >= 0 && capitan_idx < num_jugadores)
    {
        equipo_visitante.jugadores[capitan_idx].es_capitan = 1;
    }
    else
    {
        printf("Seleccion invalida. No se asignara capitan.\n");
    }

    // Gestionar ambos equipos
    gestionar_dos_equipos_momentaneos(&equipo_local, &equipo_visitante);
}

/**
 * @brief Muestra ambos equipos y ofrece opciones de gestion de jugadores
 */
void gestionar_dos_equipos_momentaneos(Equipo *equipo_local, Equipo *equipo_visitante)
{
    int salir = 0;

    while (!salir)
    {
        clear_screen();
        printf("\n=== EQUIPO LOCAL ===\n");
        mostrar_equipo(equipo_local);
        printf("\n=== EQUIPO VISITANTE ===\n");
        mostrar_equipo(equipo_visitante);

        printf("Opciones de gestion:\n");
        printf("1. Gestionar equipo LOCAL\n");
        printf("2. Gestionar equipo VISITANTE\n");
        printf("3. Simular partido\n");
        printf("4. Finalizar\n");

        int opcion = input_int("Seleccione una opcion: ");

        switch (opcion)
        {
        case 1:
            gestionar_equipo_individual(equipo_local, "LOCAL");
            break;
        case 2:
            gestionar_equipo_individual(equipo_visitante, "VISITANTE");
            break;
        case 3:
            simular_partido(equipo_local, equipo_visitante);
            break;
        case 4:
            salir = 1;
            break;
        default:
            printf("Opcion invalida.\n");
            pause_console();
        }
    }

    printf("Estos equipos son momentaneos y no se guardaran.\n");
    pause_console();
}

/**
 * @brief Gestiona un equipo individual dentro del contexto de dos equipos
 */
void gestionar_equipo_individual(Equipo *equipo, const char *tipo_equipo)
{
    int salir = 0;

    while (!salir)
    {
        clear_screen();
        printf("\n=== EQUIPO %s ===\n", tipo_equipo);
        mostrar_equipo(equipo);

        printf("Opciones de gestion para equipo %s:\n", tipo_equipo);
        printf("1. Modificar un jugador\n");
        printf("2. Agregar un jugador nuevo (si hay espacio)\n");
        printf("3. Eliminar un jugador\n");
        printf("4. Cambiar capitan\n");
        printf("5. Volver\n");

        int opcion = input_int("Seleccione una opcion: ");

        switch (opcion)
        {
        case 1:
            modificar_jugador_momentaneo(equipo);
            break;
        case 2:
            agregar_jugador_momentaneo(equipo);
            break;
        case 3:
            eliminar_jugador_momentaneo(equipo);
            break;
        case 4:
            cambiar_capitan_momentaneo(equipo);
            break;
        case 5:
            salir = 1;
            break;
        default:
            printf("Opcion invalida.\n");
            pause_console();
        }
    }
}

/**
 * @brief Funcion principal para crear equipos
 *
 * Muestra un menu que permite al usuario elegir entre crear un equipo fijo
 * (que se guarda en la base de datos) o un equipo momentaneo (que no se guarda).
 * Delegada a las funciones especificas segun la opcion seleccionada.
 */
void crear_equipo()
{
    clear_screen();
    print_header("CREAR EQUIPO");

    printf("Seleccione el tipo de equipo:\n");
    printf("1. Fijo\n");
    printf("2. Momentaneo\n");
    printf("3. Volver\n");

    int opcion = input_int(">");

    switch (opcion)
    {
    case 1:
        crear_equipo_fijo();
        break;
    case 2:
        crear_equipo_momentaneo();
        break;
    case 3:
        return;
    default:
        printf("Opcion invalida.\n");
        pause_console();
    }
}

/**
 * @brief Muestra un listado completo de todos los equipos registrados en el sistema
 *
 * Esta funcion consulta la base de datos y presenta en pantalla todos los equipos
 * con sus respectivos datos, incluyendo informacion detallada de cada jugador.
 * Muestra el ID, nombre, tipo, tipo de futbol, numero de jugadores y asignacion
 * a partidos para cada equipo registrado.
 */
void listar_equipos()
{
    clear_screen();
    print_header("LISTAR EQUIPOS");

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre, tipo, tipo_futbol, num_jugadores, partido_id FROM equipo ORDER BY id;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
    {
        ui_printf_centered_line("=== LISTA DE EQUIPOS ===");
        ui_printf("\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int id = sqlite3_column_int(stmt, 0);
            const char *nombre = (const char*)sqlite3_column_text(stmt, 1);
            int tipo = sqlite3_column_int(stmt, 2);
            int tipo_futbol = sqlite3_column_int(stmt, 3);
            int num_jugadores = sqlite3_column_int(stmt, 4);
            int partido_id = sqlite3_column_int(stmt, 5);

            ui_printf_centered_line("ID: %d", id);
            ui_printf_centered_line("Nombre: %s", nombre);
            ui_printf_centered_line("Tipo: %s", tipo == FIJO ? "Fijo" : "Momentaneo");
            ui_printf_centered_line("Tipo de Futbol: %s", get_nombre_tipo_futbol(tipo_futbol));
            ui_printf_centered_line("Numero de Jugadores: %d", num_jugadores);
            ui_printf_centered_line("Asignado a Partido: %s", partido_id == -1 ? "No" : "Si");

            // Mostrar jugadores del equipo
            ui_printf_centered_line("=== JUGADORES ===");
            mostrar_jugadores_equipo(id);
            ui_printf_centered_line("----------------------------------------");
        }

        if (!found)
        {
            mostrar_no_hay_registros("equipos registrados");
        }
    }
    else
    {
        printf("Error al obtener la lista de equipos: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Permite modificar los datos de un equipo existente en la base de datos
 *
 * Esta funcion presenta un menu interactivo que permite al usuario modificar
 * diversos aspectos de un equipo existente, incluyendo su nombre, tipo de futbol,
 * asignacion a partidos y gestion completa de jugadores (modificar, agregar,
 * eliminar o cambiar capitan). Muestra primero la lista de equipos disponibles
 * y solicita confirmacion antes de aplicar cualquier cambio.
 *
 * La complejidad cognitiva se ha reducido significativamente mediante el uso
 * de funciones helper que encapsulan logica especifica, siguiendo el principio
 * de responsabilidad unica y evitando anidamiento profundo.
 */
void modificar_equipo()
{
    clear_screen();
    print_header("MODIFICAR EQUIPO");

    // Mostrar lista de equipos disponibles
    show_available_teams_for_modification();

    // Obtener y validar ID del equipo
    int equipo_id = get_equipo_id_to_modify();
    if (equipo_id <= 0) return; // Usuario cancelo o error

    // Mostrar menu de opciones de modificacion
    printf("\nSeleccione que desea modificar:\n");
    printf("1. Nombre del equipo\n");
    printf("2. Tipo de futbol\n");
    printf("3. Asignacion a partido\n");
    printf("4. Jugadores\n");
    printf("5. Volver\n");

    int opcion = input_int(">");

    // Procesar opcion seleccionada usando funciones helper
    switch (opcion)
    {
    case 1:
        handle_modify_team_name(equipo_id);
        break;
    case 2:
        handle_modify_team_type(equipo_id);
        break;
    case 3:
        handle_modify_team_assignment(equipo_id);
        break;
    case 4:
        handle_modify_players(equipo_id);
        break;
    case 5:
        return; // Usuario cancelo
    default:
        printf("Opcion invalida.\n");
    }

    pause_console();
}

/**
 * @brief Elimina un equipo existente de la base de datos
 *
 * Esta funcion permite al usuario eliminar permanentemente un equipo y todos sus
 * jugadores asociados. Muestra primero la lista de equipos disponibles, solicita
 * confirmacion del ID a eliminar y requiere confirmacion explicita antes de
 * proceder con la eliminacion. Primero elimina todos los jugadores asociados
 * y luego elimina el registro del equipo.
 */
void eliminar_equipo()
{
    clear_screen();
    print_header("ELIMINAR EQUIPO");
    sqlite3_stmt *stmt;
    int equipo_id = select_team_id("\nIngrese el ID del equipo a eliminar (0 para cancelar): ",
                                   "equipos registrados para eliminar", 1);
    if (equipo_id == 0)
    {
        return;
    }

    if (confirmar("Esta seguro que desea eliminar este equipo? Esta accion no se puede deshacer."))
    {
        // Eliminar jugadores primero
        const char *sql_delete_jugadores = "DELETE FROM jugador WHERE equipo_id = ?;";
        if (sqlite3_prepare_v2(db, sql_delete_jugadores, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, equipo_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        // Eliminar equipo
        const char *sql_delete_equipo = "DELETE FROM equipo WHERE id = ?;";
        if (sqlite3_prepare_v2(db, sql_delete_equipo, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, equipo_id);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                printf("Equipo eliminado exitosamente.\n");
            }
            else
            {
                printf("Error al eliminar el equipo: %s\n", sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);
        }
    }
    else
    {
        printf("Eliminacion cancelada.\n");
    }

    pause_console();
}

/**
 * @brief Inicializa las estadisticas del partido
 *
 * @param stats Puntero a estructura de estadisticas del partido
 */
void inicializar_estadisticas_partido(PartidoStats *stats)
{
    stats->goles_local = 0;
    stats->goles_visitante = 0;

    for (int i = 0; i < 11; i++)
    {
        stats->goles_jugadores_local[i] = 0;
        stats->asistencias_jugadores_local[i] = 0;
        stats->goles_jugadores_visitante[i] = 0;
        stats->asistencias_jugadores_visitante[i] = 0;
    }
}

/**
 * @brief Muestra la informacion inicial del partido
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 */
void mostrar_informacion_inicial(const Equipo *equipo_local, const Equipo *equipo_visitante)
{
    printf("=== PARTIDO ENTRE %s VS %s ===\n\n", equipo_local->nombre, equipo_visitante->nombre);

    // Mostrar cancha inicial
    mostrar_cancha_animada(0, 0);

    // Mostrar equipos alineados
    printf("EQUIPO LOCAL (%s):\n", equipo_local->nombre);
    for (int i = 0; i < equipo_local->num_jugadores; i++)
    {
        printf("  %d. %s", equipo_local->jugadores[i].numero, equipo_local->jugadores[i].nombre);
        if (equipo_local->jugadores[i].es_capitan)
            printf(" (C)");
        printf("\n");
    }

    printf("\nEQUIPO VISITANTE (%s):\n", equipo_visitante->nombre);
    for (int i = 0; i < equipo_visitante->num_jugadores; i++)
    {
        printf("  %d. %s", equipo_visitante->jugadores[i].numero, equipo_visitante->jugadores[i].nombre);
        if (equipo_visitante->jugadores[i].es_capitan)
            printf(" (C)");
        printf("\n");
    }

    printf("\n*** INICIO DEL PARTIDO ***\n");
    printf("La simulacion comenzara automaticamente en 3 segundos...\n");
    Sleep(3000); // Esperar 3 segundos antes de comenzar
}

/**
 * @brief Genera un evento aleatorio y retorna el tipo
 *
 * @return Tipo de evento (0=normal, 1=gol, 2=oportunidad, 3=falta)
 */
int generar_evento_aleatorio()
{
    unsigned int random_value;
    random_value = (unsigned int)rand();
    int evento_aleatorio = random_value % 100;
    if (evento_aleatorio < 25) return 1; // gol local
    if (evento_aleatorio < 50) return 2; // gol visitante
    if (evento_aleatorio < 70) return 3; // oportunidad
    if (evento_aleatorio < 85) return 4; // falta
    return 0; // normal
}

/**
 * @brief Maneja un gol del equipo local
 *
 * @param equipo_local Puntero al equipo local
 * @param minuto_actual Minuto actual del partido
 * @param goles_local Puntero a contador de goles local
 * @param goles_jugadores_local Array de goles por jugador local
 * @param asistencias_jugadores_local Array de asistencias por jugador local
 * @return Tipo de evento para la cancha animada
 */
int manejar_gol_local(const Equipo *equipo_local, int minuto_actual, int *goles_local,
                      int goles_jugadores_local[], int asistencias_jugadores_local[])
{
    unsigned int random_value;
    random_value = (unsigned int)rand();
    int jugador_gol = random_value % equipo_local->num_jugadores;
    random_value = (unsigned int)rand();
    int jugador_asistencia = random_value % equipo_local->num_jugadores;

    // Evitar que un jugador se asista a si mismo
    while (jugador_asistencia == jugador_gol && equipo_local->num_jugadores > 1)
    {
        random_value = (unsigned int)rand();
        jugador_asistencia = random_value % equipo_local->num_jugadores;
    }

    (*goles_local)++;
    goles_jugadores_local[jugador_gol]++;

    printf("*** ¡GOOOOL! Minuto %d ***\n", minuto_actual);
    printf("   Gol de %s (%d) para %s\n",
           equipo_local->jugadores[jugador_gol].nombre,
           equipo_local->jugadores[jugador_gol].numero,
           equipo_local->nombre);

    if (jugador_asistencia != jugador_gol)
    {
        asistencias_jugadores_local[jugador_asistencia]++;
        printf("   Asistencia de %s (%d)\n",
               equipo_local->jugadores[jugador_asistencia].nombre,
               equipo_local->jugadores[jugador_asistencia].numero);
    }

    return 1; // tipo_evento para cancha
}

/**
 * @brief Maneja un gol del equipo visitante
 *
 * @param equipo_visitante Puntero al equipo visitante
 * @param minuto_actual Minuto actual del partido
 * @param goles_visitante Puntero a contador de goles visitante
 * @param goles_jugadores_visitante Array de goles por jugador visitante
 * @param asistencias_jugadores_visitante Array de asistencias por jugador visitante
 * @return Tipo de evento para la cancha animada
 */
int manejar_gol_visitante(const Equipo *equipo_visitante, int minuto_actual, int *goles_visitante,
                          int goles_jugadores_visitante[], int asistencias_jugadores_visitante[])
{
    unsigned int random_value;
    random_value = (unsigned int)rand();
    int jugador_gol = random_value % equipo_visitante->num_jugadores;
    random_value = (unsigned int)rand();
    int jugador_asistencia = random_value % equipo_visitante->num_jugadores;

    // Evitar que un jugador se asista a si mismo
    while (jugador_asistencia == jugador_gol && equipo_visitante->num_jugadores > 1)
    {
        random_value = (unsigned int)rand();
        jugador_asistencia = random_value % equipo_visitante->num_jugadores;
    }

    (*goles_visitante)++;
    goles_jugadores_visitante[jugador_gol]++;

    printf("*** ¡GOOOOL! Minuto %d ***\n", minuto_actual);
    printf("   Gol de %s (%d) para %s\n",
           equipo_visitante->jugadores[jugador_gol].nombre,
           equipo_visitante->jugadores[jugador_gol].numero,
           equipo_visitante->nombre);

    if (jugador_asistencia != jugador_gol)
    {
        asistencias_jugadores_visitante[jugador_asistencia]++;
        printf("   Asistencia de %s (%d)\n",
               equipo_visitante->jugadores[jugador_asistencia].nombre,
               equipo_visitante->jugadores[jugador_asistencia].numero);
    }

    return 1; // tipo_evento para cancha
}

/**
 * @brief Maneja una oportunidad de gol
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @param minuto_actual Minuto actual del partido
 * @return Tipo de evento para la cancha animada
 */
int manejar_oportunidad_gol(const Equipo *equipo_local, const Equipo *equipo_visitante, int minuto_actual)
{
    unsigned int random_value;
    random_value = (unsigned int)rand();
    if (random_value % 2 == 0)
    {
        printf("*** Oportunidad de gol para %s (Minuto %d) ***\n", equipo_local->nombre, minuto_actual);
    }
    else
    {
        printf("*** Oportunidad de gol para %s (Minuto %d) ***\n", equipo_visitante->nombre, minuto_actual);
    }
    return 2; // tipo_evento para cancha
}

/**
 * @brief Maneja una falta
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @param minuto_actual Minuto actual del partido
 * @return Tipo de evento para la cancha animada
 */
int manejar_falta(const Equipo *equipo_local, const Equipo *equipo_visitante, int minuto_actual)
{
    unsigned int random_value;
    random_value = (unsigned int)rand();
    if (random_value % 2 == 0)
    {
        printf("*** Falta cometida por %s (Minuto %d) ***\n", equipo_local->nombre, minuto_actual);
    }
    else
    {
        printf("*** Falta cometida por %s (Minuto %d) ***\n", equipo_visitante->nombre, minuto_actual);
    }
    return 3; // tipo_evento para cancha
}

/**
 * @brief Maneja un evento normal del partido
 *
 * @param minuto_actual Minuto actual del partido
 * @return Tipo de evento para la cancha animada
 */
int manejar_evento_normal(int minuto_actual)
{
    printf("*** El partido continua... (Minuto %d) ***\n", minuto_actual);
    return 0; // tipo_evento para cancha
}

/**
 * @brief Procesa un evento aleatorio del partido
 *
 * @param tipo_evento Tipo de evento a procesar
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @param minuto_actual Minuto actual del partido
 * @param stats Puntero a estructura de estadisticas del partido
 * @return Tipo de evento para la cancha animada
 */
int procesar_evento(int tipo_evento, const Equipo *equipo_local, const Equipo *equipo_visitante, int minuto_actual, PartidoStats *stats)
{
    switch (tipo_evento)
    {
    case 1: // gol local
        return manejar_gol_local(equipo_local, minuto_actual, &stats->goles_local,
                                 stats->goles_jugadores_local, stats->asistencias_jugadores_local);
    case 2: // gol visitante
        return manejar_gol_visitante(equipo_visitante, minuto_actual, &stats->goles_visitante,
                                     stats->goles_jugadores_visitante, stats->asistencias_jugadores_visitante);
    case 3: // oportunidad
        return manejar_oportunidad_gol(equipo_local, equipo_visitante, minuto_actual);
    case 4: // falta
        return manejar_falta(equipo_local, equipo_visitante, minuto_actual);
    default: // normal
        return manejar_evento_normal(minuto_actual);
    }
}

/**
 * @brief Simula un minuto del partido
 *
 * @param minuto_actual Minuto actual del partido
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @param stats Puntero a estructura de estadisticas del partido
 */
void simular_minuto_partido(int minuto_actual, const Equipo *equipo_local, const Equipo *equipo_visitante, PartidoStats *stats)
{
    clear_screen();
    print_header("SIMULACION DE PARTIDO");

    printf("=== %s %d - %d %s ===\n\n",
           equipo_local->nombre, stats->goles_local, stats->goles_visitante, equipo_visitante->nombre);

    printf("MINUTO: %d\n\n", minuto_actual);

    // Generar y procesar evento aleatorio
    int tipo_evento = generar_evento_aleatorio();
    int tipo_evento_cancha = procesar_evento(tipo_evento, equipo_local, equipo_visitante, minuto_actual, stats);

    // Mostrar cancha animada con balon en movimiento
    mostrar_cancha_animada(minuto_actual, tipo_evento_cancha);

    // Pausa automatica de 1 segundo para simular el tiempo real
    Sleep(1000);
}

/**
 * @brief Muestra el resultado final del partido
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @param goles_local Goles del equipo local
 * @param goles_visitante Goles del equipo visitante
 */
void mostrar_resultado_final(const Equipo *equipo_local, const Equipo *equipo_visitante,
                             int goles_local, int goles_visitante)
{
    printf("*** RESULTADO FINAL ***\n\n");
    printf("*** 60 MINUTOS COMPLETADOS ***\n\n");

    printf("*** %s %d - %d %s ***\n\n",
           equipo_local->nombre, goles_local, goles_visitante, equipo_visitante->nombre);

    // Determinar resultado
    if (goles_local > goles_visitante)
    {
        printf("*** ¡%s GANA EL PARTIDO! ***\n\n", equipo_local->nombre);
    }
    else if (goles_visitante > goles_local)
    {
        printf("*** ¡%s GANA EL PARTIDO! ***\n\n", equipo_visitante->nombre);
    }
    else
    {
        printf("*** ¡EMPATE! ***\n\n");
    }
}

/**
 * @brief Muestra las estadisticas finales de los jugadores
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 * @param stats Puntero a estructura de estadisticas del partido
 */
void mostrar_estadisticas_jugadores(const Equipo *equipo_local, const Equipo *equipo_visitante, const PartidoStats *stats)
{
    printf("*** ESTADISTICAS DEL PARTIDO ***\n\n");

    printf("EQUIPO LOCAL (%s):\n", equipo_local->nombre);
    int tiene_estadisticas_local = 0;
    for (int i = 0; i < equipo_local->num_jugadores; i++)
    {
        if (stats->goles_jugadores_local[i] > 0 || stats->asistencias_jugadores_local[i] > 0)
        {
            printf("  %s (%d): %d Goles, %d Asistencias\n",
                   equipo_local->jugadores[i].nombre,
                   equipo_local->jugadores[i].numero,
                   stats->goles_jugadores_local[i],
                   stats->asistencias_jugadores_local[i]);
            tiene_estadisticas_local = 1;
        }
    }
    if (!tiene_estadisticas_local)
    {
        printf("  Sin goles ni asistencias\n");
    }

    printf("\nEQUIPO VISITANTE (%s):\n", equipo_visitante->nombre);
    int tiene_estadisticas_visitante = 0;
    for (int i = 0; i < equipo_visitante->num_jugadores; i++)
    {
        if (stats->goles_jugadores_visitante[i] > 0 || stats->asistencias_jugadores_visitante[i] > 0)
        {
            printf("  %s (%d): %d Goles, %d Asistencias\n",
                   equipo_visitante->jugadores[i].nombre,
                   equipo_visitante->jugadores[i].numero,
                   stats->goles_jugadores_visitante[i],
                   stats->asistencias_jugadores_visitante[i]);
            tiene_estadisticas_visitante = 1;
        }
    }
    if (!tiene_estadisticas_visitante)
    {
        printf("  Sin goles ni asistencias\n");
    }
}

/**
 * @brief Simula un partido entre dos equipos en ASCII art
 *
 * Esta funcion simula un partido de futbol de 60 minutos entre dos equipos momentaneos.
 * Muestra la cancha en ASCII, los jugadores de ambos equipos, genera eventos aleatorios
 * como goles y asistencias, y muestra el marcador en tiempo real.
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 */
void simular_partido(const Equipo *equipo_local, const Equipo *equipo_visitante)
{
    clear_screen();
    printf("%s\n", ASCII_SIMULACION);
    printf("                    SIMULACION DE PARTIDO\n\n");

    // Inicializar estadisticas
    PartidoStats stats;
    inicializar_estadisticas_partido(&stats);

    // Inicializar generador de numeros aleatorios (una vez)
    static int _seeded = 0;
    if (!_seeded)
    {
        srand((unsigned)time(NULL));
        _seeded = 1;
    }

    // Mostrar informacion inicial
    mostrar_informacion_inicial(equipo_local, equipo_visitante);

    // Simulacion de 60 minutos
    for (int minuto_actual = 1; minuto_actual <= 60; minuto_actual++)
    {
        simular_minuto_partido(minuto_actual, equipo_local, equipo_visitante, &stats);
    }

    // Fin del partido
    clear_screen();
    print_header("FIN DEL PARTIDO");

    mostrar_resultado_final(equipo_local, equipo_visitante, stats.goles_local, stats.goles_visitante);
    mostrar_estadisticas_jugadores(equipo_local, equipo_visitante, &stats);

    printf("\nPresione Enter para volver al menu...");
    getchar();
}

/**
 * @brief Muestra el menu principal de gestion de equipos
 *
 * Presenta un menu interactivo con opciones para crear, listar, modificar
 * y eliminar equipos. Utiliza la funcion ejecutar_menu para manejar
 * la navegacion del menu y delega las operaciones a las funciones correspondientes.
 * Este es el punto de entrada principal para todas las operaciones relacionadas
 * con equipos en el sistema MiFutbolC.
 */
void menu_equipos()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_equipo},
        {2, "Listar", listar_equipos},
        {3, "Modificar", modificar_equipo},
        {4, "Eliminar", eliminar_equipo},
        {5, "Cargar Imagen", cargar_imagen_equipo},
        {6, "Ver Imagen", ver_imagen_equipo},
        {0, "Volver", NULL}
    };

    ejecutar_menu("EQUIPOS", items, 7);
}
