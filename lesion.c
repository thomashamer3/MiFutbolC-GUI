#include "lesion.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include "estadisticas_lesiones.h"
#include "camiseta.h"
#include "partido.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


static int current_lesion_id;

/**
 * @brief Preparar statement y reportar errores
 */
static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, NULL) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

/**
 * @brief Obtener estado por opcion
 */
static const char *estado_por_opcion(int opcion)
{
    switch (opcion)
    {
    case 1:
        return "ACTIVA";
    case 2:
        return "EN TRATAMIENTO";
    case 3:
        return "MEJORANDO";
    case 4:
        return "RECUPERADO";
    case 5:
        return "RECAIDA";
    default:
        return NULL;
    }
}

/**
 * @brief Ejecutar update simple con texto
 */
static void ejecutar_update_texto(const char *sql, const char *valor, int id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }
    sqlite3_bind_text(stmt, 1, valor, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/**
 * @brief Ejecutar update simple con entero
 */
static void ejecutar_update_int(const char *sql, int valor, int id)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, valor);
    sqlite3_bind_int(stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static void solicitar_texto_no_vacio(const char *prompt, char *buffer, int size)
{
    while (1)
    {
        input_string(prompt, buffer, size);
        if (safe_strnlen(buffer, (size_t)size) > 0)
            return;
        printf("El campo no puede estar vacio.\n");
    }
}

static const char *solicitar_estado_lesion(const char *prompt)
{
    while (1)
    {
        int opcion_estado = input_int(prompt);
        const char *estado_sel = estado_por_opcion(opcion_estado);
        if (estado_sel)
            return estado_sel;
        printf("Opcion invalida. Intente nuevamente.\n");
    }
}

/**
 * @brief Solicita al usuario el ID de un partido
 * @param permitir_omitir Si es true, permite ingresar 0 para omitir
 * @return ID del partido seleccionado (0 si se omite o cancela)
 */
static int solicitar_partido_id(int permitir_omitir)
{
    listar_partidos();

    const char *mensaje = permitir_omitir ?
                          "\nID del Partido (0 para omitir): " :
                          "\nNuevo ID del Partido (0 para quitar asociacion): ";

    int partido_id;
    int partido_valido = 0;

    while (!partido_valido)
    {
        partido_id = input_int(mensaje);

        if (partido_id == 0)
        {
            return 0;
        }

        if (existe_id("partido", partido_id))
        {
            partido_valido = 1;
        }
        else
        {
            printf("El partido no existe. Intente nuevamente.\n");
        }
    }

    return partido_id;
}

/**
 * @brief Crea una nueva lesion en la base de datos
 *
 * Solicita al usuario el tipo, descripcion de la lesion, el ID de la camiseta asociada
 * y el estado inicial, y la inserta en la tabla 'lesion'. El nombre del jugador se obtiene del usuario actual.
 * Utiliza el ID mas pequeno disponible para reutilizar IDs eliminados.
 */
void crear_lesion()
{
    clear_screen();
    char tipo[100];
    char descripcion[200];
    char fecha[20];
    char estado[50];
    int camiseta_id;
    int partido_id = 0;

    solicitar_texto_no_vacio("Tipo de lesion: ", tipo, sizeof(tipo));
    solicitar_texto_no_vacio("Descripcion: ", descripcion, sizeof(descripcion));
    while (1)
    {
        listar_camisetas();
        camiseta_id = input_int("ID de la Camiseta Asociada: ");
        if (existe_id("camiseta", camiseta_id))
            break;
        printf("La camiseta no existe. Intente nuevamente.\n");
    }

    // Asociar a un partido (opcional)
    printf("\nDesea asociar esta lesion a un partido? (S/N): ");
    char respuesta[10];
    input_string("", respuesta, sizeof(respuesta));
    if (respuesta[0] == 'S' || respuesta[0] == 's')
    {
        partido_id = solicitar_partido_id(1);
    }

    // Mostrar opciones de estado
    printf("\nEstados disponibles:\n");
    printf("1. ACTIVA - Lesion reciente, jugador NO apto\n");
    printf("2. EN TRATAMIENTO - Esta en rehabilitacion\n");
    printf("3. MEJORANDO - Evolucion positiva\n");
    printf("4. RECUPERADO - Alta medica\n");
    printf("5. RECAIDA - Vuelve la lesion\n");

    const char *estado_sel = solicitar_estado_lesion("Seleccione estado inicial (1-5): ");
    strcpy_s(estado, sizeof(estado), estado_sel);

    get_datetime(fecha, sizeof(fecha));

    char *jugador = get_user_name();
    if (!jugador)
    {
        jugador = "Usuario Desconocido";
    }

    long long id = obtener_siguiente_id("lesion");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "INSERT INTO lesion(id, jugador, tipo, descripcion, fecha, camiseta_id, estado, partido_id) VALUES(?,?,?,?,?,?,?,?)",
                &stmt))
    {
        if (strcmp(jugador, "Usuario Desconocido") != 0)
        {
            free(jugador);
        }
        return;
    }
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, jugador, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, tipo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, descripcion, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, camiseta_id);
    sqlite3_bind_text(stmt, 7, estado, -1, SQLITE_TRANSIENT);
    if (partido_id > 0)
    {
        sqlite3_bind_int(stmt, 8, partido_id);
    }
    else
    {
        sqlite3_bind_null(stmt, 8);
    }
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (strcmp(jugador, "Usuario Desconocido") != 0)
    {
        free(jugador);
    }

    printf("\nLesion creada correctamente con estado: %s\n", estado);
    if (partido_id > 0)
    {
        printf("Asociada al partido ID: %d\n", partido_id);
    }
    pause_console();
}

/**
 * @brief Muestra un listado de todas las lesiones registradas
 *
 * Consulta la base de datos y muestra en pantalla todas las lesiones
 * con sus respectivos datos: ID, tipo, descripcion, fecha y estado.
 * Si no hay lesiones registradas, muestra un mensaje informativo.
 */
void listar_lesiones()
{
    mostrar_pantalla("LISTADO DE LESIONES");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "SELECT l.id, l.jugador, l.tipo, l.descripcion, l.fecha, l.camiseta_id, l.estado, l.partido_id, "
                "c.nombre, p.fecha_hora, can.nombre "
                "FROM lesion l "
                "LEFT JOIN camiseta c ON l.camiseta_id = c.id "
                "LEFT JOIN partido p ON l.partido_id = p.id "
                "LEFT JOIN cancha can ON p.cancha_id = can.id "
                "ORDER BY l.id ASC",
                &stmt))
    {
        pause_console();
        return;
    }

    int hay = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *estado = (const char *)sqlite3_column_text(stmt, 6);
        const char *estado_display = estado ? estado : "ACTIVA";
        const char *camiseta_nombre = (const char *)sqlite3_column_text(stmt, 8);
        const char *cancha_nombre = (const char *)sqlite3_column_text(stmt, 10);
        const char *camiseta_display = camiseta_nombre ? camiseta_nombre : "Sin camiseta";
        const char *cancha_display = cancha_nombre ? cancha_nombre : "Sin cancha";

        ui_printf_centered_line("ID: %d", sqlite3_column_int(stmt, 0));
        ui_printf_centered_line("Jugador: %s", sqlite3_column_text(stmt, 1));
        ui_printf_centered_line("Tipo: %s", sqlite3_column_text(stmt, 2));
        ui_printf_centered_line("Descripcion: %s", sqlite3_column_text(stmt, 3));
        ui_printf_centered_line("Fecha: %s", sqlite3_column_text(stmt, 4));
        ui_printf_centered_line("Camiseta: %s", camiseta_display);
        ui_printf_centered_line("Estado: %s", estado_display);
        ui_printf_centered_line("Cancha: %s", cancha_display);
        ui_printf_centered_line("----------------------------------------");
        hay = 1;
    }

    if (!hay)
        mostrar_no_hay_registros("lesiones");

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Modifica el tipo de una lesion existente
 */
static void modificar_tipo_lesion()
{
    char tipo[100];
    solicitar_texto_no_vacio("Nuevo tipo de lesion: ", tipo, sizeof(tipo));
    ejecutar_update_texto("UPDATE lesion SET tipo=? WHERE id=?", tipo, current_lesion_id);
    printf("Tipo modificado correctamente\n");
    pause_console();
}

/**
 * @brief Modifica la descripcion de una lesion existente
 */
static void modificar_descripcion_lesion()
{
    char descripcion[200];
    solicitar_texto_no_vacio("Nueva descripcion: ", descripcion, sizeof(descripcion));
    ejecutar_update_texto("UPDATE lesion SET descripcion=? WHERE id=?", descripcion, current_lesion_id);
    printf("Descripcion modificada correctamente\n");
    pause_console();
}

/**
 * @brief Modifica la fecha de una lesion existente
 */
static void modificar_fecha_lesion()
{
    char fecha[20];
    char hora[10];
    char fecha_hora[30];
    printf("Nueva fecha (dd/mm/yyyy): ");
    fgets(fecha, sizeof(fecha), stdin);
    fecha[strcspn(fecha, "\n")] = 0;
    printf("Nueva hora (hh:mm): ");
    fgets(hora, sizeof(hora), stdin);
    hora[strcspn(hora, "\n")] = 0;
    snprintf(fecha_hora, sizeof(fecha_hora), "%s %s", fecha, hora);
    ejecutar_update_texto("UPDATE lesion SET fecha=? WHERE id=?", fecha_hora, current_lesion_id);
    printf("Fecha modificada correctamente\n");
    pause_console();
}

/**
 * @brief Modifica la camiseta de una lesion existente
 */
static void modificar_camiseta_lesion()
{
    listar_camisetas();
    int camiseta_id = 0;
    while (1)
    {
        camiseta_id = input_int("Nuevo ID de la Camiseta Asociada (0 para cancelar): ");
        if (camiseta_id == 0)
            return;
        if (existe_id("camiseta", camiseta_id))
            break;
        printf("La camiseta no existe. Intente nuevamente.\n");
    }
    ejecutar_update_int("UPDATE lesion SET camiseta_id=? WHERE id=?", camiseta_id, current_lesion_id);
    printf("Camiseta modificada correctamente\n");
    pause_console();
}

/**
 * @brief Modifica el estado de una lesion existente
 */
static void modificar_estado_lesion()
{
    printf("\nEstados disponibles:\n");
    printf("1. ACTIVA - Lesion reciente, jugador NO apto\n");
    printf("2. EN TRATAMIENTO - Esta en Rehabilitacion\n");
    printf("3. MEJORANDO - Evolucion Positiva\n");
    printf("4. RECUPERADO - Alta Medica\n");
    printf("5. RECAIDA - Vuelve la Lesion\n");

    const char *estado = solicitar_estado_lesion("Seleccione nuevo estado (1-5): ");

    ejecutar_update_texto("UPDATE lesion SET estado=? WHERE id=?", estado, current_lesion_id);
    printf("Estado modificado correctamente a: %s\n", estado);
}

/**
 * @brief Modifica el partido asociado a una lesion existente
 */
static void modificar_partido_lesion()
{
    printf("\nDesea asociar esta lesion a un partido? (S/N): ");
    char respuesta[10];
    input_string("", respuesta, sizeof(respuesta));

    int partido_id = 0;

    if (respuesta[0] == 'S' || respuesta[0] == 's')
    {
        partido_id = solicitar_partido_id(0);
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt("UPDATE lesion SET partido_id=? WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }

    if (partido_id > 0)
    {
        sqlite3_bind_int(stmt, 1, partido_id);
    }
    else
    {
        sqlite3_bind_null(stmt, 1);
    }
    sqlite3_bind_int(stmt, 2, current_lesion_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (partido_id > 0)
    {
        printf("Partido modificado correctamente al ID: %d\n", partido_id);
    }
    else
    {
        printf("Asociacion con partido eliminada.\n");
    }
    pause_console();
}

/**
 * @brief Modifica todos los campos de una lesion existente
 */
static void modificar_todo_lesion()
{
    char tipo[100];
    char descripcion[200];
    char fecha[20];
    char estado[50];
    int camiseta_id;

    solicitar_texto_no_vacio("Nuevo tipo de lesion: ", tipo, sizeof(tipo));
    solicitar_texto_no_vacio("Nueva descripcion: ", descripcion, sizeof(descripcion));
    input_date("Nueva fecha (DD/MM/AAAA HH:MM, Enter=ahora): ", fecha, sizeof(fecha));
    while (1)
    {
        camiseta_id = input_int("Nuevo ID de la Camiseta Asociada: ");
        if (existe_id("camiseta", camiseta_id))
            break;
        printf("La camiseta no existe. Intente nuevamente.\n");
    }

    // Mostrar opciones de estado
    printf("\nEstados disponibles:\n");
    printf("1. ACTIVA - Lesion reciente, jugador NO apto\n");
    printf("2. EN_TRATAMIENTO - Esta en rehabilitacion\n");
    printf("3. MEJORANDO - Evolucion positiva\n");
    printf("4. RECUPERADO - Alta medica\n");
    printf("5. RECAiDA - Vuelve la lesion\n");

    const char *estado_sel = solicitar_estado_lesion("Seleccione estado (1-5): ");
    strcpy_s(estado, sizeof(estado), estado_sel);

    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "UPDATE lesion SET tipo=?, descripcion=?, fecha=?, camiseta_id=?, estado=? WHERE id=?",
                &stmt))
    {
        pause_console();
        return;
    }
    sqlite3_bind_text(stmt, 1, tipo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, descripcion, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, camiseta_id);
    sqlite3_bind_text(stmt, 5, estado, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, current_lesion_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("Lesion modificada correctamente\n");
    pause_console();
}

/**
 * @brief Permite modificar una lesion existente
 *
 * Muestra la lista de lesiones disponibles, solicita el ID a modificar,
 * verifica que exista y muestra un menu con opciones para modificar campos individuales o todos.
 */
void modificar_lesion()
{
    mostrar_pantalla("MODIFICAR LESION");

    if (!hay_registros("lesion"))
    {
        mostrar_no_hay_registros("lesion");
        pause_console();
        return;
    }

    printf("Lesiones disponibles:\n\n");
    listar_lesiones();

    int id = input_int("\nID Lesion a Modificar (0 para cancelar): ");

    if (id == 0)
        return;

    if (!existe_id("lesion", id))
    {
        mostrar_no_existe("lesion");
        pause_console();
        return;
    }

    current_lesion_id = id;

    MenuItem items[] =
    {
        {1, "Tipo", modificar_tipo_lesion},
        {2, "Descripcion", modificar_descripcion_lesion},
        {3, "Fecha", modificar_fecha_lesion},
        {4, "Camiseta", modificar_camiseta_lesion},
        {5, "Estado", modificar_estado_lesion},
        {6, "Partido", modificar_partido_lesion},
        {7, "Modificar Todo", modificar_todo_lesion},
        {0, "Volver", NULL}
    };

    ejecutar_menu("MODIFICAR LESION", items, 8);
}

/**
 * @brief Elimina una lesion de la base de datos
 *
 * Muestra la lista de lesiones disponibles, solicita el ID a eliminar,
 * verifica que exista y solicita confirmacion antes de eliminar.
 * Una vez eliminada, el ID queda disponible para reutilizacion.
 */
void eliminar_lesion()
{
    mostrar_pantalla("ELIMINAR LESION");

    if (!hay_registros("lesion"))
    {
        mostrar_no_hay_registros("lesiones");
        pause_console();
        return;
    }

    printf("Lesiones disponibles:\n\n");
    listar_lesiones();

    int id = input_int("\nID a eliminar (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("lesion", id))
    {
        mostrar_no_existe("lesion");
        pause_console();
        return;
    }

    if (!confirmar("Seguro que desea eliminar esta lesion?"))
        return;

    sqlite3_stmt *stmt;
    if (!preparar_stmt("DELETE FROM lesion WHERE id=?", &stmt))
    {
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    printf("\nLesion eliminada correctamente\n");
    pause_console();
}

/**
 * @brief Calcula la diferencia en dias entre dos fechas de lesiones
 *
 * @param fecha1 Primera fecha en formato string
 * @param fecha2 Segunda fecha en formato string
 * @return Diferencia en dias (positivo si fecha2 es posterior a fecha1)
 */
static int calcular_diferencia_dias(const char *fecha1, const char *fecha2)
{
    // Para simplificar, usaremos una conversion basica
    // En un sistema real, se deberia usar una libreria de fechas mas robusta

    int dia1;
    int mes1;
    int ano1;
    int hora1;
    int min1;
    int dia2;
    int mes2;
    int ano2;
    int hora2;
    int min2;

    // Parsear fecha1 (formato esperado: "DD/MM/YYYY HH:MM" o similar)
    sscanf_s(fecha1, "%d/%d/%d %d:%d", &dia1, &mes1, &ano1, &hora1, &min1);

    // Parsear fecha2
    sscanf_s(fecha2, "%d/%d/%d %d:%d", &dia2, &mes2, &ano2, &hora2, &min2);

    // Convertir a dias desde una fecha base (simplificacion)
    int dias1 = ano1 * 365 + mes1 * 30 + dia1;
    int dias2 = ano2 * 365 + mes2 * 30 + dia2;

    return dias2 - dias1;
}

/**
 * @brief Muestra diferencias de dias entre lesiones consecutivas
 */
void mostrar_diferencias_lesiones()
{
    clear_screen();
    print_header("DIFERENCIAS ENTRE LESIONES");

    sqlite3_stmt *stmt;
    if (!preparar_stmt("SELECT id, fecha, tipo FROM lesion ORDER BY fecha ASC", &stmt))
    {
        pause_console();
        return;
    }

    char fecha_anterior[50] = "";
    int primera_lesion = 1;

    printf("Diferencias de dias entre lesiones consecutivas:\n\n");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *fecha_actual = (const char *)sqlite3_column_text(stmt, 1);
        const char *tipo = (const char *)sqlite3_column_text(stmt, 2);

        if (!primera_lesion)
        {
            int dias_diferencia = calcular_diferencia_dias(fecha_anterior, fecha_actual);
            printf("Lesion ID %d (%s) - %d dias despues\n", id, tipo, dias_diferencia);
        }
        else
        {
            printf("Lesion ID %d (%s) - Primera lesion\n", id, tipo);
            primera_lesion = 0;
        }

        strcpy_s(fecha_anterior, sizeof(fecha_anterior), fecha_actual);
    }

    sqlite3_finalize(stmt);

    if (primera_lesion)
    {
        printf("No hay lesiones registradas.\n");
    }

    pause_console();
}

/**
 * @brief Pregunta al usuario si desea actualizar el estado de las lesiones activas
 */
void actualizar_estados_lesiones()
{
    mostrar_pantalla("ACTUALIZAR ESTADOS DE LESIONES");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "SELECT id, tipo, descripcion, fecha, estado FROM lesion WHERE estado != 'RECUPERADO' ORDER BY fecha DESC",
                &stmt))
    {
        pause_console();
        return;
    }

    int lesiones_activas = 0;

    printf("Lesiones que pueden requerir Actualizacion de estado:\n\n");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *tipo = (const char *)sqlite3_column_text(stmt, 1);
        const char *descripcion = (const char *)sqlite3_column_text(stmt, 2);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 3);
        const char *estado = (const char *)sqlite3_column_text(stmt, 4);

        printf("ID: %d | Tipo: %s | Estado actual: %s | Fecha: %s\n", id, tipo, estado, fecha);
        printf("  Descripcion: %s\n\n", descripcion);
        lesiones_activas++;
    }

    sqlite3_finalize(stmt);

    if (lesiones_activas == 0)
    {
        mostrar_no_hay_registros("lesiones activas");
        pause_console();
        return;
    }

    if (confirmar("Desea actualizar el estado de alguna Lesion?"))
    {
        listar_lesiones();
        int id_lesion = input_int("Ingrese el ID de la lesion a actualizar (0 para cancelar): ");

        if (id_lesion == 0)
            return;

        if (!existe_id("lesion", id_lesion))
        {
            mostrar_no_existe("lesion");
            pause_console();
            return;
        }

        current_lesion_id = id_lesion;
        modificar_estado_lesion();
    }

    pause_console();
}

/**
 * @brief Muestra el menu principal de gestion de lesiones
 *
 * Presenta un menu interactivo con opciones para crear, listar, editar
 * y eliminar lesiones. Utiliza la funcion ejecutar_menu para manejar
 * la navegacion del menu y delega las operaciones a las funciones correspondientes.
 */
void menu_lesiones()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_lesion},
        {2, "Listar", listar_lesiones},
        {3, "Modificar", modificar_lesion},
        {4, "Eliminar", eliminar_lesion},
        {5, "Estadisticas", mostrar_estadisticas_lesiones},
        {6, "Diferencias entre Lesiones", mostrar_diferencias_lesiones},
        {7, "Actualizar Estados", actualizar_estados_lesiones},
        {0, "Volver", NULL}
    };
    ejecutar_menu("LESIONES", items, 8);
}
