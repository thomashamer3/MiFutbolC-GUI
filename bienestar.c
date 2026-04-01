/**
 * @file bienestar.c
 * @brief Menu y herramientas de bienestar
 */

#include "bienestar.h"
#include "menu.h"
#include "utils.h"
#include "db.h"
#include "export_pdf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#ifdef _WIN32
#include <process.h>
#include <io.h>
#else
#include "process.h"
#include <strings.h>
#endif


static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

static int menuimg_abrir_imagen_en_sistema(const char *ruta)
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

static int menuimg_guardar_ruta_menu(const char *menu_key, const char *ruta_relativa)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "INSERT INTO bienestar_menu_imagen(menu_key, imagen_ruta) VALUES(?, ?) "
        "ON CONFLICT(menu_key) DO UPDATE SET imagen_ruta=excluded.imagen_ruta";

    if (!preparar_stmt(&stmt, sql))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, menu_key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, ruta_relativa, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

static int menuimg_obtener_ruta_menu(const char *menu_key, char *ruta, size_t size)
{
    if (!ruta || size == 0)
    {
        return 0;
    }

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, "SELECT imagen_ruta FROM bienestar_menu_imagen WHERE menu_key=?"))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, menu_key, -1, SQLITE_TRANSIENT);
    int ok = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *valor = sqlite3_column_text(stmt, 0);
        if (valor && valor[0] != '\0' && strncpy_s(ruta, size, (const char *)valor, _TRUNCATE) == 0)
        {
            ok = 1;
        }
    }
    sqlite3_finalize(stmt);
    return ok;
}

static int menuimg_construir_ruta_absoluta(const char *menu_key, char *ruta_absoluta, size_t size)
{
    if (!ruta_absoluta || size == 0)
    {
        return 0;
    }

    char ruta_db[300] = {0};
    if (!menuimg_obtener_ruta_menu(menu_key, ruta_db, sizeof(ruta_db)))
    {
        return 0;
    }

    return db_resolve_image_absolute_path(ruta_db, ruta_absoluta, size);
}

static int menuimg_cargar_para_menu_key(const char *menu_key)
{
    char prefijo[220] = {0};
    snprintf(prefijo, sizeof(prefijo), "bienestar_%s", menu_key);

    char ruta_relativa_db[300] = {0};
    if (!app_seleccionar_y_copiar_imagen("mifutbol_imagen_sel_bienestar.txt", prefijo,
                                         ruta_relativa_db, sizeof(ruta_relativa_db)))
    {
        return 0;
    }

    if (!menuimg_guardar_ruta_menu(menu_key, ruta_relativa_db))
    {
        printf("Error al guardar ruta de imagen en DB.\n");
        return 0;
    }

    printf("\nImagen cargada correctamente.\n");
    return 1;
}

static void menuimg_cargar_menu(const char *titulo, const char *menu_key)
{
    clear_screen();
    print_header(titulo);

    if (!menuimg_cargar_para_menu_key(menu_key))
    {
        printf("No se pudo completar la carga de imagen.\n");
    }

    pause_console();
}

static void menuimg_ver_menu(const char *titulo, const char *menu_key)
{
    clear_screen();
    print_header(titulo);

    char ruta_absoluta[1200] = {0};
    if (!menuimg_construir_ruta_absoluta(menu_key, ruta_absoluta, sizeof(ruta_absoluta)))
    {
        printf("No se encontro imagen cargada para este menu.\n");
        pause_console();
        return;
    }

    if (!menuimg_abrir_imagen_en_sistema(ruta_absoluta))
    {
        printf("No se pudo abrir la imagen en el sistema.\n");
        pause_console();
        return;
    }

    printf("Abriendo imagen...\n");
    pause_console();
}

static void cargar_imagen_menu_entrenamiento(void)
{
    menuimg_cargar_menu("CARGAR IMAGEN MENU ENTRENAMIENTO", "entrenamiento");
}

static void ver_imagen_menu_entrenamiento(void)
{
    menuimg_ver_menu("VER IMAGEN MENU ENTRENAMIENTO", "entrenamiento");
}

static void cargar_imagen_menu_alimentacion(void)
{
    menuimg_cargar_menu("CARGAR IMAGEN MENU ALIMENTACION", "alimentacion");
}

static void ver_imagen_menu_alimentacion(void)
{
    menuimg_ver_menu("VER IMAGEN MENU ALIMENTACION", "alimentacion");
}

static void cargar_imagen_menu_salud(void)
{
    menuimg_cargar_menu("CARGAR IMAGEN MENU SALUD", "salud");
}

static void ver_imagen_menu_salud(void)
{
    menuimg_ver_menu("VER IMAGEN MENU SALUD", "salud");
}

static double clamp_double(double value, double min_val, double max_val)
{
    if (value < min_val)
    {
        return min_val;
    }
    if (value > max_val)
    {
        return max_val;
    }
    return value;
}

static int clamp_int(int value, int min_val, int max_val)
{
    if (value < min_val)
    {
        return min_val;
    }
    if (value > max_val)
    {
        return max_val;
    }
    return value;
}

static int obtener_double(const char *sql, double *out_value)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, sql))
    {
        *out_value = 0.0;
        return 0;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL)
    {
        *out_value = sqlite3_column_double(stmt, 0);
    }
    else
    {
        *out_value = 0.0;
    }

    sqlite3_finalize(stmt);
    return 1;
}

static int obtener_int(const char *sql, int *out_value)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt, sql))
    {
        *out_value = 0;
        return 0;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL)
    {
        *out_value = sqlite3_column_int(stmt, 0);
    }
    else
    {
        *out_value = 0;
    }

    sqlite3_finalize(stmt);
    return 1;
}

static void append_text(char *buffer, size_t size, size_t *used, const char *text)
{
    if (!buffer || !used || *used >= size)
    {
        return;
    }

    if (!text)
    {
        return;
    }

    int written = snprintf(buffer + *used, size - *used, "%s", text);

    if (written > 0)
    {
        size_t add = (size_t)written;
        if (*used + add >= size)
        {
            *used = size - 1;
        }
        else
        {
            *used += add;
        }
    }
}

static int preparar_stmt_con_mensaje(sqlite3_stmt **stmt, const char *sql, const char *mensaje)
{
    if (!preparar_stmt(stmt, sql))
    {
        printf("%s\n", mensaje);
        pause_console();
        return 0;
    }
    return 1;
}

static int preparar_consulta(sqlite3_stmt **stmt, const char *sql, const char *mensaje, int con_pause)
{
    if (!preparar_stmt(stmt, sql))
    {
        printf("%s\n", mensaje);
        if (con_pause)
        {
            pause_console();
        }
        return 0;
    }
    return 1;
}

static void finalizar_ejecucion(sqlite3_stmt *stmt, const char *ok_msg, const char *err_msg)
{
    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        printf("%s\n", err_msg);
    }
    else
    {
        printf("%s\n", ok_msg);
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void pedir_fecha(const char *prompt, char *buffer, int size);
static int input_bool(const char *prompt);
static int input_rango(const char *prompt, int min, int max);

static void actualizar_campo_fecha(const char *tabla, int id, const char *prompt,
                                   const char *ok_msg, const char *err_msg)
{
    char fecha[16];
    pedir_fecha(prompt, fecha, sizeof(fecha));

    char sql[128];
    snprintf(sql, sizeof(sql), "UPDATE %s SET fecha = ? WHERE id = ?", tabla);

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando actualizacion."))
    {
        return;
    }
    sqlite3_bind_text(stmt, 1, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);

    finalizar_ejecucion(stmt, ok_msg, err_msg);
}

typedef struct
{
    const char *ok_msg;
    const char *err_msg;
} UpdateMensajes;

static void actualizar_campo_entero(const char *tabla, int id, const char *campo,
                                    const char *prompt, int min, int max,
                                    const UpdateMensajes *mensajes)
{
    int valor = input_rango(prompt, min, max);
    char sql[128];
    snprintf(sql, sizeof(sql), "UPDATE %s SET %s = ? WHERE id = ?", tabla, campo);

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando actualizacion."))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, valor);
    sqlite3_bind_int(stmt, 2, id);

    finalizar_ejecucion(stmt, mensajes->ok_msg, mensajes->err_msg);
}

static void actualizar_campo_bool(const char *tabla, int id, const char *campo,
                                  const char *prompt, const char *ok_msg, const char *err_msg)
{
    int valor = input_bool(prompt);
    char sql[128];
    snprintf(sql, sizeof(sql), "UPDATE %s SET %s = ? WHERE id = ?", tabla, campo);

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando actualizacion."))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, valor);
    sqlite3_bind_int(stmt, 2, id);

    finalizar_ejecucion(stmt, ok_msg, err_msg);
}

static void actualizar_campo_texto(const char *tabla, int id, const char *campo,
                                   const char *prompt, size_t max_len,
                                   const char *ok_msg, const char *err_msg)
{
    char texto[512];
    if (max_len > sizeof(texto))
    {
        max_len = sizeof(texto);
    }
    int size_int = (max_len > (size_t)INT_MAX) ? INT_MAX : (int)max_len;
    input_string(prompt, texto, size_int);

    char sql[128];
    snprintf(sql, sizeof(sql), "UPDATE %s SET %s = ? WHERE id = ?", tabla, campo);

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando actualizacion."))
    {
        return;
    }
    sqlite3_bind_text(stmt, 1, texto, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);

    finalizar_ejecucion(stmt, ok_msg, err_msg);
}

static int validar_fecha(const char *fecha)
{
    if (!fecha || safe_strnlen(fecha, 11) != 10)
    {
        return 0;
    }

    for (int i = 0; i < 10; i++)
    {
        if (i == 2 || i == 5)
        {
            if (fecha[i] != '/')
            {
                return 0;
            }
        }
        else if (!isdigit((unsigned char)fecha[i]))
        {
            return 0;
        }
    }
    return 1;
}

static void fecha_hoy(char *buffer, int size)
{
    time_t ahora = time(NULL);
    struct tm tm_info;
#ifdef _WIN32
    if (localtime_s(&tm_info, &ahora) != 0)
    {
        buffer[0] = '\0';
        return;
    }
#else
    if (!localtime_r(&ahora, &tm_info))
    {
        buffer[0] = '\0';
        return;
    }
#endif
    strftime(buffer, (size_t)size, "%Y-%m-%d", &tm_info);
}

static int convertir_fecha_iso(const char *entrada, char *salida, int size)
{
    if (!validar_fecha(entrada) || size < 11)
    {
        return 0;
    }

    char dd[3] = {entrada[0], entrada[1], '\0'};
    char mm[3] = {entrada[3], entrada[4], '\0'};
    char yyyy[5] = {entrada[6], entrada[7], entrada[8], entrada[9], '\0'};

    snprintf(salida, (size_t)size, "%s-%s-%s", yyyy, mm, dd);
    return 1;
}

static void pedir_fecha(const char *prompt, char *buffer, int size)
{
    while (1)
    {
        char entrada[32];
        ui_printf("%s", prompt);
        if (!fgets(entrada, sizeof(entrada), stdin))
        {
            continue;
        }
        entrada[strcspn(entrada, "\r\n")] = '\0';

        if (safe_strnlen(entrada, sizeof(entrada)) == 0)
        {
            fecha_hoy(buffer, size);
            if (safe_strnlen(buffer, (size_t)size) > 0)
            {
                return;
            }
        }

        if (convertir_fecha_iso(entrada, buffer, size))
        {
            return;
        }
        printf("Fecha invalida. Use formato DD/MM/AAAA o Enter para hoy.\n");
    }
}

static int input_bool(const char *prompt)
{
    int valor = -1;
    while (valor != 0 && valor != 1)
    {
        valor = input_int(prompt);
        if (valor != 0 && valor != 1)
        {
            printf("Valor invalido. Use 1=Si, 0=No.\n");
        }
    }
    return valor;
}

static int input_rango(const char *prompt, int min, int max)
{
    int valor = min - 1;
    while (valor < min || valor > max)
    {
        valor = input_int(prompt);
        if (valor < min || valor > max)
        {
            printf("Valor invalido. Rango permitido: %d a %d.\n", min, max);
        }
    }
    return valor;
}

typedef struct
{
    char fecha[16];
    int dormi_bien;
    int hidratacion;
    int alcohol;
    char estado_animico[64];
    int nervios;
    int confianza;
    int motivacion;
    char notas[256];
    char tipo_diario[64];
} HabitoInput;

static void pedir_habito_input(HabitoInput *habito)
{
    pedir_fecha("Fecha (DD/MM/AAAA, Enter=hoy): ", habito->fecha, sizeof(habito->fecha));

    habito->dormi_bien = input_bool("Dormi bien? (1=Si, 0=No): ");
    habito->hidratacion = input_rango("Hidratacion (0-10): ", 0, 10);
    habito->alcohol = input_bool("Alcohol? (1=Si, 0=No): ");

    input_string("Estado animico: ", habito->estado_animico, sizeof(habito->estado_animico));

    habito->nervios = input_rango("Nervios (0-10): ", 0, 10);
    habito->confianza = input_rango("Confianza (0-10): ", 0, 10);
    habito->motivacion = input_rango("Motivacion (0-10): ", 0, 10);

    input_string("Notas libres: ", habito->notas, sizeof(habito->notas));
    habito->tipo_diario[0] = '\0';
}

static int bind_habito(sqlite3_stmt *stmt, const HabitoInput *habito, int index)
{
    sqlite3_bind_text(stmt, index, habito->fecha, -1, SQLITE_TRANSIENT);
    index++;
    sqlite3_bind_int(stmt, index, habito->dormi_bien);
    index++;
    sqlite3_bind_int(stmt, index, habito->hidratacion);
    index++;
    sqlite3_bind_int(stmt, index, habito->alcohol);
    index++;
    sqlite3_bind_text(stmt, index, habito->estado_animico, -1, SQLITE_TRANSIENT);
    index++;
    sqlite3_bind_int(stmt, index, habito->nervios);
    index++;
    sqlite3_bind_int(stmt, index, habito->confianza);
    index++;
    sqlite3_bind_int(stmt, index, habito->motivacion);
    index++;
    sqlite3_bind_text(stmt, index, habito->notas, -1, SQLITE_TRANSIENT);
    index++;
    sqlite3_bind_text(stmt, index, habito->tipo_diario, -1, SQLITE_TRANSIENT);
    index++;

    return index;
}

static void format_fecha_mostrar(const char *fecha_iso, char *salida, int size)
{
    format_date_for_display(fecha_iso, salida, size);
}

static void listar_objetivos_simple(int con_pause)
{
    const char *sql =
        "SELECT id, nombre, fecha_inicio, fecha_fin, estado "
        "FROM bienestar_objetivo ORDER BY id DESC";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando objetivos.", con_pause))
    {
        return;
    }

    ui_printf_centered_line("ID | Objetivo | Inicio | Fin | Estado");
    ui_printf_centered_line("-----------------------------------------------");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 1);
        const char *inicio = (const char *)sqlite3_column_text(stmt, 2);
        const char *fin = (const char *)sqlite3_column_text(stmt, 3);
        const char *estado = (const char *)sqlite3_column_text(stmt, 4);

        char inicio_disp[32];
        char fin_disp[32];
        format_fecha_mostrar(inicio, inicio_disp, sizeof(inicio_disp));
        format_fecha_mostrar(fin, fin_disp, sizeof(fin_disp));

        ui_printf_centered_line("%d | %s | %s | %s | %s", id, nombre, inicio_disp, fin_disp, estado);
        count++;
    }

    if (count == 0)
    {
        ui_printf_centered_line("No hay objetivos registrados.");
    }

    sqlite3_finalize(stmt);
    if (con_pause)
    {
        pause_console();
    }
}

static void listar_objetivos(void)
{
    clear_screen();
    print_header("OBJETIVOS");
    listar_objetivos_simple(1);
}

static void crear_objetivo(void)
{
    clear_screen();
    print_header("CREAR OBJETIVO");

    char nombre[128];
    char fecha_inicio[16];
    char fecha_fin[16];
    char notas[256];

    input_string("Objetivo: ", nombre, sizeof(nombre));
    pedir_fecha("Fecha inicio (DD/MM/AAAA, Enter=hoy): ", fecha_inicio, sizeof(fecha_inicio));
    pedir_fecha("Fecha fin (DD/MM/AAAA, Enter=hoy): ", fecha_fin, sizeof(fecha_fin));

    printf("Estado: 1=Activo, 2=Logrado, 3=Abandonado\n");
    int estado_opcion = input_rango("> ", 1, 3);

    input_string("Notas (opcional): ", notas, sizeof(notas));

    const char *estado = "Abandonado";
    if (estado_opcion == 1)
    {
        estado = "Activo";
    }
    else if (estado_opcion == 2)
    {
        estado = "Logrado";
    }

    const char *sql =
        "INSERT INTO bienestar_objetivo (nombre, fecha_inicio, fecha_fin, estado, notas) "
        "VALUES (?, ?, ?, ?, ?)";

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando insercion."))
    {
        return;
    }

    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, fecha_inicio, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, fecha_fin, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, estado, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, notas, -1, SQLITE_TRANSIENT);

    finalizar_ejecucion(stmt, "Objetivo guardado.", "Error guardando objetivo.");
}

static void cambiar_estado_objetivo(void)
{
    clear_screen();
    print_header("CAMBIAR ESTADO DE OBJETIVO");

    listar_objetivos();

    int id = input_int("ID del objetivo (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    printf("Nuevo estado: 1=Activo, 2=Logrado, 3=Abandonado\n");
    int estado_opcion = input_rango("> ", 1, 3);
    const char *estado = "Abandonado";
    if (estado_opcion == 1)
    {
        estado = "Activo";
    }
    else if (estado_opcion == 2)
    {
        estado = "Logrado";
    }

    const char *sql = "UPDATE bienestar_objetivo SET estado = ? WHERE id = ?";
    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando actualizacion."))
    {
        return;
    }

    sqlite3_bind_text(stmt, 1, estado, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);

    finalizar_ejecucion(stmt, "Estado actualizado.", "Error actualizando estado.");
}

static void listar_planes_entrenamiento(void)
{
    clear_screen();
    print_header("PLANES DE ENTRENAMIENTO");

    const char *sql =
        "SELECT p.id, o.nombre, p.frecuencia_semanal, p.rutina_semanal "
        "FROM bienestar_plan_entrenamiento p "
        "JOIN bienestar_objetivo o ON o.id = p.objetivo_id "
        "ORDER BY p.id DESC";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando planes.", 1))
    {
        return;
    }

    ui_printf_centered_line("ID | Objetivo | Frecuencia | Rutina");
    ui_printf_centered_line("-----------------------------------------------");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *objetivo = (const char *)sqlite3_column_text(stmt, 1);
        int frecuencia = sqlite3_column_int(stmt, 2);
        const char *rutina = (const char *)sqlite3_column_text(stmt, 3);

        ui_printf_centered_line("%d | %s | %d/sem | %s", id, objetivo, frecuencia, rutina);
        count++;
    }

    if (count == 0)
    {
        ui_printf_centered_line("No hay planes registrados.");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void crear_plan_entrenamiento(void)
{
    clear_screen();
    print_header("CREAR PLAN DE ENTRENAMIENTO");

    printf("Objetivos disponibles:\n");
    listar_objetivos_simple(0);

    int objetivo_id = input_int("ID del objetivo (0 para cancelar): ");
    if (objetivo_id == 0)
    {
        return;
    }

    int frecuencia = input_rango("Frecuencia semanal (1-14): ", 1, 14);
    char rutina[256];
    char notas[256];

    input_string_extended("Rutina semanal (ej: Lunes fuerza, Miercoles futbol): ", rutina, sizeof(rutina));
    input_string_extended("Notas (opcional): ", notas, sizeof(notas));

    const char *sql =
        "INSERT INTO bienestar_plan_entrenamiento (objetivo_id, frecuencia_semanal, rutina_semanal, notas) "
        "VALUES (?, ?, ?, ?)";

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando insercion."))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, objetivo_id);
    sqlite3_bind_int(stmt, 2, frecuencia);
    sqlite3_bind_text(stmt, 3, rutina, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, notas, -1, SQLITE_TRANSIENT);

    finalizar_ejecucion(stmt, "Plan guardado.", "Error guardando plan.");
}

static void mostrar_planificacion_personal(void)
{
    MenuItem items[] =
    {
        {1, "Crear objetivo",&crear_objetivo},
        {2, "Listar objetivos",&listar_objetivos},
        {3, "Cambiar estado de objetivo",&cambiar_estado_objetivo},
        {4, "Crear plan de entrenamiento",&crear_plan_entrenamiento},
        {5, "Listar planes", &listar_planes_entrenamiento},
        {0, "Volver", NULL}
    };

    clear_screen();
    print_header("PLANIFICACION PERSONAL");
    ejecutar_menu_estandar("PLANIFICACION PERSONAL", items, 6);
}

static void registrar_habitos(void)
{
    clear_screen();
    print_header("REGISTRO DIARIO");
    HabitoInput habito;
    pedir_habito_input(&habito);

    const char *sql =
        "INSERT INTO bienestar_habito (fecha, dormi_bien, hidratacion, alcohol, estado_animico, "
        "nervios, confianza, motivacion, notas, tipo_diario) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando insercion."))
    {
        return;
    }

    bind_habito(stmt, &habito, 1);

    finalizar_ejecucion(stmt, "Registro guardado.", "Error guardando registro.");
}

static void listar_habitos_simple(int con_pause, int con_clear)
{
    if (con_clear)
    {
        clear_screen();
        print_header("HABITOS RECIENTES");
    }

    const char *sql =
        "SELECT id, fecha, dormi_bien, hidratacion, alcohol, estado_animico, "
        "nervios, confianza, motivacion "
        "FROM bienestar_habito ORDER BY fecha DESC LIMIT 14";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando habitos.", con_pause))
    {
        return;
    }

    ui_printf_centered_line("%-4s %-12s %-8s %-6s %-5s %-12s %-4s %-5s %-4s",
                            "ID", "Fecha", "Dormir", "Hidra", "Alc", "Animo", "Ner", "Conf", "Mot");
    ui_printf_centered_line("---- ------------ -------- ------ ----- ------------ ---- ----- ----");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 1);
        int dormi = sqlite3_column_int(stmt, 2);
        int hid = sqlite3_column_int(stmt, 3);
        int alc = sqlite3_column_int(stmt, 4);
        const char *animo = (const char *)sqlite3_column_text(stmt, 5);
        int nerv = sqlite3_column_int(stmt, 6);
        int conf = sqlite3_column_int(stmt, 7);
        int mot = sqlite3_column_int(stmt, 8);

        char fecha_disp[32];
        format_fecha_mostrar(fecha, fecha_disp, sizeof(fecha_disp));
        ui_printf_centered_line("%-4d %-12s %-8s %-6d %-5s %-12s %-4d %-5d %-4d",
                                id, fecha_disp, dormi ? "Si" : "No", hid,
                                alc ? "Si" : "No", animo ? animo : "", nerv, conf, mot);
        count++;
    }

    if (count == 0)
    {
        ui_printf_centered_line("No hay registros.");
    }

    sqlite3_finalize(stmt);
    if (con_pause)
    {
        pause_console();
    }
}

static void listar_habitos(void)
{
    listar_habitos_simple(1, 1);
}

static void actualizar_habito_todo(int id)
{
    HabitoInput habito;
    pedir_habito_input(&habito);

    const char *sql =
        "UPDATE bienestar_habito "
        "SET fecha = ?, dormi_bien = ?, hidratacion = ?, alcohol = ?, estado_animico = ?, "
        "nervios = ?, confianza = ?, motivacion = ?, notas = ?, tipo_diario = ? "
        "WHERE id = ?";

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando actualizacion."))
    {
        return;
    }

    int index = bind_habito(stmt, &habito, 1);
    sqlite3_bind_int(stmt, index, id);

    finalizar_ejecucion(stmt, "Habito actualizado.", "Error actualizando habito.");
}

static void actualizar_habito_fecha(int id)
{
    actualizar_campo_fecha("bienestar_habito", id,
                           "Nueva fecha (DD/MM/AAAA, Enter=hoy): ",
                           "Habito actualizado.", "Error actualizando habito.");
}

static void actualizar_habito_entero(int id, const char *campo, const char *prompt, int min, int max)
{
    UpdateMensajes mensajes = {"Habito actualizado.", "Error actualizando habito."};
    actualizar_campo_entero("bienestar_habito", id, campo, prompt, min, max, &mensajes);
}

static void actualizar_habito_bool(int id, const char *campo, const char *prompt)
{
    actualizar_campo_bool("bienestar_habito", id, campo, prompt,
                          "Habito actualizado.", "Error actualizando habito.");
}

static void actualizar_habito_texto(int id, const char *campo, const char *prompt, size_t max_len)
{
    actualizar_campo_texto("bienestar_habito", id, campo, prompt, max_len,
                           "Habito actualizado.", "Error actualizando habito.");
}

static void actualizar_habito_aspecto(int id)
{
    printf("Modificar aspecto:\n");
    printf("1) Fecha\n");
    printf("2) Dormi bien\n");
    printf("3) Hidratacion\n");
    printf("4) Alcohol\n");
    printf("5) Estado animico\n");
    printf("6) Nervios\n");
    printf("7) Confianza\n");
    printf("8) Motivacion\n");
    printf("9) Notas\n");

    int opcion = input_rango("> ", 1, 9);

    switch (opcion)
    {
    case 1:
        actualizar_habito_fecha(id);
        break;
    case 2:
        actualizar_habito_bool(id, "dormi_bien", "Dormi bien? (1=Si, 0=No): ");
        break;
    case 3:
        actualizar_habito_entero(id, "hidratacion", "Hidratacion (0-10): ", 0, 10);
        break;
    case 4:
        actualizar_habito_bool(id, "alcohol", "Alcohol? (1=Si, 0=No): ");
        break;
    case 5:
        actualizar_habito_texto(id, "estado_animico", "Estado animico: ", 64);
        break;
    case 6:
        actualizar_habito_entero(id, "nervios", "Nervios (0-10): ", 0, 10);
        break;
    case 7:
        actualizar_habito_entero(id, "confianza", "Confianza (0-10): ", 0, 10);
        break;
    case 8:
        actualizar_habito_entero(id, "motivacion", "Motivacion (0-10): ", 0, 10);
        break;
    default:
        actualizar_habito_texto(id, "notas", "Notas libres: ", 256);
        break;
    }
}

static void modificar_habito(void)
{
    clear_screen();
    print_header("MODIFICAR HABITO");

    listar_habitos_simple(0, 0);

    int id = input_int("ID del registro (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("bienestar_habito", id))
    {
        printf("El registro no existe.\n");
        pause_console();
        return;
    }

    if (!confirmar("Confirmar modificacion del registro"))
    {
        printf("Operacion cancelada.\n");
        pause_console();
        return;
    }

    printf("1) Modificar todo\n");
    printf("2) Modificar aspecto\n");
    int opcion = input_rango("> ", 1, 2);

    if (opcion == 1)
    {
        actualizar_habito_todo(id);
    }
    else
    {
        actualizar_habito_aspecto(id);
    }
}

static void eliminar_habito(void)
{
    clear_screen();
    print_header("ELIMINAR HABITO");

    listar_habitos_simple(0, 0);

    int id = input_int("ID del registro (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!existe_id("bienestar_habito", id))
    {
        printf("El registro no existe.\n");
        pause_console();
        return;
    }

    if (!confirmar("Confirmar eliminacion del registro"))
    {
        printf("Operacion cancelada.\n");
        pause_console();
        return;
    }

    const char *sql = "DELETE FROM bienestar_habito WHERE id = ?";
    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando eliminacion."))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, id);

    finalizar_ejecucion(stmt, "Registro eliminado.", "Error eliminando registro.");
}

static void mostrar_mentalidad_habitos(void)
{
    MenuItem items[] =
    {
        {1, "Registrar habitos diarios",&registrar_habitos},
        {2, "Ver ultimos registros",&listar_habitos},
        {3, "Modificar registro",&modificar_habito},
        {4, "Eliminar registro",&eliminar_habito},
        {0, "Volver", NULL}
    };

    clear_screen();
    print_header("MENTALIDAD Y HABITOS");
    ejecutar_menu_estandar("MENTALIDAD Y HABITOS", items, 5);
}

static const char *tipo_sesion_texto(int tipo)
{
    switch (tipo)
    {
    case 1:
        return "Charla";
    case 2:
        return "Visualizacion";
    case 3:
        return "Reflexion";
    default:
        return "Reflexion";
    }
}

static const char *momento_sesion_texto(int momento)
{
    switch (momento)
    {
    case 1:
        return "Antes";
    case 2:
        return "Despues";
    default:
        return "N/A";
    }
}

static int pedir_partido_id_validado(const char *prompt)
{
    int partido_id = input_int(prompt);
    if (partido_id > 0 && !existe_id("partido", partido_id))
    {
        printf("El partido no existe. Se guardara sin ID.\n");
        return 0;
    }
    return partido_id;
}

typedef struct
{
    char fecha[16];
    int tipo;
    int momento;
    int partido_id;
    int confianza;
    int estres;
    int motivacion;
    int presion;
    int concentracion;
    char miedos[256];
    char pensamientos[256];
    char texto_libre[512];
} SesionMentalInput;

static void pedir_sesion_mental_input(SesionMentalInput *sesion)
{
    pedir_fecha("Fecha (DD/MM/AAAA, Enter=hoy): ", sesion->fecha, sizeof(sesion->fecha));

    printf("Tipo: 1=Charla, 2=Visualizacion, 3=Reflexion\n");
    sesion->tipo = input_rango("> ", 1, 3);

    printf("Momento: 1=Antes de partido, 2=Despues de partido, 3=N/A\n");
    sesion->momento = input_rango("> ", 1, 3);

    sesion->partido_id = 0;
    if (sesion->momento == 1 || sesion->momento == 2)
    {
        sesion->partido_id = pedir_partido_id_validado("ID de partido (0 si no aplica): ");
    }

    sesion->confianza = input_rango("Confianza (0-10): ", 0, 10);
    sesion->estres = input_rango("Estres (0-10): ", 0, 10);
    sesion->motivacion = input_rango("Motivacion (0-10): ", 0, 10);
    sesion->presion = input_rango("Presion (0-10): ", 0, 10);
    sesion->concentracion = input_rango("Concentracion (0-10): ", 0, 10);

    input_string("Miedos (opcional): ", sesion->miedos, sizeof(sesion->miedos));
    input_string("Pensamientos clave (opcional): ", sesion->pensamientos, sizeof(sesion->pensamientos));
    input_string("Texto libre (opcional): ", sesion->texto_libre, sizeof(sesion->texto_libre));
}

static int bind_sesion_mental(sqlite3_stmt *stmt, const SesionMentalInput *sesion, int index)
{
    sqlite3_bind_text(stmt, index, sesion->fecha, -1, SQLITE_TRANSIENT);
    index++;
    sqlite3_bind_text(stmt, index, tipo_sesion_texto(sesion->tipo), -1, SQLITE_TRANSIENT);
    index++;
    sqlite3_bind_text(stmt, index, momento_sesion_texto(sesion->momento), -1, SQLITE_TRANSIENT);
    index++;
    if (sesion->partido_id > 0)
    {
        sqlite3_bind_int(stmt, index, sesion->partido_id);
    }
    else
    {
        sqlite3_bind_null(stmt, index);
    }
    index++;
    sqlite3_bind_int(stmt, index, sesion->confianza);
    index++;
    sqlite3_bind_int(stmt, index, sesion->estres);
    index++;
    sqlite3_bind_int(stmt, index, sesion->motivacion);
    index++;
    sqlite3_bind_text(stmt, index, sesion->miedos, -1, SQLITE_TRANSIENT);
    index++;
    sqlite3_bind_int(stmt, index, sesion->presion);
    index++;
    sqlite3_bind_int(stmt, index, sesion->concentracion);
    index++;
    sqlite3_bind_text(stmt, index, sesion->pensamientos, -1, SQLITE_TRANSIENT);
    index++;
    sqlite3_bind_text(stmt, index, sesion->texto_libre, -1, SQLITE_TRANSIENT);
    index++;

    return index;
}

static void registrar_sesion_mental(void)
{
    clear_screen();
    print_header("REGISTRO MENTAL");
    SesionMentalInput sesion;
    pedir_sesion_mental_input(&sesion);

    const char *sql =
        "INSERT INTO bienestar_sesion_mental "
        "(fecha, tipo, momento, partido_id, confianza, estres, motivacion, miedos, presion, concentracion, pensamientos_clave, texto_libre) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando insercion."))
    {
        return;
    }

    bind_sesion_mental(stmt, &sesion, 1);

    finalizar_ejecucion(stmt, "Sesion mental guardada.", "Error guardando sesion mental.");
}

static void listar_sesiones_mentales(void)
{
    clear_screen();
    print_header("SESIONES MENTALES");

    const char *sql =
        "SELECT id, fecha, tipo, momento, confianza, estres, motivacion, presion, concentracion "
        "FROM bienestar_sesion_mental ORDER BY fecha DESC, id DESC LIMIT 20";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando sesiones.", 1))
    {
        return;
    }

    ui_printf_centered_line("ID | Fecha | Tipo | Momento | Confianza | Estres | Motivacion | Presion | Concentracion");
    ui_printf_centered_line("--------------------------------------------------------------------");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *fecha_iso = (const char *)sqlite3_column_text(stmt, 1);
        const char *tipo = (const char *)sqlite3_column_text(stmt, 2);
        const char *momento = (const char *)sqlite3_column_text(stmt, 3);
        int confianza = sqlite3_column_int(stmt, 4);
        int estres = sqlite3_column_int(stmt, 5);
        int motivacion = sqlite3_column_int(stmt, 6);
        int presion = sqlite3_column_int(stmt, 7);
        int concentracion = sqlite3_column_int(stmt, 8);

        char fecha_disp[32];
        format_fecha_mostrar(fecha_iso, fecha_disp, sizeof(fecha_disp));

        ui_printf_centered_line("%d | %s | %s | %s | %d | %d | %d | %d | %d",
                                id, fecha_disp, tipo, momento, confianza, estres, motivacion, presion, concentracion);
        count++;
    }

    if (count == 0)
    {
        ui_printf_centered_line("No hay sesiones registradas.");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void listar_sesiones_mentales_simple(void)
{
    const char *sql =
        "SELECT id, fecha, tipo, momento "
        "FROM bienestar_sesion_mental ORDER BY fecha DESC, id DESC LIMIT 20";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando sesiones.", 0))
    {
        return;
    }

    ui_printf_centered_line("ID | Fecha | Tipo | Momento");
    ui_printf_centered_line("-----------------------------------------");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *fecha_iso = (const char *)sqlite3_column_text(stmt, 1);
        const char *tipo = (const char *)sqlite3_column_text(stmt, 2);
        const char *momento = (const char *)sqlite3_column_text(stmt, 3);

        char fecha_disp[32];
        format_fecha_mostrar(fecha_iso, fecha_disp, sizeof(fecha_disp));

        ui_printf_centered_line("%d | %s | %s | %s", id, fecha_disp, tipo, momento);
        count++;
    }

    if (count == 0)
    {
        ui_printf_centered_line("No hay sesiones registradas.");
    }

    sqlite3_finalize(stmt);
}

static void ver_detalle_sesion_mental(void)
{
    clear_screen();
    print_header("DETALLE SESION MENTAL");

    listar_sesiones_mentales_simple();
    int id = input_int("ID de sesion (0 para cancelar): ");
    if (id == 0)
        return;

    const char *sql =
        "SELECT fecha, tipo, momento, partido_id, confianza, estres, motivacion, presion, concentracion, "
        "miedos, pensamientos_clave, texto_libre "
        "FROM bienestar_sesion_mental WHERE id = ?";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando detalle.", 1))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha_iso = (const char *)sqlite3_column_text(stmt, 0);
        const char *tipo = (const char *)sqlite3_column_text(stmt, 1);
        const char *momento = (const char *)sqlite3_column_text(stmt, 2);
        int partido_id = sqlite3_column_int(stmt, 3);
        int confianza = sqlite3_column_int(stmt, 4);
        int estres = sqlite3_column_int(stmt, 5);
        int motivacion = sqlite3_column_int(stmt, 6);
        int presion = sqlite3_column_int(stmt, 7);
        int concentracion = sqlite3_column_int(stmt, 8);
        const char *miedos = (const char *)sqlite3_column_text(stmt, 9);
        const char *pensamientos = (const char *)sqlite3_column_text(stmt, 10);
        const char *texto = (const char *)sqlite3_column_text(stmt, 11);

        char fecha_disp[32];
        format_fecha_mostrar(fecha_iso, fecha_disp, sizeof(fecha_disp));

        ui_printf_centered_line("Fecha: %s", fecha_disp);
        ui_printf_centered_line("Tipo: %s | Momento: %s", tipo, momento);
        if (sqlite3_column_type(stmt, 3) == SQLITE_NULL)
        {
            ui_printf_centered_line("Partido: N/A");
        }
        else
        {
            ui_printf_centered_line("Partido ID: %d", partido_id);
        }
        ui_printf_centered_line("Confianza: %d | Estres: %d | Motivacion: %d", confianza, estres, motivacion);
        ui_printf_centered_line("Presion: %d | Concentracion: %d", presion, concentracion);
        ui_printf_centered_line("Miedos: %s", miedos ? miedos : "");
        ui_printf_centered_line("Pensamientos clave: %s", pensamientos ? pensamientos : "");
        ui_printf_centered_line("Texto libre: %s", texto ? texto : "");
    }
    else
    {
        ui_printf_centered_line("Sesion no encontrada.");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void actualizar_sesion_mental_todo(int id)
{
    SesionMentalInput sesion;
    pedir_sesion_mental_input(&sesion);

    const char *sql =
        "UPDATE bienestar_sesion_mental "
        "SET fecha = ?, tipo = ?, momento = ?, partido_id = ?, confianza = ?, estres = ?, motivacion = ?, "
        "miedos = ?, presion = ?, concentracion = ?, pensamientos_clave = ?, texto_libre = ? "
        "WHERE id = ?";

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando actualizacion."))
    {
        return;
    }

    int index = bind_sesion_mental(stmt, &sesion, 1);
    sqlite3_bind_int(stmt, index, id);

    finalizar_ejecucion(stmt, "Sesion mental actualizada.", "Error actualizando sesion mental.");
}

static void actualizar_sesion_mental_fecha(int id)
{
    actualizar_campo_fecha("bienestar_sesion_mental", id,
                           "Nueva fecha (DD/MM/AAAA, Enter=hoy): ",
                           "Sesion mental actualizada.",
                           "Error actualizando sesion mental.");
}

static void actualizar_sesion_mental_tipo(int id)
{
    printf("Tipo: 1=Charla, 2=Visualizacion, 3=Reflexion\n");
    int tipo = input_rango("> ", 1, 3);

    const char *sql = "UPDATE bienestar_sesion_mental SET tipo = ? WHERE id = ?";
    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando actualizacion."))
    {
        return;
    }
    sqlite3_bind_text(stmt, 1, tipo_sesion_texto(tipo), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);

    finalizar_ejecucion(stmt, "Sesion mental actualizada.", "Error actualizando sesion mental.");
}

static void actualizar_sesion_mental_momento(int id)
{
    printf("Momento: 1=Antes de partido, 2=Despues de partido, 3=N/A\n");
    int momento = input_rango("> ", 1, 3);
    int partido_id = 0;
    if (momento == 1 || momento == 2)
    {
        partido_id = pedir_partido_id_validado("ID de partido (0 si no aplica): ");
    }

    const char *sql = "UPDATE bienestar_sesion_mental SET momento = ?, partido_id = ? WHERE id = ?";
    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando actualizacion."))
    {
        return;
    }
    sqlite3_bind_text(stmt, 1, momento_sesion_texto(momento), -1, SQLITE_TRANSIENT);
    if (partido_id > 0)
    {
        sqlite3_bind_int(stmt, 2, partido_id);
    }
    else
    {
        sqlite3_bind_null(stmt, 2);
    }
    sqlite3_bind_int(stmt, 3, id);

    finalizar_ejecucion(stmt, "Sesion mental actualizada.", "Error actualizando sesion mental.");
}

static void actualizar_sesion_mental_partido(int id)
{
    int partido_id = pedir_partido_id_validado("ID de partido (0 para quitar): ");

    const char *sql = "UPDATE bienestar_sesion_mental SET partido_id = ? WHERE id = ?";
    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando actualizacion."))
    {
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
    sqlite3_bind_int(stmt, 2, id);

    finalizar_ejecucion(stmt, "Sesion mental actualizada.", "Error actualizando sesion mental.");
}

static void actualizar_sesion_mental_entero(int id, const char *campo, const char *prompt, int min, int max)
{
    UpdateMensajes mensajes = {"Sesion mental actualizada.", "Error actualizando sesion mental."};
    actualizar_campo_entero("bienestar_sesion_mental", id, campo, prompt, min, max, &mensajes);
}

static void actualizar_sesion_mental_texto(int id, const char *campo, const char *prompt, size_t max_len)
{
    actualizar_campo_texto("bienestar_sesion_mental", id, campo, prompt, max_len,
                           "Sesion mental actualizada.",
                           "Error actualizando sesion mental.");
}

static void actualizar_sesion_mental_aspecto(int id)
{
    printf("Modificar aspecto:\n");
    printf("1) Fecha\n");
    printf("2) Tipo\n");
    printf("3) Momento\n");
    printf("4) Partido ID\n");
    printf("5) Confianza\n");
    printf("6) Estres\n");
    printf("7) Motivacion\n");
    printf("8) Presion\n");
    printf("9) Concentracion\n");
    printf("10) Miedos\n");
    printf("11) Pensamientos clave\n");
    printf("12) Texto libre\n");

    int opcion = input_rango("> ", 1, 12);

    switch (opcion)
    {
    case 1:
        actualizar_sesion_mental_fecha(id);
        break;
    case 2:
        actualizar_sesion_mental_tipo(id);
        break;
    case 3:
        actualizar_sesion_mental_momento(id);
        break;
    case 4:
        actualizar_sesion_mental_partido(id);
        break;
    case 5:
        actualizar_sesion_mental_entero(id, "confianza", "Confianza (0-10): ", 0, 10);
        break;
    case 6:
        actualizar_sesion_mental_entero(id, "estres", "Estres (0-10): ", 0, 10);
        break;
    case 7:
        actualizar_sesion_mental_entero(id, "motivacion", "Motivacion (0-10): ", 0, 10);
        break;
    case 8:
        actualizar_sesion_mental_entero(id, "presion", "Presion (0-10): ", 0, 10);
        break;
    case 9:
        actualizar_sesion_mental_entero(id, "concentracion", "Concentracion (0-10): ", 0, 10);
        break;
    case 10:
        actualizar_sesion_mental_texto(id, "miedos", "Miedos (opcional): ", 256);
        break;
    case 11:
        actualizar_sesion_mental_texto(id, "pensamientos_clave", "Pensamientos clave (opcional): ", 256);
        break;
    default:
        actualizar_sesion_mental_texto(id, "texto_libre", "Texto libre (opcional): ", 512);
        break;
    }
}

static void modificar_sesion_mental(void)
{
    clear_screen();
    print_header("MODIFICAR SESION MENTAL");

    listar_sesiones_mentales_simple();
    int id = input_int("ID de sesion (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("bienestar_sesion_mental", id))
    {
        printf("La sesion no existe.\n");
        pause_console();
        return;
    }

    if (!confirmar("Confirmar modificacion de la sesion"))
    {
        printf("Operacion cancelada.\n");
        pause_console();
        return;
    }

    printf("1) Modificar todo\n");
    printf("2) Modificar aspecto\n");
    int opcion = input_rango("> ", 1, 2);

    if (opcion == 1)
    {
        actualizar_sesion_mental_todo(id);
    }
    else
    {
        actualizar_sesion_mental_aspecto(id);
    }
}

static void eliminar_sesion_mental(void)
{
    clear_screen();
    print_header("ELIMINAR SESION MENTAL");

    listar_sesiones_mentales_simple();
    int id = input_int("ID de sesion (0 para cancelar): ");
    if (id == 0)
        return;

    if (!existe_id("bienestar_sesion_mental", id))
    {
        printf("La sesion no existe.\n");
        pause_console();
        return;
    }

    if (!confirmar("Confirmar eliminacion de la sesion"))
    {
        printf("Operacion cancelada.\n");
        pause_console();
        return;
    }

    const char *sql = "DELETE FROM bienestar_sesion_mental WHERE id = ?";
    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando eliminacion."))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, id);

    finalizar_ejecucion(stmt, "Sesion mental eliminada.", "Error eliminando sesion mental.");
}

static void sesiones_antes_despues_partido(void)
{
    clear_screen();
    print_header("ANTES / DESPUES DE PARTIDO");

    const char *sql =
        "SELECT s.fecha, s.momento, s.confianza, s.estres, s.motivacion, p.rendimiento_general "
        "FROM bienestar_sesion_mental s "
        "LEFT JOIN partido p ON (s.partido_id = p.id) "
        "ORDER BY s.fecha DESC, s.id DESC LIMIT 20";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando sesiones.", 1))
    {
        return;
    }

    printf("Fecha | Momento | Confianza | Estres | Motivacion | Rendimiento\n");
    printf("--------------------------------------------------------\n");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha_iso = (const char *)sqlite3_column_text(stmt, 0);
        const char *momento = (const char *)sqlite3_column_text(stmt, 1);
        int confianza = sqlite3_column_int(stmt, 2);
        int estres = sqlite3_column_int(stmt, 3);
        int motivacion = sqlite3_column_int(stmt, 4);
        int rendimiento = sqlite3_column_int(stmt, 5);

        char fecha_disp[32];
        format_fecha_mostrar(fecha_iso, fecha_disp, sizeof(fecha_disp));

        if (sqlite3_column_type(stmt, 5) == SQLITE_NULL)
        {
            printf("%s | %s | %d | %d | %d | N/A\n", fecha_disp, momento, confianza, estres, motivacion);
        }
        else
        {
            printf("%s | %s | %d | %d | %d | %d\n", fecha_disp, momento, confianza, estres, motivacion, rendimiento);
        }
        count++;
    }

    if (count == 0)
    {
        printf("No hay sesiones vinculadas.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void tendencias_mentales(void)
{
    clear_screen();
    print_header("TENDENCIAS MENTALES");

    const char *sql =
        "SELECT "
        "(SELECT AVG(p.rendimiento_general) FROM partido p "
        " WHERE (substr(p.fecha_hora,7,4)||'-'||substr(p.fecha_hora,4,2)||'-'||substr(p.fecha_hora,1,2)) IN "
        " (SELECT DISTINCT fecha FROM bienestar_sesion_mental WHERE confianza >= 8)) AS avg_alta, "
        "(SELECT AVG(p.rendimiento_general) FROM partido p "
        " WHERE (substr(p.fecha_hora,7,4)||'-'||substr(p.fecha_hora,4,2)||'-'||substr(p.fecha_hora,1,2)) IN "
        " (SELECT DISTINCT fecha FROM bienestar_sesion_mental WHERE confianza <= 4)) AS avg_baja";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando tendencias.", 1))
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        double avg_alta = sqlite3_column_double(stmt, 0);
        double avg_baja = sqlite3_column_double(stmt, 1);

        printf("Rendimiento promedio con confianza alta (>=8): %.2f\n", avg_alta);
        printf("Rendimiento promedio con confianza baja (<=4): %.2f\n", avg_baja);

        if (avg_alta > avg_baja)
        {
            printf("Tendencia: Jugas mejor cuando la confianza esta alta.\n");
        }
        else if (avg_baja > avg_alta)
        {
            printf("Tendencia: Tu rendimiento cae cuando la confianza baja.\n");
        }
        else
        {
            printf("Tendencia: No hay diferencia clara aun.\n");
        }
    }
    else
    {
        printf("No hay datos suficientes para tendencias.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void mostrar_mental_deportivo(void)
{
    MenuItem items[] =
    {
        {1, "Registrar sesion mental",&registrar_sesion_mental},
        {2, "Listar sesiones",&listar_sesiones_mentales},
        {3, "Ver detalle", &ver_detalle_sesion_mental},
        {4, "Modificar sesion", &modificar_sesion_mental},
        {5, "Eliminar sesion", &eliminar_sesion_mental},
        {6, "Antes / despues de partidos", &sesiones_antes_despues_partido},
        {7, "Tendencias", &tendencias_mentales},
        {0, "Volver", NULL}
    };

    clear_screen();
    print_header("MENTAL (AUTOCONOCIMIENTO)");
    ejecutar_menu_estandar("MENTAL (AUTOCONOCIMIENTO)", items, 8);
}

static int parse_mes_anio(const char *entrada, char *salida, int size)
{
    if (!entrada)
        return 0;

    if (safe_strnlen(entrada, 16) != 7)
    {
        return 0;
    }

    if (!isdigit((unsigned char)entrada[0]) || !isdigit((unsigned char)entrada[1]) || entrada[2] != '/' ||
            !isdigit((unsigned char)entrada[3]) || !isdigit((unsigned char)entrada[4]) ||
            !isdigit((unsigned char)entrada[5]) || !isdigit((unsigned char)entrada[6]))
    {
        return 0;
    }

    char mm[3] = {entrada[0], entrada[1], '\0'};
    char yyyy[5] = {entrada[3], entrada[4], entrada[5], entrada[6], '\0'};

    int mes = atoi(mm);
    if (mes < 1 || mes > 12)
        return 0;

    snprintf(salida, (size_t)size, "%s-%s", yyyy, mm);
    return 1;
}

static void pedir_mes_anio(char *salida, int size)
{
    while (1)
    {
        char entrada[16];
        ui_printf("Mes y anio (MM/AAAA, ej: 03/2026, Enter=actual): ");
        if (!fgets(entrada, sizeof(entrada), stdin))
        {
            continue;
        }
        entrada[strcspn(entrada, "\r\n")] = '\0';

        if (safe_strnlen(entrada, sizeof(entrada)) == 0)
        {
            time_t ahora = time(NULL);
            struct tm tm_info;
#ifdef _WIN32
            if (localtime_s(&tm_info, &ahora) != 0)
                continue;
#else
            if (!localtime_r(&ahora, &tm_info))
                continue;
#endif
            {
                char tmp[16];
                int written = snprintf(tmp, sizeof(tmp), "%04d-%02d", tm_info.tm_year + 1900, tm_info.tm_mon + 1);
                if (written < 0 || (size_t)written >= sizeof(tmp))
                    continue;
#ifdef _WIN32
                strncpy_s(salida, (rsize_t)size, tmp, _TRUNCATE);
#else
                strncpy(salida, tmp, (size_t)size);
                if (size > 0)
                    salida[size - 1] = '\0';
#endif
            }
            return;
        }

        if (parse_mes_anio(entrada, salida, size))
        {
            return;
        }

        printf("Mes invalido. Use formato MM/AAAA (ejemplo: 03/2026).\n");
    }
}

static void informe_personal_mensual_pdf(void)
{
    clear_screen();
    print_header("INFORME PERSONAL MENSUAL");

    char mes_yyyy_mm[16];
    pedir_mes_anio(mes_yyyy_mm, sizeof(mes_yyyy_mm));

    if (generar_informe_personal_mensual_pdf(mes_yyyy_mm))
    {
        printf("Informe mensual generado correctamente.\n");
    }
    else
    {
        printf("No se pudo generar el informe mensual.\n");
    }

    pause_console();
}

static const char *tipo_entrenamiento_texto(int tipo)
{
    switch (tipo)
    {
    case 1:
        return "Fuerza";
    case 2:
        return "Resistencia";
    case 3:
        return "Tecnica";
    case 4:
        return "Recuperacion";
    default:
        return "Otro";
    }
}

static void registrar_entrenamiento(void)
{
    clear_screen();
    print_header("REGISTRAR ENTRENAMIENTO");

    char fecha[16];
    char notas[256];

    pedir_fecha("Fecha (DD/MM/AAAA, Enter=hoy): ", fecha, sizeof(fecha));

    printf("Tipo: 1=Fuerza, 2=Resistencia, 3=Tecnica, 4=Recuperacion\n");
    int tipo = input_rango("> ", 1, 4);
    int duracion = input_rango("Duracion (min): ", 1, 600);
    int intensidad = input_rango("Intensidad (1-10): ", 1, 10);

    input_string("Notas (opcional): ", notas, sizeof(notas));

    const char *sql =
        "INSERT INTO bienestar_entrenamiento (fecha, tipo, duracion_min, intensidad, omitido, notas) "
        "VALUES (?, ?, ?, ?, 0, ?)";

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando insercion."))
    {
        return;
    }

    sqlite3_bind_text(stmt, 1, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, tipo_entrenamiento_texto(tipo), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, duracion);
    sqlite3_bind_int(stmt, 4, intensidad);
    sqlite3_bind_text(stmt, 5, notas, -1, SQLITE_TRANSIENT);

    finalizar_ejecucion(stmt, "Entrenamiento guardado.", "Error guardando entrenamiento.");
}

static void listar_entrenamientos_simple(int con_pause, int con_clear)
{
    if (con_clear)
    {
        clear_screen();
        print_header("ENTRENAMIENTOS RECIENTES");
    }

    const char *sql =
        "SELECT id, fecha, tipo, duracion_min, intensidad, omitido "
        "FROM bienestar_entrenamiento ORDER BY fecha DESC, id DESC LIMIT 20";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando entrenamientos.", con_pause))
    {
        return;
    }

    ui_printf_centered_line("ID | Fecha | Tipo | Duracion (min) | Intensidad | Omitido");
    ui_printf_centered_line("----------------------------------------------------");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 1);
        const char *tipo = (const char *)sqlite3_column_text(stmt, 2);
        int dur = sqlite3_column_int(stmt, 3);
        int intensidad = sqlite3_column_int(stmt, 4);
        int omitido = sqlite3_column_int(stmt, 5);

        char fecha_disp[32];
        format_fecha_mostrar(fecha, fecha_disp, sizeof(fecha_disp));
        ui_printf_centered_line("%d | %s | %s | %d | %d | %s", id, fecha_disp, tipo, dur, intensidad,
                                omitido ? "Si" : "No");
        count++;
    }

    if (count == 0)
    {
        ui_printf_centered_line("No hay entrenamientos registrados.");
    }

    sqlite3_finalize(stmt);
    if (con_pause)
    {
        pause_console();
    }
}

static void listar_entrenamientos(void)
{
    listar_entrenamientos_simple(1, 1);
}

static void marcar_entrenamiento_omitido(void)
{
    clear_screen();
    print_header("MARCAR ENTRENAMIENTO OMITIDO");

    listar_entrenamientos_simple(0, 0);

    int id = input_int("ID del entrenamiento (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    const char *sql = "UPDATE bienestar_entrenamiento SET omitido = 1 WHERE id = ?";
    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando actualizacion."))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, id);

    finalizar_ejecucion(stmt, "Entrenamiento marcado como omitido.", "Error actualizando entrenamiento.");
}

static void registrar_ejercicio(void)
{
    clear_screen();
    print_header("REGISTRAR EJERCICIO");

    char nombre[128];
    char grupo[128];

    input_string("Nombre del ejercicio: ", nombre, sizeof(nombre));
    input_string("Grupo muscular: ", grupo, sizeof(grupo));

    const char *sql =
        "INSERT INTO bienestar_ejercicio (nombre, grupo_muscular) VALUES (?, ?)";

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando insercion."))
    {
        return;
    }

    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, grupo, -1, SQLITE_TRANSIENT);

    finalizar_ejecucion(stmt, "Ejercicio guardado.", "Error guardando ejercicio.");
}

static void listar_ejercicios(void)
{
    const char *sql = "SELECT id, nombre, grupo_muscular FROM bienestar_ejercicio ORDER BY id DESC";
    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando ejercicios.", 0))
    {
        return;
    }

    ui_printf_centered_line("ID | Ejercicio | Grupo");
    ui_printf_centered_line("------------------------------");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 1);
        const char *grupo = (const char *)sqlite3_column_text(stmt, 2);

        ui_printf_centered_line("%d | %s | %s", id, nombre, grupo);
        count++;
    }

    if (count == 0)
    {
        ui_printf_centered_line("No hay ejercicios registrados.");
    }

    sqlite3_finalize(stmt);
}

static void asociar_ejercicio(void)
{
    clear_screen();
    print_header("ASOCIAR EJERCICIO A ENTRENAMIENTO");

    printf("Entrenamientos:\n");
    listar_entrenamientos_simple(0, 0);

    int entrenamiento_id = input_int("ID de entrenamiento (0 para cancelar): ");
    if (entrenamiento_id == 0)
    {
        return;
    }

    printf("Ejercicios:\n");
    listar_ejercicios();

    int ejercicio_id = input_int("ID de ejercicio (0 para cancelar): ");
    if (ejercicio_id == 0)
    {
        return;
    }

    int series = input_rango("Series: ", 0, 100);
    int repeticiones = input_rango("Repeticiones: ", 0, 500);
    int tiempo = input_rango("Tiempo (min): ", 0, 600);

    const char *sql =
        "INSERT INTO bienestar_entrenamiento_ejercicio "
        "(entrenamiento_id, ejercicio_id, series, repeticiones, tiempo_min) "
        "VALUES (?, ?, ?, ?, ?)";

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando insercion."))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, entrenamiento_id);
    sqlite3_bind_int(stmt, 2, ejercicio_id);
    sqlite3_bind_int(stmt, 3, series);
    sqlite3_bind_int(stmt, 4, repeticiones);
    sqlite3_bind_int(stmt, 5, tiempo);

    finalizar_ejecucion(stmt, "Asociacion guardada.", "Error guardando asociacion.");
}

static void comparar_entrenamiento_vs_rendimiento(void)
{
    clear_screen();
    print_header("COMPARAR ENTRENAMIENTO VS RENDIMIENTO");

    const char *sql =
        "SELECT "
        "(SELECT COUNT(DISTINCT fecha) FROM bienestar_entrenamiento WHERE omitido = 0) AS dias_entrenamiento, "
        "(SELECT COUNT(*) FROM partido) AS total_partidos, "
        "(SELECT AVG(rendimiento_general) FROM partido "
        " WHERE (substr(fecha_hora,7,4)||'-'||substr(fecha_hora,4,2)||'-'||substr(fecha_hora,1,2)) IN "
        " (SELECT fecha FROM bienestar_entrenamiento WHERE omitido = 0)) AS avg_con_entrenamiento, "
        "(SELECT AVG(rendimiento_general) FROM partido "
        " WHERE (substr(fecha_hora,7,4)||'-'||substr(fecha_hora,4,2)||'-'||substr(fecha_hora,1,2)) NOT IN "
        " (SELECT fecha FROM bienestar_entrenamiento WHERE omitido = 0)) AS avg_sin_entrenamiento";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando comparacion.", 1))
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int dias_entrenamiento = sqlite3_column_int(stmt, 0);
        int total_partidos = sqlite3_column_int(stmt, 1);
        double avg_ent = sqlite3_column_double(stmt, 2);
        double avg_sin = sqlite3_column_double(stmt, 3);

        printf("Dias con entrenamiento registrado: %d\n", dias_entrenamiento);
        printf("Partidos registrados: %d\n", total_partidos);
        printf("Rendimiento promedio (dias con entrenamiento): %.2f\n", avg_ent);
        printf("Rendimiento promedio (dias sin entrenamiento): %.2f\n", avg_sin);
    }
    else
    {
        printf("No hay datos suficientes para comparar.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void alertas_entrenamiento(void)
{
    clear_screen();
    print_header("ALERTAS DE INTENSIDAD");

    const char *sql =
        "SELECT COUNT(*) FROM bienestar_entrenamiento "
        "WHERE omitido = 0 AND intensidad >= 8 AND fecha >= date('now', '-6 day')";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando alertas.", 1))
    {
        return;
    }

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    if (count >= 4)
    {
        printf("Alerta: Muchos dias seguidos de alta intensidad (ultimos 7 dias: %d).\n", count);
    }
    else
    {
        printf("Sin alertas de intensidad en los ultimos 7 dias.\n");
    }

    pause_console();
}

static void mostrar_entrenamiento_expandido(void)
{
    MenuItem items[] =
    {
        {1, "Registrar entrenamiento",&registrar_entrenamiento},
        {2, "Listar entrenamientos",&listar_entrenamientos},
        {3, "Marcar entrenamiento omitido",&marcar_entrenamiento_omitido},
        {4, "Registrar ejercicio",&registrar_ejercicio},
        {5, "Asociar ejercicio a entrenamiento",&asociar_ejercicio},
        {6, "Comparar entrenamiento vs rendimiento",&comparar_entrenamiento_vs_rendimiento},
        {7, "Alertas de intensidad", &alertas_entrenamiento},
        {8, "Cargar imagen del menu", &cargar_imagen_menu_entrenamiento},
        {9, "Ver imagen del menu", &ver_imagen_menu_entrenamiento},
        {0, "Volver", NULL}
    };

    clear_screen();
    print_header("ENTRENAMIENTO (EXPANDIDO)");
    ejecutar_menu_estandar("ENTRENAMIENTO (EXPANDIDO)", items, 10);
}

static const char *tipo_comida_texto(int tipo)
{
    switch (tipo)
    {
    case 1:
        return "Desayuno";
    case 2:
        return "Almuerzo";
    case 3:
        return "Cena";
    case 4:
        return "Snack";
    default:
        return "Otro";
    }
}

static const char *calidad_comida_texto(int calidad)
{
    switch (calidad)
    {
    case 1:
        return "Mala";
    case 2:
        return "Regular";
    case 3:
        return "Buena";
    default:
        return "Regular";
    }
}

static const char *hidratacion_texto(int nivel)
{
    switch (nivel)
    {
    case 1:
        return "Baja";
    case 2:
        return "Media";
    case 3:
        return "Alta";
    default:
        return "Media";
    }
}

static void registrar_comida(void)
{
    clear_screen();
    print_header("REGISTRAR COMIDA");

    char fecha[16];
    char descripcion[256];

    pedir_fecha("Fecha (DD/MM/AAAA, Enter=hoy): ", fecha, sizeof(fecha));
    printf("Tipo: 1=Desayuno, 2=Almuerzo, 3=Cena, 4=Snack\n");
    int tipo = input_rango("> ", 1, 4);

    printf("Calidad: 1=Mala, 2=Regular, 3=Buena\n");
    int calidad = input_rango("> ", 1, 3);

    input_string("Descripcion (opcional): ", descripcion, sizeof(descripcion));

    const char *sql =
        "INSERT INTO bienestar_comida (fecha, tipo, calidad, descripcion) "
        "VALUES (?, ?, ?, ?)";

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando insercion."))
    {
        return;
    }

    sqlite3_bind_text(stmt, 1, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, tipo_comida_texto(tipo), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, calidad_comida_texto(calidad), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, descripcion, -1, SQLITE_TRANSIENT);

    finalizar_ejecucion(stmt, "Comida guardada.", "Error guardando comida.");
}

static void listar_comidas(void)
{
    clear_screen();
    print_header("COMIDAS RECIENTES");

    const char *sql =
        "SELECT fecha, tipo, calidad, descripcion "
        "FROM bienestar_comida ORDER BY fecha DESC, id DESC LIMIT 20";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando comidas.", 1))
    {
        return;
    }

    ui_printf_centered_line("Fecha | Tipo | Calidad | Descripcion");
    ui_printf_centered_line("----------------------------------------------");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha = (const char *)sqlite3_column_text(stmt, 0);
        const char *tipo = (const char *)sqlite3_column_text(stmt, 1);
        const char *calidad = (const char *)sqlite3_column_text(stmt, 2);
        const char *desc = (const char *)sqlite3_column_text(stmt, 3);

        char fecha_disp[32];
        format_fecha_mostrar(fecha, fecha_disp, sizeof(fecha_disp));
        ui_printf_centered_line("%s | %s | %s | %s", fecha_disp, tipo, calidad, desc ? desc : "");
        count++;
    }

    if (count == 0)
    {
        ui_printf_centered_line("No hay comidas registradas.");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void registrar_dia_nutricional(void)
{
    clear_screen();
    print_header("DIA NUTRICIONAL");

    char fecha[16];
    char notas[256];
    char peso_str[32];
    double peso = -1.0;

    pedir_fecha("Fecha (DD/MM/AAAA, Enter=hoy): ", fecha, sizeof(fecha));

    printf("Hidratacion: 1=Baja, 2=Media, 3=Alta\n");
    int hidratacion = input_rango("> ", 1, 3);
    int alcohol = input_bool("Alcohol? (1=Si, 0=No): ");

    input_string_extended("Peso corporal (opcional, Enter para omitir): ", peso_str, sizeof(peso_str));
    if (safe_strnlen(peso_str, sizeof(peso_str)) > 0)
    {
        peso = atof(peso_str);
    }

    input_string_extended("Notas (opcional): ", notas, sizeof(notas));

    const char *sql =
        "INSERT INTO bienestar_dia_nutricional (fecha, hidratacion, alcohol, peso_corporal, notas) "
        "VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(fecha) DO UPDATE SET hidratacion=excluded.hidratacion, "
        "alcohol=excluded.alcohol, peso_corporal=excluded.peso_corporal, notas=excluded.notas";

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando insercion."))
    {
        return;
    }

    sqlite3_bind_text(stmt, 1, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hidratacion_texto(hidratacion), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, alcohol);
    if (peso >= 0.0)
    {
        sqlite3_bind_double(stmt, 4, peso);
    }
    else
    {
        sqlite3_bind_null(stmt, 4);
    }
    sqlite3_bind_text(stmt, 5, notas, -1, SQLITE_TRANSIENT);

    finalizar_ejecucion(stmt, "Dia nutricional guardado.", "Error guardando dia nutricional.");
}

static void listar_dias_nutricionales(void)
{
    clear_screen();
    print_header("DIAS NUTRICIONALES");

    const char *sql =
        "SELECT fecha, hidratacion, alcohol, peso_corporal "
        "FROM bienestar_dia_nutricional ORDER BY fecha DESC LIMIT 14";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando dias nutricionales.", 1))
    {
        return;
    }

    ui_printf_centered_line("Fecha | Hidratacion | Alcohol | Peso");
    ui_printf_centered_line("-----------------------------------------");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha = (const char *)sqlite3_column_text(stmt, 0);
        const char *hid = (const char *)sqlite3_column_text(stmt, 1);
        int alcohol = sqlite3_column_int(stmt, 2);
        if (sqlite3_column_type(stmt, 3) == SQLITE_NULL)
        {
            char fecha_disp[32];
            format_fecha_mostrar(fecha, fecha_disp, sizeof(fecha_disp));
            ui_printf_centered_line("%s | %s | %s | N/A", fecha_disp, hid, alcohol ? "Si" : "No");
        }
        else
        {
            double peso = sqlite3_column_double(stmt, 3);
            char fecha_disp[32];
            format_fecha_mostrar(fecha, fecha_disp, sizeof(fecha_disp));
            ui_printf_centered_line("%s | %s | %s | %.1f", fecha_disp, hid, alcohol ? "Si" : "No", peso);
        }
        count++;
    }

    if (count == 0)
    {
        ui_printf_centered_line("No hay dias registrados.");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void estadisticas_alimentacion(void)
{
    clear_screen();
    print_header("ESTADISTICAS ALIMENTACION");

    const char *sql =
        "SELECT "
        "COUNT(DISTINCT fecha) AS total_dias, "
        "COUNT(DISTINCT CASE WHEN calidad = 'Buena' THEN fecha END) AS dias_buena "
        "FROM bienestar_comida";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando estadisticas.", 1))
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int total = sqlite3_column_int(stmt, 0);
        int buenas = sqlite3_column_int(stmt, 1);
        double pct = (total > 0) ? ((double)buenas / (double)total) * 100.0 : 0.0;

        printf("Dias con alimentacion registrada: %d\n", total);
        printf("Dias con alimentacion buena: %d\n", buenas);
        printf("Porcentaje de dias con buena alimentacion: %.1f%%\n", pct);
    }
    else
    {
        printf("No hay datos suficientes.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void comparacion_alimentacion_vs_rendimiento(void)
{
    clear_screen();
    print_header("ALIMENTACION VS RENDIMIENTO");

    const char *sql =
        "SELECT "
        "(SELECT AVG(rendimiento_general) FROM partido "
        " WHERE strftime('%%Y-%%m-%%d', fecha_hora) IN "
        " (SELECT DISTINCT fecha FROM bienestar_comida WHERE calidad = 'Buena')) AS avg_buena, "
        "(SELECT AVG(rendimiento_general) FROM partido "
        " WHERE strftime('%%Y-%%m-%%d', fecha_hora) IN "
        " (SELECT DISTINCT fecha FROM bienestar_comida WHERE calidad = 'Mala')) AS avg_mala";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando comparacion.", 1))
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        double avg_buena = sqlite3_column_double(stmt, 0);
        double avg_mala = sqlite3_column_double(stmt, 1);

        printf("Rendimiento promedio en dias con alimentacion buena: %.2f\n", avg_buena);
        printf("Rendimiento promedio en dias con alimentacion mala: %.2f\n", avg_mala);
    }
    else
    {
        printf("No hay datos suficientes para comparar.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void flags_alimentacion(void)
{
    clear_screen();
    print_header("FLAGS DE ALIMENTACION");

    const char *sql =
        "SELECT p.fecha_hora, p.rendimiento_general "
        "FROM partido p "
        "JOIN bienestar_comida c ON c.fecha = strftime('%%Y-%%m-%%d', p.fecha_hora) "
        "WHERE c.calidad = 'Mala' "
        "ORDER BY p.fecha_hora DESC LIMIT 10";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando flags.", 1))
    {
        return;
    }

    printf("Partidos con alimentacion mala el mismo dia:\n");
    printf("Fecha | Rendimiento\n");
    printf("---------------------------\n");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha = (const char *)sqlite3_column_text(stmt, 0);
        int rendimiento = sqlite3_column_int(stmt, 1);
        char fecha_disp[32];
        format_date_for_display(fecha, fecha_disp, sizeof(fecha_disp));
        printf("%s | %d\n", fecha_disp, rendimiento);
        count++;
    }

    if (count == 0)
    {
        printf("No hay partidos marcados con mala alimentacion.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void mostrar_alimentacion_expandido(void)
{
    MenuItem items[] =
    {
        {1, "Registrar comida",&registrar_comida},
        {2, "Listar comidas",&listar_comidas},
        {3, "Registrar dia nutricional",&registrar_dia_nutricional},
        {4, "Listar dias nutricionales",&listar_dias_nutricionales},
        {5, "Alimentacion vs rendimiento",&comparacion_alimentacion_vs_rendimiento},
        {6, "Flags: comiste mal antes del partido",&flags_alimentacion},
        {7, "Estadisticas de alimentacion", &estadisticas_alimentacion},
        {8, "Cargar imagen del menu", &cargar_imagen_menu_alimentacion},
        {9, "Ver imagen del menu", &ver_imagen_menu_alimentacion},
        {0, "Volver", NULL}
    };

    clear_screen();
    print_header("ALIMENTACION (EXPANDIDO)");
    ejecutar_menu_estandar("ALIMENTACION (EXPANDIDO)", items, 10);
}

static void mostrar_salud_perfil(void)
{
    clear_screen();
    print_header("SALUD - PERFIL");

    const char *sql =
        "SELECT altura_cm, peso_kg, tipo_sangre, ultima_revision, medidas, notas "
        "FROM bienestar_salud WHERE id = 1";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando salud.", 1))
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        double altura = sqlite3_column_double(stmt, 0);
        double peso = sqlite3_column_double(stmt, 1);
        const char *tipo = (const char *)sqlite3_column_text(stmt, 2);
        const char *revision = (const char *)sqlite3_column_text(stmt, 3);
        const char *medidas = (const char *)sqlite3_column_text(stmt, 4);
        const char *notas = (const char *)sqlite3_column_text(stmt, 5);

        char revision_disp[32];
        format_fecha_mostrar(revision, revision_disp, sizeof(revision_disp));

        printf("Altura (cm): %.1f\n", altura);
        printf("Peso (kg): %.1f\n", peso);
        printf("Tipo de sangre: %s\n", tipo ? tipo : "");
        printf("Ultimo control: %s\n", (revision && revision[0]) ? revision_disp : "N/A");
        printf("Mediciones: %s\n", medidas ? medidas : "");
        printf("Notas: %s\n", notas ? notas : "");
    }
    else
    {
        printf("No hay datos de salud registrados.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void actualizar_salud_perfil(void)
{
    clear_screen();
    print_header("ACTUALIZAR SALUD");

    char altura_str[32];
    char peso_str[32];
    char tipo_sangre[16];
    char ultima_revision[16];
    char medidas[128];
    char notas[256];

    input_string_extended("Altura en cm (ej: 175.5): ", altura_str, sizeof(altura_str));
    input_string_extended("Peso en kg (ej: 70.2): ", peso_str, sizeof(peso_str));
    input_string_extended("Tipo de sangre (ej: O+): ", tipo_sangre, sizeof(tipo_sangre));
    pedir_fecha("Ultimo control (DD/MM/AAAA, Enter=hoy): ", ultima_revision, sizeof(ultima_revision));
    input_string_extended("Mediciones (ej: cintura 80, cadera 95): ", medidas, sizeof(medidas));
    input_string_extended("Notas (opcional): ", notas, sizeof(notas));

    double altura = atof(altura_str);
    double peso = atof(peso_str);

    const char *sql =
        "INSERT INTO bienestar_salud (id, altura_cm, peso_kg, tipo_sangre, ultima_revision, medidas, notas) "
        "VALUES (1, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET altura_cm=excluded.altura_cm, peso_kg=excluded.peso_kg, "
        "tipo_sangre=excluded.tipo_sangre, ultima_revision=excluded.ultima_revision, "
        "medidas=excluded.medidas, notas=excluded.notas";

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando insercion."))
    {
        return;
    }

    sqlite3_bind_double(stmt, 1, altura);
    sqlite3_bind_double(stmt, 2, peso);
    sqlite3_bind_text(stmt, 3, tipo_sangre, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, ultima_revision, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, medidas, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, notas, -1, SQLITE_TRANSIENT);

    finalizar_ejecucion(stmt, "Datos de salud actualizados.", "Error guardando datos de salud.");
}

typedef struct
{
    char fecha[16];
    char tipo[64];
    char profesional[64];
    char resultado[128];
    char notas[256];
} ControlMedicoInput;

static void pedir_control_medico_input(ControlMedicoInput *control, const char *prompt_fecha,
                                       const char *prompt_tipo, const char *prompt_profesional,
                                       const char *prompt_resultado, const char *prompt_notas)
{
    pedir_fecha(prompt_fecha, control->fecha, sizeof(control->fecha));
    input_string_extended(prompt_tipo, control->tipo, sizeof(control->tipo));
    input_string(prompt_profesional, control->profesional, sizeof(control->profesional));
    input_string_extended(prompt_resultado, control->resultado, sizeof(control->resultado));
    input_string_extended(prompt_notas, control->notas, sizeof(control->notas));
}

static int bind_control_medico(sqlite3_stmt *stmt, const ControlMedicoInput *control, int index)
{
    sqlite3_bind_text(stmt, index, control->fecha, -1, SQLITE_TRANSIENT);
    index++;
    sqlite3_bind_text(stmt, index, control->tipo, -1, SQLITE_TRANSIENT);
    index++;
    sqlite3_bind_text(stmt, index, control->profesional, -1, SQLITE_TRANSIENT);
    index++;
    sqlite3_bind_text(stmt, index, control->resultado, -1, SQLITE_TRANSIENT);
    index++;
    sqlite3_bind_text(stmt, index, control->notas, -1, SQLITE_TRANSIENT);
    index++;

    return index;
}

static void registrar_control_medico(void)
{
    clear_screen();
    print_header("REGISTRAR CONTROL MEDICO");
    ControlMedicoInput control;
    pedir_control_medico_input(&control,
                               "Fecha del control (DD/MM/AAAA, Enter=hoy): ",
                               "Tipo de control (ej: medico general, cardiologia): ",
                               "Profesional (opcional): ",
                               "Resultado (opcional): ",
                               "Notas (opcional): ");

    const char *sql =
        "INSERT INTO bienestar_control_medico (fecha, tipo, profesional, resultado, notas) "
        "VALUES (?, ?, ?, ?, ?)";

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando insercion."))
    {
        return;
    }

    bind_control_medico(stmt, &control, 1);

    finalizar_ejecucion(stmt, "Control medico guardado.", "Error guardando control medico.");
}

static void listar_controles_medicos(void)
{
    clear_screen();
    print_header("CONTROLES MEDICOS");

    const char *sql =
        "SELECT fecha, tipo, profesional, resultado "
        "FROM bienestar_control_medico ORDER BY fecha DESC, id DESC LIMIT 20";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando controles.", 1))
    {
        return;
    }

    ui_printf_centered_line("Fecha | Tipo | Profesional | Resultado");
    ui_printf_centered_line("-----------------------------------------------------");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha_iso = (const char *)sqlite3_column_text(stmt, 0);
        const char *tipo = (const char *)sqlite3_column_text(stmt, 1);
        const char *prof = (const char *)sqlite3_column_text(stmt, 2);
        const char *res = (const char *)sqlite3_column_text(stmt, 3);

        char fecha_disp[32];
        format_fecha_mostrar(fecha_iso, fecha_disp, sizeof(fecha_disp));

        ui_printf_centered_line("%s | %s | %s | %s",
                                fecha_disp,
                                tipo ? tipo : "",
                                prof ? prof : "",
                                res ? res : "");
        count++;
    }

    if (count == 0)
    {
        ui_printf_centered_line("No hay controles registrados.");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void editar_control_medico(void)
{
    clear_screen();
    print_header("EDITAR CONTROL MEDICO");

    listar_controles_medicos();

    int id = input_int("ID del control (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }
    ControlMedicoInput control;
    pedir_control_medico_input(&control,
                               "Nueva fecha (DD/MM/AAAA, Enter=hoy): ",
                               "Tipo de control: ",
                               "Profesional: ",
                               "Resultado: ",
                               "Notas: ");

    const char *sql =
        "UPDATE bienestar_control_medico "
        "SET fecha = ?, tipo = ?, profesional = ?, resultado = ?, notas = ? "
        "WHERE id = ?";

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando actualizacion."))
    {
        return;
    }

    int index = bind_control_medico(stmt, &control, 1);
    sqlite3_bind_int(stmt, index, id);

    finalizar_ejecucion(stmt, "Control medico actualizado.", "Error actualizando control medico.");
}

static void eliminar_control_medico(void)
{
    clear_screen();
    print_header("ELIMINAR CONTROL MEDICO");

    listar_controles_medicos();

    int id = input_int("ID del control (0 para cancelar): ");
    if (id == 0)
    {
        return;
    }

    if (!confirmar("Confirmar eliminacion del control medico"))
    {
        printf("Operacion cancelada.\n");
        pause_console();
        return;
    }

    const char *sql = "DELETE FROM bienestar_control_medico WHERE id = ?";
    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando eliminacion."))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, id);

    finalizar_ejecucion(stmt, "Control medico eliminado.", "Error eliminando control medico.");
}

/* ========== ARCHIVOS ADJUNTOS A CONTROLES MEDICOS ========== */

static int estudio_arch_es_imagen(const char *ext)
{
    if (!ext)
    {
        return 0;
    }
#ifdef _WIN32
    return _stricmp(ext, ".jpg") == 0 || _stricmp(ext, ".jpeg") == 0 ||
           _stricmp(ext, ".png") == 0 || _stricmp(ext, ".bmp") == 0 ||
           _stricmp(ext, ".webp") == 0;
#else
    return strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0 ||
           strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".bmp") == 0 ||
           strcasecmp(ext, ".webp") == 0;
#endif
}

static int estudio_arch_extension_soportada(const char *ext)
{
    if (!ext)
    {
        return 0;
    }
    if (estudio_arch_es_imagen(ext))
    {
        return 1;
    }
#ifdef _WIN32
    return _stricmp(ext, ".pdf") == 0 || _stricmp(ext, ".docx") == 0 ||
           _stricmp(ext, ".doc") == 0 || _stricmp(ext, ".txt") == 0 ||
           _stricmp(ext, ".xlsx") == 0 || _stricmp(ext, ".csv") == 0;
#else
    return strcasecmp(ext, ".pdf") == 0 || strcasecmp(ext, ".docx") == 0 ||
           strcasecmp(ext, ".doc") == 0 || strcasecmp(ext, ".txt") == 0 ||
           strcasecmp(ext, ".xlsx") == 0 || strcasecmp(ext, ".csv") == 0;
#endif
}

static int estudio_arch_seleccionar_archivo(char *ruta, size_t size)
{
    if (!ruta || size == 0)
    {
        return 0;
    }
#ifdef _WIN32
    const char *arch_temp = "mifutbol_estudio_arch_sel.txt";
    remove(arch_temp);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "powershell -NoProfile -Command \""
             "Add-Type -AssemblyName System.Windows.Forms; "
             "$dlg = New-Object System.Windows.Forms.OpenFileDialog; "
             "$dlg.InitialDirectory = [System.IO.Path]::Combine($env:USERPROFILE, 'Downloads'); "
             "$dlg.Filter = 'Archivos de estudio|*.jpg;*.jpeg;*.png;*.bmp;*.webp;*.pdf;*.docx;*.doc;*.txt;*.xlsx;*.csv|Todos|*.*'; "
             "if ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) "
             "{ [System.IO.File]::WriteAllText('%s', $dlg.FileName) }\"",
             arch_temp);

    system(cmd);

    FILE *f = NULL;
    if (fopen_s(&f, arch_temp, "r") != 0 || !f)
    {
        return 0;
    }

    if (!fgets(ruta, (int)size, f))
    {
        fclose(f);
        remove(arch_temp);
        return 0;
    }

    fclose(f);
    remove(arch_temp);
    trim_whitespace(ruta);
    return ruta[0] != '\0';
#else
    const char *arch_temp = "mifutbol_estudio_arch_sel.txt";
    remove(arch_temp);

    if (app_command_exists("zenity"))
    {
        char cmd[2200];
        snprintf(cmd,
                 sizeof(cmd),
                 "zenity --file-selection --title=\"Seleccionar archivo de estudio\" > \"%s\" 2>/dev/null",
                 arch_temp);
        system(cmd);
    }
    else if (app_command_exists("kdialog"))
    {
        char cmd[2200];
        snprintf(cmd,
                 sizeof(cmd),
                 "kdialog --getopenfilename ~ > \"%s\" 2>/dev/null",
                 arch_temp);
        system(cmd);
    }

    FILE *f = NULL;
    if (fopen_s(&f, arch_temp, "r") == 0 && f)
    {
        if (fgets(ruta, (int)size, f))
        {
            fclose(f);
            remove(arch_temp);
            trim_whitespace(ruta);
            if (ruta[0] != '\0')
            {
                return 1;
            }
        }
        else
        {
            fclose(f);
            remove(arch_temp);
        }
    }

    input_string("Ruta del archivo: ", ruta, (int)size);
    trim_whitespace(ruta);
    return ruta[0] != '\0';
#endif
}

static void listar_controles_medicos_ids(void)
{
    const char *sql =
        "SELECT id, fecha, tipo, profesional "
        "FROM bienestar_control_medico ORDER BY fecha DESC, id DESC LIMIT 30";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando controles.", 0))
    {
        return;
    }

    ui_printf_centered_line("ID  | Fecha      | Tipo                 | Profesional");
    ui_printf_centered_line("---------------------------------------------------------");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *fecha_iso = (const char *)sqlite3_column_text(stmt, 1);
        const char *tipo = (const char *)sqlite3_column_text(stmt, 2);
        const char *prof = (const char *)sqlite3_column_text(stmt, 3);

        char fecha_disp[32];
        format_fecha_mostrar(fecha_iso, fecha_disp, sizeof(fecha_disp));

        ui_printf_centered_line("%-4d| %-10s | %-20s | %s",
                                id, fecha_disp, tipo ? tipo : "", prof ? prof : "");
        count++;
    }

    if (count == 0)
    {
        ui_printf_centered_line("No hay controles registrados.");
    }

    sqlite3_finalize(stmt);
}

static void adjuntar_archivo_control(void)
{
    clear_screen();
    print_header("ADJUNTAR ARCHIVO A CONTROL MEDICO");

    printf("Controles medicos disponibles:\n");
    listar_controles_medicos_ids();

    int control_id = input_int("\nID del control (0 para cancelar): ");
    if (control_id == 0)
    {
        return;
    }

    sqlite3_stmt *check;
    if (!preparar_stmt(&check, "SELECT id FROM bienestar_control_medico WHERE id = ?"))
    {
        printf("Error verificando control.\n");
        pause_console();
        return;
    }
    sqlite3_bind_int(check, 1, control_id);
    int existe = sqlite3_step(check) == SQLITE_ROW;
    sqlite3_finalize(check);

    if (!existe)
    {
        printf("No se encontro el control con ID %d.\n", control_id);
        pause_console();
        return;
    }

    printf("\nSe abrira el selector de archivos (imagenes y documentos).\n");
    char ruta_origen[1024] = {0};
    if (!estudio_arch_seleccionar_archivo(ruta_origen, sizeof(ruta_origen)))
    {
        printf("No se selecciono ningun archivo.\n");
        pause_console();
        return;
    }

    const char *ext = app_get_file_extension(ruta_origen);
    if (!estudio_arch_extension_soportada(ext))
    {
        printf("Formato no soportado. Usa: JPG, PNG, BMP, WEBP, PDF, DOCX, DOC, TXT, XLSX o CSV.\n");
        pause_console();
        return;
    }

    const char *images_dir = get_images_dir();
    if (!images_dir)
    {
        printf("No se pudo preparar la carpeta Imagenes.\n");
        pause_console();
        return;
    }

    char nombre_original[260] = {0};
    app_get_file_name_from_path(ruta_origen, nombre_original, sizeof(nombre_original));

    char ts[32] = {0};
    get_timestamp(ts, (int)sizeof(ts));

    char nombre_destino[300] = {0};
    snprintf(nombre_destino, sizeof(nombre_destino), "estudio_%d_%s%s",
             control_id, ts, ext ? ext : "");

    char ruta_destino[1200] = {0};
    app_build_path(ruta_destino, sizeof(ruta_destino), images_dir, nombre_destino);

    int copiado = 0;
    if (estudio_arch_es_imagen(ext))
    {
        char nombre_opt[300] = {0};
        snprintf(nombre_opt, sizeof(nombre_opt), "estudio_%d_%s.jpg", control_id, ts);

        char ruta_destino_opt[1200] = {0};
        app_build_path(ruta_destino_opt, sizeof(ruta_destino_opt), images_dir, nombre_opt);
        if (app_optimize_image_file(ruta_origen, ruta_destino_opt))
        {
            strncpy_s(nombre_destino, sizeof(nombre_destino), nombre_opt, _TRUNCATE);
            strncpy_s(ruta_destino, sizeof(ruta_destino), ruta_destino_opt, _TRUNCATE);
            copiado = 1;
        }
    }

    if (!copiado && !app_copy_binary_file(ruta_origen, ruta_destino))
    {
        printf("No se pudo copiar el archivo.\n");
        pause_console();
        return;
    }

    const char *tipo_archivo = estudio_arch_es_imagen(ext) ? "imagen" : "documento";

    char ruta_relativa_db[320] = {0};
    snprintf(ruta_relativa_db, sizeof(ruta_relativa_db), "Imagenes/%s", nombre_destino);

    char fecha_hoy_buf[32] = {0};
    fecha_hoy(fecha_hoy_buf, sizeof(fecha_hoy_buf));

    const char *sql =
        "INSERT INTO bienestar_estudio_archivo "
        "(control_id, nombre_original, ruta_archivo, tipo_archivo, fecha_subida) "
        "VALUES (?, ?, ?, ?, ?)";

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando insercion."))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, control_id);
    sqlite3_bind_text(stmt, 2, nombre_original, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, ruta_relativa_db, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, tipo_archivo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, fecha_hoy_buf, -1, SQLITE_TRANSIENT);

    finalizar_ejecucion(stmt, "Archivo adjuntado correctamente.", "Error guardando archivo en DB.");
}

static void ver_archivos_control(void)
{
    clear_screen();
    print_header("ARCHIVOS DE CONTROL MEDICO");

    printf("Controles medicos disponibles:\n");
    listar_controles_medicos_ids();

    int control_id = input_int("\nID del control (0 para cancelar): ");
    if (control_id == 0)
    {
        return;
    }

    const char *sql =
        "SELECT id, nombre_original, tipo_archivo, fecha_subida "
        "FROM bienestar_estudio_archivo WHERE control_id = ? ORDER BY id ASC";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando archivos.", 1))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, control_id);

    clear_screen();
    print_header("ARCHIVOS DEL CONTROL");

    ui_printf_centered_line("ID  | Nombre                      | Tipo      | Fecha");
    ui_printf_centered_line("-------------------------------------------------------");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 1);
        const char *tipo = (const char *)sqlite3_column_text(stmt, 2);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 3);

        char fecha_disp[32];
        format_fecha_mostrar(fecha, fecha_disp, sizeof(fecha_disp));

        ui_printf_centered_line("%-4d| %-28s | %-9s | %s",
                                id, nombre ? nombre : "", tipo ? tipo : "", fecha_disp);
        count++;
    }

    sqlite3_finalize(stmt);

    if (count == 0)
    {
        ui_printf_centered_line("No hay archivos adjuntos para este control.");
        pause_console();
        return;
    }

    int archivo_id = input_int("\nID del archivo para abrir (0 para volver): ");
    if (archivo_id == 0)
    {
        return;
    }

    sqlite3_stmt *stmt2;
    if (!preparar_stmt(&stmt2,
                       "SELECT ruta_archivo FROM bienestar_estudio_archivo "
                       "WHERE id = ? AND control_id = ?"))
    {
        printf("Error consultando archivo.\n");
        pause_console();
        return;
    }
    sqlite3_bind_int(stmt2, 1, archivo_id);
    sqlite3_bind_int(stmt2, 2, control_id);

    char ruta_relativa[400] = {0};
    if (sqlite3_step(stmt2) == SQLITE_ROW)
    {
        const unsigned char *r = sqlite3_column_text(stmt2, 0);
        if (r)
        {
            strncpy_s(ruta_relativa, sizeof(ruta_relativa), (const char *)r, _TRUNCATE);
        }
    }
    sqlite3_finalize(stmt2);

    if (ruta_relativa[0] == '\0')
    {
        printf("Archivo no encontrado.\n");
        pause_console();
        return;
    }

    char nombre_archivo[260] = {0};
    app_get_file_name_from_path(ruta_relativa, nombre_archivo, sizeof(nombre_archivo));

    const char *images_dir = get_images_dir();
    if (!images_dir)
    {
        printf("No se pudo obtener la carpeta de imagenes.\n");
        pause_console();
        return;
    }

    char ruta_absoluta[1200] = {0};
    app_build_path(ruta_absoluta, sizeof(ruta_absoluta), images_dir, nombre_archivo);

    if (!menuimg_abrir_imagen_en_sistema(ruta_absoluta))
    {
        printf("No se pudo abrir el archivo en el sistema.\n");
        pause_console();
        return;
    }

    printf("Abriendo archivo...\n");
    pause_console();
}

static void eliminar_archivo_control(void)
{
    clear_screen();
    print_header("ELIMINAR ARCHIVO DE CONTROL MEDICO");

    printf("Controles medicos disponibles:\n");
    listar_controles_medicos_ids();

    int control_id = input_int("\nID del control (0 para cancelar): ");
    if (control_id == 0)
    {
        return;
    }

    const char *sql =
        "SELECT id, nombre_original, tipo_archivo "
        "FROM bienestar_estudio_archivo WHERE control_id = ? ORDER BY id ASC";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando archivos.", 1))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, control_id);

    ui_printf_centered_line("ID  | Nombre                      | Tipo");
    ui_printf_centered_line("-------------------------------------------");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 1);
        const char *tipo = (const char *)sqlite3_column_text(stmt, 2);

        ui_printf_centered_line("%-4d| %-28s | %s",
                                id, nombre ? nombre : "", tipo ? tipo : "");
        count++;
    }

    sqlite3_finalize(stmt);

    if (count == 0)
    {
        ui_printf_centered_line("No hay archivos adjuntos para este control.");
        pause_console();
        return;
    }

    int archivo_id = input_int("\nID del archivo a eliminar (0 para cancelar): ");
    if (archivo_id == 0)
    {
        return;
    }

    if (!confirmar("Confirmar eliminacion del archivo"))
    {
        printf("Operacion cancelada.\n");
        pause_console();
        return;
    }

    sqlite3_stmt *stmt2;
    if (!preparar_stmt_con_mensaje(&stmt2,
                                   "DELETE FROM bienestar_estudio_archivo "
                                   "WHERE id = ? AND control_id = ?",
                                   "Error preparando eliminacion."))
    {
        return;
    }
    sqlite3_bind_int(stmt2, 1, archivo_id);
    sqlite3_bind_int(stmt2, 2, control_id);

    finalizar_ejecucion(stmt2, "Archivo eliminado del registro.", "Error eliminando archivo.");
}

static void menu_salud(void)
{
    MenuItem items[] =
    {
        {1, "Ver perfil de salud", &mostrar_salud_perfil},
        {2, "Actualizar perfil", &actualizar_salud_perfil},
        {3, "Registrar control medico", &registrar_control_medico},
        {4, "Ver controles medicos", &listar_controles_medicos},
        {5, "Editar control medico", &editar_control_medico},
        {6, "Eliminar control medico", &eliminar_control_medico},
        {7, "Adjuntar archivo a control", &adjuntar_archivo_control},
        {8, "Ver archivos de control", &ver_archivos_control},
        {9, "Eliminar archivo de control", &eliminar_archivo_control},
        {10, "Cargar imagen del menu", &cargar_imagen_menu_salud},
        {11, "Ver imagen del menu", &ver_imagen_menu_salud},
        {0, "Volver", NULL}
    };

    clear_screen();
    print_header("SALUD");
    ejecutar_menu_estandar("SALUD", items, 12);
}

static void guardar_recomendacion(const char *fecha, int score, int riesgo, const char *resumen, const char *rutina)
{
    const char *sql =
        "INSERT INTO bienestar_recomendacion "
        "(fecha, score_preparacion, riesgo_lesion, resumen, rutina) "
        "VALUES (?, ?, ?, ?, ?)";

    sqlite3_stmt *stmt;
    if (!preparar_stmt_con_mensaje(&stmt, sql, "Error preparando insercion."))
    {
        return;
    }

    sqlite3_bind_text(stmt, 1, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, score);
    sqlite3_bind_int(stmt, 3, riesgo);
    sqlite3_bind_text(stmt, 4, resumen, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, rutina, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        printf("Error guardando recomendacion.\n");
    }
    sqlite3_finalize(stmt);
}

static void listar_recomendaciones_entrenamiento(void)
{
    clear_screen();
    print_header("HISTORIAL RECOMENDACIONES");

    const char *sql =
        "SELECT id, fecha, score_preparacion, riesgo_lesion, resumen "
        "FROM bienestar_recomendacion "
        "ORDER BY id DESC LIMIT 10";

    sqlite3_stmt *stmt;
    if (!preparar_consulta(&stmt, sql, "Error consultando recomendaciones.", 1))
    {
        return;
    }

    ui_printf_centered_line("%-4s %-12s %-6s %-7s %s",
                            "ID", "Fecha", "Prep", "Riesgo", "Resumen");
    ui_printf_centered_line("---- ------------ ------ ------- --------------------------------");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 1);
        int score = sqlite3_column_int(stmt, 2);
        int riesgo = sqlite3_column_int(stmt, 3);
        const char *resumen = (const char *)sqlite3_column_text(stmt, 4);

        char fecha_disp[32];
        format_fecha_mostrar(fecha, fecha_disp, sizeof(fecha_disp));
        ui_printf_centered_line("%-4d %-12s %-6d %-7d %s", id, fecha_disp, score, riesgo, resumen ? resumen : "");
        count++;
    }

    sqlite3_finalize(stmt);

    if (count == 0)
    {
        ui_printf_centered_line("No hay recomendaciones guardadas.");
        pause_console();
        return;
    }

    printf("\nID para ver rutina completa (0 para volver): ");
    int sel = input_int("");
    if (sel <= 0)
    {
        return;
    }

    const char *sql_detalle =
        "SELECT fecha, score_preparacion, riesgo_lesion, resumen, rutina "
        "FROM bienestar_recomendacion WHERE id = ?";

    if (!preparar_consulta(&stmt, sql_detalle, "Error consultando detalle.", 1))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, sel);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *fecha = (const char *)sqlite3_column_text(stmt, 0);
        int score = sqlite3_column_int(stmt, 1);
        int riesgo = sqlite3_column_int(stmt, 2);
        const char *resumen = (const char *)sqlite3_column_text(stmt, 3);
        const char *rutina = (const char *)sqlite3_column_text(stmt, 4);

        char fecha_disp[32];
        format_fecha_mostrar(fecha, fecha_disp, sizeof(fecha_disp));

        clear_screen();
        print_header("RUTINA RECOMENDADA");
        printf("Fecha: %s\n", fecha_disp);
        printf("Preparacion: %d/100 | Riesgo: %d/100\n", score, riesgo);
        printf("Resumen: %s\n\n", resumen ? resumen : "");
        printf("%s\n", rutina ? rutina : "(Sin rutina)");
    }
    else
    {
        printf("Recomendacion no encontrada.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

static void construir_rutina_texto(char *rutina, size_t size, int nivel, int lesiones_activas, const char *objetivo)
{
    size_t used = 0;
    const char *objetivo_texto = (objetivo && objetivo[0]) ? objetivo : "Sin objetivo activo";
    char linea_objetivo[256];

    snprintf(linea_objetivo, sizeof(linea_objetivo), "Objetivo: %s\n", objetivo_texto);
    append_text(rutina, size, &used, linea_objetivo);

    if (lesiones_activas > 0)
    {
        append_text(rutina, size, &used, "Enfoque: recuperacion y movilidad.\n");
        append_text(rutina, size, &used, "Lunes: movilidad + estiramientos (30-40 min)\n");
        append_text(rutina, size, &used, "Martes: tecnica suave (30 min)\n");
        append_text(rutina, size, &used, "Miercoles: descanso\n");
        append_text(rutina, size, &used, "Jueves: fuerza ligera + estabilidad (30-40 min)\n");
        append_text(rutina, size, &used, "Viernes: movilidad (30 min)\n");
        append_text(rutina, size, &used, "Sabado: descanso activo (caminar)\n");
        append_text(rutina, size, &used, "Domingo: descanso\n");
        return;
    }

    if (nivel >= 2)
    {
        append_text(rutina, size, &used, "Semana de carga alta.\n");
        append_text(rutina, size, &used, "Lunes: fuerza + tecnica (70-90 min)\n");
        append_text(rutina, size, &used, "Martes: resistencia (60 min)\n");
        append_text(rutina, size, &used, "Miercoles: tecnica + velocidad (60 min)\n");
        append_text(rutina, size, &used, "Jueves: fuerza + estabilidad (60 min)\n");
        append_text(rutina, size, &used, "Viernes: partido/simulacion (70 min)\n");
        append_text(rutina, size, &used, "Sabado: movilidad (30 min)\n");
        append_text(rutina, size, &used, "Domingo: descanso\n");
    }
    else if (nivel == 1)
    {
        append_text(rutina, size, &used, "Semana de carga moderada.\n");
        append_text(rutina, size, &used, "Lunes: fuerza (60 min)\n");
        append_text(rutina, size, &used, "Martes: tecnica (50 min)\n");
        append_text(rutina, size, &used, "Miercoles: descanso activo\n");
        append_text(rutina, size, &used, "Jueves: resistencia (50 min)\n");
        append_text(rutina, size, &used, "Viernes: movilidad (30 min)\n");
        append_text(rutina, size, &used, "Sabado: partido/amisto suave\n");
        append_text(rutina, size, &used, "Domingo: descanso\n");
    }
    else
    {
        append_text(rutina, size, &used, "Semana de carga baja.\n");
        append_text(rutina, size, &used, "Lunes: movilidad (30 min)\n");
        append_text(rutina, size, &used, "Martes: tecnica ligera (40 min)\n");
        append_text(rutina, size, &used, "Miercoles: descanso\n");
        append_text(rutina, size, &used, "Jueves: resistencia suave (40 min)\n");
        append_text(rutina, size, &used, "Viernes: estiramientos (25 min)\n");
        append_text(rutina, size, &used, "Sabado: descanso\n");
        append_text(rutina, size, &used, "Domingo: descanso\n");
    }
}

typedef struct
{
    double avg_rend;
    int lesiones_activas;
    double hab_dormi;
    double hab_hid;
    double hab_alc;
    double hab_nerv;
    double hab_conf;
    double hab_mot;
    double comida_avg;
    double dia_hid_avg;
    double dia_alc_avg;
    int entreno_count;
    double entreno_int_avg;
    double entreno_dur_avg;
    int entreno_intenso_count;
    double mental_conf;
    double mental_estres;
    double mental_mot;
    double mental_pres;
    double mental_conc;
} AsistenteMetricas;

static void cargar_metricas_habitos(AsistenteMetricas *metricas)
{
    const char *sql =
        "SELECT AVG(dormi_bien), AVG(hidratacion), AVG(alcohol), AVG(nervios), AVG(confianza), AVG(motivacion) "
        "FROM (SELECT dormi_bien, hidratacion, alcohol, nervios, confianza, motivacion "
        "FROM bienestar_habito ORDER BY fecha DESC LIMIT 14);";
    sqlite3_stmt *stmt;
    if (preparar_stmt(&stmt, sql))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            metricas->hab_dormi = sqlite3_column_double(stmt, 0);
            metricas->hab_hid = sqlite3_column_double(stmt, 1);
            metricas->hab_alc = sqlite3_column_double(stmt, 2);
            metricas->hab_nerv = sqlite3_column_double(stmt, 3);
            metricas->hab_conf = sqlite3_column_double(stmt, 4);
            metricas->hab_mot = sqlite3_column_double(stmt, 5);
        }
        sqlite3_finalize(stmt);
    }
}

static void cargar_metricas_nutricion(AsistenteMetricas *metricas)
{
    obtener_double(
        "SELECT AVG(CASE calidad WHEN 'Buena' THEN 3 WHEN 'Regular' THEN 2 ELSE 1 END) "
        "FROM (SELECT calidad FROM bienestar_comida ORDER BY fecha DESC, id DESC LIMIT 14);",
        &metricas->comida_avg);

    const char *sql =
        "SELECT AVG(CASE hidratacion WHEN 'Alta' THEN 3 WHEN 'Media' THEN 2 ELSE 1 END), AVG(alcohol) "
        "FROM (SELECT hidratacion, alcohol FROM bienestar_dia_nutricional ORDER BY fecha DESC LIMIT 14);";
    sqlite3_stmt *stmt;
    if (preparar_stmt(&stmt, sql))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            metricas->dia_hid_avg = sqlite3_column_double(stmt, 0);
            metricas->dia_alc_avg = sqlite3_column_double(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }
}

static void cargar_metricas_entrenamiento(AsistenteMetricas *metricas)
{
    const char *sql =
        "SELECT COUNT(*), AVG(intensidad), AVG(duracion_min) "
        "FROM (SELECT intensidad, duracion_min FROM bienestar_entrenamiento WHERE omitido = 0 "
        "ORDER BY fecha DESC LIMIT 14);";
    sqlite3_stmt *stmt;
    if (preparar_stmt(&stmt, sql))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            metricas->entreno_count = sqlite3_column_int(stmt, 0);
            metricas->entreno_int_avg = sqlite3_column_double(stmt, 1);
            metricas->entreno_dur_avg = sqlite3_column_double(stmt, 2);
        }
        sqlite3_finalize(stmt);
    }

    obtener_int("SELECT COUNT(*) FROM bienestar_entrenamiento WHERE omitido = 0 AND intensidad >= 8 AND fecha >= date('now', '-6 day');",
                &metricas->entreno_intenso_count);
}

static void cargar_metricas_mentales(AsistenteMetricas *metricas)
{
    const char *sql =
        "SELECT AVG(confianza), AVG(estres), AVG(motivacion), AVG(presion), AVG(concentracion) "
        "FROM (SELECT confianza, estres, motivacion, presion, concentracion "
        "FROM bienestar_sesion_mental ORDER BY fecha DESC, id DESC LIMIT 10);";
    sqlite3_stmt *stmt;
    if (preparar_stmt(&stmt, sql))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            metricas->mental_conf = sqlite3_column_double(stmt, 0);
            metricas->mental_estres = sqlite3_column_double(stmt, 1);
            metricas->mental_mot = sqlite3_column_double(stmt, 2);
            metricas->mental_pres = sqlite3_column_double(stmt, 3);
            metricas->mental_conc = sqlite3_column_double(stmt, 4);
        }
        sqlite3_finalize(stmt);
    }
}

static void cargar_objetivo_activo(char *objetivo, size_t size)
{
    const char *sql =
        "SELECT nombre FROM bienestar_objetivo WHERE estado = 'Activo' "
        "ORDER BY fecha_fin DESC LIMIT 1";
    sqlite3_stmt *stmt;
    if (preparar_stmt(&stmt, sql))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
            if (nombre && nombre[0])
            {
                snprintf(objetivo, size, "%s", nombre);
            }
        }
        sqlite3_finalize(stmt);
    }
}

static void cargar_metricas_asistente(AsistenteMetricas *metricas)
{
    memset(metricas, 0, sizeof(*metricas));
    obtener_double("SELECT AVG(rendimiento_general) FROM (SELECT rendimiento_general FROM partido ORDER BY fecha_hora DESC LIMIT 10);", &metricas->avg_rend);
    obtener_int("SELECT COUNT(*) FROM lesion WHERE estado = 'Activa';", &metricas->lesiones_activas);
    cargar_metricas_habitos(metricas);
    cargar_metricas_nutricion(metricas);
    cargar_metricas_entrenamiento(metricas);
    cargar_metricas_mentales(metricas);
}

static double calcular_score_entrenamiento(int entreno_count, double entreno_int_avg, double entreno_dur_avg)
{
    double score_ent = 40.0;

    if (entreno_count <= 0)
    {
        return score_ent;
    }

    if (entreno_int_avg >= 8.0)
    {
        score_ent = 55.0;
    }
    else if (entreno_int_avg >= 6.0)
    {
        score_ent = 75.0;
    }
    else if (entreno_int_avg >= 4.0)
    {
        score_ent = 70.0;
    }
    else
    {
        score_ent = 60.0;
    }

    if (entreno_dur_avg > 90.0)
    {
        score_ent -= 5.0;
    }
    if (entreno_dur_avg < 30.0)
    {
        score_ent -= 5.0;
    }

    return score_ent;
}

static void calcular_asistente_scores(const AsistenteMetricas *metricas, double *prep_score, int *riesgo, int *nivel)
{
    double score_rend = clamp_double(metricas->avg_rend * 10.0, 0.0, 100.0);
    double dormi_score = metricas->hab_dormi * 100.0;
    double hid_score = (metricas->hab_hid / 10.0) * 100.0;
    double nerv_score = ((10.0 - metricas->hab_nerv) / 10.0) * 100.0;
    double conf_score = (metricas->hab_conf / 10.0) * 100.0;
    double mot_score = (metricas->hab_mot / 10.0) * 100.0;
    double score_hab = (dormi_score + hid_score + nerv_score + conf_score + mot_score) / 5.0;
    score_hab = clamp_double(score_hab - (metricas->hab_alc * 15.0), 0.0, 100.0);

    double comida_score = ((metricas->comida_avg - 1.0) / 2.0) * 100.0;
    double dia_hid_score = ((metricas->dia_hid_avg - 1.0) / 2.0) * 100.0;
    comida_score = clamp_double(comida_score, 0.0, 100.0);
    dia_hid_score = clamp_double(dia_hid_score, 0.0, 100.0);
    double score_nut = (comida_score + dia_hid_score) / 2.0;
    score_nut = clamp_double(score_nut - (metricas->dia_alc_avg * 15.0), 0.0, 100.0);

    double score_ent = calcular_score_entrenamiento(metricas->entreno_count, metricas->entreno_int_avg, metricas->entreno_dur_avg);
    score_ent = clamp_double(score_ent, 0.0, 100.0);

    double mental_pos = (metricas->mental_conf + metricas->mental_mot + metricas->mental_conc) / 3.0;
    double mental_neg = (metricas->mental_estres + metricas->mental_pres) / 2.0;
    double score_mental = (mental_pos / 10.0) * 100.0 - (mental_neg / 10.0) * 30.0;
    score_mental = clamp_double(score_mental, 0.0, 100.0);

    *prep_score =
        (score_rend * 0.30) + (score_hab * 0.20) + (score_nut * 0.20) + (score_mental * 0.20) + (score_ent * 0.10);
    *prep_score = clamp_double(*prep_score, 0.0, 100.0);

    if (metricas->lesiones_activas > 0)
    {
        *riesgo = clamp_int(70 + (metricas->lesiones_activas * 5), 70, 100);
    }
    else
    {
        double base = (100.0 - *prep_score) * 0.6 + (metricas->mental_estres * 3.0) + (metricas->entreno_intenso_count * 5.0);
        *riesgo = clamp_int((int)(base + 0.5), 0, 100);
    }

    *nivel = 0;
    if (metricas->lesiones_activas == 0 && *prep_score >= 75.0)
    {
        *nivel = 2;
    }
    else if (metricas->lesiones_activas == 0 && *prep_score >= 50.0)
    {
        *nivel = 1;
    }
}

static void generar_asistente_entrenamiento(void)
{
    clear_screen();
    print_header("ASISTENTE ENTRENAMIENTOS");

    AsistenteMetricas metricas;
    cargar_metricas_asistente(&metricas);

    double prep_score = 0.0;
    int riesgo = 0;
    int nivel = 0;
    calcular_asistente_scores(&metricas, &prep_score, &riesgo, &nivel);

    char objetivo[128] = {0};
    cargar_objetivo_activo(objetivo, sizeof(objetivo));

    char rutina[2048];
    rutina[0] = '\0';
    construir_rutina_texto(rutina, sizeof(rutina), nivel, metricas.lesiones_activas, objetivo);
    if (riesgo >= 70)
    {
        size_t used = safe_strnlen(rutina, sizeof(rutina));
        append_text(rutina, sizeof(rutina), &used, "\nAlerta: riesgo de lesion elevado. Prioriza recuperacion.\n");
    }

    char resumen[256];
    snprintf(resumen, sizeof(resumen), "Prep %d/100 | Riesgo %d/100 | Lesiones %d | Rend %.1f",
             (int)(prep_score + 0.5), riesgo, metricas.lesiones_activas, metricas.avg_rend);

    char fecha_iso[16];
    fecha_hoy(fecha_iso, sizeof(fecha_iso));
    guardar_recomendacion(fecha_iso, (int)(prep_score + 0.5), riesgo, resumen, rutina);

    printf("Resumen: %s\n", resumen);
    printf("\nRutina sugerida:\n%s\n", rutina);
    pause_console();
}

static void menu_asistente_entrenamiento(void)
{
    MenuItem items[] =
    {
        {1, "Generar rutina personalizada", &generar_asistente_entrenamiento},
        {2, "Ver historial de recomendaciones", &listar_recomendaciones_entrenamiento},
        {0, "Volver", NULL}
    };

    clear_screen();
    print_header("ASISTENTE ENTRENAMIENTOS");
    ejecutar_menu_estandar("ASISTENTE ENTRENAMIENTOS", items, 3);
}

void menu_bienestar(void)
{
    MenuItem items[] =
    {
        {1, "Planificacion Personal",&mostrar_planificacion_personal},
        {2, "Mentalidad y Habitos",&mostrar_mentalidad_habitos},
        {3, "Entrenamiento", &mostrar_entrenamiento_expandido},
        {4, "Alimentacion", &mostrar_alimentacion_expandido},
        {5, "Asistente Entrenamientos Personalizados", &menu_asistente_entrenamiento},
        {6, "Mental", &mostrar_mental_deportivo},
        {7, "Informe Personal Mensual (PDF)", &informe_personal_mensual_pdf},
        {8, "Salud", &menu_salud},
        {0, "Volver", NULL}
    };

    clear_screen();
    print_header("BIENESTAR");
    ejecutar_menu_estandar("BIENESTAR", items, 9);
}
