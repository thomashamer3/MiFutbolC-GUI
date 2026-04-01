#include "torneo.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "equipo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite3.h"


/**
 * @file torneo.c
 * @brief Implementacion de funciones para la gestion de torneos en MiFutbolC
 */

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, 0) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

// Forward declaration to fix implicit function declaration
void generar_fixture(int torneo_id);
void gestionar_tablas_goleadores_asistidores();
void listar_tablas_goleadores_asistidores(int torneo_id);
void agregar_registro_goleador_asistidor(int torneo_id);
void eliminar_registro_goleador_asistidor(int torneo_id);
void modificar_registro_goleador_asistidor(int torneo_id);

// Prototipos estaticos para funciones usadas antes de su definicion
static void listar_equipos_asociados(int torneo_id);
static void actualizar_nombre_torneo(int torneo_id);
static void actualizar_equipo_fijo(int torneo_id);
static void actualizar_tipo_formato_torneo(int torneo_id, int cantidad);

// Stubs para funciones no implementadas pero usadas
static void actualizar_cantidad_equipos(int torneo_id)
{
    (void)torneo_id;
    printf("Actualizar cantidad de equipos no completamente implementado.\n");
}

static void aplicar_actualizacion_formato(int torneo_id, int tipo, int formato)
{
    (void)torneo_id;
    (void)tipo;
    (void)formato;
    printf("Aplicar actualizacion de formato no completamente implementado.\n");
}

/**
 * Traduce valores enumerados de tipos de torneo a nombres legibles para la interfaz de usuario,
 * facilitando la comprension de las opciones disponibles.
 */
const char* get_nombre_tipo_torneo(TipoTorneos tipo)
{
    switch (tipo)
    {
    case IDA_Y_VUELTA:
        return "Ida y Vuelta";
    case SOLO_IDA:
        return "Solo Ida";
    case ELIMINACION_DIRECTA:
        return "Eliminacion Directa";
    case GRUPOS_Y_ELIMINACION:
        return "Grupos y Eliminacion";
    default:
        return "Desconocido";
    }
}

/**
 * @brief Convierte un formato de torneo enumerado a su nombre textual
 *
 * Esta funcion toma un valor del enum FormatoTorneos y devuelve la cadena
 * correspondiente en espanol para mostrar al usuario.
 *
 * @param formato El valor enumerado del formato de torneo
 * @return Cadena constante con el nombre del formato de torneo, o "Desconocido" si no es valido
 */
const char* get_nombre_formato_torneo(FormatoTorneos formato)
{
    switch (formato)
    {
    case ROUND_ROBIN:
        return "Round-robin (sistema liga)";
    case MINI_GRUPO_CON_FINAL:
        return "Mini grupo con final";
    case LIGA_SIMPLE:
        return "Liga simple";
    case LIGA_DOBLE:
        return "Liga doble";
    case GRUPOS_CON_FINAL:
        return "Grupos + final";
    case COPA_SIMPLE:
        return "Copa simple";
    case GRUPOS_ELIMINACION:
        return "Grupos + eliminacion";
    case COPA_REPECHAJE:
        return "Copa + repechaje";
    case LIGA_GRANDE:
        return "Liga grande";
    case MULTIPLES_GRUPOS:
        return "Multiples grupos";
    case ELIMINACION_FASES:
        return "Eliminacion directa por fases";
    default:
        return "Desconocido";
    }
}

/**
 * @brief Funcion generica para listar torneos
 */
static int listar_torneos_generico(const char *no_records_msg)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre FROM torneo ORDER BY id;";

    if (!preparar_stmt(sql, &stmt))
    {
        printf("Error al obtener la lista de torneos: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    ui_printf_centered_line("=== TORNEOS DISPONIBLES ===");
    ui_printf("\n");

    int found = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = 1;
        int id = sqlite3_column_int(stmt, 0);
        const char *nombre = (const char*)sqlite3_column_text(stmt, 1);
        ui_printf_centered_line("%d. %s", id, nombre);
    }

    sqlite3_finalize(stmt);

    if (!found)
    {
        mostrar_no_hay_registros(no_records_msg);
        return 0;
    }

    return 1;
}

/**
 * @brief Funcion generica para obtener formato segun cantidad de equipos y opcion
 */
static void obtener_formato_por_cantidad(int opcion, int cantidad, TipoTorneos *tipo, FormatoTorneos *formato)
{
    *tipo = SOLO_IDA;
    *formato = LIGA_SIMPLE;

    if (cantidad >= 4 && cantidad <= 6)
    {
        if (opcion == 1)
        {
            *formato = ROUND_ROBIN;
            *tipo = IDA_Y_VUELTA;
        }
        else if (opcion == 2)
        {
            *formato = MINI_GRUPO_CON_FINAL;
            *tipo = GRUPOS_Y_ELIMINACION;
        }
    }
    else if (cantidad >= 7 && cantidad <= 12)
    {
        switch (opcion)
        {
        case 2:
            *formato = LIGA_DOBLE;
            *tipo = IDA_Y_VUELTA;
            break;
        case 3:
            *formato = GRUPOS_CON_FINAL;
            *tipo = GRUPOS_Y_ELIMINACION;
            break;
        case 4:
            *formato = COPA_SIMPLE;
            *tipo = ELIMINACION_DIRECTA;
            break;
        default:
            break;
        }
    }
    else if (cantidad >= 13 && cantidad <= 20)
    {
        *tipo = GRUPOS_Y_ELIMINACION;
        *formato = GRUPOS_ELIMINACION;
        switch (opcion)
        {
        case 2:
            *formato = COPA_REPECHAJE;
            *tipo = ELIMINACION_DIRECTA;
            break;
        case 3:
            *formato = LIGA_GRANDE;
            *tipo = IDA_Y_VUELTA;
            break;
        default:
            break;
        }
    }
    else if (cantidad >= 21)
    {
        *tipo = GRUPOS_Y_ELIMINACION;
        *formato = MULTIPLES_GRUPOS;
        if (opcion == 2)
        {
            *formato = ELIMINACION_FASES;
            *tipo = ELIMINACION_DIRECTA;
        }
    }
}

/**
 * Muestra informacion completa de torneo para confirmacion del usuario.
 * Necesario porque la estructura interna no es legible para humanos.
 */
void mostrar_torneo(Torneo *torneo)
{
    ui_printf_centered_line("=== INFORMACION DEL TORNEO ===");
    ui_printf_centered_line("Nombre: %s", torneo->nombre);
    ui_printf_centered_line("Tiene equipo fijo: %s", torneo->tiene_equipo_fijo ? "Si" : "No");
    if (torneo->tiene_equipo_fijo)
    {
        ui_printf_centered_line("Equipo fijo ID: %d", torneo->equipo_fijo_id);
    }
    ui_printf_centered_line("Cantidad de equipos: %d", torneo->cantidad_equipos);
    ui_printf_centered_line("Tipo de torneo: %s", get_nombre_tipo_torneo(torneo->tipo_torneo));
    ui_printf_centered_line("Formato de torneo: %s", get_nombre_formato_torneo(torneo->formato_torneo));
    ui_printf("\n");
}

/**
 * @brief Agrega un equipo al torneo solo por nombre (sin crear equipo completo)
 */
static void agregar_equipo_nombre_torneo(int torneo_id)
{
    char nombre[50] = {0};
    input_string("Nombre del equipo: ", nombre, sizeof(nombre));
    if (nombre[0] == '\0')
    {
        printf("El nombre no puede estar vacio.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    const char *sql_check = "SELECT COUNT(*) FROM equipo_torneo_nombre WHERE torneo_id = ? AND nombre = ?;";
    if (preparar_stmt(sql_check, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0)
        {
            printf("Ya existe un equipo con ese nombre en el torneo.\n");
            sqlite3_finalize(stmt);
            pause_console();
            return;
        }
        sqlite3_finalize(stmt);
    }

    const char *sql_insert = "INSERT INTO equipo_torneo_nombre (torneo_id, nombre) VALUES (?, ?);";
    if (preparar_stmt(sql_insert, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE)
            printf("Equipo '%s' agregado al torneo.\n", nombre);
        else
            printf("Error al agregar equipo: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
    }
    pause_console();
}

/**
 * @brief Agrega varios equipos por nombre de una vez
 */
static void agregar_equipos_nombres_torneo(int torneo_id)
{
    clear_screen();
    print_header("AGREGAR EQUIPOS POR NOMBRE");
    printf("Escriba los nombres de los equipos (uno por linea, linea vacia para terminar):\n\n");

    int count = 0;
    for (;;)
    {
        char nombre[50] = {0};
        printf("Equipo %d (Enter para terminar): ", count + 1);
        if (fgets(nombre, sizeof(nombre), stdin))
        {
            // trim newline
            size_t len = strlen_s(nombre, sizeof(nombre));
            while (len > 0 && (nombre[len-1] == '\n' || nombre[len-1] == '\r'))
                nombre[--len] = '\0';
        }
        if (nombre[0] == '\0') break;

        sqlite3_stmt *stmt;
        const char *sql = "INSERT INTO equipo_torneo_nombre (torneo_id, nombre) VALUES (?, ?);";
        if (preparar_stmt(sql, &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_DONE)
                count++;
            else
                printf("Error al agregar '%s': %s\n", nombre, sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
        }
    }
    printf("\n%d equipo(s) agregado(s) al torneo.\n", count);
    pause_console();
}

/**
 * @brief Asocia equipos a un torneo
 */
void asociar_equipos_torneo(int torneo_id)
{
    clear_screen();
    print_header("ASOCIAR EQUIPOS A TORNEO");
    sqlite3_stmt *stmt;
    int equipo_id = select_team_id("\nIngrese el ID del equipo a asociar (0 para cancelar): ",
                                   "equipos registrados para asociar", 1);
    if (equipo_id == 0)
    {
        return;
    }

    // Verificar si ya esta asociado
    const char *sql_check = "SELECT COUNT(*) FROM equipo_torneo WHERE torneo_id = ? AND equipo_id = ?;";
    if (preparar_stmt(sql_check, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, equipo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int count = sqlite3_column_int(stmt, 0);
            if (count > 0)
            {
                printf("Este equipo ya esta asociado al torneo.\n");
                sqlite3_finalize(stmt);
                pause_console();
                return;
            }
        }
        sqlite3_finalize(stmt);
    }

    // Asociar equipo a torneo
    const char *sql_insert = "INSERT INTO equipo_torneo (torneo_id, equipo_id) VALUES (?, ?);";
    if (preparar_stmt(sql_insert, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);
        sqlite3_bind_int(stmt, 2, equipo_id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Equipo asociado al torneo exitosamente.\n");
        }
        else
        {
            printf("Error al asociar equipo al torneo: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }

    pause_console();
}

/**
 * @brief Crea un equipo fijo para un torneo
 *
 * Esta funcion crea un nuevo equipo y lo asocia como equipo fijo a un torneo.
 * Si torneo_id es -1, solo crea el equipo sin asociarlo.
 *
 * @param torneo_id ID del torneo al que se asociara el equipo fijo, o -1 para solo crear el equipo
 */
void crear_equipo_fijo_torneo(int torneo_id)
{
    crear_equipo();

    sqlite3_stmt *stmt;
    const char *sql = "SELECT last_insert_rowid();";

    int equipo_id = -1;
    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            equipo_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (equipo_id == -1)
    {
        printf("No se pudo obtener el ID del equipo creado.\n");
        pause_console();
        return;
    }

    if (torneo_id != -1)
    {
        const char *sql_insert = "INSERT INTO equipo_torneo (torneo_id, equipo_id) VALUES (?, ?);";
        if (preparar_stmt(sql_insert, &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, equipo_id);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                printf("Equipo fijo creado y asociado al torneo exitosamente.\n");
            }
            else
            {
                printf("Error al asociar equipo al torneo: %s\n", sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);
        }
    }
    else
    {
        printf("Equipo fijo creado exitosamente. ID: %d\n", equipo_id);
    }

    pause_console();
}

/**
 * Solicita al usuario los datos basicos del torneo (nombre, equipo fijo).
 * Maneja la creacion de equipo fijo si es necesario.
 * Retorna 0 si se debe cancelar la creacion, 1 si continua.
 */
static int input_torneo_data(Torneo *torneo)
{
    input_string("Ingrese el nombre del torneo: ", torneo->nombre, sizeof(torneo->nombre));

    torneo->tiene_equipo_fijo = confirmar("El torneo tiene equipo fijo?");

    if (torneo->tiene_equipo_fijo)
    {
        listar_equipos();
        int equipo_id = input_int("\nIngrese el ID del equipo fijo (0 para crear nuevo equipo): ");

        if (equipo_id == 0)
        {
            crear_equipo_fijo_torneo(-1);
            return 0;
        }
        else if (existe_id("equipo", equipo_id))
        {
            torneo->equipo_fijo_id = equipo_id;
        }
        else
        {
            printf("ID de equipo invalido.\n");
            pause_console();
            return 0;
        }
    }

    torneo->cantidad_equipos = input_int("Ingrese la cantidad de equipos en el torneo: ");
    return 1;
}

/**
 * Determina el formato y tipo de torneo basado en la cantidad de equipos.
 * Utiliza logica de rangos para simplificar la seleccion automatica.
 */
static void determine_formato_torneo(Torneo *torneo)
{
    int cantidad = torneo->cantidad_equipos;
    int opcion = 0;

    if (cantidad >= 4 && cantidad <= 6)
    {
        printf("\nPara 4-6 equipos, seleccione el formato:\n");
        printf("1. Round-robin (sistema liga)\n");
        printf("2. Mini grupo con final\n");
        opcion = input_int(">");
    }
    else if (cantidad >= 7 && cantidad <= 12)
    {
        printf("\nPara 7-12 equipos, seleccione el formato:\n");
        printf("1. Liga simple\n");
        printf("2. Liga doble\n");
        printf("3. Grupos + final\n");
        printf("4. Copa simple\n");
        opcion = input_int(">");
    }
    else if (cantidad >= 13 && cantidad <= 20)
    {
        printf("\nPara 13-20 equipos, seleccione el formato:\n");
        printf("1. Grupos (4-5 grupos) + eliminacion\n");
        printf("2. Copa + repechaje\n");
        printf("3. Liga grande\n");
        opcion = input_int(">");
    }
    else if (cantidad >= 21)
    {
        printf("\nPara 21 o mas equipos, seleccione el formato:\n");
        printf("1. Multiples grupos\n");
        printf("2. Eliminacion directa por fases\n");
        opcion = input_int(">");
    }
    else
    {
        printf("Cantidad de equipos no valida. Se seleccionara formato por defecto.\n");
        torneo->formato_torneo = ROUND_ROBIN;
        torneo->tipo_torneo = IDA_Y_VUELTA;
        return;
    }

    obtener_formato_por_cantidad(opcion, cantidad, &torneo->tipo_torneo, &torneo->formato_torneo);
}

/**
 * Guarda el torneo en la base de datos y maneja asociaciones iniciales.
 * Retorna el ID del torneo creado o -1 si hay error.
 */
static int save_torneo_to_db(Torneo const *torneo)
{
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO torneo (nombre, tiene_equipo_fijo, equipo_fijo_id, cantidad_equipos, tipo_torneo, formato_torneo) VALUES (?, ?, ?, ?, ?, ?);";

    if (!preparar_stmt(sql, &stmt))
    {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, torneo->nombre, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, torneo->tiene_equipo_fijo);
    sqlite3_bind_int(stmt, 3, torneo->equipo_fijo_id);
    sqlite3_bind_int(stmt, 4, torneo->cantidad_equipos);
    sqlite3_bind_int(stmt, 5, torneo->tipo_torneo);
    sqlite3_bind_int(stmt, 6, torneo->formato_torneo);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        printf("Error al guardar el torneo: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    int torneo_id = (int)sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);

    if (torneo->tiene_equipo_fijo && torneo->equipo_fijo_id != -1)
    {
        const char *sql_asociar = "INSERT INTO equipo_torneo (torneo_id, equipo_id) VALUES (?, ?);";
        if (preparar_stmt(sql_asociar, &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_bind_int(stmt, 2, torneo->equipo_fijo_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    return torneo_id;
}

void crear_torneo()
{
    clear_screen();
    print_header("CREAR TORNEO");

    Torneo torneo = {0};
    torneo.tiene_equipo_fijo = 0;
    torneo.equipo_fijo_id = -1;

    if (!input_torneo_data(&torneo))
        return;

    determine_formato_torneo(&torneo);

    clear_screen();
    mostrar_torneo(&torneo);

    int torneo_id = save_torneo_to_db(&torneo);
    if (torneo_id == -1)
        return;

    printf("Torneo guardado exitosamente con ID: %d\n", torneo_id);

    printf("\nAgregar equipos al torneo:\n");
    printf("1. Agregar equipos por nombre (solo nombres para llaves)\n");
    printf("2. Asociar equipos guardados\n");
    printf("0. Continuar sin agregar\n");
    int opc_eq = input_int(">");
    if (opc_eq == 1)
        agregar_equipos_nombres_torneo(torneo_id);
    else if (opc_eq == 2)
        asociar_equipos_torneo(torneo_id);

    pause_console();
}

void listar_torneos()
{
    clear_screen();
    print_header("LISTAR TORNEOS");

    if (!listar_torneos_generico("torneos registrados"))
    {
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre, tiene_equipo_fijo, equipo_fijo_id, cantidad_equipos, tipo_torneo, formato_torneo FROM torneo ORDER BY id;";

    if (preparar_stmt(sql, &stmt))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            Torneo torneo;
            torneo.id = sqlite3_column_int(stmt, 0);
            strncpy_s(torneo.nombre, sizeof(torneo.nombre), (const char*)sqlite3_column_text(stmt, 1), sizeof(torneo.nombre));
            torneo.tiene_equipo_fijo = sqlite3_column_int(stmt, 2);
            torneo.equipo_fijo_id = sqlite3_column_int(stmt, 3);
            torneo.cantidad_equipos = sqlite3_column_int(stmt, 4);
            torneo.tipo_torneo = sqlite3_column_int(stmt, 5);
            torneo.formato_torneo = sqlite3_column_int(stmt, 6);

            mostrar_torneo(&torneo);
            listar_equipos_asociados(torneo.id);

            printf("----------------------------------------\n");
        }
    }
    else
    {
        printf("Error al obtener la lista de torneos: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    pause_console();
}

void modificar_torneo()
{
    clear_screen();
    print_header("MODIFICAR TORNEO");

    if (!listar_torneos_generico("torneos registrados para modificar"))
    {
        pause_console();
        return;
    }

    int torneo_id = input_int("\nIngrese el ID del torneo a modificar (0 para cancelar): ");

    if (torneo_id == 0) return;

    if (!existe_id("torneo", torneo_id))
    {
        printf("ID de torneo invalido.\n");
        pause_console();
        return;
    }

    Torneo torneo = {0};
    sqlite3_stmt *stmt;
    const char *sql_torneo = "SELECT nombre, tiene_equipo_fijo, equipo_fijo_id, cantidad_equipos, tipo_torneo, formato_torneo FROM torneo WHERE id = ?;";

    if (preparar_stmt(sql_torneo, &stmt))
    {
        sqlite3_bind_int(stmt, 1, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            strncpy_s(torneo.nombre, sizeof(torneo.nombre), (const char*)sqlite3_column_text(stmt, 0), sizeof(torneo.nombre));
            torneo.tiene_equipo_fijo = sqlite3_column_int(stmt, 1);
            torneo.equipo_fijo_id = sqlite3_column_int(stmt, 2);
            torneo.cantidad_equipos = sqlite3_column_int(stmt, 3);
            torneo.tipo_torneo = sqlite3_column_int(stmt, 4);
            torneo.formato_torneo = sqlite3_column_int(stmt, 5);
        }
        sqlite3_finalize(stmt);
    }

    printf("\nSeleccione que desea modificar:\n");
    printf("1. Nombre del torneo\n");
    printf("2. Equipo fijo\n");
    printf("3. Cantidad de equipos\n");
    printf("4. Tipo y formato de torneo\n");
    printf("5. Asociar equipos guardados\n");
    printf("6. Agregar equipos por nombre\n");
    printf("7. Agregar un equipo por nombre\n");
    printf("8. Volver\n");

    int opcion = input_int(">");

    switch (opcion)
    {
    case 1:
        actualizar_nombre_torneo(torneo_id);
        break;
    case 2:
        actualizar_equipo_fijo(torneo_id);
        break;
    case 3:
        actualizar_cantidad_equipos(torneo_id);
        break;
    case 4:
        actualizar_tipo_formato_torneo(torneo_id, torneo.cantidad_equipos);
        break;
    case 5:
        asociar_equipos_torneo(torneo_id);
        break;
    case 6:
        agregar_equipos_nombres_torneo(torneo_id);
        break;
    case 7:
        agregar_equipo_nombre_torneo(torneo_id);
        break;
    case 8:
        break;
    default:
        printf("Opcion invalida.\n");
    }

    pause_console();
}

void eliminar_torneo()
{
    clear_screen();
    print_header("ELIMINAR TORNEO");

    if (!listar_torneos_generico("torneos registrados para eliminar"))
    {
        pause_console();
        return;
    }

    int torneo_id = input_int("\nIngrese el ID del torneo a eliminar (0 para cancelar): ");

    if (torneo_id == 0) return;

    if (!existe_id("torneo", torneo_id))
    {
        printf("ID de torneo invalido.\n");
        pause_console();
        return;
    }

    if (!confirmar("Esta seguro de eliminar este torneo? Se eliminaran datos asociados."))
    {
        printf("Eliminacion cancelada.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    const char *sqls[] =
    {
        "DELETE FROM equipo_fase WHERE torneo_id = ?;",
        "DELETE FROM torneo_fases WHERE torneo_id = ?;",
        "DELETE FROM jugador_estadisticas WHERE torneo_id = ?;",
        "DELETE FROM equipo_torneo_estadisticas WHERE torneo_id = ?;",
        "DELETE FROM partido_torneo WHERE torneo_id = ?;",
        "DELETE FROM equipo_torneo WHERE torneo_id = ?;",
        "DELETE FROM equipo_torneo_nombre WHERE torneo_id = ?;",
        "DELETE FROM equipo_historial WHERE torneo_id = ?;",
        "DELETE FROM torneo_temporada WHERE torneo_id = ?;",
        "DELETE FROM torneo WHERE id = ?;",
        NULL
    };

    for (int i = 0; sqls[i] != NULL; i++)
    {
        if (preparar_stmt(sqls[i], &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    printf("Torneo eliminado exitosamente.\n");
    pause_console();
}

/**
 * @brief Actualiza el nombre del torneo en la base de datos
 */
static void actualizar_nombre_torneo(int torneo_id)
{
    char nuevo_nombre[50];
    input_string("Ingrese el nuevo nombre: ", nuevo_nombre, sizeof(nuevo_nombre));

    sqlite3_stmt *stmt;
    const char *sql_update = "UPDATE torneo SET nombre = ? WHERE id = ?;";
    if (preparar_stmt(sql_update, &stmt))
    {
        sqlite3_bind_text(stmt, 1, nuevo_nombre, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Nombre actualizado exitosamente.\n");
        }
        else
        {
            printf("Error al actualizar el nombre: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Muestra lista de equipos disponibles y retorna ID seleccionado
 * @return ID del equipo seleccionado, o 0 si se cancela
 */
static int seleccionar_equipo_disponible(void)
{
    return select_team_id("\nIngrese el ID del equipo fijo (0 para cancelar): ",
                          "equipos registrados", 1);
}

/**
 * @brief Actualiza el equipo fijo del torneo
 */
static void actualizar_equipo_fijo(int torneo_id)
{
    int nuevo_tiene_equipo_fijo = confirmar("El torneo tiene equipo fijo?");

    if (!nuevo_tiene_equipo_fijo)
    {
        sqlite3_stmt *stmt;
        const char *sql_update = "UPDATE torneo SET tiene_equipo_fijo = 0, equipo_fijo_id = -1 WHERE id = ?;";
        if (preparar_stmt(sql_update, &stmt))
        {
            sqlite3_bind_int(stmt, 1, torneo_id);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                printf("Equipo fijo removido exitosamente.\n");
            }
            else
            {
                printf("Error al remover el equipo fijo: %s\n", sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);
        }
        return;
    }

    int equipo_id = seleccionar_equipo_disponible();
    if (equipo_id == 0) return;

    sqlite3_stmt *stmt;
    const char *sql_update = "UPDATE torneo SET tiene_equipo_fijo = ?, equipo_fijo_id = ? WHERE id = ?;";
    if (preparar_stmt(sql_update, &stmt))
    {
        sqlite3_bind_int(stmt, 1, 1);
        sqlite3_bind_int(stmt, 2, equipo_id);
        sqlite3_bind_int(stmt, 3, torneo_id);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Equipo fijo actualizado exitosamente.\n");
        }
        else
        {
            printf("Error al actualizar el equipo fijo: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Maneja la actualizacion de tipo y formato de torneo
 */
static void actualizar_tipo_formato_torneo(int torneo_id, int cantidad)
{
    int opcion = 0;

    if (cantidad >= 4 && cantidad <= 6)
    {
        printf("\nPara 4-6 equipos, seleccione el formato:\n");
        printf("1. Round-robin (sistema liga)\n");
        printf("2. Mini grupo con final\n");
        opcion = input_int(">");
    }
    else if (cantidad >= 7 && cantidad <= 12)
    {
        printf("\nPara 7-12 equipos, seleccione el formato:\n");
        printf("1. Liga simple\n");
        printf("2. Liga doble\n");
        printf("3. Grupos + final\n");
        printf("4. Copa simple\n");
        opcion = input_int(">");
    }
    else if (cantidad >= 13 && cantidad <= 20)
    {
        printf("\nPara 13-20 equipos, seleccione el formato:\n");
        printf("1. Grupos (4-5 grupos) + eliminacion\n");
        printf("2. Copa + repechaje\n");
        printf("3. Liga grande\n");
        opcion = input_int(">");
    }
    else if (cantidad >= 21)
    {
        printf("\nPara 21 o mas equipos, seleccione el formato:\n");
        printf("1. Multiples grupos\n");
        printf("2. Eliminacion directa por fases\n");
        opcion = input_int(">");
    }
    else
    {
        printf("Cantidad de equipos no valida. No se actualizara el formato.\n");
        return;
    }

    TipoTorneos tipo;
    FormatoTorneos formato;
    obtener_formato_por_cantidad(opcion, cantidad, &tipo, &formato);
    aplicar_actualizacion_formato(torneo_id, tipo, formato);
}

/**
 * @brief Muestra los equipos asociados a un torneo
 */
static void listar_equipos_asociados(int torneo_id)
{
    printf("=== EQUIPOS ASOCIADOS ===\n");
    int has_equipos = 0;
    int count = 1;

    // Equipos guardados (de tabla equipo)
    sqlite3_stmt *stmt_equipos;
    const char *sql_equipos = "SELECT e.id, e.nombre FROM equipo e JOIN equipo_torneo et ON e.id = et.equipo_id WHERE et.torneo_id = ? ORDER BY e.id;";
    if (preparar_stmt(sql_equipos, &stmt_equipos))
    {
        sqlite3_bind_int(stmt_equipos, 1, torneo_id);
        while (sqlite3_step(stmt_equipos) == SQLITE_ROW)
        {
            has_equipos = 1;
            const char *equipo_nombre = (const char*)sqlite3_column_text(stmt_equipos, 1);
            printf("%d. %s\n", count++, equipo_nombre);
        }
        sqlite3_finalize(stmt_equipos);
    }

    // Equipos por nombre (solo nombre, sin equipo completo)
    sqlite3_stmt *stmt_nombres;
    const char *sql_nombres = "SELECT nombre FROM equipo_torneo_nombre WHERE torneo_id = ? ORDER BY id;";
    if (preparar_stmt(sql_nombres, &stmt_nombres))
    {
        sqlite3_bind_int(stmt_nombres, 1, torneo_id);
        while (sqlite3_step(stmt_nombres) == SQLITE_ROW)
        {
            has_equipos = 1;
            const char *nombre = (const char*)sqlite3_column_text(stmt_nombres, 0);
            printf("%d. %s\n", count++, nombre);
        }
        sqlite3_finalize(stmt_nombres);
    }

    if (!has_equipos)
    {
        mostrar_no_hay_registros("equipos asociados a este torneo");
    }
}
/**
 * @brief Obtiene el nombre de un equipo por su ID
 * @param equipo_id ID del equipo
 * @return Nombre del equipo o "Desconocido"
 */
const char* get_equipo_nombre(int equipo_id)
{
    static char nombre[50];
    nombre[0] = '\0';

    sqlite3_stmt *stmt;
    const char *sql = "SELECT nombre FROM equipo WHERE id = ?;";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, equipo_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const unsigned char *text = sqlite3_column_text(stmt, 0);
            if (text)
            {
                strncpy_s(nombre, sizeof(nombre), (const char *)text, sizeof(nombre) - 1);
            }
            else
            {
                strcpy_s(nombre, sizeof(nombre), "Desconocido");
            }
        }
        else
        {
            strcpy_s(nombre, sizeof(nombre), "Desconocido");
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        strcpy_s(nombre, sizeof(nombre), "Desconocido");
    }

    return nombre;
}

/**
 * @brief Menu principal de gestion de torneos
 */
void menu_torneos()
{
    MenuItem items[] =
    {
        {1, "Crear Torneo", crear_torneo},
        {2, "Listar Torneos", listar_torneos},
        {3, "Modificar Torneo", modificar_torneo},
        {4, "Eliminar Torneo", eliminar_torneo},
        {0, "Volver", NULL}
    };

    ejecutar_menu_estandar("TORNEOS", items, 5);
}
