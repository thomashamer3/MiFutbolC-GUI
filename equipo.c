/**
* @file equipo.c
* @brief Implementacion de funciones para la gestion de equipos en MiFutbolC
*/

#include "equipo.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "partido.h"
#include "raylib.h"
#include "gui_components.h"
#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

static void equipo_sleep_ms(unsigned int ms)
{
    if (ms == 0U)
    {
        return;
    }

    WaitTime((double)ms / 1000.0);
}


static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, 0) == SQLITE_OK;
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

// Prototipo para obtener maximo de jugadores segun tipo de futbol
int get_num_jugadores_por_tipo(TipoFutbol tipo_futbol);

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

void agregar_nuevo_jugador(int equipo_id,
                           int jugador_count,
                           const int *jugadores_numeros,
                           const int *jugadores_posiciones,
                           int max_jugadores);
void eliminar_jugador_existente(const int *jugadores_ids, const int *jugadores_numeros, int jugador_count);

static int obtener_tipo_futbol_equipo_por_id(int equipo_id, TipoFutbol *tipo_out);
static int sincronizar_num_jugadores_equipo(int equipo_id);
static void asegurar_capitan_en_equipo(int equipo_id);
static int actualizar_tipo_futbol_equipo(int equipo_id, TipoFutbol tipo_futbol);
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

static int obtener_tipo_futbol_equipo_por_id(int equipo_id, TipoFutbol *tipo_out)
{
    sqlite3_stmt *stmt = NULL;
    int ok = 0;

    if (!tipo_out)
    {
        return 0;
    }

    if (!preparar_stmt(&stmt, "SELECT tipo_futbol FROM equipo WHERE id = ?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, equipo_id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        *tipo_out = (TipoFutbol)sqlite3_column_int(stmt, 0);
        ok = 1;
    }

    sqlite3_finalize(stmt);
    return ok;
}

static int sincronizar_num_jugadores_equipo(int equipo_id)
{
    sqlite3_stmt *stmt = NULL;
    int cantidad = -1;

    if (!preparar_stmt(&stmt, "SELECT COUNT(*) FROM jugador WHERE equipo_id = ?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, equipo_id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        cantidad = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    if (cantidad < 0)
    {
        return 0;
    }

    return ejecutar_update_int("UPDATE equipo SET num_jugadores = ? WHERE id = ?", cantidad, equipo_id);
}

static void asegurar_capitan_en_equipo(int equipo_id)
{
    sqlite3_stmt *stmt = NULL;
    int capitanes = 0;
    int primer_jugador_id = 0;

    if (preparar_stmt(&stmt, "SELECT COUNT(*) FROM jugador WHERE equipo_id = ? AND es_capitan = 1"))
    {
        sqlite3_bind_int(stmt, 1, equipo_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            capitanes = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (capitanes > 0)
    {
        return;
    }

    if (preparar_stmt(&stmt, "SELECT id FROM jugador WHERE equipo_id = ? ORDER BY numero LIMIT 1"))
    {
        sqlite3_bind_int(stmt, 1, equipo_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            primer_jugador_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (primer_jugador_id > 0)
    {
        (void)ejecutar_update_id("UPDATE jugador SET es_capitan = 1 WHERE id = ?", primer_jugador_id);
    }
}

static int actualizar_tipo_futbol_equipo(int equipo_id, TipoFutbol tipo_futbol)
{
    if (!ejecutar_update_int("UPDATE equipo SET tipo_futbol = ? WHERE id = ?", tipo_futbol, equipo_id))
    {
        return 0;
    }

    (void)sincronizar_num_jugadores_equipo(equipo_id);
    return 1;
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
    printf("3. Futbol 11\n");

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
        tipo_futbol = FUTBOL_11;
        break;
    default:
        printf("Opcion invalida.\n");
        pause_console();
        return;
    }

    if (actualizar_tipo_futbol_equipo(equipo_id, tipo_futbol))
    {
        printf("Tipo de futbol actualizado exitosamente.\n");
        printf("Ahora puede ajustar jugadores segun el maximo del tipo elegido.\n");
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

static int ejecutar_opcion_modificar_jugadores(int opcion_jugador,
                                               int equipo_id,
                                               int max_jugadores,
                                               EquipoPlayerInfo *info)
{
    switch (opcion_jugador)
    {
    case 1:
        if (!info || info->jugador_count <= 0)
        {
            printf("No hay jugadores para modificar.\n");
            pause_console();
            return 1;
        }

        modificar_jugador_existente(info->jugadores_ids,
                                    info->jugadores_nombres,
                                    info->jugadores_numeros,
                                    info->jugadores_posiciones,
                                    info->jugadores_capitanes,
                                    info->jugador_count);
        return 1;

    case 2:
        agregar_nuevo_jugador(equipo_id,
                              info ? info->jugador_count : 0,
                              info ? info->jugadores_numeros : NULL,
                              info ? info->jugadores_posiciones : NULL,
                              max_jugadores);
        (void)sincronizar_num_jugadores_equipo(equipo_id);
        return 1;

    case 3:
        if (!info || info->jugador_count <= 1)
        {
            printf("No se puede eliminar: minimo 1 jugador por equipo.\n");
            pause_console();
            return 1;
        }

        eliminar_jugador_existente(info->jugadores_ids,
                                   info->jugadores_numeros,
                                   info->jugador_count);
        asegurar_capitan_en_equipo(equipo_id);
        (void)sincronizar_num_jugadores_equipo(equipo_id);
        return 1;

    case 4:
        if (!info || info->jugador_count <= 0)
        {
            printf("No hay jugadores para cambiar capitan.\n");
            pause_console();
            return 1;
        }

        cambiar_capitan_equipo(equipo_id, info);
        return 1;

    case 5:
        (void)sincronizar_num_jugadores_equipo(equipo_id);
        return 0;

    default:
        printf("Opcion invalida.\n");
        pause_console();
        return 1;
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
    while (1)
    {
        sqlite3_stmt *stmt_jugadores = NULL;
        const char *sql_jugadores = "SELECT id, nombre, numero, posicion, es_capitan FROM jugador WHERE equipo_id = ? ORDER BY numero;";
        TipoFutbol tipo_futbol = FUTBOL_5;
        int max_jugadores;
        int jugador_count = 0;
        int jugadores_ids[11] = {0};
        char jugadores_nombres[11][50] = {{0}};
        int jugadores_numeros[11] = {0};
        int jugadores_posiciones[11] = {0};
        int jugadores_capitanes[11] = {0};

        if (!obtener_tipo_futbol_equipo_por_id(equipo_id, &tipo_futbol))
        {
            printf("No se pudo obtener el tipo de futbol del equipo.\n");
            pause_console();
            return;
        }

        max_jugadores = get_num_jugadores_por_tipo(tipo_futbol);
        if (max_jugadores < 1 || max_jugadores > 11)
        {
            max_jugadores = 11;
        }

        if (!preparar_stmt(&stmt_jugadores, sql_jugadores))
        {
            printf("Error al cargar jugadores: %s\n", sqlite3_errmsg(db));
            pause_console();
            return;
        }

        sqlite3_bind_int(stmt_jugadores, 1, equipo_id);

        printf("\n=== MODIFICAR JUGADORES ===\n");
        printf("Tipo actual: %s\n", get_nombre_tipo_futbol(tipo_futbol));
        printf("Jugadores actuales:\n");

        while (sqlite3_step(stmt_jugadores) == SQLITE_ROW)
        {
            if (jugador_count >= 11)
            {
                continue;
            }

            jugadores_ids[jugador_count] = sqlite3_column_int(stmt_jugadores, 0);
            snprintf(jugadores_nombres[jugador_count],
                     sizeof(jugadores_nombres[jugador_count]),
                     "%s",
                     (const char *)sqlite3_column_text(stmt_jugadores, 1));
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

        printf("Total: %d/%d jugadores\n", jugador_count, max_jugadores);

        // Mostrar opciones de modificacion
        printf("\nSeleccione que desea hacer:\n");
        printf("1. Modificar un jugador existente\n");
        printf("2. Agregar un nuevo jugador\n");
        printf("3. Eliminar un jugador\n");
        printf("4. Cambiar capitan\n");
        printf("5. Volver\n");

        EquipoPlayerInfo info =
        {
            .jugador_count = jugador_count
        };
        memcpy(info.jugadores_ids, jugadores_ids, sizeof(int) * jugador_count);
        memcpy(info.jugadores_nombres, jugadores_nombres, sizeof(jugadores_nombres));
        memcpy(info.jugadores_numeros, jugadores_numeros, sizeof(int) * jugador_count);
        memcpy(info.jugadores_posiciones, jugadores_posiciones, sizeof(int) * jugador_count);
        memcpy(info.jugadores_capitanes, jugadores_capitanes, sizeof(int) * jugador_count);

        int opcion_jugador = input_int(">");

        if (!ejecutar_opcion_modificar_jugadores(opcion_jugador,
                                                 equipo_id,
                                                 max_jugadores,
                                                 &info))
        {
            return;
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
void agregar_nuevo_jugador(int equipo_id,
                           int jugador_count,
                           const int *jugadores_numeros,
                           const int *jugadores_posiciones,
                           int max_jugadores)
{
    if (max_jugadores <= 0 || max_jugadores > 11)
    {
        max_jugadores = 11;
    }

    if (jugador_count >= max_jugadores)
    {
        printf("El equipo ya tiene el maximo de jugadores para este tipo (%d).\n", max_jugadores);
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
static void mostrar_equipo(const Equipo *equipo)
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

typedef struct
{
    int id;
    char nombre[128];
    int tipo;
    int tipo_futbol;
    int num_jugadores;
    int partido_id;
} EquipoGuiRow;

typedef struct
{
    char nombre[64];
    int numero;
    int posicion;
    int es_capitan;
} EquipoGuiJugadorRow;

typedef struct
{
    int x;
    int y;
    int w;
    int h;
    int visible_rows;
} EquipoGuiPanel;

enum
{
    EQUIPO_GUI_EDIT_CANCELAR = 0,
    EQUIPO_GUI_EDIT_NOMBRE = 1,
    EQUIPO_GUI_EDIT_TIPO_FUTBOL = 2,
    EQUIPO_GUI_EDIT_ASIGNACION = 3,
    EQUIPO_GUI_EDIT_JUGADORES = 4
};

static int actualizar_scroll_equipos_gui(int scroll, int count, int visible_rows, Rectangle area);

static int ampliar_capacidad_equipos_gui(EquipoGuiRow **rows, int *cap)
{
    int new_cap = (*cap) * 2;
    EquipoGuiRow *tmp = (EquipoGuiRow *)realloc(*rows, (size_t)new_cap * sizeof(EquipoGuiRow));
    if (!tmp)
    {
        return 0;
    }

    memset(tmp + (*cap), 0, (size_t)(new_cap - (*cap)) * sizeof(EquipoGuiRow));
    *rows = tmp;
    *cap = new_cap;
    return 1;
}

static int cargar_equipos_gui_rows(EquipoGuiRow **rows_out, int *count_out)
{
    sqlite3_stmt *stmt = NULL;
    int cap = 32;
    int count = 0;
    EquipoGuiRow *rows;

    if (!rows_out || !count_out)
    {
        return 0;
    }

    rows = (EquipoGuiRow *)calloc((size_t)cap, sizeof(EquipoGuiRow));
    if (!rows)
    {
        return 0;
    }

    if (!preparar_stmt(&stmt,
                       "SELECT id, nombre, tipo, tipo_futbol, num_jugadores, partido_id "
                       "FROM equipo ORDER BY id"))
    {
        free(rows);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (count >= cap && !ampliar_capacidad_equipos_gui(&rows, &cap))
        {
            sqlite3_finalize(stmt);
            free(rows);
            return 0;
        }

        rows[count].id = sqlite3_column_int(stmt, 0);
        snprintf(rows[count].nombre,
                 sizeof(rows[count].nombre),
                 "%s",
                 (const char *)(sqlite3_column_text(stmt, 1) ? sqlite3_column_text(stmt, 1) : (const unsigned char *)"(sin nombre)"));
        rows[count].tipo = sqlite3_column_int(stmt, 2);
        rows[count].tipo_futbol = sqlite3_column_int(stmt, 3);
        rows[count].num_jugadores = sqlite3_column_int(stmt, 4);
        rows[count].partido_id = sqlite3_column_int(stmt, 5);
        count++;
    }

    sqlite3_finalize(stmt);
    *rows_out = rows;
    *count_out = count;
    return 1;
}

static int ampliar_capacidad_jugadores_gui(EquipoGuiJugadorRow **rows, int *cap)
{
    int new_cap = (*cap) * 2;
    EquipoGuiJugadorRow *tmp = (EquipoGuiJugadorRow *)realloc(*rows, (size_t)new_cap * sizeof(EquipoGuiJugadorRow));
    if (!tmp)
    {
        return 0;
    }

    memset(tmp + (*cap), 0, (size_t)(new_cap - (*cap)) * sizeof(EquipoGuiJugadorRow));
    *rows = tmp;
    *cap = new_cap;
    return 1;
}

static int cargar_jugadores_equipo_gui_rows(int equipo_id, EquipoGuiJugadorRow **rows_out, int *count_out)
{
    sqlite3_stmt *stmt = NULL;
    int cap = 16;
    int count = 0;
    EquipoGuiJugadorRow *rows;

    if (!rows_out || !count_out)
    {
        return 0;
    }

    rows = (EquipoGuiJugadorRow *)calloc((size_t)cap, sizeof(EquipoGuiJugadorRow));
    if (!rows)
    {
        return 0;
    }

    if (!preparar_stmt(&stmt,
                       "SELECT nombre, numero, posicion, es_capitan "
                       "FROM jugador WHERE equipo_id = ? ORDER BY numero"))
    {
        free(rows);
        return 0;
    }

    sqlite3_bind_int(stmt, 1, equipo_id);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (count >= cap && !ampliar_capacidad_jugadores_gui(&rows, &cap))
        {
            sqlite3_finalize(stmt);
            free(rows);
            return 0;
        }

        snprintf(rows[count].nombre,
                 sizeof(rows[count].nombre),
                 "%s",
                 (const char *)(sqlite3_column_text(stmt, 0) ? sqlite3_column_text(stmt, 0) : (const unsigned char *)"(sin nombre)"));
        rows[count].numero = sqlite3_column_int(stmt, 1);
        rows[count].posicion = sqlite3_column_int(stmt, 2);
        rows[count].es_capitan = sqlite3_column_int(stmt, 3);
        count++;
    }

    sqlite3_finalize(stmt);
    *rows_out = rows;
    *count_out = count;
    return 1;
}

static int equipo_gui_visible_rows(int list_h, int row_h)
{
    int visible_rows = list_h / row_h;
    if (visible_rows < 1)
    {
        return 1;
    }
    return visible_rows;
}

static void equipo_gui_draw_resumen_detalle(const EquipoGuiRow *equipo,
                                            int panel_x,
                                            int panel_y,
                                            int panel_w)
{
    gui_text_truncated(TextFormat("ID: %d | Nombre: %s", equipo->id, equipo->nombre),
                       (float)panel_x + 24.0f,
                       (float)panel_y + 28.0f,
                       24.0f,
                       (float)panel_w - 48.0f,
                       gui_get_active_theme()->text_primary);
    gui_text_truncated(TextFormat("Tipo: %s | Futbol: %s | Jugadores: %d | Asignado a partido: %s",
                                  equipo->tipo == FIJO ? "Fijo" : "Momentaneo",
                                  get_nombre_tipo_futbol((TipoFutbol)equipo->tipo_futbol),
                                  equipo->num_jugadores,
                                  equipo->partido_id == -1 ? "No" : "Si"),
                       (float)panel_x + 24.0f,
                       (float)panel_y + 68.0f,
                       18.0f,
                       (float)panel_w - 48.0f,
                       gui_get_active_theme()->text_secondary);
    gui_text("Jugadores", (float)panel_x + 24.0f, (float)panel_y + 112.0f, 20.0f, gui_get_active_theme()->text_primary);
}

static void equipo_gui_draw_jugadores_detalle_rows(const EquipoGuiJugadorRow *jugadores,
                                                   int jugadores_count,
                                                   int scroll,
                                                   int row_h,
                                                   Rectangle list_area)
{
    int list_x = (int)list_area.x;
    int list_y = (int)list_area.y;
    int list_w = (int)list_area.width;
    int list_h = (int)list_area.height;

    BeginScissorMode(list_x, list_y, list_w, list_h);
    for (int i = scroll; i < jugadores_count; i++)
    {
        int row = i - scroll;
        int y = list_y + row * row_h;
        Rectangle fila;

        if (y + row_h > list_y + list_h)
        {
            break;
        }

        fila = (Rectangle){(float)(list_x + 6), (float)y, (float)(list_w - 12), (float)(row_h - 2)};
        gui_draw_list_row_bg(fila, row, CheckCollisionPointRec(GetMousePosition(), fila));

        gui_text(TextFormat("%3d", jugadores[i].numero),
                 (float)(list_x + 12),
                 (float)(y + 6),
                 18.0f,
                 (Color){220, 238, 225, 255});

        gui_text_truncated(TextFormat("%s | %s%s",
                                      jugadores[i].nombre,
                                      get_nombre_posicion((Posicion)jugadores[i].posicion),
                                      jugadores[i].es_capitan ? " [CAPITAN]" : ""),
                           (float)(list_x + 80),
                           (float)(y + 6),
                           18.0f,
                           (float)list_w - 100.0f,
                           (Color){233, 247, 236, 255});
    }
    EndScissorMode();
}

static void equipo_gui_draw_jugadores_detalle(const EquipoGuiJugadorRow *jugadores,
                                              int jugadores_count,
                                              int jugadores_cargados,
                                              int scroll,
                                              int row_h,
                                              Rectangle list_area)
{
    if (!jugadores_cargados)
    {
        gui_text("No se pudieron cargar los jugadores de este equipo.",
                 list_area.x + 12.0f,
                 list_area.y + 8.0f,
                 18.0f,
                 gui_get_active_theme()->text_secondary);
        return;
    }

    if (jugadores_count == 0)
    {
        gui_text("Este equipo no tiene jugadores registrados.",
                 list_area.x + 12.0f,
                 list_area.y + 8.0f,
                 18.0f,
                 gui_get_active_theme()->text_secondary);
        return;
    }

    equipo_gui_draw_jugadores_detalle_rows(jugadores,
                                           jugadores_count,
                                           scroll,
                                           row_h,
                                           list_area);
}

static const EquipoGuiRow *equipo_gui_buscar_row_por_id(const EquipoGuiRow *rows, int count, int id)
{
    for (int i = 0; i < count; i++)
    {
        if (rows[i].id == id)
        {
            return &rows[i];
        }
    }

    return NULL;
}

static void modal_detalle_equipo_gui(const EquipoGuiRow *equipo)
{
    EquipoGuiJugadorRow *jugadores = NULL;
    int jugadores_count = 0;
    int scroll = 0;
    int jugadores_cargados = 0;
    const int row_h = 30;

    if (!equipo)
    {
        return;
    }

    jugadores_cargados = cargar_jugadores_equipo_gui_rows(equipo->id, &jugadores, &jugadores_count);

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = (sw > 980) ? 60 : 20;
        int panel_y = 110;
        int panel_w = sw - (panel_x * 2);
        int panel_h = sh - 180;
        int list_x = panel_x + 24;
        int list_y = panel_y + 210;
        int list_w = panel_w - 48;
        int list_h = panel_h - 246;
        int visible_rows;
        Rectangle list_area;

        if (list_h < row_h)
        {
            list_h = row_h;
        }

        visible_rows = equipo_gui_visible_rows(list_h, row_h);
        list_area = (Rectangle){(float)list_x, (float)list_y, (float)list_w, (float)list_h};
        scroll = actualizar_scroll_equipos_gui(scroll,
                                               jugadores_count,
                                               visible_rows,
                                               list_area);

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header("DETALLE DEL EQUIPO", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);
        equipo_gui_draw_resumen_detalle(equipo, panel_x, panel_y, panel_w);
        gui_draw_list_shell((Rectangle){(float)list_x, (float)(list_y - 30), (float)list_w, (float)(list_h + 30)},
                            "N", 12.0f,
                            "NOMBRE / POSICION", 80.0f);
        equipo_gui_draw_jugadores_detalle(jugadores,
                                          jugadores_count,
                                          jugadores_cargados,
                                          scroll,
                                          row_h,
                                          list_area);

        gui_draw_footer_hint("Rueda: scroll jugadores | ESC/Enter: volver", (float)panel_x, sh);
        EndDrawing();

        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            break;
        }
    }

    free(jugadores);
}

static int clamp_scroll_equipos_gui(int scroll, int count, int visible_rows)
{
    int max_scroll = count - visible_rows;
    if (max_scroll < 0)
    {
        max_scroll = 0;
    }

    if (scroll < 0)
    {
        return 0;
    }

    if (scroll > max_scroll)
    {
        return max_scroll;
    }

    return scroll;
}

static int actualizar_scroll_equipos_gui(int scroll, int count, int visible_rows, Rectangle area)
{
    float wheel = GetMouseWheelMove();
    if (CheckCollisionPointRec(GetMousePosition(), area))
    {
        if (wheel < 0.0f && scroll < count - visible_rows)
        {
            scroll++;
        }
        else if (wheel > 0.0f && scroll > 0)
        {
            scroll--;
        }
    }

    return clamp_scroll_equipos_gui(scroll, count, visible_rows);
}

static void calcular_panel_listado_equipos_gui(int sw, int sh, int row_h, EquipoGuiPanel *panel)
{
    panel->x = (sw > 900) ? 80 : 20;
    panel->y = 130;
    panel->w = sw - (panel->x * 2);
    panel->h = sh - 220;
    if (panel->h < 200)
    {
        panel->h = 200;
    }

    panel->visible_rows = panel->h / row_h;
    if (panel->visible_rows < 1)
    {
        panel->visible_rows = 1;
    }
}

static void equipo_gui_append_printable_ascii(char *buffer, int *cursor, size_t buffer_size, int only_digits)
{
    int key = GetCharPressed();
    while (key > 0)
    {
        if (key >= 32 && key <= 126 && *cursor < (int)buffer_size - 1)
        {
            if (!only_digits || isdigit((unsigned char)key))
            {
                buffer[*cursor] = (char)key;
                (*cursor)++;
                buffer[*cursor] = '\0';
            }
        }
        key = GetCharPressed();
    }
}

static void equipo_gui_handle_backspace(char *buffer, int *cursor)
{
    if (!IsKeyPressed(KEY_BACKSPACE))
    {
        return;
    }

    {
        size_t len = strlen(buffer);
        if (len > 0)
        {
            buffer[len - 1] = '\0';
            *cursor = (int)len - 1;
        }
    }
}

static void equipo_gui_draw_input_caret(const char *text, Rectangle input_rect)
{
    int blink_on = (((int)(GetTime() * 2.0)) % 2) == 0;
    if (!blink_on)
    {
        return;
    }

    Vector2 text_size = gui_text_measure(text ? text : "", 18.0f);
    float caret_x = input_rect.x + 8.0f + text_size.x + 1.0f;
    float caret_right_limit = input_rect.x + input_rect.width - 8.0f;
    caret_x = (caret_x > caret_right_limit) ? caret_right_limit : caret_x;

    DrawLineEx((Vector2){caret_x, input_rect.y + 7.0f},
               (Vector2){caret_x, input_rect.y + input_rect.height - 7.0f},
               2.0f,
               (Color){241, 252, 244, 255});
}

static void equipo_gui_draw_action_toast(int sw, int sh, const char *msg, int success)
{
    int toast_w = 560;
    int toast_h = 44;
    int toast_x = (sw - toast_w) / 2;
    int toast_y = sh - 120;
    Color bg = success ? (Color){28, 94, 52, 240} : (Color){120, 45, 38, 240};

    DrawRectangle(toast_x, toast_y, toast_w, toast_h, bg);
    DrawRectangleLines(toast_x, toast_y, toast_w, toast_h, (Color){198, 230, 205, 255});
    gui_text(msg ? msg : "Operacion completada",
             (float)toast_x + 14.0f,
             (float)toast_y + 12.0f,
             18.0f,
             (Color){241, 252, 244, 255});
}

static int equipo_gui_tick_toast(float *timer)
{
    if (!timer || *timer <= 0.0f)
    {
        return 0;
    }

    *timer -= GetFrameTime();
    return *timer <= 0.0f;
}

static int equipo_gui_draw_action_button(Rectangle rect, const char *label, int primary)
{
    const GuiTheme *theme = gui_get_active_theme();
    int hovered = CheckCollisionPointRec(GetMousePosition(), rect);
    Color fill = primary ? theme->accent_primary : theme->bg_sidebar;
    Color border = primary ? theme->accent_primary_hv : theme->border;
    Color text = primary ? (Color){255, 255, 255, 255} : theme->text_primary;

    if (hovered)
    {
        fill = primary ? theme->accent_primary_hv : theme->row_hover;
    }

    DrawRectangleRec(rect, fill);
    DrawRectangleLinesEx(rect, 1.0f, border);

    {
        Vector2 m = gui_text_measure(label ? label : "", 18.0f);
        float tx = rect.x + (rect.width - m.x) * 0.5f;
        float ty = rect.y + (rect.height - m.y) * 0.5f;
        gui_text(label ? label : "", tx, ty, 18.0f, text);
    }

    return hovered;
}

static void equipo_gui_show_action_feedback(const char *title, const char *msg, int success)
{
    float timer = 1.15f;
    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header(title ? title : "EQUIPOS", sw);
        equipo_gui_draw_action_toast(sw, sh, msg, success);
        gui_draw_footer_hint("Continuando...", 40.0f, sh);
        EndDrawing();

        if (equipo_gui_tick_toast(&timer))
        {
            return;
        }
    }
}

static int dibujar_y_detectar_click_equipos_gui(const EquipoGuiRow *rows,
                                                 int count,
                                                 int scroll,
                                                 int row_h,
                                                 const EquipoGuiPanel *panel)
{
    if (count == 0)
    {
        gui_text("No hay equipos registrados.",
                 (float)(panel->x + 24),
                 (float)(panel->y + 24),
                 24.0f,
                 (Color){233, 247, 236, 255});
        return 0;
    }

    BeginScissorMode(panel->x, panel->y, panel->w, panel->h);
    for (int i = scroll; i < count; i++)
    {
        int row = i - scroll;
        int y = panel->y + row * row_h;
        Rectangle fila;
        char detalle[256];
        const char *tipo = (rows[i].tipo == FIJO) ? "Fijo" : "Momentaneo";
        const char *tipo_fut = get_nombre_tipo_futbol((TipoFutbol)rows[i].tipo_futbol);
        const char *asignado = (rows[i].partido_id == -1) ? "No" : "Si";

        if (y + row_h > panel->y + panel->h)
        {
            break;
        }

        fila = (Rectangle){(float)(panel->x + 6), (float)y, (float)(panel->w - 12), (float)(row_h - 2)};
        gui_draw_list_row_bg(fila, row, CheckCollisionPointRec(GetMousePosition(), fila));
        if (CheckCollisionPointRec(GetMousePosition(), fila) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            EndScissorMode();
            return rows[i].id;
        }

        snprintf(detalle,
                 sizeof(detalle),
                 "%s | %s | %s | Jug: %d | Partido: %s",
                 rows[i].nombre,
                 tipo,
                 tipo_fut ? tipo_fut : "N/A",
                 rows[i].num_jugadores,
                 asignado);

        gui_text(TextFormat("%3d", rows[i].id),
                 (float)(panel->x + 12),
                 (float)(y + 7),
                 18.0f,
                 (Color){220, 238, 225, 255});
        gui_text_truncated(detalle,
                           (float)(panel->x + 80),
                           (float)(y + 7),
                           18.0f,
                           (float)panel->w - 100.0f,
                           (Color){233, 247, 236, 255});
    }
    EndScissorMode();

    return 0;
}

static int seleccionar_equipo_gui(const char *titulo, const char *ayuda, int *id_out)
{
    EquipoGuiRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 34;
    EquipoGuiPanel panel = {80, 130, 1100, 520, 1};

    if (!id_out)
    {
        return 0;
    }
    *id_out = 0;

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    if (!cargar_equipos_gui_rows(&rows, &count))
    {
        return 0;
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int clicked_id;
        Rectangle area;
        EquipoGuiPanel content = {0};

        calcular_panel_listado_equipos_gui(sw, sh, row_h, &panel);
        area = (Rectangle){(float)panel.x, (float)(panel.y + 32), (float)panel.w, (float)(panel.h - 32)};
        scroll = actualizar_scroll_equipos_gui(scroll,
                                               count,
                                               panel.visible_rows,
                                               area);

        content.x = panel.x;
        content.y = panel.y + 32;
        content.w = panel.w;
        content.h = panel.h - 32;
        content.visible_rows = panel.visible_rows;

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header(titulo ? titulo : "EQUIPOS", sw);
        gui_text(ayuda ? ayuda : "Selecciona un equipo", (float)panel.x, 92.0f, 18.0f,
                 gui_get_active_theme()->text_secondary);
        gui_draw_list_shell((Rectangle){(float)panel.x, (float)panel.y, (float)panel.w, (float)panel.h},
                            "ID", 12.0f,
                            "NOMBRE / DATOS", 80.0f);

        clicked_id = dibujar_y_detectar_click_equipos_gui(rows, count, scroll, row_h, &content);
        gui_draw_footer_hint("Click: seleccionar | Rueda: scroll | ESC/Enter: volver",
                             (float)panel.x, sh);
        EndDrawing();

        if (clicked_id > 0)
        {
            *id_out = clicked_id;
            free(rows);
            return 1;
        }

        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            break;
        }
    }

    free(rows);
    return 0;
}

static int listar_equipos_gui(void)
{
    EquipoGuiRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 34;

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    if (!cargar_equipos_gui_rows(&rows, &count))
    {
        return 0;
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int clicked_id;
        Rectangle area;
        EquipoGuiPanel panel = {0};
        EquipoGuiPanel content = {0};

        calcular_panel_listado_equipos_gui(sw, sh, row_h, &panel);
        area = (Rectangle){(float)panel.x, (float)(panel.y + 32), (float)panel.w, (float)(panel.h - 32)};
        scroll = actualizar_scroll_equipos_gui(scroll,
                                               count,
                                               panel.visible_rows,
                                               area);

        content.x = panel.x;
        content.y = panel.y + 32;
        content.w = panel.w;
        content.h = panel.h - 32;
        content.visible_rows = panel.visible_rows;

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header("LISTADO DE EQUIPOS", sw);
        gui_draw_list_shell((Rectangle){(float)panel.x, (float)panel.y, (float)panel.w, (float)panel.h},
                            "ID", 12.0f,
                            "NOMBRE / DATOS", 80.0f);
        clicked_id = dibujar_y_detectar_click_equipos_gui(rows, count, scroll, row_h, &content);
        gui_draw_footer_hint("Click: ver detalle y jugadores | Rueda: scroll | ESC/Enter: volver", (float)panel.x, sh);
        EndDrawing();

        if (clicked_id > 0)
        {
            const EquipoGuiRow *equipo = equipo_gui_buscar_row_por_id(rows, count, clicked_id);
            if (equipo)
            {
                modal_detalle_equipo_gui(equipo);
            }
            continue;
        }

        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            break;
        }
    }

    free(rows);
    return 1;
}

static int modal_asignacion_equipo_gui(int equipo_id);

static int equipo_gui_numero_repetido_creacion(const Jugador *jugadores, int count, int numero)
{
    for (int i = 0; i < count; i++)
    {
        if (jugadores[i].numero == numero)
        {
            return 1;
        }
    }
    return 0;
}

static int equipo_gui_hay_arquero_creacion(const Jugador *jugadores, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (jugadores[i].posicion == ARQUERO)
        {
            return 1;
        }
    }
    return 0;
}

static const char *equipo_gui_nombre_tipo_equipo(TipoEquipo tipo)
{
    return (tipo == FIJO) ? "FIJO" : "MOMENTANEO";
}

static int modal_tipo_futbol_creacion_equipo_gui(TipoEquipo tipo, TipoFutbol *tipo_out) /* NOSONAR */
{
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ONE);
    input_consume_key(KEY_FIVE);
    input_consume_key(KEY_SEVEN);
    input_consume_key(KEY_EIGHT);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 760 : sw - 40;
        int panel_h = 250;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle b1 = {(float)panel_x + 24.0f, (float)panel_y + 126.0f, 160.0f, 42.0f};
        Rectangle b2 = {(float)panel_x + 196.0f, (float)panel_y + 126.0f, 160.0f, 42.0f};
        Rectangle b3 = {(float)panel_x + 368.0f, (float)panel_y + 126.0f, 160.0f, 42.0f};
        Rectangle b4 = {(float)panel_x + 540.0f, (float)panel_y + 126.0f, 160.0f, 42.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header("CREAR EQUIPO", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text(TextFormat("Tipo de equipo: %s", equipo_gui_nombre_tipo_equipo(tipo)),
                 (float)panel_x + 24.0f,
                 (float)panel_y + 42.0f,
                 22.0f,
                 gui_get_active_theme()->text_primary);
        gui_text("Selecciona el tipo de futbol", (float)panel_x + 24.0f,
                 (float)panel_y + 84.0f,
                 18.0f,
                 gui_get_active_theme()->text_secondary);

        equipo_gui_draw_action_button(b1, "Futbol 5", 1);
        equipo_gui_draw_action_button(b2, "Futbol 7", 0);
        equipo_gui_draw_action_button(b3, "Futbol 8", 0);
        equipo_gui_draw_action_button(b4, "Futbol 11", 0);
        gui_draw_footer_hint("Click o teclas 5/7/8/1 | ESC: volver", (float)panel_x, sh);
        EndDrawing();

        if (click && CheckCollisionPointRec(GetMousePosition(), b1))
        {
            if (tipo_out)
            {
                *tipo_out = FUTBOL_5;
            }
            return 1;
        }
        if (click && CheckCollisionPointRec(GetMousePosition(), b2))
        {
            if (tipo_out)
            {
                *tipo_out = FUTBOL_7;
            }
            return 1;
        }
        if (click && CheckCollisionPointRec(GetMousePosition(), b3))
        {
            if (tipo_out)
            {
                *tipo_out = FUTBOL_8;
            }
            return 1;
        }
        if (click && CheckCollisionPointRec(GetMousePosition(), b4))
        {
            if (tipo_out)
            {
                *tipo_out = FUTBOL_11;
            }
            return 1;
        }

        if (IsKeyPressed(KEY_FIVE) || IsKeyPressed(KEY_KP_5))
        {
            if (tipo_out)
            {
                *tipo_out = FUTBOL_5;
            }
            return 1;
        }
        if (IsKeyPressed(KEY_SEVEN) || IsKeyPressed(KEY_KP_7))
        {
            if (tipo_out)
            {
                *tipo_out = FUTBOL_7;
            }
            return 1;
        }
        if (IsKeyPressed(KEY_EIGHT) || IsKeyPressed(KEY_KP_8))
        {
            if (tipo_out)
            {
                *tipo_out = FUTBOL_8;
            }
            return 1;
        }
        if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1))
        {
            if (tipo_out)
            {
                *tipo_out = FUTBOL_11;
            }
            return 1;
        }

        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static int modal_nombre_equipo_creacion_gui(TipoEquipo tipo, char *nombre_out, size_t nombre_size)
{
    char nombre[64] = {0};
    int cursor = 0;
    float toast_timer = 0.0f;
    int toast_ok = 0;
    char toast_msg[120] = {0};

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = (sw > 900) ? 80 : 20;
        int panel_y = 140;
        int panel_w = sw - panel_x * 2;
        int panel_h = 220;
        Rectangle input_rect = {(float)(panel_x + 24), (float)(panel_y + 74), (float)(panel_w - 48), 36.0f};

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header("CREAR EQUIPO", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);
        gui_text(TextFormat("Tipo de equipo: %s", equipo_gui_nombre_tipo_equipo(tipo)),
                 (float)(panel_x + 24),
                 (float)(panel_y + 32),
                 20.0f,
                 gui_get_active_theme()->text_primary);
        gui_text("Nombre del equipo:",
                 (float)(panel_x + 24),
                 (float)(panel_y + 58),
                 18.0f,
                 gui_get_active_theme()->text_secondary);

        DrawRectangleRec(input_rect, (Color){18, 36, 28, 255});
        DrawRectangleLines((int)input_rect.x,
                           (int)input_rect.y,
                           (int)input_rect.width,
                           (int)input_rect.height,
                           (Color){55, 100, 72, 255});
        gui_text(nombre, input_rect.x + 8.0f, input_rect.y + 8.0f, 18.0f,
                 (Color){233, 247, 236, 255});

        if (toast_timer <= 0.0f)
        {
            equipo_gui_draw_input_caret(nombre, input_rect);
        }
        if (toast_timer > 0.0f)
        {
            equipo_gui_draw_action_toast(sw, sh, toast_msg, toast_ok);
        }

        gui_draw_footer_hint("ENTER: continuar | ESC: cancelar", (float)panel_x, sh);
        EndDrawing();

        if (equipo_gui_tick_toast(&toast_timer) && toast_ok)
        {
            if (nombre_out && nombre_size > 0)
            {
                snprintf(nombre_out, nombre_size, "%s", nombre);
            }
            return 1;
        }

        if (toast_timer > 0.0f)
        {
            continue;
        }

        equipo_gui_append_printable_ascii(nombre, &cursor, sizeof(nombre), 0);
        equipo_gui_handle_backspace(nombre, &cursor);

        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }

        if (IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ENTER);
            trim_whitespace(nombre);

            if (nombre[0] == '\0')
            {
                toast_ok = 0;
                snprintf(toast_msg, sizeof(toast_msg), "El nombre no puede estar vacio");
                toast_timer = 1.2f;
            }
            else
            {
                toast_ok = 1;
                snprintf(toast_msg, sizeof(toast_msg), "Nombre cargado");
                toast_timer = 0.65f;
            }
        }
    }

    return 0;
}

static void equipo_gui_dibujar_posicion_selector(Rectangle r1,
                                                 Rectangle r2,
                                                 Rectangle r3,
                                                 Rectangle r4,
                                                 Posicion posicion)
{
    equipo_gui_draw_action_button(r1, "Arquero", posicion == ARQUERO);
    equipo_gui_draw_action_button(r2, "Defensor", posicion == DEFENSOR);
    equipo_gui_draw_action_button(r3, "Mediocampista", posicion == MEDIOCAMPISTA);
    equipo_gui_draw_action_button(r4, "Delantero", posicion == DELANTERO);
}

typedef struct
{
    TipoEquipo tipo;
    int index;
    const Jugador *cargados;
} EquipoGuiJugadorValidacion;

typedef struct
{
    char nombre[50];
    char numero[8];
    int cursor_nombre;
    int cursor_numero;
    int foco_nombre;
    Posicion posicion;
    float toast_timer;
    int toast_ok;
    char toast_msg[120];
} EquipoGuiJugadorFormState;

typedef struct
{
    int sw;
    int sh;
    int panel_x;
    int panel_y;
    int panel_w;
    int panel_h;
    Rectangle nombre_rect;
    Rectangle numero_rect;
    Rectangle pos1;
    Rectangle pos2;
    Rectangle pos3;
    Rectangle pos4;
    Rectangle btn_guardar;
    Rectangle btn_cancelar;
    int click;
} EquipoGuiJugadorFormLayout;

static int equipo_gui_validar_y_cargar_jugador(const EquipoGuiJugadorValidacion *ctx,
                                               char *nombre_txt,
                                               const char *numero_txt,
                                               Posicion posicion,
                                               Jugador *jugador_out,
                                               char *error_msg,
                                               size_t error_msg_size)
{
    int numero = 0;

    if (!ctx || !nombre_txt || !numero_txt || !jugador_out || !error_msg || error_msg_size == 0)
    {
        return 0;
    }

    trim_whitespace(nombre_txt);
    if (nombre_txt[0] == '\0')
    {
        snprintf(error_msg, error_msg_size, "El nombre del jugador no puede estar vacio");
        return 0;
    }

    if (ctx->tipo == MOMENTANEO)
    {
        numero = ctx->index + 1;
    }
    else
    {
        numero = atoi(numero_txt);
        if (numero <= 0)
        {
            snprintf(error_msg, error_msg_size, "El numero debe ser mayor a cero");
            return 0;
        }
        if (equipo_gui_numero_repetido_creacion(ctx->cargados, ctx->index, numero))
        {
            snprintf(error_msg, error_msg_size, "Numero repetido en el equipo");
            return 0;
        }
    }

    if (posicion == ARQUERO && equipo_gui_hay_arquero_creacion(ctx->cargados, ctx->index))
    {
        snprintf(error_msg, error_msg_size, "Solo se permite un arquero por equipo");
        return 0;
    }

    snprintf(jugador_out->nombre, sizeof(jugador_out->nombre), "%s", nombre_txt);
    jugador_out->numero = numero;
    jugador_out->posicion = posicion;
    jugador_out->es_capitan = 0;
    return 1;
}

static void equipo_gui_jugador_form_init(EquipoGuiJugadorFormState *state)
{
    if (!state)
    {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->foco_nombre = 1;
    state->posicion = DELANTERO;
}

static void equipo_gui_jugador_form_build_layout(EquipoGuiJugadorFormLayout *layout)
{
    if (!layout)
    {
        return;
    }

    layout->sw = GetScreenWidth();
    layout->sh = GetScreenHeight();
    layout->panel_x = (layout->sw > 980) ? 60 : 20;
    layout->panel_y = 120;
    layout->panel_w = layout->sw - (layout->panel_x * 2);
    layout->panel_h = layout->sh - 220;
    layout->nombre_rect = (Rectangle){(float)(layout->panel_x + 24), (float)(layout->panel_y + 92), (float)(layout->panel_w - 48), 36.0f};
    layout->numero_rect = (Rectangle){(float)(layout->panel_x + 24), (float)(layout->panel_y + 168), 180.0f, 36.0f};
    layout->pos1 = (Rectangle){(float)(layout->panel_x + 220), (float)(layout->panel_y + 168), 156.0f, 36.0f};
    layout->pos2 = (Rectangle){(float)(layout->panel_x + 388), (float)(layout->panel_y + 168), 156.0f, 36.0f};
    layout->pos3 = (Rectangle){(float)(layout->panel_x + 556), (float)(layout->panel_y + 168), 156.0f, 36.0f};
    layout->pos4 = (Rectangle){(float)(layout->panel_x + 724), (float)(layout->panel_y + 168), 156.0f, 36.0f};
    layout->btn_guardar = (Rectangle){(float)(layout->panel_x + layout->panel_w - 364), (float)(layout->panel_y + layout->panel_h - 54), 168.0f, 38.0f};
    layout->btn_cancelar = (Rectangle){(float)(layout->panel_x + layout->panel_w - 184), (float)(layout->panel_y + layout->panel_h - 54), 168.0f, 38.0f};
    layout->click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static void equipo_gui_jugador_form_draw(const EquipoGuiJugadorFormLayout *layout,
                                         const EquipoGuiJugadorFormState *state,
                                         TipoEquipo tipo,
                                         int jugador_idx,
                                         int total_jugadores,
                                         const Jugador *cargados)
{
    BeginDrawing();
    ClearBackground(gui_get_active_theme()->bg_main);
    gui_draw_module_header("CREAR EQUIPO", layout->sw);

    DrawRectangle(layout->panel_x, layout->panel_y, layout->panel_w, layout->panel_h, gui_get_active_theme()->card_bg);
    DrawRectangleLines(layout->panel_x, layout->panel_y, layout->panel_w, layout->panel_h, gui_get_active_theme()->card_border);

    gui_text(TextFormat("Jugador %d de %d", jugador_idx + 1, total_jugadores),
             (float)(layout->panel_x + 24),
             (float)(layout->panel_y + 36),
             24.0f,
             gui_get_active_theme()->text_primary);
    gui_text("Nombre:",
             (float)(layout->panel_x + 24),
             (float)(layout->panel_y + 68),
             18.0f,
             gui_get_active_theme()->text_secondary);

    DrawRectangleRec(layout->nombre_rect, (Color){18, 36, 28, 255});
    DrawRectangleLines((int)layout->nombre_rect.x,
                       (int)layout->nombre_rect.y,
                       (int)layout->nombre_rect.width,
                       (int)layout->nombre_rect.height,
                       (Color){55, 100, 72, 255});
    gui_text(state->nombre, layout->nombre_rect.x + 8.0f, layout->nombre_rect.y + 8.0f, 18.0f,
             (Color){233, 247, 236, 255});

    gui_text("Numero:",
             (float)(layout->panel_x + 24),
             (float)(layout->panel_y + 144),
             18.0f,
             gui_get_active_theme()->text_secondary);

    DrawRectangleRec(layout->numero_rect, (Color){18, 36, 28, 255});
    DrawRectangleLines((int)layout->numero_rect.x,
                       (int)layout->numero_rect.y,
                       (int)layout->numero_rect.width,
                       (int)layout->numero_rect.height,
                       (Color){55, 100, 72, 255});
    if (tipo == MOMENTANEO)
    {
        gui_text(TextFormat("%d (auto)", jugador_idx + 1),
                 layout->numero_rect.x + 8.0f,
                 layout->numero_rect.y + 8.0f,
                 18.0f,
                 (Color){233, 247, 236, 255});
    }
    else
    {
        gui_text(state->numero, layout->numero_rect.x + 8.0f, layout->numero_rect.y + 8.0f, 18.0f,
                 (Color){233, 247, 236, 255});
    }

    gui_text("Posicion:",
             (float)(layout->panel_x + 220),
             (float)(layout->panel_y + 144),
             18.0f,
             gui_get_active_theme()->text_secondary);
    equipo_gui_dibujar_posicion_selector(layout->pos1, layout->pos2, layout->pos3, layout->pos4, state->posicion);

    if (jugador_idx > 0)
    {
        int y_base = layout->panel_y + 230;
        gui_text("Jugadores cargados:",
                 (float)(layout->panel_x + 24),
                 (float)y_base,
                 18.0f,
                 gui_get_active_theme()->text_secondary);
        for (int i = 0; i < jugador_idx && i < 6; i++)
        {
            gui_text_truncated(TextFormat("%d. %s (%d, %s)",
                                          i + 1,
                                          cargados[i].nombre,
                                          cargados[i].numero,
                                          get_nombre_posicion(cargados[i].posicion)),
                               (float)(layout->panel_x + 24),
                               (float)(y_base + 28 + i * 24),
                               16.0f,
                               (float)layout->panel_w - 48.0f,
                               gui_get_active_theme()->text_secondary);
        }
    }

    if (state->toast_timer <= 0.0f)
    {
        if (state->foco_nombre)
        {
            equipo_gui_draw_input_caret(state->nombre, layout->nombre_rect);
        }
        else if (tipo == FIJO)
        {
            equipo_gui_draw_input_caret(state->numero, layout->numero_rect);
        }
    }

    equipo_gui_draw_action_button(layout->btn_guardar, "Guardar", 1);
    equipo_gui_draw_action_button(layout->btn_cancelar, "Cancelar", 0);

    if (state->toast_timer > 0.0f)
    {
        equipo_gui_draw_action_toast(layout->sw, layout->sh, state->toast_msg, state->toast_ok);
    }

    gui_draw_footer_hint("TAB: cambiar campo | ENTER: guardar | ESC: cancelar", (float)layout->panel_x, layout->sh);
    EndDrawing();
}

static void equipo_gui_jugador_form_handle_focus(EquipoGuiJugadorFormState *state,
                                                  const EquipoGuiJugadorFormLayout *layout,
                                                  TipoEquipo tipo)
{
    if (layout->click && CheckCollisionPointRec(GetMousePosition(), layout->nombre_rect))
    {
        state->foco_nombre = 1;
    }
    else if (tipo == FIJO && layout->click && CheckCollisionPointRec(GetMousePosition(), layout->numero_rect))
    {
        state->foco_nombre = 0;
    }
}

static void equipo_gui_jugador_form_handle_posicion(EquipoGuiJugadorFormState *state,
                                                     const EquipoGuiJugadorFormLayout *layout)
{
    if (layout->click && CheckCollisionPointRec(GetMousePosition(), layout->pos1))
    {
        state->posicion = ARQUERO;
    }
    else if (layout->click && CheckCollisionPointRec(GetMousePosition(), layout->pos2))
    {
        state->posicion = DEFENSOR;
    }
    else if (layout->click && CheckCollisionPointRec(GetMousePosition(), layout->pos3))
    {
        state->posicion = MEDIOCAMPISTA;
    }
    else if (layout->click && CheckCollisionPointRec(GetMousePosition(), layout->pos4))
    {
        state->posicion = DELANTERO;
    }
}

static void equipo_gui_jugador_form_handle_texto(EquipoGuiJugadorFormState *state, TipoEquipo tipo)
{
    if (state->foco_nombre)
    {
        equipo_gui_append_printable_ascii(state->nombre, &state->cursor_nombre, sizeof(state->nombre), 0);
        if (IsKeyPressed(KEY_BACKSPACE))
        {
            equipo_gui_handle_backspace(state->nombre, &state->cursor_nombre);
        }
        return;
    }

    if (tipo == FIJO)
    {
        equipo_gui_append_printable_ascii(state->numero, &state->cursor_numero, sizeof(state->numero), 1);
        if (IsKeyPressed(KEY_BACKSPACE))
        {
            equipo_gui_handle_backspace(state->numero, &state->cursor_numero);
        }
    }
}

static void equipo_gui_jugador_form_intentar_guardar(EquipoGuiJugadorFormState *state,
                                                      const EquipoGuiJugadorValidacion *ctx,
                                                      Jugador *jugador_out)
{
    if (equipo_gui_validar_y_cargar_jugador(ctx,
                                            state->nombre,
                                            state->numero,
                                            state->posicion,
                                            jugador_out,
                                            state->toast_msg,
                                            sizeof(state->toast_msg)))
    {
        state->toast_ok = 1;
        snprintf(state->toast_msg, sizeof(state->toast_msg), "Jugador cargado");
        state->toast_timer = 0.55f;
    }
    else
    {
        state->toast_ok = 0;
        state->toast_timer = 1.2f;
    }
}

static int modal_jugador_creacion_equipo_gui(TipoEquipo tipo,
                                              int jugador_idx,
                                              int total_jugadores,
                                              const Jugador *cargados,
                                              Jugador *jugador_out)
{
    EquipoGuiJugadorFormState state;
    EquipoGuiJugadorValidacion validacion;

    equipo_gui_jugador_form_init(&state);
    validacion.tipo = tipo;
    validacion.index = jugador_idx;
    validacion.cargados = cargados;

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        EquipoGuiJugadorFormLayout layout;
        equipo_gui_jugador_form_build_layout(&layout);

        equipo_gui_jugador_form_draw(&layout,
                                     &state,
                                     tipo,
                                     jugador_idx,
                                     total_jugadores,
                                     cargados);

        if (equipo_gui_tick_toast(&state.toast_timer) && state.toast_ok)
        {
            return 1;
        }

        if (state.toast_timer > 0.0f)
        {
            continue;
        }

        equipo_gui_jugador_form_handle_focus(&state, &layout, tipo);
        equipo_gui_jugador_form_handle_posicion(&state, &layout);

        if (IsKeyPressed(KEY_TAB) && tipo == FIJO)
        {
            state.foco_nombre = !state.foco_nombre;
        }

        equipo_gui_jugador_form_handle_texto(&state, tipo);

        if (IsKeyPressed(KEY_ESCAPE) || (layout.click && CheckCollisionPointRec(GetMousePosition(), layout.btn_cancelar)))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }

        if (IsKeyPressed(KEY_ENTER) || (layout.click && CheckCollisionPointRec(GetMousePosition(), layout.btn_guardar)))
        {
            input_consume_key(KEY_ENTER);
            equipo_gui_jugador_form_intentar_guardar(&state, &validacion, jugador_out);
        }
    }

    return 0;
}

static void equipo_gui_selector_navegar(int total_jugadores, int *seleccionado)
{
    if (!seleccionado)
    {
        return;
    }

    if (IsKeyPressed(KEY_DOWN) && *seleccionado < total_jugadores - 1)
    {
        (*seleccionado)++;
    }
    if (IsKeyPressed(KEY_UP) && *seleccionado > 0)
    {
        (*seleccionado)--;
    }
}

typedef struct
{
    int panel_x;
    int panel_y;
    int panel_w;
    int click;
    int seleccionado;
    int mostrar_capitan;
} EquipoGuiSelectorRowsContext;

static int equipo_gui_selector_draw_rows(const Jugador *jugadores,
                                         int total_jugadores,
                                         const EquipoGuiSelectorRowsContext *ctx)
{
    int clicked_idx = -1;

    for (int i = 0; i < total_jugadores; i++)
    {
        Rectangle fila = {(float)ctx->panel_x + 24.0f,
                          (float)ctx->panel_y + 32.0f + (float)(i * 38),
                          (float)ctx->panel_w - 48.0f,
                          34.0f};
        int hovered = CheckCollisionPointRec(GetMousePosition(), fila);
        int activo = (i == ctx->seleccionado);
        const char *cap = (ctx->mostrar_capitan && jugadores[i].es_capitan) ? " [CAPITAN]" : "";
        Color fill = activo ? gui_get_active_theme()->accent_primary : gui_get_active_theme()->bg_sidebar;

        if (hovered)
        {
            fill = gui_get_active_theme()->row_hover;
        }

        DrawRectangleRec(fila, fill);
        DrawRectangleLinesEx(fila, 1.0f, gui_get_active_theme()->border);
        gui_text(TextFormat("%d. %s (N:%d, %s)%s",
                            i + 1,
                            jugadores[i].nombre,
                            jugadores[i].numero,
                            get_nombre_posicion(jugadores[i].posicion),
                            cap),
                 fila.x + 10.0f,
                 fila.y + 8.0f,
                 18.0f,
                 gui_get_active_theme()->text_primary);

        if (ctx->click && hovered)
        {
            clicked_idx = i;
        }
    }

    return clicked_idx;
}

static int modal_selector_jugador_gui(const char *titulo,
                                      const Jugador *jugadores,
                                      int total_jugadores,
                                      int *jugador_idx_out,
                                      int mostrar_capitan)
{
    int seleccionado = 0;

    if (!jugadores || total_jugadores <= 0 || !jugador_idx_out)
    {
        return 0;
    }

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 760 : sw - 40;
        int panel_h = sh > 760 ? 560 : sh - 120;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        int clicked_idx;
        EquipoGuiSelectorRowsContext rows_ctx;

        equipo_gui_selector_navegar(total_jugadores, &seleccionado);

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header(titulo ? titulo : "SELECCIONAR JUGADOR", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        rows_ctx.panel_x = panel_x;
        rows_ctx.panel_y = panel_y;
        rows_ctx.panel_w = panel_w;
        rows_ctx.click = click;
        rows_ctx.seleccionado = seleccionado;
        rows_ctx.mostrar_capitan = mostrar_capitan;

        clicked_idx = equipo_gui_selector_draw_rows(jugadores,
                                total_jugadores,
                                &rows_ctx);

        gui_draw_footer_hint("Click o ENTER para confirmar | Flechas: mover | ESC: cancelar",
                             (float)panel_x,
                             sh);
        EndDrawing();

        if (clicked_idx >= 0)
        {
            *jugador_idx_out = clicked_idx;
            return 1;
        }
        if (IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ENTER);
            *jugador_idx_out = seleccionado;
            return 1;
        }
        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static int modal_capitan_creacion_equipo_gui(const Jugador *jugadores,
                                             int total_jugadores,
                                             int *capitan_idx_out)
{
    return modal_selector_jugador_gui("SELECCIONAR CAPITAN",
                                      jugadores,
                                      total_jugadores,
                                      capitan_idx_out,
                                      0);
}

static int equipo_gui_guardar_fijo(const Equipo *equipo, int *equipo_id_out)
{
    int equipo_id = insert_equipo_record(equipo);
    if (equipo_id <= 0)
    {
        return 0;
    }

    insert_jugadores_for_equipo(equipo_id, equipo);
    if (equipo_id_out)
    {
        *equipo_id_out = equipo_id;
    }

    {
        char log_msg[220];
        snprintf(log_msg,
                 sizeof(log_msg),
                 "Creado equipo fijo id=%d nombre=%.120s",
                 equipo_id,
                 equipo->nombre);
        app_log_event("EQUIPO", log_msg);
    }
    return 1;
}

static int modal_post_creacion_equipo_fijo_gui(int equipo_id)
{
    input_consume_key(KEY_ENTER);
    input_consume_key(KEY_ESCAPE);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 760 : sw - 40;
        int panel_h = 220;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle btn_si = {(float)panel_x + panel_w - 362.0f, (float)panel_y + panel_h - 54.0f, 168.0f, 38.0f};
        Rectangle btn_no = {(float)panel_x + panel_w - 184.0f, (float)panel_y + panel_h - 54.0f, 168.0f, 38.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header("EQUIPO CREADO", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text(TextFormat("Equipo fijo ID %d creado correctamente", equipo_id),
                 (float)panel_x + 24.0f,
                 (float)panel_y + 42.0f,
                 22.0f,
                 gui_get_active_theme()->text_primary);
        gui_text("Deseas asignarlo a un partido ahora?",
                 (float)panel_x + 24.0f,
                 (float)panel_y + 86.0f,
                 18.0f,
                 gui_get_active_theme()->text_secondary);

        equipo_gui_draw_action_button(btn_si, "Asignar", 1);
        equipo_gui_draw_action_button(btn_no, "Omitir", 0);
        gui_draw_footer_hint("ENTER: asignar | ESC: omitir", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_si)) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ENTER);
            return 1;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_no)) || IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static int crear_equipo_desde_wizard_gui(TipoEquipo tipo)
{
    TipoFutbol tipo_futbol;
    int num_jugadores;
    char nombre_equipo[50] = {0};
    Jugador jugadores[11] = {0};
    int capitan_idx = -1;
    Equipo equipo = {0};

    if (!modal_tipo_futbol_creacion_equipo_gui(tipo, &tipo_futbol))
    {
        return 0;
    }

    num_jugadores = get_num_jugadores_por_tipo(tipo_futbol);
    if (!modal_nombre_equipo_creacion_gui(tipo, nombre_equipo, sizeof(nombre_equipo)))
    {
        return 0;
    }

    for (int i = 0; i < num_jugadores; i++)
    {
        if (!modal_jugador_creacion_equipo_gui(tipo, i, num_jugadores, jugadores, &jugadores[i]))
        {
            return 0;
        }
    }

    if (!modal_capitan_creacion_equipo_gui(jugadores, num_jugadores, &capitan_idx))
    {
        return 0;
    }

    equipo.tipo = tipo;
    equipo.tipo_futbol = tipo_futbol;
    equipo.num_jugadores = num_jugadores;
    equipo.partido_id = -1;
    snprintf(equipo.nombre, sizeof(equipo.nombre), "%s", nombre_equipo);

    for (int i = 0; i < num_jugadores; i++)
    {
        equipo.jugadores[i] = jugadores[i];
        equipo.jugadores[i].es_capitan = (i == capitan_idx) ? 1 : 0;
    }

    if (tipo == FIJO)
    {
        int equipo_id = 0;
        if (!equipo_gui_guardar_fijo(&equipo, &equipo_id))
        {
            equipo_gui_show_action_feedback("CREAR EQUIPO", "No se pudo crear el equipo fijo", 0);
            return 0;
        }

        equipo_gui_show_action_feedback("CREAR EQUIPO", "Equipo fijo creado correctamente", 1);
        if (modal_post_creacion_equipo_fijo_gui(equipo_id))
        {
            (void)modal_asignacion_equipo_gui(equipo_id);
        }
        return 1;
    }

    {
        char log_msg[220];
        snprintf(log_msg,
                 sizeof(log_msg),
                 "Creado equipo momentaneo nombre=%.120s jugadores=%d",
                 equipo.nombre,
                 equipo.num_jugadores);
        app_log_event("EQUIPO", log_msg);
    }
    equipo_gui_show_action_feedback("CREAR EQUIPO", "Equipo momentaneo creado correctamente", 1);
    return 1;
}

static int equipo_gui_cargar_equipo_desde_wizard(TipoEquipo tipo,
                                                 int usar_tipo_forzado,
                                                 TipoFutbol tipo_forzado,
                                                 Equipo *equipo_out)
{
    TipoFutbol tipo_futbol;
    int num_jugadores;
    char nombre_equipo[50] = {0};
    Jugador jugadores[11] = {0};
    int capitan_idx = -1;

    if (!equipo_out)
    {
        return 0;
    }

    if (usar_tipo_forzado)
    {
        tipo_futbol = tipo_forzado;
    }
    else if (!modal_tipo_futbol_creacion_equipo_gui(tipo, &tipo_futbol))
    {
        return 0;
    }

    num_jugadores = get_num_jugadores_por_tipo(tipo_futbol);
    if (!modal_nombre_equipo_creacion_gui(tipo, nombre_equipo, sizeof(nombre_equipo)))
    {
        return 0;
    }

    for (int i = 0; i < num_jugadores; i++)
    {
        if (!modal_jugador_creacion_equipo_gui(tipo, i, num_jugadores, jugadores, &jugadores[i]))
        {
            return 0;
        }
    }

    if (!modal_capitan_creacion_equipo_gui(jugadores, num_jugadores, &capitan_idx))
    {
        return 0;
    }

    memset(equipo_out, 0, sizeof(*equipo_out));
    equipo_out->tipo = tipo;
    equipo_out->tipo_futbol = tipo_futbol;
    equipo_out->num_jugadores = num_jugadores;
    equipo_out->partido_id = -1;
    snprintf(equipo_out->nombre, sizeof(equipo_out->nombre), "%s", nombre_equipo);

    for (int i = 0; i < num_jugadores; i++)
    {
        equipo_out->jugadores[i] = jugadores[i];
        equipo_out->jugadores[i].es_capitan = (i == capitan_idx) ? 1 : 0;
    }

    return 1;
}

static int equipo_gui_max_jugadores_por_tipo(TipoFutbol tipo)
{
    switch (tipo)
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
        return 11;
    }
}

static int modal_cantidad_momentaneos_gui(void)
{
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);
    input_consume_key(KEY_U);
    input_consume_key(KEY_D);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 820 : sw - 40;
        int panel_h = 230;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle btn_uno = {(float)panel_x + 24.0f, (float)panel_y + 130.0f, 380.0f, 46.0f};
        Rectangle btn_dos = {(float)panel_x + 416.0f, (float)panel_y + 130.0f, 380.0f, 46.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header("EQUIPO MOMENTANEO", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text("Cuantos equipos momentaneos deseas crear?",
                 (float)panel_x + 24.0f,
                 (float)panel_y + 44.0f,
                 24.0f,
                 gui_get_active_theme()->text_primary);
        gui_text("Puedes crear uno o preparar local/visitante para simular partido",
                 (float)panel_x + 24.0f,
                 (float)panel_y + 84.0f,
                 18.0f,
                 gui_get_active_theme()->text_secondary);

        equipo_gui_draw_action_button(btn_uno, "Un equipo", 1);
        equipo_gui_draw_action_button(btn_dos, "Dos equipos", 0);
        gui_draw_footer_hint("U/ENTER: uno | D: dos | ESC: volver", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_uno)) ||
            IsKeyPressed(KEY_U) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_U);
            input_consume_key(KEY_ENTER);
            return 1;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_dos)) || IsKeyPressed(KEY_D))
        {
            input_consume_key(KEY_D);
            return 2;
        }

        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static int modal_seleccionar_jugador_momentaneo_gui(const Equipo *equipo,
                                                     const char *titulo,
                                                     int *idx_out)
{
    if (!equipo)
    {
        return 0;
    }

    return modal_selector_jugador_gui(titulo ? titulo : "SELECCIONAR JUGADOR",
                                      equipo->jugadores,
                                      equipo->num_jugadores,
                                      idx_out,
                                      1);
}

static int modal_confirmacion_simple_gui(const char *titulo,
                                         const char *mensaje,
                                         const char *accion)
{
    input_consume_key(KEY_ENTER);
    input_consume_key(KEY_ESCAPE);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 740 : sw - 40;
        int panel_h = 220;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle btn_ok = {(float)panel_x + panel_w - 362.0f, (float)panel_y + panel_h - 54.0f, 168.0f, 38.0f};
        Rectangle btn_cancel = {(float)panel_x + panel_w - 184.0f, (float)panel_y + panel_h - 54.0f, 168.0f, 38.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header(titulo ? titulo : "CONFIRMAR", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text(mensaje ? mensaje : "Confirma la accion",
                 (float)panel_x + 24.0f,
                 (float)panel_y + 48.0f,
                 20.0f,
                 gui_get_active_theme()->text_primary);

        equipo_gui_draw_action_button(btn_ok, accion ? accion : "Confirmar", 1);
        equipo_gui_draw_action_button(btn_cancel, "Cancelar", 0);
        gui_draw_footer_hint("ENTER: confirmar | ESC: cancelar", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_ok)) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ENTER);
            return 1;
        }
        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_cancel)) || IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static int modal_editar_jugador_momentaneo_gui(Equipo *equipo, int jugador_idx) /* NOSONAR */
{
    char nombre[50] = {0};
    char numero[8] = {0};
    int cursor_nombre;
    int cursor_numero;
    int foco_nombre = 1;
    Posicion posicion;
    float toast_timer = 0.0f;
    int toast_ok = 0;
    char toast_msg[120] = {0};

    if (!equipo || jugador_idx < 0 || jugador_idx >= equipo->num_jugadores)
    {
        return 0;
    }

    snprintf(nombre, sizeof(nombre), "%s", equipo->jugadores[jugador_idx].nombre);
    snprintf(numero, sizeof(numero), "%d", equipo->jugadores[jugador_idx].numero);
    cursor_nombre = (int)safe_strnlen(nombre, sizeof(nombre));
    cursor_numero = (int)safe_strnlen(numero, sizeof(numero));
    posicion = equipo->jugadores[jugador_idx].posicion;

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 820 : sw - 40;
        int panel_h = 300;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle nombre_rect = {(float)panel_x + 24.0f, (float)panel_y + 88.0f, (float)panel_w - 48.0f, 36.0f};
        Rectangle numero_rect = {(float)panel_x + 24.0f, (float)panel_y + 160.0f, 180.0f, 36.0f};
        Rectangle pos1 = {(float)panel_x + 220.0f, (float)panel_y + 160.0f, 138.0f, 36.0f};
        Rectangle pos2 = {(float)panel_x + 370.0f, (float)panel_y + 160.0f, 138.0f, 36.0f};
        Rectangle pos3 = {(float)panel_x + 520.0f, (float)panel_y + 160.0f, 138.0f, 36.0f};
        Rectangle pos4 = {(float)panel_x + 670.0f, (float)panel_y + 160.0f, 138.0f, 36.0f};
        Rectangle btn_guardar = {(float)panel_x + panel_w - 362.0f, (float)panel_y + panel_h - 54.0f, 168.0f, 38.0f};
        Rectangle btn_cancel = {(float)panel_x + panel_w - 184.0f, (float)panel_y + panel_h - 54.0f, 168.0f, 38.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header("EDITAR JUGADOR", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text(TextFormat("Jugador %d", jugador_idx + 1),
                 (float)panel_x + 24.0f,
                 (float)panel_y + 42.0f,
                 22.0f,
                 gui_get_active_theme()->text_primary);

        gui_text("Nombre:", (float)panel_x + 24.0f, (float)panel_y + 66.0f, 18.0f, gui_get_active_theme()->text_secondary);
        DrawRectangleRec(nombre_rect, (Color){18, 36, 28, 255});
        DrawRectangleLines((int)nombre_rect.x, (int)nombre_rect.y, (int)nombre_rect.width, (int)nombre_rect.height, (Color){55, 100, 72, 255});
        gui_text(nombre, nombre_rect.x + 8.0f, nombre_rect.y + 8.0f, 18.0f, (Color){233, 247, 236, 255});

        gui_text("Numero:", (float)panel_x + 24.0f, (float)panel_y + 138.0f, 18.0f, gui_get_active_theme()->text_secondary);
        DrawRectangleRec(numero_rect, (Color){18, 36, 28, 255});
        DrawRectangleLines((int)numero_rect.x, (int)numero_rect.y, (int)numero_rect.width, (int)numero_rect.height, (Color){55, 100, 72, 255});
        gui_text(numero, numero_rect.x + 8.0f, numero_rect.y + 8.0f, 18.0f, (Color){233, 247, 236, 255});

        gui_text("Posicion:", (float)panel_x + 220.0f, (float)panel_y + 138.0f, 18.0f, gui_get_active_theme()->text_secondary);
        equipo_gui_dibujar_posicion_selector(pos1, pos2, pos3, pos4, posicion);

        if (toast_timer <= 0.0f)
        {
            if (foco_nombre)
            {
                equipo_gui_draw_input_caret(nombre, nombre_rect);
            }
            else
            {
                equipo_gui_draw_input_caret(numero, numero_rect);
            }
        }

        equipo_gui_draw_action_button(btn_guardar, "Guardar", 1);
        equipo_gui_draw_action_button(btn_cancel, "Cancelar", 0);

        if (toast_timer > 0.0f)
        {
            equipo_gui_draw_action_toast(sw, sh, toast_msg, toast_ok);
        }

        gui_draw_footer_hint("TAB: cambiar campo | ENTER: guardar | ESC: cancelar", (float)panel_x, sh);
        EndDrawing();

        if (equipo_gui_tick_toast(&toast_timer) && toast_ok)
        {
            return 1;
        }

        if (toast_timer > 0.0f)
        {
            continue;
        }

        if (click && CheckCollisionPointRec(GetMousePosition(), nombre_rect))
        {
            foco_nombre = 1;
        }
        else if (click && CheckCollisionPointRec(GetMousePosition(), numero_rect))
        {
            foco_nombre = 0;
        }

        if (click && CheckCollisionPointRec(GetMousePosition(), pos1))
        {
            posicion = ARQUERO;
        }
        if (click && CheckCollisionPointRec(GetMousePosition(), pos2))
        {
            posicion = DEFENSOR;
        }
        if (click && CheckCollisionPointRec(GetMousePosition(), pos3))
        {
            posicion = MEDIOCAMPISTA;
        }
        if (click && CheckCollisionPointRec(GetMousePosition(), pos4))
        {
            posicion = DELANTERO;
        }

        if (IsKeyPressed(KEY_TAB))
        {
            foco_nombre = !foco_nombre;
        }

        if (foco_nombre)
        {
            equipo_gui_append_printable_ascii(nombre, &cursor_nombre, sizeof(nombre), 0);
            if (IsKeyPressed(KEY_BACKSPACE))
            {
                equipo_gui_handle_backspace(nombre, &cursor_nombre);
            }
        }
        else
        {
            equipo_gui_append_printable_ascii(numero, &cursor_numero, sizeof(numero), 1);
            if (IsKeyPressed(KEY_BACKSPACE))
            {
                equipo_gui_handle_backspace(numero, &cursor_numero);
            }
        }

        if (IsKeyPressed(KEY_ESCAPE) || (click && CheckCollisionPointRec(GetMousePosition(), btn_cancel)))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }

        if (IsKeyPressed(KEY_ENTER) || (click && CheckCollisionPointRec(GetMousePosition(), btn_guardar)))
        {
            int nuevo_numero;
            input_consume_key(KEY_ENTER);
            trim_whitespace(nombre);

            if (nombre[0] == '\0')
            {
                toast_ok = 0;
                snprintf(toast_msg, sizeof(toast_msg), "El nombre no puede estar vacio");
                toast_timer = 1.2f;
                continue;
            }

            nuevo_numero = atoi(numero);
            if (nuevo_numero <= 0)
            {
                toast_ok = 0;
                snprintf(toast_msg, sizeof(toast_msg), "Numero invalido");
                toast_timer = 1.2f;
                continue;
            }

            if (numero_repetido_en_equipo(equipo, equipo->num_jugadores, nuevo_numero, jugador_idx))
            {
                toast_ok = 0;
                snprintf(toast_msg, sizeof(toast_msg), "Numero repetido en el equipo");
                toast_timer = 1.2f;
                continue;
            }

            if (posicion == ARQUERO && ya_hay_arquero_en_equipo(equipo, equipo->num_jugadores, jugador_idx))
            {
                toast_ok = 0;
                snprintf(toast_msg, sizeof(toast_msg), "Solo se permite un arquero por equipo");
                toast_timer = 1.2f;
                continue;
            }

            snprintf(equipo->jugadores[jugador_idx].nombre, sizeof(equipo->jugadores[jugador_idx].nombre), "%s", nombre);
            equipo->jugadores[jugador_idx].numero = nuevo_numero;
            equipo->jugadores[jugador_idx].posicion = posicion;
            toast_ok = 1;
            snprintf(toast_msg, sizeof(toast_msg), "Jugador actualizado");
            toast_timer = 0.8f;
        }
    }

    return 0;
}

static int equipo_gui_agregar_jugador_momentaneo(Equipo *equipo)
{
    int max_jugadores = equipo_gui_max_jugadores_por_tipo(equipo->tipo_futbol);
    Jugador nuevo = {0};

    if (!equipo)
    {
        return 0;
    }

    if (equipo->num_jugadores >= max_jugadores)
    {
        equipo_gui_show_action_feedback("EQUIPO MOMENTANEO", "El equipo ya alcanzo el maximo de jugadores", 0);
        return 0;
    }

    if (!modal_jugador_creacion_equipo_gui(MOMENTANEO,
                                            equipo->num_jugadores,
                                            max_jugadores,
                                            equipo->jugadores,
                                            &nuevo))
    {
        return 0;
    }

    /* Permitir definir numero manual luego de la creacion auto para mantener flexibilidad. */
    nuevo.numero = equipo->num_jugadores + 1;
    while (numero_repetido_en_equipo(equipo, equipo->num_jugadores, nuevo.numero, -1))
    {
        nuevo.numero++;
    }

    equipo->jugadores[equipo->num_jugadores] = nuevo;
    equipo->num_jugadores++;
    return 1;
}

static int equipo_gui_eliminar_jugador_momentaneo(Equipo *equipo)
{
    int idx = -1;

    if (!equipo || equipo->num_jugadores <= 1)
    {
        equipo_gui_show_action_feedback("EQUIPO MOMENTANEO", "No se puede eliminar: minimo 1 jugador", 0);
        return 0;
    }

    if (!modal_seleccionar_jugador_momentaneo_gui(equipo, "ELIMINAR JUGADOR", &idx))
    {
        return 0;
    }

    if (!modal_confirmacion_simple_gui("ELIMINAR JUGADOR",
                                        TextFormat("Eliminar a %s?", equipo->jugadores[idx].nombre),
                                        "Eliminar"))
    {
        return 0;
    }

    for (int i = idx; i < equipo->num_jugadores - 1; i++)
    {
        equipo->jugadores[i] = equipo->jugadores[i + 1];
    }
    equipo->num_jugadores--;

    {
        int tiene_capitan = 0;
        for (int i = 0; i < equipo->num_jugadores; i++)
        {
            if (equipo->jugadores[i].es_capitan)
            {
                tiene_capitan = 1;
                break;
            }
        }
        if (!tiene_capitan && equipo->num_jugadores > 0)
        {
            equipo->jugadores[0].es_capitan = 1;
        }
    }

    return 1;
}

static int equipo_gui_cambiar_capitan_momentaneo(Equipo *equipo)
{
    int idx = -1;

    if (!equipo || equipo->num_jugadores <= 0)
    {
        return 0;
    }

    if (!modal_seleccionar_jugador_momentaneo_gui(equipo, "CAMBIAR CAPITAN", &idx))
    {
        return 0;
    }

    for (int i = 0; i < equipo->num_jugadores; i++)
    {
        equipo->jugadores[i].es_capitan = 0;
    }
    equipo->jugadores[idx].es_capitan = 1;
    return 1;
}

static void equipo_gui_dibujar_resumen_momentaneo(const Equipo *equipo,
                                                   const char *titulo,
                                                   int sw,
                                                   int sh)
{
    int panel_x = (sw > 980) ? 40 : 20;
    int panel_y = 96;
    int panel_w = sw - panel_x * 2;
    int panel_h = sh - 188;

    DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
    DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

    gui_text(titulo ? titulo : "EQUIPO MOMENTANEO",
             (float)panel_x + 20.0f,
             (float)panel_y + 18.0f,
             24.0f,
             gui_get_active_theme()->text_primary);
    gui_text(TextFormat("%s | %s | Jugadores: %d",
                        equipo->nombre,
                        get_nombre_tipo_futbol(equipo->tipo_futbol),
                        equipo->num_jugadores),
             (float)panel_x + 20.0f,
             (float)panel_y + 52.0f,
             18.0f,
             gui_get_active_theme()->text_secondary);

    for (int i = 0; i < equipo->num_jugadores && i < 11; i++)
    {
        gui_text_truncated(TextFormat("%d. %s (N:%d, %s)%s",
                                      i + 1,
                                      equipo->jugadores[i].nombre,
                                      equipo->jugadores[i].numero,
                                      get_nombre_posicion(equipo->jugadores[i].posicion),
                                      equipo->jugadores[i].es_capitan ? " [CAPITAN]" : ""),
                           (float)panel_x + 20.0f,
                           (float)panel_y + 90.0f + (float)(i * 26),
                           17.0f,
                           (float)panel_w - 40.0f,
                           gui_get_active_theme()->text_secondary);
    }
}

static int gestionar_equipo_momentaneo_gui(Equipo *equipo, const char *titulo) /* NOSONAR */
{
    input_consume_key(KEY_ESCAPE);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        Rectangle btn_mod = {(float)sw - 868.0f, (float)sh - 74.0f, 160.0f, 40.0f};
        Rectangle btn_add = {(float)sw - 698.0f, (float)sh - 74.0f, 160.0f, 40.0f};
        Rectangle btn_del = {(float)sw - 528.0f, (float)sh - 74.0f, 160.0f, 40.0f};
        Rectangle btn_cap = {(float)sw - 358.0f, (float)sh - 74.0f, 160.0f, 40.0f};
        Rectangle btn_fin = {(float)sw - 188.0f, (float)sh - 74.0f, 160.0f, 40.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header(titulo ? titulo : "EQUIPO MOMENTANEO", sw);
        equipo_gui_dibujar_resumen_momentaneo(equipo, titulo ? titulo : "EQUIPO MOMENTANEO", sw, sh);

        equipo_gui_draw_action_button(btn_mod, "Modificar", 0);
        equipo_gui_draw_action_button(btn_add, "Agregar", 0);
        equipo_gui_draw_action_button(btn_del, "Eliminar", 0);
        equipo_gui_draw_action_button(btn_cap, "Capitan", 0);
        equipo_gui_draw_action_button(btn_fin, "Finalizar", 1);

        gui_draw_footer_hint("Gestion completa del equipo momentaneo", 40.0f, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_mod)) || IsKeyPressed(KEY_M))
        {
            int idx = -1;
            input_consume_key(KEY_M);
            if (modal_seleccionar_jugador_momentaneo_gui(equipo, "MODIFICAR JUGADOR", &idx))
            {
                (void)modal_editar_jugador_momentaneo_gui(equipo, idx);
            }
            continue;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_add)) || IsKeyPressed(KEY_A))
        {
            input_consume_key(KEY_A);
            if (equipo_gui_agregar_jugador_momentaneo(equipo))
            {
                equipo_gui_show_action_feedback("EQUIPO MOMENTANEO", "Jugador agregado", 1);
            }
            continue;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_del)) || IsKeyPressed(KEY_D))
        {
            input_consume_key(KEY_D);
            if (equipo_gui_eliminar_jugador_momentaneo(equipo))
            {
                equipo_gui_show_action_feedback("EQUIPO MOMENTANEO", "Jugador eliminado", 1);
            }
            continue;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_cap)) || IsKeyPressed(KEY_C))
        {
            input_consume_key(KEY_C);
            if (equipo_gui_cambiar_capitan_momentaneo(equipo))
            {
                equipo_gui_show_action_feedback("EQUIPO MOMENTANEO", "Capitan actualizado", 1);
            }
            continue;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_fin)) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ENTER);
            input_consume_key(KEY_ESCAPE);
            return 1;
        }
    }

    return 0;
}

typedef struct
{
    int minuto;
    int goles_local;
    int goles_visitante;
    float tick;
    int finalizado;
    char evento[160];
} EquipoGuiPartidoState;

static void equipo_gui_partido_state_init(EquipoGuiPartidoState *state)
{
    static int seeded = 0;

    if (!state)
    {
        return;
    }

    state->minuto = 1;
    state->goles_local = 0;
    state->goles_visitante = 0;
    state->tick = 0.0f;
    state->finalizado = 0;
    snprintf(state->evento, sizeof(state->evento), "Comienza el partido");

    if (!seeded)
    {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }
}

static void equipo_gui_partido_tick(EquipoGuiPartidoState *state,
                                    const Equipo *equipo_local,
                                    const Equipo *equipo_visitante)
{
    int ev;

    if (!state || state->finalizado)
    {
        return;
    }

    state->tick += GetFrameTime();
    if (state->tick < 0.12f)
    {
        return;
    }

    state->tick = 0.0f;
    ev = rand() % 100;

    if (ev < 22)
    {
        state->goles_local++;
        snprintf(state->evento, sizeof(state->evento), "Min %d: Gol de %s", state->minuto, equipo_local->nombre);
    }
    else if (ev < 44)
    {
        state->goles_visitante++;
        snprintf(state->evento, sizeof(state->evento), "Min %d: Gol de %s", state->minuto, equipo_visitante->nombre);
    }
    else if (ev < 65)
    {
        snprintf(state->evento, sizeof(state->evento), "Min %d: Ataque peligroso", state->minuto);
    }
    else if (ev < 80)
    {
        snprintf(state->evento, sizeof(state->evento), "Min %d: Falta fuerte", state->minuto);
    }
    else
    {
        snprintf(state->evento, sizeof(state->evento), "Min %d: Juego en medio campo", state->minuto);
    }

    state->minuto++;
    if (state->minuto > 60)
    {
        state->finalizado = 1;
    }
}

static const char *equipo_gui_partido_resultado(const EquipoGuiPartidoState *state)
{
    if (!state)
    {
        return "Empate";
    }

    if (state->goles_local > state->goles_visitante)
    {
        return "Gana el equipo local";
    }
    if (state->goles_visitante > state->goles_local)
    {
        return "Gana el equipo visitante";
    }
    return "Empate";
}

static void equipo_gui_partido_draw(const Equipo *equipo_local,
                                    const Equipo *equipo_visitante,
                                    const EquipoGuiPartidoState *state)
{
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    int panel_x = (sw > 980) ? 60 : 20;
    int panel_y = 120;
    int panel_w = sw - panel_x * 2;
    int panel_h = sh - 220;

    BeginDrawing();
    ClearBackground(gui_get_active_theme()->bg_main);
    gui_draw_module_header("SIMULACION DE PARTIDO", sw);

    DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
    DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

    gui_text(TextFormat("%s  %d  -  %d  %s",
                        equipo_local->nombre,
                        state->goles_local,
                        state->goles_visitante,
                        equipo_visitante->nombre),
             (float)panel_x + 24.0f,
             (float)panel_y + 34.0f,
             32.0f,
             gui_get_active_theme()->text_primary);

    if (!state->finalizado)
    {
        gui_text(TextFormat("Minuto: %d/60", state->minuto),
                 (float)panel_x + 24.0f,
                 (float)panel_y + 86.0f,
                 20.0f,
                 gui_get_active_theme()->text_secondary);
        DrawRectangle(panel_x + 24, panel_y + 124, panel_w - 48, 18, (Color){30, 52, 40, 255});
        DrawRectangle(panel_x + 24,
                      panel_y + 124,
                      (int)((float)(panel_w - 48) * ((float)(state->minuto - 1) / 60.0f)),
                      18,
                      gui_get_active_theme()->accent_primary);
        gui_text_truncated(state->evento,
                           (float)panel_x + 24.0f,
                           (float)panel_y + 162.0f,
                           20.0f,
                           (float)panel_w - 48.0f,
                           gui_get_active_theme()->text_secondary);
        gui_draw_footer_hint("Simulando...", (float)panel_x, sh);
    }
    else
    {
        gui_text("PARTIDO FINALIZADO",
                 (float)panel_x + 24.0f,
                 (float)panel_y + 98.0f,
                 24.0f,
                 gui_get_active_theme()->text_primary);
        gui_text(equipo_gui_partido_resultado(state),
                 (float)panel_x + 24.0f,
                 (float)panel_y + 138.0f,
                 22.0f,
                 gui_get_active_theme()->text_secondary);
        gui_text_truncated(state->evento,
                           (float)panel_x + 24.0f,
                           (float)panel_y + 176.0f,
                           18.0f,
                           (float)panel_w - 48.0f,
                           gui_get_active_theme()->text_secondary);
        gui_draw_footer_hint("ENTER o ESC para volver", (float)panel_x, sh);
    }

    EndDrawing();
}

static void simular_partido_momentaneo_gui(const Equipo *equipo_local,
                                           const Equipo *equipo_visitante)
{
    EquipoGuiPartidoState state;

    equipo_gui_partido_state_init(&state);

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        equipo_gui_partido_tick(&state, equipo_local, equipo_visitante);
        equipo_gui_partido_draw(equipo_local, equipo_visitante, &state);

        if (state.finalizado && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)))
        {
            input_consume_key(KEY_ENTER);
            input_consume_key(KEY_ESCAPE);
            return;
        }
    }
}

static int gestionar_dos_equipos_momentaneos_gui(Equipo *equipo_local,
                                                  Equipo *equipo_visitante)
{
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = (sw > 980) ? 40 : 20;
        int panel_y = 100;
        int panel_w = sw - panel_x * 2;
        int panel_h = sh - 190;
        Rectangle btn_local = {(float)panel_x + 24.0f, (float)panel_y + panel_h - 52.0f, 168.0f, 38.0f};
        Rectangle btn_visit = {(float)panel_x + 202.0f, (float)panel_y + panel_h - 52.0f, 168.0f, 38.0f};
        Rectangle btn_sim = {(float)panel_x + panel_w - 362.0f, (float)panel_y + panel_h - 52.0f, 168.0f, 38.0f};
        Rectangle btn_fin = {(float)panel_x + panel_w - 184.0f, (float)panel_y + panel_h - 52.0f, 168.0f, 38.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header("EQUIPOS MOMENTANEOS", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text("LOCAL", (float)panel_x + 24.0f, (float)panel_y + 18.0f, 22.0f, gui_get_active_theme()->text_primary);
        gui_text_truncated(TextFormat("%s | %s | Jugadores: %d",
                                      equipo_local->nombre,
                                      get_nombre_tipo_futbol(equipo_local->tipo_futbol),
                                      equipo_local->num_jugadores),
                           (float)panel_x + 24.0f,
                           (float)panel_y + 46.0f,
                           17.0f,
                           (float)panel_w - 48.0f,
                           gui_get_active_theme()->text_secondary);

        gui_text("VISITANTE", (float)panel_x + 24.0f, (float)panel_y + 94.0f, 22.0f, gui_get_active_theme()->text_primary);
        gui_text_truncated(TextFormat("%s | %s | Jugadores: %d",
                                      equipo_visitante->nombre,
                                      get_nombre_tipo_futbol(equipo_visitante->tipo_futbol),
                                      equipo_visitante->num_jugadores),
                           (float)panel_x + 24.0f,
                           (float)panel_y + 122.0f,
                           17.0f,
                           (float)panel_w - 48.0f,
                           gui_get_active_theme()->text_secondary);

        equipo_gui_draw_action_button(btn_local, "Gestionar Local", 0);
        equipo_gui_draw_action_button(btn_visit, "Gestionar Visit", 0);
        equipo_gui_draw_action_button(btn_sim, "Simular", 1);
        equipo_gui_draw_action_button(btn_fin, "Finalizar", 0);

        gui_draw_footer_hint("Gestiona alineaciones y simula el partido", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_local)) || IsKeyPressed(KEY_L))
        {
            input_consume_key(KEY_L);
            (void)gestionar_equipo_momentaneo_gui(equipo_local, "EQUIPO LOCAL");
            continue;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_visit)) || IsKeyPressed(KEY_V))
        {
            input_consume_key(KEY_V);
            (void)gestionar_equipo_momentaneo_gui(equipo_visitante, "EQUIPO VISITANTE");
            continue;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_sim)) || IsKeyPressed(KEY_S))
        {
            input_consume_key(KEY_S);
            simular_partido_momentaneo_gui(equipo_local, equipo_visitante);
            continue;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_fin)) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            return 1;
        }
    }

    return 0;
}

static int crear_flujo_momentaneo_gui(void)
{
    int cantidad = modal_cantidad_momentaneos_gui();

    if (cantidad <= 0)
    {
        return 0;
    }

    if (cantidad == 1)
    {
        Equipo equipo = {0};
        if (!equipo_gui_cargar_equipo_desde_wizard(MOMENTANEO, 0, FUTBOL_5, &equipo))
        {
            return 0;
        }

        {
            char log_msg[220];
            snprintf(log_msg,
                     sizeof(log_msg),
                     "Creado equipo momentaneo nombre=%.120s jugadores=%d",
                     equipo.nombre,
                     equipo.num_jugadores);
            app_log_event("EQUIPO", log_msg);
        }

        equipo_gui_show_action_feedback("CREAR EQUIPO", "Equipo momentaneo creado correctamente", 1);
        (void)gestionar_equipo_momentaneo_gui(&equipo, "EQUIPO MOMENTANEO");
        equipo_gui_show_action_feedback("EQUIPO MOMENTANEO", "Equipo momentaneo finalizado (no se guarda)", 1);
        return 1;
    }
    else
    {
        TipoFutbol tipo;
        Equipo local = {0};
        Equipo visitante = {0};

        if (!modal_tipo_futbol_creacion_equipo_gui(MOMENTANEO, &tipo))
        {
            return 0;
        }

        equipo_gui_show_action_feedback("CREAR EQUIPOS", "Configura el equipo LOCAL", 1);
        if (!equipo_gui_cargar_equipo_desde_wizard(MOMENTANEO, 1, tipo, &local))
        {
            return 0;
        }

        equipo_gui_show_action_feedback("CREAR EQUIPOS", "Configura el equipo VISITANTE", 1);
        if (!equipo_gui_cargar_equipo_desde_wizard(MOMENTANEO, 1, tipo, &visitante))
        {
            return 0;
        }

        app_log_event("EQUIPO", "Creados dos equipos momentaneos para simulacion");
        (void)gestionar_dos_equipos_momentaneos_gui(&local, &visitante);
        equipo_gui_show_action_feedback("EQUIPOS MOMENTANEOS", "Sesion finalizada (no se guarda)", 1);
        return 1;
    }
}

static int crear_equipo_gui(void)
{
    double key_block_until = 0.0;

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);
    input_consume_key(KEY_F);
    input_consume_key(KEY_M);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 760 : sw - 40;
        int panel_h = 220;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle btn_fijo = {(float)panel_x + 24.0f, (float)panel_y + 128.0f, 320.0f, 46.0f};
        Rectangle btn_momentaneo = {(float)panel_x + 360.0f, (float)panel_y + 128.0f, 320.0f, 46.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        int keys_blocked = (GetTime() < key_block_until);

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header("CREAR EQUIPO", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text("Selecciona el tipo de creacion", (float)panel_x + 24.0f,
                 (float)panel_y + 44.0f, 24.0f, gui_get_active_theme()->text_primary);
        gui_text("El asistente de jugadores se mantiene y reutiliza la logica actual.",
                 (float)panel_x + 24.0f,
                 (float)panel_y + 86.0f,
                 18.0f,
                 gui_get_active_theme()->text_secondary);

        equipo_gui_draw_action_button(btn_fijo, "Fijo", 1);
        equipo_gui_draw_action_button(btn_momentaneo, "Momentaneo", 0);
        gui_draw_footer_hint("F/ENTER: fijo | M: momentaneo | ESC: volver", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_fijo)) ||
            (!keys_blocked && (IsKeyPressed(KEY_F) || IsKeyPressed(KEY_ENTER))))
        {
            input_consume_key(KEY_F);
            input_consume_key(KEY_ENTER);
            (void)crear_equipo_desde_wizard_gui(FIJO);
            key_block_until = GetTime() + 0.20;
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            input_consume_key(KEY_F);
            input_consume_key(KEY_M);
            continue;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_momentaneo)) ||
            (!keys_blocked && IsKeyPressed(KEY_M)))
        {
            input_consume_key(KEY_M);
            (void)crear_flujo_momentaneo_gui();
            key_block_until = GetTime() + 0.20;
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            input_consume_key(KEY_F);
            input_consume_key(KEY_M);
            continue;
        }

        if (!keys_blocked && IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static int modal_accion_modificar_equipo_gui(int id)
{
    input_consume_key(KEY_ENTER);
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_N);
    input_consume_key(KEY_T);
    input_consume_key(KEY_A);
    input_consume_key(KEY_J);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 800 : sw - 40;
        int panel_h = 250;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle btn_nombre = {(float)panel_x + 24.0f, (float)panel_y + 122.0f, 178.0f, 42.0f};
        Rectangle btn_tipo = {(float)panel_x + 214.0f, (float)panel_y + 122.0f, 178.0f, 42.0f};
        Rectangle btn_asig = {(float)panel_x + 404.0f, (float)panel_y + 122.0f, 178.0f, 42.0f};
        Rectangle btn_jug = {(float)panel_x + 594.0f, (float)panel_y + 122.0f, 178.0f, 42.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header("MODIFICAR EQUIPO", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text(TextFormat("ID seleccionado: %d", id),
                 (float)panel_x + 24.0f,
                 (float)panel_y + 44.0f,
                 22.0f,
                 gui_get_active_theme()->text_primary);
        gui_text("Elige que deseas modificar", (float)panel_x + 24.0f,
                 (float)panel_y + 82.0f,
                 18.0f,
                 gui_get_active_theme()->text_secondary);

        equipo_gui_draw_action_button(btn_nombre, "Nombre", 1);
        equipo_gui_draw_action_button(btn_tipo, "Tipo Futbol", 0);
        equipo_gui_draw_action_button(btn_asig, "Asignacion", 0);
        equipo_gui_draw_action_button(btn_jug, "Jugadores", 0);
        gui_draw_footer_hint("N: nombre | T: tipo | A: asignacion | J: jugadores | ESC: cancelar",
                             (float)panel_x,
                             sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_nombre)) || IsKeyPressed(KEY_N))
        {
            input_consume_key(KEY_N);
            return EQUIPO_GUI_EDIT_NOMBRE;
        }
        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_tipo)) || IsKeyPressed(KEY_T))
        {
            input_consume_key(KEY_T);
            return EQUIPO_GUI_EDIT_TIPO_FUTBOL;
        }
        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_asig)) || IsKeyPressed(KEY_A))
        {
            input_consume_key(KEY_A);
            return EQUIPO_GUI_EDIT_ASIGNACION;
        }
        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_jug)) || IsKeyPressed(KEY_J))
        {
            input_consume_key(KEY_J);
            return EQUIPO_GUI_EDIT_JUGADORES;
        }
        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return EQUIPO_GUI_EDIT_CANCELAR;
        }
    }

    return EQUIPO_GUI_EDIT_CANCELAR;
}

static int modal_editar_nombre_equipo_gui(int equipo_id)
{
    char nombre[64] = {0};
    int cursor = 0;
    float toast_timer = 0.0f;
    int toast_ok = 0;
    char toast_msg[120] = {0};

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = (sw > 900) ? 80 : 20;
        int panel_y = 140;
        int panel_w = sw - panel_x * 2;
        int panel_h = 220;
        Rectangle input_rect = {(float)(panel_x + 24), (float)(panel_y + 74), (float)(panel_w - 48), 36.0f};

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header(TextFormat("MODIFICAR EQUIPO %d", equipo_id), sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){19, 40, 27, 255});
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){110, 161, 125, 255});
        gui_text("Nuevo nombre:", (float)(panel_x + 24), (float)(panel_y + 38), 18.0f,
                 (Color){233, 247, 236, 255});

        DrawRectangleRec(input_rect, (Color){18, 36, 28, 255});
        DrawRectangleLines((int)input_rect.x,
                           (int)input_rect.y,
                           (int)input_rect.width,
                           (int)input_rect.height,
                           (Color){55, 100, 72, 255});
        gui_text(nombre, input_rect.x + 8.0f, input_rect.y + 8.0f, 18.0f,
                 (Color){233, 247, 236, 255});

        if (toast_timer <= 0.0f)
        {
            equipo_gui_draw_input_caret(nombre, input_rect);
        }
        if (toast_timer > 0.0f)
        {
            equipo_gui_draw_action_toast(sw, sh, toast_msg, toast_ok);
        }

        gui_draw_footer_hint("ENTER: guardar | ESC: volver", (float)panel_x, sh);
        EndDrawing();

        if (equipo_gui_tick_toast(&toast_timer) && toast_ok)
        {
            return 1;
        }

        if (toast_timer > 0.0f)
        {
            continue;
        }

        equipo_gui_append_printable_ascii(nombre, &cursor, sizeof(nombre), 0);
        equipo_gui_handle_backspace(nombre, &cursor);

        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }

        if (IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ENTER);
            trim_whitespace(nombre);
            if (nombre[0] == '\0')
            {
                toast_ok = 0;
                snprintf(toast_msg, sizeof(toast_msg), "El nombre no puede estar vacio");
                toast_timer = 1.2f;
            }
            else if (ejecutar_update_text("UPDATE equipo SET nombre = ? WHERE id = ?", nombre, equipo_id))
            {
                toast_ok = 1;
                snprintf(toast_msg, sizeof(toast_msg), "Equipo modificado correctamente");
                toast_timer = 1.2f;
            }
            else
            {
                toast_ok = 0;
                snprintf(toast_msg, sizeof(toast_msg), "No se pudo modificar el equipo");
                toast_timer = 1.2f;
            }
        }
    }

    return 0;
}

static int modal_tipo_futbol_equipo_gui(int equipo_id)
{
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 760 : sw - 40;
        int panel_h = 240;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle b1 = {(float)panel_x + 24.0f, (float)panel_y + 122.0f, 220.0f, 42.0f};
        Rectangle b2 = {(float)panel_x + 256.0f, (float)panel_y + 122.0f, 220.0f, 42.0f};
        Rectangle b3 = {(float)panel_x + 488.0f, (float)panel_y + 122.0f, 220.0f, 42.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header("MODIFICAR TIPO FUTBOL", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text(TextFormat("Equipo ID %d", equipo_id),
                 (float)panel_x + 24.0f,
                 (float)panel_y + 46.0f,
                 22.0f,
                 gui_get_active_theme()->text_primary);
        gui_text("Selecciona el nuevo tipo de futbol (5, 7 o 11)", (float)panel_x + 24.0f,
                 (float)panel_y + 84.0f,
                 18.0f,
                 gui_get_active_theme()->text_secondary);

        equipo_gui_draw_action_button(b1, "Futbol 5", 1);
        equipo_gui_draw_action_button(b2, "Futbol 7", 0);
        equipo_gui_draw_action_button(b3, "Futbol 11", 0);
        gui_draw_footer_hint("Click para aplicar | ESC: cancelar", (float)panel_x, sh);
        EndDrawing();

        if (click && CheckCollisionPointRec(GetMousePosition(), b1))
        {
            return actualizar_tipo_futbol_equipo(equipo_id, FUTBOL_5);
        }
        if (click && CheckCollisionPointRec(GetMousePosition(), b2))
        {
            return actualizar_tipo_futbol_equipo(equipo_id, FUTBOL_7);
        }
        if (click && CheckCollisionPointRec(GetMousePosition(), b3))
        {
            return actualizar_tipo_futbol_equipo(equipo_id, FUTBOL_11);
        }
        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static void equipo_gui_set_toast_state(int *toast_ok,
                                       char *toast_msg,
                                       size_t toast_msg_size,
                                       float *toast_timer,
                                       int ok,
                                       const char *msg)
{
    if (toast_ok)
    {
        *toast_ok = ok;
    }
    if (toast_msg && toast_msg_size > 0)
    {
        snprintf(toast_msg, toast_msg_size, "%s", msg ? msg : "Operacion completada");
    }
    if (toast_timer)
    {
        *toast_timer = 1.2f;
    }
}

static void equipo_gui_accion_asignar_partido(int equipo_id,
                                               const char *partido_id_txt,
                                               int *toast_ok,
                                               char *toast_msg,
                                               size_t toast_msg_size,
                                               float *toast_timer)
{
    int partido_id = atoi(partido_id_txt ? partido_id_txt : "0");

    if (partido_id <= 0 || !existe_id("partido", partido_id))
    {
        equipo_gui_set_toast_state(toast_ok,
                                   toast_msg,
                                   toast_msg_size,
                                   toast_timer,
                                   0,
                                   "ID de partido invalido");
        return;
    }

    if (ejecutar_update_int("UPDATE equipo SET partido_id = ? WHERE id = ?", partido_id, equipo_id))
    {
        equipo_gui_set_toast_state(toast_ok,
                                   toast_msg,
                                   toast_msg_size,
                                   toast_timer,
                                   1,
                                   "Partido asignado correctamente");
        return;
    }

    equipo_gui_set_toast_state(toast_ok,
                               toast_msg,
                               toast_msg_size,
                               toast_timer,
                               0,
                               "No se pudo asignar el partido");
}

static void equipo_gui_accion_quitar_asignacion(int equipo_id,
                                                 int *toast_ok,
                                                 char *toast_msg,
                                                 size_t toast_msg_size,
                                                 float *toast_timer)
{
    if (ejecutar_update_id("UPDATE equipo SET partido_id = -1 WHERE id = ?", equipo_id))
    {
        equipo_gui_set_toast_state(toast_ok,
                                   toast_msg,
                                   toast_msg_size,
                                   toast_timer,
                                   1,
                                   "Asignacion removida");
        return;
    }

    equipo_gui_set_toast_state(toast_ok,
                               toast_msg,
                               toast_msg_size,
                               toast_timer,
                               0,
                               "No se pudo quitar la asignacion");
}

static int modal_asignacion_equipo_gui(int equipo_id)
{
    char partido_id_txt[16] = {0};
    int cursor = 0;
    float toast_timer = 0.0f;
    int toast_ok = 0;
    char toast_msg[120] = {0};

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);
    input_consume_key(KEY_R);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 760 : sw - 40;
        int panel_h = 250;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle input_rect = {(float)panel_x + 24.0f, (float)panel_y + 106.0f, 220.0f, 36.0f};
        Rectangle btn_asignar = {(float)panel_x + 280.0f, (float)panel_y + 102.0f, 180.0f, 42.0f};
        Rectangle btn_quitar = {(float)panel_x + 474.0f, (float)panel_y + 102.0f, 180.0f, 42.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        int accion_asignar = (click && CheckCollisionPointRec(GetMousePosition(), btn_asignar)) || IsKeyPressed(KEY_ENTER);
        int accion_quitar = (click && CheckCollisionPointRec(GetMousePosition(), btn_quitar)) || IsKeyPressed(KEY_R);
        int cancelar = IsKeyPressed(KEY_ESCAPE);

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header("ASIGNACION A PARTIDO", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text(TextFormat("Equipo ID %d", equipo_id),
                 (float)panel_x + 24.0f,
                 (float)panel_y + 44.0f,
                 22.0f,
                 gui_get_active_theme()->text_primary);
        gui_text("ID de partido:", (float)panel_x + 24.0f, (float)panel_y + 82.0f,
                 18.0f,
                 gui_get_active_theme()->text_secondary);

        DrawRectangleRec(input_rect, (Color){18, 36, 28, 255});
        DrawRectangleLines((int)input_rect.x,
                           (int)input_rect.y,
                           (int)input_rect.width,
                           (int)input_rect.height,
                           (Color){55, 100, 72, 255});
        gui_text(partido_id_txt, input_rect.x + 8.0f, input_rect.y + 8.0f, 18.0f,
                 (Color){233, 247, 236, 255});
        if (toast_timer <= 0.0f)
        {
            equipo_gui_draw_input_caret(partido_id_txt, input_rect);
        }

        equipo_gui_draw_action_button(btn_asignar, "Asignar", 1);
        equipo_gui_draw_action_button(btn_quitar, "Quitar", 0);

        if (toast_timer > 0.0f)
        {
            equipo_gui_draw_action_toast(sw, sh, toast_msg, toast_ok);
        }

        gui_draw_footer_hint("ENTER: asignar | R: quitar asignacion | ESC: volver", (float)panel_x, sh);
        EndDrawing();

        if (equipo_gui_tick_toast(&toast_timer) && toast_ok)
        {
            return 1;
        }
        if (toast_timer > 0.0f)
        {
            continue;
        }

        equipo_gui_append_printable_ascii(partido_id_txt, &cursor, sizeof(partido_id_txt), 1);
        equipo_gui_handle_backspace(partido_id_txt, &cursor);

        if (accion_asignar)
        {
            input_consume_key(KEY_ENTER);
            equipo_gui_accion_asignar_partido(equipo_id,
                                              partido_id_txt,
                                              &toast_ok,
                                              toast_msg,
                                              sizeof(toast_msg),
                                              &toast_timer);
        }

        if (accion_quitar)
        {
            input_consume_key(KEY_R);
            equipo_gui_accion_quitar_asignacion(equipo_id,
                                                &toast_ok,
                                                toast_msg,
                                                sizeof(toast_msg),
                                                &toast_timer);
        }

        if (cancelar)
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static int cargar_equipo_completo_para_edicion_gui(int equipo_id, Equipo *equipo_out)
{
    sqlite3_stmt *stmt = NULL;
    int jugador_idx = 0;

    if (!equipo_out)
    {
        return 0;
    }

    memset(equipo_out, 0, sizeof(*equipo_out));
    equipo_out->id = equipo_id;

    if (!preparar_stmt(&stmt,
                       "SELECT nombre, tipo, tipo_futbol, partido_id "
                       "FROM equipo WHERE id = ?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, equipo_id);
    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    snprintf(equipo_out->nombre,
             sizeof(equipo_out->nombre),
             "%s",
             (const char *)(sqlite3_column_text(stmt, 0) ? sqlite3_column_text(stmt, 0) : (const unsigned char *)"(sin nombre)"));
    equipo_out->tipo = (TipoEquipo)sqlite3_column_int(stmt, 1);
    equipo_out->tipo_futbol = (TipoFutbol)sqlite3_column_int(stmt, 2);
    equipo_out->partido_id = sqlite3_column_int(stmt, 3);
    sqlite3_finalize(stmt);

    if (!preparar_stmt(&stmt,
                       "SELECT nombre, numero, posicion, es_capitan "
                       "FROM jugador WHERE equipo_id = ? ORDER BY numero"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, equipo_id);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (jugador_idx >= 11)
        {
            sqlite3_finalize(stmt);
            return 0;
        }

        snprintf(equipo_out->jugadores[jugador_idx].nombre,
                 sizeof(equipo_out->jugadores[jugador_idx].nombre),
                 "%s",
                 (const char *)(sqlite3_column_text(stmt, 0) ? sqlite3_column_text(stmt, 0) : (const unsigned char *)"(sin nombre)"));
        equipo_out->jugadores[jugador_idx].numero = sqlite3_column_int(stmt, 1);
        equipo_out->jugadores[jugador_idx].posicion = (Posicion)sqlite3_column_int(stmt, 2);
        equipo_out->jugadores[jugador_idx].es_capitan = sqlite3_column_int(stmt, 3);
        jugador_idx++;
    }
    sqlite3_finalize(stmt);

    equipo_out->num_jugadores = jugador_idx;
    return 1;
}

static int guardar_jugadores_equipo_editado_gui(const Equipo *equipo)
{
    sqlite3_stmt *stmt = NULL;
    int ok = 1;

    if (!equipo)
    {
        return 0;
    }

    if (sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL) != SQLITE_OK)
    {
        return 0;
    }

    if (!preparar_stmt(&stmt, "DELETE FROM jugador WHERE equipo_id = ?"))
    {
        ok = 0;
    }
    if (ok)
    {
        sqlite3_bind_int(stmt, 1, equipo->id);
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            ok = 0;
        }
    }
    if (stmt)
    {
        sqlite3_finalize(stmt);
        stmt = NULL;
    }

    for (int i = 0; ok && i < equipo->num_jugadores; i++)
    {
        if (!preparar_stmt(&stmt,
                           "INSERT INTO jugador (equipo_id, nombre, numero, posicion, es_capitan) "
                           "VALUES (?, ?, ?, ?, ?)"))
        {
            ok = 0;
            continue;
        }

        sqlite3_bind_int(stmt, 1, equipo->id);
        sqlite3_bind_text(stmt, 2, equipo->jugadores[i].nombre, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, equipo->jugadores[i].numero);
        sqlite3_bind_int(stmt, 4, equipo->jugadores[i].posicion);
        sqlite3_bind_int(stmt, 5, equipo->jugadores[i].es_capitan);

        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            ok = 0;
        }

        if (stmt)
        {
            sqlite3_finalize(stmt);
            stmt = NULL;
        }
    }

    if (ok && !sincronizar_num_jugadores_equipo(equipo->id))
    {
        ok = 0;
    }

    if (ok)
    {
        asegurar_capitan_en_equipo(equipo->id);
    }

    if (ok && sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL) != SQLITE_OK)
    {
        ok = 0;
    }

    if (!ok)
    {
        if (stmt)
        {
            sqlite3_finalize(stmt);
        }
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return 0;
    }

    return 1;
}

static int editar_jugadores_equipo_existente_gui(int equipo_id)
{
    Equipo equipo = {0};

    if (!cargar_equipo_completo_para_edicion_gui(equipo_id, &equipo))
    {
        return 0;
    }

    if (!gestionar_equipo_momentaneo_gui(&equipo, "MODIFICAR JUGADORES"))
    {
        return 0;
    }

    return guardar_jugadores_equipo_editado_gui(&equipo);
}

static int modificar_equipo_gui(void)
{
    int equipo_id = 0;
    int accion = EQUIPO_GUI_EDIT_CANCELAR;

    if (!seleccionar_equipo_gui("MODIFICAR EQUIPO",
                                "Click para seleccionar equipo | ESC/Enter: volver",
                                &equipo_id))
    {
        return 1;
    }

    if (equipo_id <= 0)
    {
        return 1;
    }

    accion = modal_accion_modificar_equipo_gui(equipo_id);
    if (accion == EQUIPO_GUI_EDIT_CANCELAR)
    {
        return 1;
    }

    if (accion == EQUIPO_GUI_EDIT_NOMBRE)
    {
        if (modal_editar_nombre_equipo_gui(equipo_id))
        {
            equipo_gui_show_action_feedback("MODIFICAR EQUIPO", "Nombre actualizado correctamente", 1);
        }
        return 1;
    }

    if (accion == EQUIPO_GUI_EDIT_TIPO_FUTBOL)
    {
        if (modal_tipo_futbol_equipo_gui(equipo_id))
        {
            equipo_gui_show_action_feedback("MODIFICAR EQUIPO", "Tipo de futbol actualizado", 1);
        }
        return 1;
    }

    if (accion == EQUIPO_GUI_EDIT_ASIGNACION)
    {
        (void)modal_asignacion_equipo_gui(equipo_id);
        return 1;
    }

    if (accion == EQUIPO_GUI_EDIT_JUGADORES)
    {
        if (editar_jugadores_equipo_existente_gui(equipo_id))
        {
            equipo_gui_show_action_feedback("MODIFICAR EQUIPO", "Jugadores actualizados correctamente", 1);
        }
        else
        {
            equipo_gui_show_action_feedback("MODIFICAR EQUIPO", "No se pudieron actualizar jugadores", 0);
        }
        return 1;
    }

    return 1;
}

static int equipo_gui_obtener_nombre_por_id(int id, char *nombre_out, size_t nombre_size)
{
    sqlite3_stmt *stmt = NULL;
    int ok = 0;

    if (!nombre_out || nombre_size == 0)
    {
        return 0;
    }

    nombre_out[0] = '\0';

    if (!preparar_stmt(&stmt, "SELECT nombre FROM equipo WHERE id = ?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        snprintf(nombre_out,
                 nombre_size,
                 "%s",
                 (const char *)(sqlite3_column_text(stmt, 0) ? sqlite3_column_text(stmt, 0) : (const unsigned char *)"(sin nombre)"));
        ok = 1;
    }

    sqlite3_finalize(stmt);
    return ok;
}

static int eliminar_equipo_por_id_gui(int equipo_id)
{
    sqlite3_stmt *stmt = NULL;

    if (!preparar_stmt(&stmt, "DELETE FROM jugador WHERE equipo_id = ?"))
    {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, equipo_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (!preparar_stmt(&stmt, "DELETE FROM equipo WHERE id = ?"))
    {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, equipo_id);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);

    {
        char log_msg[128];
        snprintf(log_msg, sizeof(log_msg), "Eliminado equipo id=%d", equipo_id);
        app_log_event("EQUIPO", log_msg);
    }

    return 1;
}

static int modal_confirmar_eliminar_equipo_gui(int id, const char *nombre)
{
    input_consume_key(KEY_Y);
    input_consume_key(KEY_N);
    input_consume_key(KEY_ESCAPE);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 700 : sw - 40;
        int panel_h = 210;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle btn_confirm = {(float)panel_x + panel_w - 362.0f, (float)panel_y + panel_h - 54.0f, 168.0f, 38.0f};
        Rectangle btn_cancel = {(float)panel_x + panel_w - 184.0f, (float)panel_y + panel_h - 54.0f, 168.0f, 38.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header("CONFIRMAR ELIMINACION", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text("Confirmar eliminacion de equipo", (float)panel_x + 24.0f,
                 (float)panel_y + 44.0f,
                 24.0f,
                 gui_get_active_theme()->text_primary);
        gui_text_truncated(TextFormat("%d (%s)", id, nombre ? nombre : "(sin nombre)"),
                           (float)panel_x + 24.0f,
                           (float)panel_y + 82.0f,
                           18.0f,
                           (float)panel_w - 48.0f,
                           gui_get_active_theme()->text_secondary);
        gui_text("Usa botones o teclas ENTER/ESC", (float)panel_x + 24.0f,
                 (float)panel_y + 112.0f,
                 18.0f,
                 gui_get_active_theme()->text_secondary);

        equipo_gui_draw_action_button(btn_confirm, "Eliminar", 1);
        equipo_gui_draw_action_button(btn_cancel, "Cancelar", 0);
        gui_draw_footer_hint("Esta accion no se puede deshacer", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_confirm)) ||
            IsKeyPressed(KEY_Y) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_Y);
            input_consume_key(KEY_ENTER);
            return 1;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_cancel)) ||
            IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_N);
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static int eliminar_equipo_gui(void)
{
    int equipo_id = 0;
    char nombre[128] = {0};

    if (!seleccionar_equipo_gui("ELIMINAR EQUIPO",
                                "Click para seleccionar equipo | ESC/Enter: volver",
                                &equipo_id))
    {
        return 1;
    }

    if (equipo_id <= 0)
    {
        return 1;
    }

    if (!equipo_gui_obtener_nombre_por_id(equipo_id, nombre, sizeof(nombre)))
    {
        snprintf(nombre, sizeof(nombre), "(sin nombre)");
    }

    if (!modal_confirmar_eliminar_equipo_gui(equipo_id, nombre))
    {
        equipo_gui_show_action_feedback("ELIMINAR EQUIPO", "Eliminacion cancelada", 0);
        return 1;
    }

    if (eliminar_equipo_por_id_gui(equipo_id))
    {
        equipo_gui_show_action_feedback("ELIMINAR EQUIPO", "Equipo eliminado correctamente", 1);
    }
    else
    {
        equipo_gui_show_action_feedback("ELIMINAR EQUIPO", "No se pudo eliminar el equipo", 0);
    }

    return 1;
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
    (void)crear_equipo_gui();
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
    app_log_event("EQUIPO", "Listado de equipos consultado");
    (void)listar_equipos_gui();
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
    (void)modificar_equipo_gui();
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
    (void)eliminar_equipo_gui();
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
    equipo_sleep_ms(3000); // Esperar 3 segundos antes de comenzar
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
    equipo_sleep_ms(1000);
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
 * @brief Simula un partido entre dos equipos
 *
 * Esta funcion simula un partido de futbol de 60 minutos entre dos equipos momentaneos.
 * Muestra la cancha y los jugadores de ambos equipos, genera eventos aleatorios
 * como goles y asistencias, y muestra el marcador en tiempo real.
 *
 * @param equipo_local Puntero al equipo local
 * @param equipo_visitante Puntero al equipo visitante
 */
void simular_partido(const Equipo *equipo_local, const Equipo *equipo_visitante)
{
    clear_screen();
    printf("========================================\n");
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
        {0, "Volver", NULL}
    };

    ejecutar_menu_estandar("EQUIPOS", items, 5);
}
