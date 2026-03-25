#include "menu.h"
#include "utils.h"
#include "gui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <ctype.h>
#ifdef _WIN32
#ifdef _WIN32
#include <windows.h>
#else
#include "compat_windows.h"
#endif
#endif
#include "db.h"
#include "camiseta.h"
#include "cancha.h"
#include "partido.h"
#include "lesion.h"
#include "estadisticas.h"
#include "analisis.h"
#include "logros.h"
#include "export.h"
#include "export_all.h"
#include "import.h"
#include "equipo.h"
#include "torneo.h"
#include "temporada.h"
#include "ascii_art.h"
#include "settings.h"
#include "financiamiento.h"
#include "entrenador_ia.h"
#include "bienestar.h"
#include "carrera.h"

// Definir items del menu principal directamente con inicializacion static
struct MenuItemDefinition
{
    int opcion;
    const char* texto;
    void (*accion)(void);
    MenuCategory categoria;
};

static void abrir_menu_equipos(void)
{
    app_log_event("EQUIPOS", "Ingreso al modulo Equipos");
    menu_equipos();
}

static void abrir_menu_partidos(void)
{
    app_log_event("PARTIDOS", "Ingreso al modulo Partidos");
    menu_partidos();
}

static void abrir_menu_lesiones(void)
{
    app_log_event("LESIONES", "Ingreso al modulo Lesiones");
    menu_lesiones();
}

static void abrir_menu_estadisticas(void)
{
    app_log_event("ESTADISTICAS", "Ingreso al modulo Estadisticas");
    menu_estadisticas();
}

static void abrir_menu_logros(void)
{
    app_log_event("LOGROS", "Ingreso al modulo Logros");
    menu_logros();
}

static void abrir_menu_financiamiento(void)
{
    app_log_event("FINANCIAMIENTO", "Ingreso al modulo Financiamiento");
    menu_financiamiento();
}

static void abrir_menu_torneos(void)
{
    app_log_event("TORNEOS", "Ingreso al modulo Torneos");
    menu_torneos();
}

static void abrir_menu_temporadas(void)
{
    app_log_event("TEMPORADA", "Ingreso al modulo Temporada");
    menu_temporadas();
}

static void abrir_menu_analisis(void)
{
    app_log_event("ANALISIS", "Ingreso al modulo Analisis");
    mostrar_analisis();
}

static void abrir_menu_bienestar(void)
{
    app_log_event("BIENESTAR", "Ingreso al modulo Bienestar");
    menu_bienestar();
}

static void abrir_menu_settings(void)
{
    app_log_event("SETTINGS", "Ingreso al modulo Ajustes");
    menu_settings();
}

static void abrir_menu_carrera(void)
{
    app_log_event("CARRERA", "Ingreso al modulo Carrera Futbolistica");
    menu_carrera_futbolistica();
}

static const struct MenuItemDefinition MENU_ITEMS[] =
{
    {1, "Camisetas", &menu_camisetas, MENU_CATEGORY_GESTION},
    {2, "Canchas", &menu_canchas, MENU_CATEGORY_GESTION},
    {3, "Equipos", &abrir_menu_equipos, MENU_CATEGORY_GESTION},
    {4, "Partidos", &abrir_menu_partidos, MENU_CATEGORY_GESTION},
    {5, "Lesiones", &abrir_menu_lesiones, MENU_CATEGORY_GESTION},
    {6, "Estadisticas", &abrir_menu_estadisticas, MENU_CATEGORY_ANALISIS},
    {7, "Logros", &abrir_menu_logros, MENU_CATEGORY_ANALISIS},
    {9, "Financiamiento", &abrir_menu_financiamiento, MENU_CATEGORY_ADMIN},
    {10, "Torneos", &abrir_menu_torneos, MENU_CATEGORY_COMPETENCIA},
    {11, "Temporada", &abrir_menu_temporadas, MENU_CATEGORY_COMPETENCIA},
    {12, "Analisis", &abrir_menu_analisis, MENU_CATEGORY_ANALISIS},
    {13, "Bienestar", &abrir_menu_bienestar, MENU_CATEGORY_ANALISIS},
    {14, "Carrera Futbolistica", &abrir_menu_carrera, MENU_CATEGORY_COMPETENCIA},
    {15, "Ajustes", &abrir_menu_settings, MENU_CATEGORY_ADMIN},
    {0, "Salir", NULL, MENU_CATEGORY_ADMIN}
};

// Numero de items en el menu principal
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
static const size_t MENU_ITEM_COUNT = ARRAY_SIZE(MENU_ITEMS);
static int g_gui_menus_enabled = 0;

void menu_set_gui_enabled(int enabled)
{
    g_gui_menus_enabled = (enabled != 0);
}

int menu_is_gui_enabled(void)
{
    return g_gui_menus_enabled;
}

static const MenuItem *buscar_item(const MenuItem *items, int cantidad, int opcion)
{
    for (int i = 0; i < cantidad; i++)
    {
        if (items[i].opcion == opcion)
        {
            return &items[i];
        }
    }
    return NULL;
}


#ifdef UNIT_TEST
static MenuTestCapture *g_menu_test_capture = NULL;

int menu_get_item_count(void)
{
    return (int)MENU_ITEM_COUNT;
}

const MenuItem *menu_get_items(void)
{
    return (const MenuItem *)MENU_ITEMS;
}

const MenuItem *menu_buscar_item(const MenuItem *items, int cantidad, int opcion)
{
    return buscar_item(items, cantidad, opcion);
}

void menu_test_set_capture(MenuTestCapture *capture)
{
    g_menu_test_capture = capture;
}
#endif

/**
 * @brief Inicializa la aplicacion: consola, locale, base de datos y configuracion
 */
void initialize_application()
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
    setlocale(LC_ALL, "");

    if (!db_init())
    {
        printf("Error inicializando base de datos\n");
        exit(1);
    }

    if (db_get_active_user()[0] != '\0')
    {
        set_user_name(db_get_active_user());
    }

    settings_init();
    app_log_event("APP", "Aplicacion iniciada");

}

/**
 * @brief Maneja la verificacion y creacion del nombre de usuario
 */
void handle_user_name()
{
    char* nombre_usuario;
    char buffer[256];

    nombre_usuario = get_user_name();

    if (!nombre_usuario && db_get_active_user()[0] != '\0')
    {
        set_user_name(db_get_active_user());
        nombre_usuario = get_user_name();
    }

    if (nombre_usuario)
    {
        snprintf(buffer, sizeof(buffer), "%s, %s\n", get_text("welcome_message"), nombre_usuario);
        snprintf(buffer, sizeof(buffer), "Sesion iniciada por usuario: %.180s", nombre_usuario);
        app_log_event("APP", buffer);
        snprintf(buffer, sizeof(buffer), "%s, %s\n", get_text("welcome_message"), nombre_usuario);
        fputs(buffer, stdout);
        pause_console();
        free(nombre_usuario);
    }
}

/**
 * @brief Crea el menu filtrado dinamicamente
 */
MenuItem* create_filtered_menu(int* count)
{
    *count = (int)MENU_ITEM_COUNT;

    MenuItem* filtered_items = (MenuItem*)calloc((size_t)(*count), sizeof(MenuItem));
    if (!filtered_items)
    {
        printf("Error de memoria\n");
        db_close();
        exit(1);
    }

    for (int i = 0; i < *count; i++)
    {
        filtered_items[i].opcion = (MENU_ITEMS[i].opcion == 0) ? 0 : (i + 1);
        filtered_items[i].texto = MENU_ITEMS[i].texto;
        filtered_items[i].accion = MENU_ITEMS[i].accion;
        filtered_items[i].categoria = MENU_ITEMS[i].categoria;
    }

    return filtered_items;
}

/**
 * @brief Ejecuta el menu principal y libera recursos
 */
void run_menu(MenuItem* filtered_items, int count)
{
    ejecutar_menu(get_text("menu_title"), filtered_items, count);
    free(filtered_items);
    db_close();
}

static const char *menu_safe_title(const char *titulo)
{
    return titulo ? titulo : "(sin titulo)";
}

#ifdef UNIT_TEST
static int manejar_captura_test_menu(const char *titulo, const MenuItem *items, int cantidad)
{
    if (!g_menu_test_capture)
        return 0;

    g_menu_test_capture->titulo = titulo;
    g_menu_test_capture->cantidad = cantidad;
    if (items && cantidad > 0)
    {
        g_menu_test_capture->last_item = items[cantidad - 1];
    }
    else
    {
        g_menu_test_capture->last_item.opcion = 0;
        g_menu_test_capture->last_item.texto = NULL;
        g_menu_test_capture->last_item.accion = NULL;
        g_menu_test_capture->last_item.categoria = MENU_CATEGORY_ALL;
    }

    return 1;
}
#endif

static void log_menu_opcion_invalida(const char *titulo, int opcion, char *log_msg, size_t log_size)
{
    snprintf(log_msg, log_size, "Menu %.120s -> opcion invalida: %d", menu_safe_title(titulo), opcion);
    app_log_event("MENU", log_msg);
}

static int ejecutar_accion_menu(const char *titulo, const MenuItem *selected, char *log_msg, size_t log_size)
{
    snprintf(log_msg, log_size, "Menu %.120s -> opcion %d (%.120s)",
             menu_safe_title(titulo),
             selected->opcion,
             selected->texto ? selected->texto : "(sin texto)");
    app_log_event("MENU", log_msg);

    if (!selected->accion)
    {
        snprintf(log_msg, log_size, "Salida del menu: %.180s", menu_safe_title(titulo));
        app_log_event("MENU", log_msg);
        return 0;
    }

    selected->accion();
    return 1;
}

/**
 * @brief Ejecuta un menu interactivo en la consola
 *
 * Esta funcion muestra un menu con el titulo proporcionado y una lista de opciones.
 * Permite al usuario seleccionar una opcion y ejecuta la accion correspondiente.
 * Si la accion es NULL, sale del menu.
 */
void ejecutar_menu(const char *titulo, const MenuItem *items, int cantidad)
{
#ifdef UNIT_TEST
    if (manejar_captura_test_menu(titulo, items, cantidad))
        return;
#endif

#ifdef ENABLE_RAYLIB_GUI
    if (g_gui_menus_enabled)
    {
        gui_set_context_title(menu_safe_title(titulo));
        while (1)
        {
            int gui_result = run_raylib_gui(items, cantidad);
            int selected_index = gui_get_last_selected_index();

            if (gui_result == GUI_ACTION_EXIT)
            {
                db_close();
                exit(0);
            }

            if (gui_result == GUI_ACTION_OPEN_CLASSIC_MENU)
            {
                break;
            }

            if (gui_result == GUI_ACTION_RUN_SELECTED_OPTION)
            {
                const MenuItem *selected = NULL;

                if (selected_index < 0 || selected_index >= cantidad)
                    continue;

                selected = &items[selected_index];
                if (!selected->accion)
                    return;

                selected->accion();
                continue;
            }

            return;
        }
    }
#endif

    int opcion;
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "Ingreso al menu: %.180s", menu_safe_title(titulo));
    app_log_event("MENU", log_msg);

    while (1)
    {
        clear_screen();
        print_header(titulo);

        for (int i = 0; i < cantidad; i++)
        {
            printf("%d.%s\n", items[i].opcion, items[i].texto);
        }

        opcion = input_int(">");

        if (opcion == -1)
        {
            snprintf(log_msg, sizeof(log_msg),
                     "Menu %.120s -> entrada no valida repetida/EOF, salida preventiva",
                     menu_safe_title(titulo));
            app_log_event("MENU", log_msg);
            return;
        }

        const MenuItem *selected = buscar_item(items, cantidad, opcion);
        if (!selected)
        {
            log_menu_opcion_invalida(titulo, opcion, log_msg, sizeof(log_msg));
            continue;
        }

        if (!ejecutar_accion_menu(titulo, selected, log_msg, sizeof(log_msg)))
            return;
    }
}
