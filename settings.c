/**
 * @file settings.c
 * @brief Implementacion del sistema de configuracion avanzada
 *
 * Incluye temas de interfaz, internacionalizacion y persistencia en base de datos.
 */

#include "settings.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "export.h"
#include "export_all.h"
#include "import.h"
#include "ascii_art.h"
#include "cJSON.h"
#ifdef _WIN32
#include <windows.h>
#else
#include "compat_windows.h"
#endif
#ifdef _WIN32
#include <shellapi.h>
#endif
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
typedef HRESULT (WINAPI *URLDownloadToFileAFunc)(LPUNKNOWN, LPCSTR, LPCSTR, DWORD, LPVOID);
#endif

void menu_exportar();
void menu_update();

// Repositorio usado para buscar la release. Formato: owner/repo
// Si deseas cambiar, modifica esta constante.
#define UPDATE_REPO "thomashamer3/MiFutbolC"

// Version actual de la aplicacion. Debe mantenerse en sincronia con el instalador (MiFutbolC.iss)
#define APP_VERSION "3.9"

// Configuracion global
static AppSettings current_settings = {THEME_LIGHT, LANGUAGE_SPANISH, MODE_SIMPLE, TEXT_SIZE_MEDIUM};

// Flag para rastrear cambios en menu personalizado
static int custom_menu_changed = 0;

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, 0) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

static void settings_apply_text_size();
static void habilitar_menus_basicos_custom(void);
static void confirmar_guardado_si_cambios(void);

static void set_theme_int(int value)
{
    settings_set_theme((ThemeType)value);
}

static void set_language_int(int value)
{
    settings_set_language((LanguageType)value);
}

static void set_mode_int(int value)
{
    settings_set_mode((ModeType)value);
}

static void set_text_size_int(int value)
{
    settings_set_text_size((TextSizeType)value);
}

static void aplicar_config_y_pausar(void (*setter)(int), int value)
{
    setter(value);
    printf("%s\n", get_text("settings_saved"));
    pause_console();
}

static void aplicar_tema_texto_y_pausar(ThemeType theme, TextSizeType text_size)
{
    settings_set_theme(theme);
    settings_set_text_size(text_size);
    printf("%s\n", get_text("settings_saved"));
    pause_console();
}

/* Wrappers para usar como acciones en MenuItem (sin argumentos) */
static void theme_set_light()
{
    aplicar_config_y_pausar(set_theme_int, THEME_LIGHT);
}
static void theme_set_dark()
{
    aplicar_config_y_pausar(set_theme_int, THEME_DARK);
}
static void theme_set_blue()
{
    aplicar_config_y_pausar(set_theme_int, THEME_BLUE);
}
static void theme_set_green()
{
    aplicar_config_y_pausar(set_theme_int, THEME_GREEN);
}
static void theme_set_red()
{
    aplicar_config_y_pausar(set_theme_int, THEME_RED);
}
static void theme_set_purple()
{
    aplicar_config_y_pausar(set_theme_int, THEME_PURPLE);
}
static void theme_set_classic()
{
    aplicar_config_y_pausar(set_theme_int, THEME_CLASSIC);
}
static void theme_set_high_contrast()
{
    aplicar_config_y_pausar(set_theme_int, THEME_HIGH_CONTRAST);
}

static void lang_set_spanish()
{
    aplicar_config_y_pausar(set_language_int, LANGUAGE_SPANISH);
}
static void lang_set_english()
{
    aplicar_config_y_pausar(set_language_int, LANGUAGE_ENGLISH);
}

static void text_size_small()
{
    aplicar_config_y_pausar(set_text_size_int, TEXT_SIZE_SMALL);
}
static void text_size_medium()
{
    aplicar_config_y_pausar(set_text_size_int, TEXT_SIZE_MEDIUM);
}
static void text_size_large()
{
    aplicar_config_y_pausar(set_text_size_int, TEXT_SIZE_LARGE);
}

static void accessibility_high_contrast()
{
    aplicar_config_y_pausar(set_theme_int, THEME_HIGH_CONTRAST);
}
static void accessibility_normal_theme_text()
{
    aplicar_tema_texto_y_pausar(THEME_LIGHT, TEXT_SIZE_MEDIUM);
}

static void mode_set_simple()
{
    aplicar_config_y_pausar(set_mode_int, MODE_SIMPLE);
}
static void mode_set_advanced()
{
    aplicar_config_y_pausar(set_mode_int, MODE_ADVANCED);
}
static void mode_set_custom()
{
    current_settings.mode = MODE_CUSTOM;
    habilitar_menus_basicos_custom();
    menu_custom_menus();
    confirmar_guardado_si_cambios();
    pause_console();
}

static void habilitar_menus_basicos_custom()
{
    set_custom_menu_enabled("camisetas", 1);
    set_custom_menu_enabled("canchas", 1);
    set_custom_menu_enabled("partidos", 1);
    set_custom_menu_enabled("lesiones", 1);
    set_custom_menu_enabled("equipos", 1);
    set_custom_menu_enabled("estadisticas", 1);
    custom_menu_changed = 0; // Reset flag after setting defaults
}

struct MenuOption
{
    int opcion;
    const char* name;
    const char* display_name;
};

static int confirmar_guardado_configuracion(int default_on_fail)
{
    printf("Guardar configuracion? (S/N): ");
    char confirm;
    char input[16];
    if (!fgets(input, sizeof(input), stdin)
#if defined(_WIN32) && defined(_MSC_VER)
            || sscanf_s(input, " %c", &confirm, 1) != 1)
#else
            || sscanf(input, " %c", &confirm) != 1)
#endif
    {
        confirm = default_on_fail ? 'S' : 'N';
    }

    if (confirm == '\n' || confirm == 's' || confirm == 'S')
    {
        settings_save();
        printf("%s\n", get_text("settings_saved"));
        return 1;
    }

    printf("Configuracion no guardada.\n");
    return 0;
}

static void confirmar_guardado_si_cambios()
{
    if (!custom_menu_changed)
    {
        return;
    }

    confirmar_guardado_configuracion(0);
}

static const struct MenuOption* buscar_opcion_menu(const struct MenuOption *options, int opcion)
{
    for (int j = 0; options[j].name != NULL; j++)
    {
        if (options[j].opcion == opcion)
        {
            return &options[j];
        }
    }
    return NULL;
}

// Textos en diferentes idiomas
typedef struct
{
    const char* key;
    const char* spanish;
    const char* english;
} TextEntry;

static const TextEntry text_entries[] =
{
    {"menu_title", "MI FUTBOL C", "MI FUTBOL C"},
    {"menu_camisetas", "Camisetas", "Shirts"},
    {"menu_canchas", "Canchas", "Fields"},
    {"menu_partidos", "Partidos", "Matches"},
    {"menu_equipos", "Equipos", "Teams"},
    {"menu_estadisticas", "Estadisticas", "Statistics"},
    {"menu_logros", "Logros", "Achievements"},
    {"menu_analisis", "Analisis", "Analysis"},
    {"menu_bienestar", "Bienestar", "Wellness"},
    {"menu_lesiones", "Lesiones", "Injuries"},
    {"menu_financiamiento", "Financiamiento", "Financing"},
    {"menu_exportar", "Exportar", "Export"},
    {"menu_importar", "Importar", "Import"},
    {"menu_usuario", "Usuario", "User"},
    {"menu_torneos", "Torneos", "Tournaments"},
    {"menu_temporadas", "Temporadas", "Seasons"},
    {"menu_temporada", "Temporada", "Season"},
    {"menu_qr", "Codigos QR", "QR Codes"},
    {"menu_entrenador_ia", "Entrenador Virtual (IA)", "Virtual Coach (AI)"},
    {"menu_comparador", "Comparador", "Comparator"},
    {"menu_comparaciones", "Comparaciones", "Comparisons"},
    {"menu_settings", "Ajustes", "Settings"},
    {"menu_exit", "Salir", "Exit"},
    {"settings_theme", "Tema de Interfaz", "Interface Theme"},
    {"settings_language", "Idioma", "Language"},
    {"settings_mode", "Modo", "Mode"},
    {"settings_accessibility", "Accesibilidad", "Accessibility"},
    {"settings_visual_mode", "Modo Visual", "Visual Mode"},
    {"visual_mode_classic", "Modo Clasico", "Classic Mode"},
    {"visual_mode_current", "Actual", "Current"},
    {"settings_text_size", "Tamanio de texto", "Text size"},
    {"text_size_small", "Pequenio", "Small"},
    {"text_size_medium", "Mediano", "Medium"},
    {"text_size_large", "Grande", "Large"},
    {"settings_high_contrast", "Alto Contraste", "High Contrast"},
    {"settings_accessibility_normal", "Configuracion normal", "Normal settings"},
    {"mode_simple", "Sencillo", "Simple"},
    {"mode_advanced", "Avanzado", "Advanced"},
    {"mode_custom", "Personalizado", "Custom"},
    {"theme_light", "Claro", "Light"},
    {"theme_dark", "Oscuro", "Dark"},
    {"theme_blue", "Azul", "Blue"},
    {"theme_green", "Verde", "Green"},
    {"theme_red", "Rojo", "Red"},
    {"theme_purple", "Morado", "Purple"},
    {"theme_classic", "Clasico", "Classic"},
    {"theme_high_contrast", "Alto Contraste", "High Contrast"},
    {"lang_spanish", "Espaniol", "Spanish"},
    {"lang_english", "Ingles", "English"},
    {"settings_saved", "Configuracion guardada exitosamente.", "Settings saved successfully."},
    {"invalid_option", "Opcion invalida.", "Invalid option."},
    {"press_enter", "Presione Enter para continuar...", "Press Enter to continue..."},
    {"welcome_back", "Bienvenido De Vuelta", "Welcome Back"},
    {"menu_back", "Volver", "Back"},
    {"export_menu_title", "EXPORTAR DATOS", "EXPORT DATA"},
    {"export_partidos_menu_title", "EXPORTAR PARTIDOS", "EXPORT MATCHES"},
    {"export_estadisticas_generales_menu_title", "EXPORTAR ESTADISTICAS GENERALES", "EXPORT GENERAL STATISTICS"},
    {"export_camisetas", "Camisetas", "Shirts"},
    {"export_partidos", "Partidos", "Matches"},
    {"export_lesiones", "Lesiones", "Injuries"},
    {"export_estadisticas", "Estadisticas", "Statistics"},
    {"export_analisis", "Analisis", "Analysis"},
    {"export_estadisticas_generales", "Estadisticas Generales", "General Statistics"},
    {"export_analisis_avanzado", "Analisis Avanzado", "Advanced Analysis"},
    {"export_base_datos", "Base de Datos", "Database"},
    {"export_todo", "Todo", "All"},
    {"export_informe_total_pdf", "Informe Total en PDF", "Full Report (PDF)"},
    {"export_todo_json", "Todo (JSON)", "All (JSON)"},
    {"export_todo_csv", "Todo (CSV)", "All (CSV)"},
    {"export_todos_partidos", "Todos los Partidos", "All Matches"},
    {"export_partido_mas_goles", "Partido con Mas Goles", "Match with Most Goals"},
    {"export_partido_mas_asistencias", "Partido con Mas Asistencias", "Match with Most Assists"},
    {"export_partido_menos_goles_reciente", "Partido Menos Goles Reciente", "Most Recent Match with Fewest Goals"},
    {"export_partido_menos_asistencias_reciente", "Partido Menos Asistencias Reciente", "Most Recent Match with Fewest Assists"},
    {"export_estadisticas_generales_item", "Estadisticas Generales", "General Statistics"},
    {"export_estadisticas_por_mes", "Estadisticas Por Mes", "Monthly Statistics"},
    {"export_estadisticas_por_anio", "Estadisticas Por Anio", "Yearly Statistics"},
    {"export_records_rankings", "Records & Rankings", "Records & Rankings"},
    {"import_menu_title", "IMPORTAR DATOS", "IMPORT DATA"},
    {"import_menu_json_title", "IMPORTAR DATOS DESDE JSON", "IMPORT DATA FROM JSON"},
    {"import_menu_txt_title", "IMPORTAR DATOS DESDE TXT", "IMPORT DATA FROM TXT"},
    {"import_menu_csv_title", "IMPORTAR DATOS DESDE CSV", "IMPORT DATA FROM CSV"},
    {"import_menu_html_title", "IMPORTAR DATOS DESDE HTML", "IMPORT DATA FROM HTML"},
    {"import_from_json", "Importar desde JSON", "Import from JSON"},
    {"import_from_txt", "Importar desde TXT", "Import from TXT"},
    {"import_from_csv", "Importar desde CSV", "Import from CSV"},
    {"import_from_html", "Importar desde HTML", "Import from HTML"},
    {"import_from_db", "Importar desde Base de Datos", "Import from Database"},
    {"import_camisetas", "Camisetas", "Shirts"},
    {"import_partidos", "Partidos", "Matches"},
    {"import_lesiones", "Lesiones", "Injuries"},
    {"import_estadisticas", "Estadisticas", "Statistics"},
    {"import_todo", "Todo", "All"},
    {"import_todo_json_rapido", "Importar TODO desde JSON", "Import ALL from JSON"},
    {"import_todo_csv_rapido", "Importar TODO desde CSV", "Import ALL from CSV"},
    {"backup_created", "Backup automatico creado:", "Automatic backup created:"},
    {"backup_failed", "Error creando backup automatico.", "Failed to create automatic backup."},
    {"current_settings", "Configuracion Actual", "Current Settings"},
    {"reset_settings", "Restablecer Configuracion", "Reset Settings"},
    {"reset_confirm", "Esta seguro de que desea restablecer toda la configuracion a valores por defecto?", "Are you sure you want to reset all settings to default values?"},
    {"reset_cancelled", "Operacion cancelada.", "Operation cancelled."},
    {"reset_success", "Configuracion restablecida a valores por defecto.", "Settings reset to default values."},
    {"show_current", "Ver Configuracion Actual", "Show Current Settings"},
    {"reset_defaults", "Restablecer a Valores por Defecto", "Reset to Default Values"},
    {"welcome_message", "Bienvenido De Vuelta", "Welcome Back"},
    {"menu_update", "Actualizar", "Update"},
    {NULL, NULL, NULL} // Terminador
};

static void ensure_settings_schema()
{
    char *err = NULL;
    sqlite3_exec(db, "ALTER TABLE settings ADD COLUMN mode INTEGER DEFAULT 0;", NULL, NULL, &err);
    if (err)
    {
        sqlite3_free(err);
        err = NULL;
    }

    sqlite3_exec(db, "ALTER TABLE settings ADD COLUMN text_size INTEGER DEFAULT 1;", NULL, NULL, &err);
    if (err)
    {
        sqlite3_free(err);
    }

    err = NULL;
    sqlite3_exec(db, "ALTER TABLE settings ADD COLUMN image_viewer TEXT DEFAULT '';", NULL, NULL, &err);
    if (err)
    {
        sqlite3_free(err);
    }

    err = NULL;
}

/**
 * @brief Inicializa el sistema de configuracion cargando desde BD
 */
void settings_init()
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT theme, language, mode, text_size FROM settings WHERE id = 1;";
    int has_settings = 0;

    ensure_settings_schema();

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            current_settings.theme = sqlite3_column_int(stmt, 0);
            current_settings.language = sqlite3_column_int(stmt, 1);
            current_settings.mode = sqlite3_column_int(stmt, 2);
            current_settings.text_size = sqlite3_column_int(stmt, 3);
            has_settings = 1;
        }
        sqlite3_finalize(stmt);
    }

    // Si no hay configuracion guardada, pedir al usuario que elija el modo
    if (!has_settings)
    {
        clear_screen();
        print_header(get_text("settings_mode"));

        printf("1. %s\n", get_text("mode_simple"));
        printf("2. %s\n", get_text("mode_advanced"));
        printf("3. %s\n", get_text("mode_custom"));
        printf("0. %s\n", get_text("menu_exit"));

        int opcion = input_int("> ");

        switch (opcion)
        {
        case 1:
            settings_set_mode(MODE_SIMPLE);
            printf("%s\n", get_text("settings_saved"));
            break;
        case 2:
            settings_set_mode(MODE_ADVANCED);
            printf("%s\n", get_text("settings_saved"));
            break;
        case 3:
            current_settings.mode = MODE_CUSTOM;
            // Habilitar menus basicos por defecto en modo personalizado
            habilitar_menus_basicos_custom();
            // Mostrar menu para configurar menus
            menu_custom_menus();
            // Ask for confirmation only if changes were made
            confirmar_guardado_si_cambios();
            break;
        case 0:
            // Exit the program
            db_close();
            exit(0);
        default:
            current_settings.mode = MODE_SIMPLE;
            settings_save();
            break;
        }
    }

    // Aplicar tema al iniciar
    settings_apply_theme();
    settings_apply_text_size();
}

/**
 * @brief Guarda la configuracion actual en la base de datos
 */
void settings_save()
{
    sqlite3_stmt *stmt;
    const char *sql = "INSERT OR REPLACE INTO settings (id, theme, language, mode, text_size) VALUES (1, ?, ?, ?, ?);";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, current_settings.theme);
        sqlite3_bind_int(stmt, 2, current_settings.language);
        sqlite3_bind_int(stmt, 3, current_settings.mode);
        sqlite3_bind_int(stmt, 4, current_settings.text_size);
        int result = sqlite3_step(stmt);
        if (result != SQLITE_DONE)
        {
            printf("Error guardando configuracion: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Obtiene la configuracion actual
 */
AppSettings* settings_get()
{
    return &current_settings;
}

/**
 * @brief Establece el tema de la interfaz
 */
void settings_set_theme(ThemeType theme)
{
    current_settings.theme = theme;
    settings_apply_theme();
    settings_save();
}

/**
 * @brief Establece el idioma de la aplicacion
 */
void settings_set_language(LanguageType language)
{
    current_settings.language = language;
    settings_save();
}

/**
 * @brief Establece el tamano de texto de la aplicacion
 */
void settings_set_text_size(TextSizeType text_size)
{
    current_settings.text_size = text_size;
    settings_apply_text_size();
    settings_save();
}

/**
 * @brief Establece el modo de la aplicacion
 */
void settings_set_mode(ModeType mode)
{
    current_settings.mode = mode;

    // Si se cambia a modo personalizado, habilitar menus basicos por defecto
    if (mode == MODE_CUSTOM)
    {
        // Habilitar menus basicos por defecto en modo personalizado
        habilitar_menus_basicos_custom();
    }

    settings_save();
}

/**
 * @brief Obtiene el modo actual de la aplicacion
 */
ModeType settings_get_mode()
{
    return current_settings.mode;
}

/**
 * @brief Aplica el tema actual a la consola
 */
void settings_apply_theme()
{
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);

    WORD color = 0;

    switch (current_settings.theme)
    {
    case THEME_LIGHT:
        // Tema claro: fondo blanco, texto negro
        color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Texto negro
        break;

    case THEME_DARK:
        // Tema oscuro: fondo negro, texto blanco
        color = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE; // Fondo negro
        color |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Texto blanco
        break;

    case THEME_BLUE:
        // Tema azul: fondo azul oscuro, texto blanco
        color = BACKGROUND_BLUE; // Fondo azul
        color |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Texto blanco
        break;

    case THEME_GREEN:
        // Tema verde: fondo verde oscuro, texto negro
        color = BACKGROUND_GREEN; // Fondo verde
        color |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Texto negro
        break;

    case THEME_RED:
        // Tema rojo: fondo rojo oscuro, texto blanco
        color = BACKGROUND_RED; // Fondo rojo
        color |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Texto blanco
        break;

    case THEME_PURPLE:
        // Tema morado: fondo magenta, texto blanco
        color = BACKGROUND_RED | BACKGROUND_BLUE; // Fondo magenta
        color |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Texto blanco
        break;

    case THEME_CLASSIC:
        // Tema clasico: colores por defecto de Windows
        color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Gris por defecto
        break;

    case THEME_HIGH_CONTRAST:
        // Alto contraste: fondo negro, texto amarillo brillante
        color = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE; // Fondo negro
        color |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; // Texto amarillo brillante
        break;

    default:
        // Por defecto, tema claro
        color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        break;
    }

    SetConsoleTextAttribute(hConsole, color);
#endif
}

static void settings_apply_text_size()
{
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFOEX cfi;
    memset(&cfi, 0, sizeof(cfi));
    cfi.cbSize = sizeof(cfi);

    if (!GetCurrentConsoleFontEx(hConsole, FALSE, &cfi))
    {
        return;
    }

    switch (current_settings.text_size)
    {
    case TEXT_SIZE_SMALL:
        cfi.dwFontSize.Y = 16;
        break;
    case TEXT_SIZE_LARGE:
        cfi.dwFontSize.Y = 24;
        break;
    case TEXT_SIZE_MEDIUM:
    default:
        cfi.dwFontSize.Y = 20;
        break;
    }

    cfi.dwFontSize.X = 0;
    SetCurrentConsoleFontEx(hConsole, FALSE, &cfi);
#endif
}

/**
 * @brief Obtiene el texto correspondiente al idioma actual
 */
const char* get_text(const char* key)
{
    for (int i = 0; text_entries[i].key != NULL; i++)
    {
        if (strcmp(text_entries[i].key, key) == 0)
        {
            return (current_settings.language == LANGUAGE_SPANISH) ?
                   text_entries[i].spanish : text_entries[i].english;
        }
    }
    return key; // Retornar la clave si no se encuentra
}

// Funciones wrapper para menu dinamico
const char* get_menu_camisetas()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_SIMPLE || mode == MODE_ADVANCED)
    {
        return get_text("menu_camisetas");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("camisetas"))
    {
        return get_text("menu_camisetas");
    }
    return NULL; // No mostrar en modo personalizado si no esta habilitado
}

const char* get_menu_canchas()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_SIMPLE || mode == MODE_ADVANCED)
    {
        return get_text("menu_canchas");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("canchas"))
    {
        return get_text("menu_canchas");
    }
    return NULL; // No mostrar en modo personalizado si no esta habilitado
}

const char* get_menu_partidos()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_SIMPLE || mode == MODE_ADVANCED)
    {
        return get_text("menu_partidos");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("partidos"))
    {
        return get_text("menu_partidos");
    }
    return NULL; // No mostrar en modo personalizado si no esta habilitado
}

const char* get_menu_equipos()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_equipos");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("equipos"))
    {
        return get_text("menu_equipos");
    }
    return NULL; // No mostrar en modo simple o personalizado si no esta habilitado
}

const char* get_menu_estadisticas()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_estadisticas");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("estadisticas"))
    {
        return get_text("menu_estadisticas");
    }
    return NULL; // No mostrar en modo simple o personalizado si no esta habilitado
}

const char* get_menu_logros()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_logros");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("logros"))
    {
        return get_text("menu_logros");
    }
    return NULL; // No mostrar en modo simple o personalizado si no esta habilitado
}

const char* get_menu_analisis()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_analisis");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("analisis"))
    {
        return get_text("menu_analisis");
    }
    return NULL; // No mostrar en modo simple o personalizado si no esta habilitado
}

const char* get_menu_bienestar()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_bienestar");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("bienestar"))
    {
        return get_text("menu_bienestar");
    }
    return NULL; // No mostrar en modo simple o personalizado si no esta habilitado
}

const char* get_menu_lesiones()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_SIMPLE || mode == MODE_ADVANCED)
    {
        return get_text("menu_lesiones");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("lesiones"))
    {
        return get_text("menu_lesiones");
    }
    return NULL; // No mostrar en modo personalizado si no esta habilitado
}

const char* get_menu_financiamiento()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_financiamiento");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("financiamiento"))
    {
        return get_text("menu_financiamiento");
    }
    return NULL; // No mostrar en modo simple o personalizado si no esta habilitado
}

const char* get_menu_exportar()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_exportar");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("exportar"))
    {
        return get_text("menu_exportar");
    }
    return NULL; // No mostrar en modo simple o personalizado si no esta habilitado
}

const char* get_menu_importar()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_importar");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("importar"))
    {
        return get_text("menu_importar");
    }
    return NULL; // No mostrar en modo simple o personalizado si no esta habilitado
}

const char* get_menu_torneos()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_torneos");
    }
    return NULL; // No mostrar en modos simple y personalizado
}

const char* get_menu_temporada()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_temporada");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("temporada"))
    {
        return get_text("menu_temporada");
    }
    return NULL; // No mostrar en modo simple o personalizado si no esta habilitado
}

const char* get_menu_entrenador_ia()
{
    ModeType mode = settings_get_mode();
    if (mode == MODE_ADVANCED)
    {
        return get_text("menu_entrenador_ia");
    }
    else if (mode == MODE_CUSTOM && is_custom_menu_enabled("entrenador_ia"))
    {
        return get_text("menu_entrenador_ia");
    }
    return NULL; // No mostrar en modo simple o personalizado si no esta habilitado
}

const char* get_menu_settings()
{
    return get_text("menu_settings");
}

const char* get_menu_exit()
{
    return get_text("menu_exit");
}

const char* get_menu_title()
{
    return get_text("menu_title");
}

const char* get_settings_theme()
{
    return get_text("settings_theme");
}

const char* get_settings_language()
{
    return get_text("settings_language");
}

const char* get_menu_usuario()
{
    return get_text("menu_usuario");
}

const char* get_show_current()
{
    return get_text("show_current");
}

const char* get_reset_defaults()
{
    return get_text("reset_defaults");
}

const char* get_menu_back()
{
    return get_text("menu_back");
}

#ifdef _WIN32
static void obtener_nombre_repo(const char *owner_repo, char *repo_name, size_t repo_name_size)
{
    const char *slash = strrchr(owner_repo, '/');
    if (slash && *(slash + 1) != '\0')
    {
        snprintf(repo_name, repo_name_size, "%s", slash + 1);
        return;
    }

    snprintf(repo_name, repo_name_size, "%s", owner_repo);
}

static int cargar_descargador(URLDownloadToFileAFunc *out_downloader, HMODULE *out_module)
{
    *out_downloader = NULL;
    *out_module = LoadLibraryA("urlmon.dll");
    if (*out_module)
    {
        /* Evitar warning de cast entre tipos de funcion distintos */
        union
        {
            FARPROC fp;
            URLDownloadToFileAFunc fn;
        } downloader_union;

        downloader_union.fp = GetProcAddress(*out_module, "URLDownloadToFileA");
        *out_downloader = downloader_union.fn;
    }

    return *out_downloader != NULL;
}

static int leer_archivo_completo(const char *path, char **out_data);
static const char *obtener_release_label(const cJSON *tag, const cJSON *name);

static void liberar_descargador(HMODULE module)
{
    if (module)
    {
        FreeLibrary(module);
    }
}

static void liberar_releases(char *release_names[], char *asset_urls[], int count)
{
    for (int i = 0; i < count; i++)
    {
        free(release_names[i]);
        free(asset_urls[i]);
    }
}

static int descargar_archivo(URLDownloadToFileAFunc downloader, const char *url, const char *dest)
{
    HRESULT hr = E_FAIL;
    if (downloader)
    {
        hr = downloader(NULL, url, dest, 0, NULL);
    }

    return hr == S_OK;
}

static int ejecutar_instalador(const char *dest)
{
    HINSTANCE h = ShellExecuteA(NULL, "open", dest, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32)
    {
        // Intentar elevar privilegios si la ejecucion fallo la primera vez.
        HINSTANCE h2 = ShellExecuteA(NULL, "runas", dest, NULL, NULL, SW_SHOWNORMAL);
        if ((INT_PTR)h2 > 32)
        {
            return 1;
        }

        // Si falla, es probable que el ejecutable este bloqueado (porque MiFutbolC esta en ejecucion).
        printf("Error al ejecutar el instalador. Asegurate de cerrar MiFutbolC y vuelve a intentarlo.\n");
        return 0;
    }
    return 1;
}

// Compara versiones semanticas simples (mayor.minor.patch).
// Retorna -1 si a < b, 0 si son iguales, 1 si a > b.
static int comparar_versiones(const char *a, const char *b)
{
    int am = 0;
    int an = 0;
    int ap = 0;
    int bm = 0;
    int bn = 0;
    int bp = 0;

    // Permitir cadenas que comiencen con "v" o "V".
    if (a && (*a == 'v' || *a == 'V'))
    {
        a++;
    }
    if (b && (*b == 'v' || *b == 'V'))
    {
        b++;
    }

    sscanf_s(a ? a : "", "%d.%d.%d", &am, &an, &ap);
    sscanf_s(b ? b : "", "%d.%d.%d", &bm, &bn, &bp);

    if (am != bm) return am < bm ? -1 : 1;
    if (an != bn) return an < bn ? -1 : 1;
    if (ap != bp) return ap < bp ? -1 : 1;
    return 0;
}

// Descarga la informacion de la ultima release desde GitHub y devuelve su tag (tag_name o name).
// El valor devuelto debe liberarse con free() por el llamador.
static char *obtener_latest_release_tag(const char *owner_repo, const char *repo_name, const char *temp_path)
{
    URLDownloadToFileAFunc downloader;
    HMODULE module;
    if (!cargar_descargador(&downloader, &module))
    {
        return NULL;
    }

    char api_url[1024];
    char json_path[1024];
    snprintf(api_url, sizeof(api_url), "https://api.github.com/repos/%s/releases/latest", owner_repo);
    snprintf(json_path, sizeof(json_path), "%s%s_latest_release.json", temp_path, repo_name);

    if (!descargar_archivo(downloader, api_url, json_path))
    {
        liberar_descargador(module);
        return NULL;
    }

    char *json_data = NULL;
    if (!leer_archivo_completo(json_path, &json_data))
    {
        liberar_descargador(module);
        return NULL;
    }

    cJSON *root = cJSON_Parse(json_data);
    free(json_data);
    if (!root || !cJSON_IsObject(root))
    {
        if (root)
        {
            cJSON_Delete(root);
        }
        liberar_descargador(module);
        return NULL;
    }

    const cJSON *tag = cJSON_GetObjectItem(root, "tag_name");
    const cJSON *name = cJSON_GetObjectItem(root, "name");
    const char *label = obtener_release_label(tag, name);

    char *result = _strdup(label);
    cJSON_Delete(root);
    liberar_descargador(module);
    return result;
}

static int leer_archivo_completo(const char *path, char **out_data)
{
    FILE *f = NULL;
    errno_t open_err = fopen_s(&f, path, "rb");
    if (open_err != 0 || !f)
    {
        return 0;
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return 0;
    }

    long fsize = ftell(f);
    if (fsize <= 0)
    {
        fclose(f);
        return 0;
    }

    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return 0;
    }

    size_t bytes = (size_t)fsize;
    char *data = (char *)malloc(bytes + 1);
    if (!data)
    {
        fclose(f);
        return 0;
    }

    size_t read_bytes = fread(data, 1, bytes, f);
    fclose(f);

    if (read_bytes != bytes)
    {
        free(data);
        return 0;
    }

    data[bytes] = '\0';
    *out_data = data;
    return 1;
}

static const char *obtener_release_label(const cJSON *tag, const cJSON *name)
{
    if (tag && cJSON_IsString(tag) && tag->valuestring)
    {
        return tag->valuestring;
    }
    if (name && cJSON_IsString(name) && name->valuestring)
    {
        return name->valuestring;
    }
    return "(sin nombre)";
}

static int extraer_asset_exe(const cJSON *assets, const char **asset_url)
{
    if (!assets || !cJSON_IsArray(assets))
    {
        return 0;
    }

    const cJSON *asset = NULL;
    cJSON_ArrayForEach(asset, assets)
    {
        const cJSON *asset_name_item = cJSON_GetObjectItem(asset, "name");
        const cJSON *url_item = cJSON_GetObjectItem(asset, "browser_download_url");
        if (!asset_name_item || !url_item || !cJSON_IsString(asset_name_item) || !cJSON_IsString(url_item))
        {
            continue;
        }

        if (strstr(asset_name_item->valuestring, ".exe") != NULL)
        {
            *asset_url = url_item->valuestring;
            return 1;
        }
    }

    return 0;
}

static int cargar_releases_ejecutables(cJSON *root, char *release_names[], char *asset_urls[], int max_releases)
{
    int count = 0;
    cJSON *rel = NULL;

    cJSON_ArrayForEach(rel, root)
    {
        if (count >= max_releases)
        {
            break;
        }

        const cJSON *tag = cJSON_GetObjectItem(rel, "tag_name");
        const cJSON *name = cJSON_GetObjectItem(rel, "name");
        const char *release_label = obtener_release_label(tag, name);
        const cJSON *assets = cJSON_GetObjectItem(rel, "assets");
        const char *asset_url = NULL;

        if (!extraer_asset_exe(assets, &asset_url))
        {
            continue;
        }

        release_names[count] = _strdup(release_label);
        asset_urls[count] = _strdup(asset_url);
        if (!release_names[count] || !asset_urls[count])
        {
            free(release_names[count]);
            free(asset_urls[count]);
            break;
        }
        count++;
    }

    return count;
}

static int descargar_y_ejecutar_latest(const char *owner_repo, const char *repo_name, const char *temp_path)
{
    URLDownloadToFileAFunc downloader;
    HMODULE module;
    if (!cargar_descargador(&downloader, &module))
    {
        printf("Error cargando modulo de descarga (urlmon.dll).\n");
        return 0;
    }

    char download_url[1024];
    char dest[1024];
    snprintf(download_url, sizeof(download_url), "https://github.com/%s/releases/latest/download/%s.exe", owner_repo, repo_name);
    snprintf(dest, sizeof(dest), "%s%s_latest.exe", temp_path, repo_name);

    printf("Descargando %s -> %s\n", download_url, dest);
    int ok = descargar_archivo(downloader, download_url, dest);
    liberar_descargador(module);

    if (!ok)
    {
        printf("Error descargando la release.\n");
        return 0;
    }

    printf("Descarga completada: %s\n", dest);
    ejecutar_instalador(dest);
    return 1;
}

static int descargar_y_ejecutar_release_seleccionada(const char *owner_repo, const char *repo_name, const char *temp_path)
{
    URLDownloadToFileAFunc downloader;
    HMODULE module;
    if (!cargar_descargador(&downloader, &module))
    {
        printf("Error cargando modulo de descarga (urlmon.dll).\n");
        return 0;
    }

    char api_url[1024];
    char json_path[1024];
    snprintf(api_url, sizeof(api_url), "https://api.github.com/repos/%s/releases", owner_repo);
    snprintf(json_path, sizeof(json_path), "%s%s_releases.json", temp_path, repo_name);

    printf("Obteniendo lista de releases...\n");
    if (!descargar_archivo(downloader, api_url, json_path))
    {
        printf("Error descargando lista de releases.\n");
        liberar_descargador(module);
        return 0;
    }

    char *json_data = NULL;
    if (!leer_archivo_completo(json_path, &json_data))
    {
        printf("Error abriendo o leyendo archivo de releases descargado.\n");
        liberar_descargador(module);
        return 0;
    }

    cJSON *root = cJSON_Parse(json_data);
    free(json_data);
    if (!root || !cJSON_IsArray(root))
    {
        printf("Respuesta invalida de GitHub API.\n");
        if (root)
        {
            cJSON_Delete(root);
        }
        liberar_descargador(module);
        return 0;
    }

    enum { SETTINGS_MAX_RELEASES = 64 };
    char *asset_urls[SETTINGS_MAX_RELEASES] = {0};
    char *release_names[SETTINGS_MAX_RELEASES] = {0};
    int count = cargar_releases_ejecutables(root, release_names, asset_urls,
                                            SETTINGS_MAX_RELEASES);
    cJSON_Delete(root);

    if (count == 0)
    {
        printf("No se encontraron assets .exe en las releases.\n");
        liberar_descargador(module);
        return 0;
    }

    printf("Selecciona la version a descargar:\n");
    for (int i = 0; i < count; i++)
    {
        printf("%d. %s\n", i + 1, release_names[i]);
    }
    printf("0. %s\n", get_text("menu_back"));

    int choice = input_int("> ");
    if (choice <= 0 || choice > count)
    {
        liberar_releases(release_names, asset_urls, count);
        liberar_descargador(module);
        return 0;
    }

    int selected_index = choice - 1;
    const char *chosen_url = asset_urls[selected_index];
    char dest[1024];
    snprintf(dest, sizeof(dest), "%s%s_selected.exe", temp_path, repo_name);

    printf("Descargando %s -> %s\n", chosen_url, dest);
    int ok = descargar_archivo(downloader, chosen_url, dest);

    liberar_releases(release_names, asset_urls, count);
    liberar_descargador(module);

    if (!ok)
    {
        printf("Error descargando el asset.\n");
        return 0;
    }

    printf("Descarga completada: %s\n", dest);
    ejecutar_instalador(dest);
    return 1;
}
#endif

/**
 * @brief Submenu para configuracion de temas
 */
static void menu_theme_settings()
{
    MenuItem items[] =
    {
        {1, get_text("theme_light"), theme_set_light},
        {2, get_text("theme_dark"), theme_set_dark},
        {3, get_text("theme_blue"), theme_set_blue},
        {4, get_text("theme_green"), theme_set_green},
        {5, get_text("theme_red"), theme_set_red},
        {6, get_text("theme_purple"), theme_set_purple},
        {7, get_text("theme_classic"), theme_set_classic},
        {8, get_text("theme_high_contrast"), theme_set_high_contrast},
        {0, get_text("menu_back"), NULL}
    };

    ejecutar_menu(get_text("settings_theme"), items, (int)(sizeof(items)/sizeof(items[0])));
}

/**
 * @brief Submenu para configuracion de idioma
 */
static void menu_language_settings()
{
    MenuItem items[] =
    {
        {1, get_text("lang_spanish"), lang_set_spanish},
        {2, get_text("lang_english"), lang_set_english},
        {0, get_text("menu_back"), NULL}
    };

    ejecutar_menu(get_text("settings_language"), items, (int)(sizeof(items)/sizeof(items[0])));
}

/**
 * @brief Obtiene el nombre del tema actual
 */
static const char* get_current_theme_name()
{
    switch (current_settings.theme)
    {
    case THEME_LIGHT:
        return get_text("theme_light");
    case THEME_DARK:
        return get_text("theme_dark");
    case THEME_BLUE:
        return get_text("theme_blue");
    case THEME_GREEN:
        return get_text("theme_green");
    case THEME_RED:
        return get_text("theme_red");
    case THEME_PURPLE:
        return get_text("theme_purple");
    case THEME_CLASSIC:
        return get_text("theme_classic");
    case THEME_HIGH_CONTRAST:
        return get_text("theme_high_contrast");
    default:
        return get_text("theme_light");
    }
}

static const char* get_current_text_size_name()
{
    switch (current_settings.text_size)
    {
    case TEXT_SIZE_SMALL:
        return get_text("text_size_small");
    case TEXT_SIZE_LARGE:
        return get_text("text_size_large");
    case TEXT_SIZE_MEDIUM:
    default:
        return get_text("text_size_medium");
    }
}

/**
 * @brief Muestra la configuracion actual
 */
static void show_current_settings()
{
    clear_screen();
    print_header(get_text("current_settings"));

    printf("Tema: %s\n", get_current_theme_name());
    printf("Idioma: %s\n", current_settings.language == LANGUAGE_SPANISH ? get_text("lang_spanish") : get_text("lang_english"));
    printf("Tamanio de texto: %s\n", get_current_text_size_name());

    char *usuario = get_user_name();
    if (usuario)
    {
        printf("Usuario: %s\n", usuario);
        free(usuario);
    }
    else
    {
        printf("Usuario: No configurado\n");
    }

    printf("\n");
    pause_console();
}

/**
 * @brief Submenu para tamano de texto
 */
static void menu_text_size_settings()
{
    MenuItem items[] =
    {
        {1, get_text("text_size_small"), text_size_small},
        {2, get_text("text_size_medium"), text_size_medium},
        {3, get_text("text_size_large"), text_size_large},
        {0, get_text("menu_back"), NULL}
    };

    ejecutar_menu(get_text("settings_text_size"), items, (int)(sizeof(items)/sizeof(items[0])));
}

/**
 * @brief Submenu de accesibilidad
 */
static void menu_accessibility_settings()
{
    MenuItem items[] =
    {
        {1, get_text("settings_text_size"), menu_text_size_settings},
        {2, get_text("settings_high_contrast"), accessibility_high_contrast},
        {3, get_text("settings_accessibility_normal"), accessibility_normal_theme_text},
        {0, get_text("menu_back"), NULL}
    };

    ejecutar_menu(get_text("settings_accessibility"), items, (int)(sizeof(items)/sizeof(items[0])));
}

/**
 * @brief Restablece la configuracion a valores por defecto
 */
static void reset_settings_to_defaults()
{
    clear_screen();
    print_header(get_text("reset_settings"));

    printf("%s\n", get_text("reset_confirm"));
    printf("(S/N): ");

    char confirm;
    char input[16];
    if (!fgets(input, sizeof(input), stdin)
#if defined(_WIN32) && defined(_MSC_VER)
            || sscanf_s(input, " %c", &confirm, 1) != 1)
#else
            || sscanf(input, " %c", &confirm) != 1)
#endif
    {
        confirm = 'N';
    }

    if (confirm == 's' || confirm == 'S')
    {
        current_settings.theme = THEME_LIGHT;
        current_settings.language = LANGUAGE_SPANISH;
        settings_apply_theme();
        settings_save();

        // Limpiar nombre de usuario tambien
        sqlite3_stmt *stmt;
        const char *sql = "DELETE FROM usuario;";
        if (preparar_stmt(sql, &stmt))
        {
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        printf("%s\n", get_text("reset_success"));
    }
    else
    {
        printf("%s\n", get_text("reset_cancelled"));
    }

    pause_console();
}

/**
 * @brief Verifica si un menu esta habilitado en modo Custom
 */
int is_custom_menu_enabled(const char* menu_name)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT enabled FROM custom_menus WHERE menu_name = ?;";
    int enabled = 0;

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_text(stmt, 1, menu_name, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            enabled = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return enabled;
}

/**
 * @brief Establece el estado de un menu en modo Custom
 */
void set_custom_menu_enabled(const char* menu_name, int enabled)
{
    sqlite3_stmt *stmt;
    const char *sql = "INSERT OR REPLACE INTO custom_menus (menu_name, enabled) VALUES (?, ?);";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_text(stmt, 1, menu_name, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, enabled);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        custom_menu_changed = 1; // Marcar que se hizo un cambio
    }
}

/**
 * @brief Submenu para configuracion de modo
 */
static void menu_mode_settings()
{
    MenuItem items[] =
    {
        {1, get_text("mode_simple"), mode_set_simple},
        {2, get_text("mode_advanced"), mode_set_advanced},
        {3, get_text("mode_custom"), mode_set_custom},
        {0, get_text("menu_back"), NULL}
    };

    ejecutar_menu(get_text("settings_mode"), items, (int)(sizeof(items)/sizeof(items[0])));
}

/**
 * @brief Menu para configurar menus personalizados en modo Custom
 */
void menu_custom_menus()
{
#if defined(UNIT_TEST)
    return;
#else
    custom_menu_changed = 0; // Reset flag at the beginning
    clear_screen();
    print_header(get_text("menu_settings"));

    printf("Selecciona los menus que deseas habilitar/deshabilitar:\n\n");

    // Lista de menus disponibles (excluyendo exit que siempre esta)
    struct MenuOption options[] =
    {
        {1, "camisetas", get_text("menu_camisetas")},
        {2, "canchas", get_text("menu_canchas")},
        {3, "partidos", get_text("menu_partidos")},
        {4, "equipos", get_text("menu_equipos")},
        {5, "estadisticas", get_text("menu_estadisticas")},
        {6, "logros", get_text("menu_logros")},
        {8, "analisis", get_text("menu_analisis")},
        {9, "bienestar", get_text("menu_bienestar")},
        {10, "lesiones", get_text("menu_lesiones")},
        {11, "financiamiento", get_text("menu_financiamiento")},
        {12, "torneos", get_text("menu_torneos")},
        {13, "temporada", get_text("menu_temporada")},
        {14, "settings", get_text("menu_settings")},
        {0, NULL, NULL}
    };

    for (int j = 0; options[j].name != NULL; j++)
    {
        int enabled = is_custom_menu_enabled(options[j].name);
        printf("%d. [%s] %s\n", options[j].opcion, enabled ? "X" : " ", options[j].display_name);
    }

    printf("0. %s\n", get_text("menu_back"));
    printf("\nIngresa el numero del menu para alternar su estado:\n");

    int opcion = input_int("> ");

    if (opcion == 0)
    {
        return;
    }

    const struct MenuOption *option = buscar_opcion_menu(options, opcion);
    if (!option)
    {
        printf("%s\n", get_text("invalid_option"));
        pause_console();
        menu_custom_menus(); // Recargar menu
        return;
    }

    int current_state = is_custom_menu_enabled(option->name);
    set_custom_menu_enabled(option->name, !current_state);
    printf("Menu %s %s.\n", option->display_name, !current_state ? "habilitado" : "deshabilitado");
    confirmar_guardado_configuracion(1);
    pause_console();
    menu_custom_menus(); // Recargar menu
#endif
}

/**
 * @brief Descarga la ultima release .exe desde GitHub Releases y la ejecuta (Windows)
 */
void menu_update()
{
#if defined(UNIT_TEST)
    return;
#else
#ifdef _WIN32
    clear_screen();
    print_header(get_text("menu_update"));
    const char *owner_repo = UPDATE_REPO; // formato owner/repo
    char repo_name[128] = {0};
    obtener_nombre_repo(owner_repo, repo_name, sizeof(repo_name));

    // Verificar version actual contra la ultima version en GitHub.
    char temp_path[MAX_PATH];
    if (GetTempPathA(MAX_PATH, temp_path) == 0)
    {
        printf("Error obteniendo carpeta temporal.\n");
        pause_console();
        return;
    }

    char *latest_tag = obtener_latest_release_tag(owner_repo, repo_name, temp_path);
    if (latest_tag)
    {
        int cmp = comparar_versiones(APP_VERSION, latest_tag);
        if (cmp >= 0)
        {
            printf("Ya tienes la ultima version (%s).\n", APP_VERSION);
            free(latest_tag);
            pause_console();
            return;
        }

        printf("Nueva version disponible: %s (actual: %s).\n", latest_tag, APP_VERSION);
        free(latest_tag);
    }

    // Opciones al usuario
    printf("1. %s\n", "Ultima version (latest)");
    printf("2. %s\n", "Elegir una version de la lista");
    printf("0. %s\n", get_text("menu_back"));

    int modo = input_int("> ");
    if (modo == 0)
    {
        return;
    }

    if (modo == 1)
    {
        descargar_y_ejecutar_latest(owner_repo, repo_name, temp_path);
        pause_console();
        return;
    }

    descargar_y_ejecutar_release_seleccionada(owner_repo, repo_name, temp_path);
    pause_console();
#else
    printf("Actualizar solo esta soportado en Windows.\n");
    pause_console();
#endif
#endif
}

/**
 * @brief Menu principal de configuracion
 */
void menu_settings()
{
    MenuItem items[] =
    {
        {1, get_text("settings_theme"), menu_theme_settings},
        {2, get_text("settings_language"), menu_language_settings},
        {3, get_text("settings_accessibility"), menu_accessibility_settings},
        {4, get_text("menu_usuario"), menu_usuario},
        {5, get_text("show_current"), show_current_settings},
        {6, get_text("reset_defaults"), reset_settings_to_defaults},
        {7, get_text("settings_mode"), menu_mode_settings},
        {8, get_text("menu_exportar"), menu_exportar},
        {9, get_text("menu_importar"), menu_importar},
        {10, get_text("menu_update"), menu_update},
        {0, get_text("menu_back"), NULL}
    };

    const int item_count = (int)(sizeof(items) / sizeof(items[0]));

    ejecutar_menu(get_text("menu_settings"), items, item_count);
}
