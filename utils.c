/**
 * @file utils.c
 * @brief Funciones utilitarias para entrada/salida, manejo de fechas y
 * operaciones de base de datos.
 *
 * Este archivo contiene funciones auxiliares para interactuar con el usuario,
 * manejar fechas y horas, limpiar la pantalla, verificar existencia de IDs en
 * la base de datos, y gestionar directorios de exportacion.
 */

#include "utils.h"
#include "export.h"
#include "db.h"
#include "menu.h"
#include "settings.h"
#include "cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <stddef.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#ifdef _WIN32
#include <conio.h>
#ifdef _WIN32
#include <direct.h>
#else
#include "direct.h"
#endif
#define MKDIR(path) _mkdir(path)
#ifdef _WIN32
#include <windows.h>
#else
#include "compat_windows.h"
#endif
#else
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#ifdef _WIN32
/* Funcion deshabilitada: evitar maximizar la consola automaticamente
 * Se deja como no-op para que el usuario pueda cerrar/reducir la ventana.
 */
void ensure_console_maximized_windows(void)
{
    /* Intencionalmente vacio */
}
#else
void ensure_console_maximized_windows(void)
{
    /* No-op en otros sistemas */
}
#endif

static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    return 1;
}

static uint64_t auth_fnv1a64_update(uint64_t hash, const unsigned char *data, size_t len)
{
    const uint64_t prime = 1099511628211ULL;
    for (size_t i = 0; i < len; i++)
    {
        hash ^= (uint64_t)data[i];
        hash *= prime;
    }
    return hash;
}

static uint64_t auth_fnv1a64_string(const char *text)
{
    const uint64_t offset_basis = 14695981039346656037ULL;
    if (!text)
    {
        return offset_basis;
    }
    return auth_fnv1a64_update(offset_basis, (const unsigned char *)text, strlen_s(text, SIZE_MAX));
}

static void auth_generate_salt_hex(char *salt_out, size_t out_size)
{
    static const char hex[] = "0123456789abcdef";
    static int seeded = 0;
    unsigned char salt[16];

    if (!salt_out || out_size < 33)
    {
        return;
    }

    if (!seeded)
    {
        srand((unsigned int)time(NULL) ^ (unsigned int)clock());
        seeded = 1;
    }

    for (int i = 0; i < 16; i++)
    {
        salt[i] = (unsigned char)(rand() % 256);
        salt_out[i * 2] = hex[(salt[i] >> 4) & 0x0F];
        salt_out[i * 2 + 1] = hex[salt[i] & 0x0F];
    }
    salt_out[32] = '\0';
}

static void auth_build_password_hash(const char *plain_password, const char *salt_hex,
                                     char *hash_out, size_t out_size)
{
    char round_input[512];
    uint64_t h1;
    uint64_t h2;

    if (!plain_password || !salt_hex || !hash_out || out_size < 17)
    {
        if (hash_out && out_size > 0)
        {
            hash_out[0] = '\0';
        }
        return;
    }

    snprintf(round_input, sizeof(round_input), "%s:%s", salt_hex, plain_password);
    h1 = auth_fnv1a64_string(round_input);
    snprintf(round_input, sizeof(round_input), "%s:%016" PRIx64 ":%s", plain_password, h1, salt_hex);
    h2 = auth_fnv1a64_string(round_input);
    snprintf(hash_out, out_size, "%016" PRIx64, h2);
}

static int auth_username_exists(sqlite3 *auth_db, const char *username);
static int auth_upsert_user(sqlite3 *auth_db, const char *username, const char *plain_password);

static int app_get_env_var_copy(const char *name, char *buffer, size_t size)
{
    if (!name || !buffer || size == 0)
    {
        return 0;
    }

#if defined(_WIN32) && defined(_MSC_VER)
    char *value = NULL;
    size_t value_len = 0;
    if (_dupenv_s(&value, &value_len, name) != 0 || !value || value_len == 0)
    {
        if (value)
        {
            free(value);
        }
        buffer[0] = '\0';
        return 0;
    }

    strncpy_s(buffer, size, value, _TRUNCATE);
    free(value);
    return buffer[0] != '\0';
#else
    const char *value = getenv(name);
    if (!value || value[0] == '\0')
    {
        buffer[0] = '\0';
        return 0;
    }

    strncpy_s(buffer, size, value, _TRUNCATE);
    return buffer[0] != '\0';
#endif
}

static int app_build_local_appdata_path(char *dest, size_t size, const char *suffix)
{
#ifdef _WIN32
    char local_app_data[1024] = {0};
    if (!dest || size == 0 || !suffix)
    {
        return 0;
    }

    if (!app_get_env_var_copy("LOCALAPPDATA", local_app_data, sizeof(local_app_data)))
    {
        return 0;
    }

    if (strcpy_s(dest, size, local_app_data) != 0)
    {
        return 0;
    }
    if (strcat_s(dest, size, "\\MiFutbolC") != 0)
    {
        return 0;
    }

    if (suffix[0] != '\0')
    {
        if (strcat_s(dest, size, "\\") != 0)
        {
            return 0;
        }
        if (strcat_s(dest, size, suffix) != 0)
        {
            return 0;
        }
    }
    return 1;
#else
    (void)dest;
    (void)size;
    (void)suffix;
    return 0;
#endif
}

static void auth_get_db_path(char *path, size_t size)
{
#ifdef _WIN32
    char local_app_data[1024] = {0};
    if (app_get_env_var_copy("LOCALAPPDATA", local_app_data, sizeof(local_app_data)))
    {
        snprintf(path, size, "%s\\MiFutbolC\\data\\users.db", local_app_data);
        return;
    }
#endif
    snprintf(path, size, "./data/users.db");
}

static void auth_get_user_data_paths(const char *username,
                                     char *db_path, size_t db_size,
                                     char *log_path, size_t log_size)
{
#ifdef _WIN32
    char local_app_data[1024] = {0};
    if (app_get_env_var_copy("LOCALAPPDATA", local_app_data, sizeof(local_app_data)))
    {
        snprintf(db_path, db_size, "%s\\MiFutbolC\\data\\mifutbol_%s.db", local_app_data, username);
        snprintf(log_path, log_size, "%s\\MiFutbolC\\data\\mifutbol_%s.log", local_app_data, username);
        return;
    }
#endif
    snprintf(db_path, db_size, "./data/mifutbol_%s.db", username);
    snprintf(log_path, log_size, "./data/mifutbol_%s.log", username);
}

static int auth_ensure_parent_dirs(void)
{
#ifdef _WIN32
    char base_dir[1024];
    char data_dir[1024];

    if (app_build_local_appdata_path(base_dir, sizeof(base_dir), "") &&
            app_build_local_appdata_path(data_dir, sizeof(data_dir), "data"))
    {
        MKDIR(base_dir);
        MKDIR(data_dir);
        return 1;
    }
#endif
    MKDIR("./data");
    return 1;
}

static int auth_username_valido(const char *username)
{
    size_t len;

    if (!username)
    {
        return 0;
    }

    len = safe_strnlen(username, 128);
    if (len < 3 || len > 32)
    {
        return 0;
    }

    for (size_t i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char)username[i];
        if (!(isalnum(c) || c == '_' || c == '-'))
        {
            return 0;
        }
    }

    return 1;
}

static int auth_open(sqlite3 **out_db)
{
    char path[1024];
    const char *schema =
        "CREATE TABLE IF NOT EXISTS local_users ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " username TEXT NOT NULL UNIQUE,"
        " password_salt TEXT DEFAULT '',"
        " password_hash TEXT DEFAULT '',"
        " created_at TEXT DEFAULT CURRENT_TIMESTAMP);";

    if (!out_db)
    {
        return 0;
    }

    auth_ensure_parent_dirs();
    auth_get_db_path(path, sizeof(path));

    if (sqlite3_open(path, out_db) != SQLITE_OK)
    {
        return 0;
    }

    if (sqlite3_exec(*out_db, schema, NULL, NULL, NULL) != SQLITE_OK)
    {
        sqlite3_close(*out_db);
        *out_db = NULL;
        return 0;
    }

    return 1;
}

static int auth_user_count(sqlite3 *auth_db)
{
    sqlite3_stmt *stmt = NULL;
    int count = 0;

    if (!auth_db)
    {
        return 0;
    }

    if (sqlite3_prepare_v2(auth_db, "SELECT COUNT(*) FROM local_users;", -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return count;
}

static int auth_file_exists(const char *path)
{
    FILE *f = NULL;
#ifdef _WIN32
    if (fopen_s(&f, path, "rb") != 0 || !f)
    {
        return 0;
    }
#else
    f = fopen(path, "rb");
    if (!f)
    {
        return 0;
    }
#endif
    fclose(f);
    return 1;
}

static void auth_normalizar_username_legado(const char *input, char *output, size_t out_size)
{
    size_t j = 0;

    if (!output || out_size == 0)
    {
        return;
    }

    output[0] = '\0';
    if (!input)
    {
        return;
    }

    const char *p = input;
    while (*p != '\0' && j + 1 < out_size)
    {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c) || c == '_' || c == '-')
        {
            output[j++] = (char)c;
        }
        else
        {
            output[j++] = '_';
        }
        p++;
    }
    output[j] = '\0';

    if (j < 3)
    {
        strncpy_s(output, out_size, "usuario", out_size - 1);
    }
    if (j > 32)
    {
        output[32] = '\0';
    }
}

static int auth_legacy_open_if_exists(const char *legacy_db_path, sqlite3 **legacy_db)
{
    if (!legacy_db || !legacy_db_path || !auth_file_exists(legacy_db_path))
    {
        return 0;
    }

    if (sqlite3_open(legacy_db_path, legacy_db) != SQLITE_OK)
    {
        if (*legacy_db)
        {
            sqlite3_close(*legacy_db);
            *legacy_db = NULL;
        }
        return 0;
    }

    return 1;
}

static int auth_legacy_load_user_with_password(sqlite3 *legacy_db,
        char *username_raw, size_t username_size,
        char *salt, size_t salt_size,
        char *hash, size_t hash_size)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if (sqlite3_prepare_v2(legacy_db,
                           "SELECT nombre, password_salt, password_hash FROM usuario WHERE id = 1 LIMIT 1;",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
            const char *salt_db = (const char *)sqlite3_column_text(stmt, 1);
            const char *hash_db = (const char *)sqlite3_column_text(stmt, 2);
            strncpy_s(username_raw, username_size, nombre ? nombre : "", username_size - 1);
            strncpy_s(salt, salt_size, salt_db ? salt_db : "", salt_size - 1);
            strncpy_s(hash, hash_size, hash_db ? hash_db : "", hash_size - 1);
            found = 1;
        }
        sqlite3_finalize(stmt);
    }

    return found;
}

static int auth_legacy_load_fallback_user(sqlite3 *legacy_db, char *username_raw, size_t username_size)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if (sqlite3_prepare_v2(legacy_db,
                           "SELECT nombre FROM usuario LIMIT 1;",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
            strncpy_s(username_raw, username_size, nombre ? nombre : "", username_size - 1);
            found = 1;
        }
        sqlite3_finalize(stmt);
    }

    return found;
}

static int auth_insert_legacy_user(sqlite3 *auth_db, const char *username_norm,
                                   const char *salt, const char *hash)
{
    int inserted = 0;

    if (salt[0] != '\0' && hash[0] != '\0')
    {
        sqlite3_stmt *ins = NULL;
        if (sqlite3_prepare_v2(auth_db,
                               "INSERT INTO local_users(username, password_salt, password_hash) VALUES(?, ?, ?);",
                               -1, &ins, NULL) == SQLITE_OK)
        {
            sqlite3_bind_text(ins, 1, username_norm, -1, SQLITE_STATIC);
            sqlite3_bind_text(ins, 2, salt, -1, SQLITE_STATIC);
            sqlite3_bind_text(ins, 3, hash, -1, SQLITE_STATIC);
            inserted = (sqlite3_step(ins) == SQLITE_DONE);
            sqlite3_finalize(ins);
        }
    }
    else
    {
        inserted = auth_upsert_user(auth_db, username_norm, "");
    }

    return inserted;
}

static int auth_import_legacy_from_db(sqlite3 *auth_db, const char *legacy_db_path)
{
    sqlite3 *legacy_db = NULL;
    char username_raw[128] = "";
    char username_norm[64] = "";
    char salt[64] = "";
    char hash[64] = "";
    int inserted = 0;

    if (!auth_legacy_open_if_exists(legacy_db_path, &legacy_db))
    {
        return 0;
    }

    auth_legacy_load_user_with_password(legacy_db,
                                        username_raw, sizeof(username_raw),
                                        salt, sizeof(salt),
                                        hash, sizeof(hash));

    if (username_raw[0] == '\0')
    {
        auth_legacy_load_fallback_user(legacy_db, username_raw, sizeof(username_raw));
    }

    sqlite3_close(legacy_db);

    if (username_raw[0] == '\0')
    {
        return 0;
    }

    auth_normalizar_username_legado(username_raw, username_norm, sizeof(username_norm));
    if (!auth_username_valido(username_norm) || auth_username_exists(auth_db, username_norm))
    {
        return 0;
    }

    inserted = auth_insert_legacy_user(auth_db, username_norm, salt, hash);

    if (inserted)
    {
        ui_printf("Usuario legado detectado e importado: %s\n", username_norm);
    }

    return inserted;
}

static int auth_importar_usuario_legado_si_existe(sqlite3 *auth_db)
{
    char legacy1[1024];
    char legacy2[1024];
#ifdef _WIN32
    if (app_build_local_appdata_path(legacy1, sizeof(legacy1), "data\\mifutbol.db"))
    {
        /* ruta lista */
    }
    else
    {
        legacy1[0] = '\0';
    }
#else
    legacy1[0] = '\0';
#endif
    snprintf(legacy2, sizeof(legacy2), "./data/mifutbol.db");

    if (legacy1[0] != '\0' && auth_import_legacy_from_db(auth_db, legacy1))
    {
        return 1;
    }

    if (auth_import_legacy_from_db(auth_db, legacy2))
    {
        return 1;
    }

    return 0;
}

static void uppercase_ascii(const char *src, char *dst, size_t size)
{
    size_t i = 0;
    if (!dst || size == 0)
    {
        return;
    }
    if (!src)
    {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i + 1 < size)
    {
        unsigned char ch = (unsigned char)src[i];
        dst[i] = (char)toupper(ch);
        i++;
    }
    dst[i] = '\0';
}

int ui_printf(const char *fmt, ...) // NOSONAR
{
    va_list args;
    va_start(args, fmt);
    char *formatted = sqlite3_vmprintf(fmt, args);
    va_end(args);

    if (!formatted)
    {
        return -1;
    }
    int rc = fprintf(stdout, "%s", formatted);
    sqlite3_free(formatted);
    return rc;
}

int ui_puts(const char *s)
{
    const char *text = s;
    if (!text)
    {
        text = "";
    }
    if (fputs(text, stdout) < 0)
    {
        return EOF;
    }
    if (fputc('\n', stdout) == EOF)
    {
        return EOF;
    }
    return 0;
}

int ui_putchar(int c)
{
    return fputc(c, stdout);
}

int ui_printf_centered_line(const char *fmt, ...) // NOSONAR
{
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    char *formatted = sqlite3_vmprintf(fmt, args);
    va_end(args);

    if (!formatted)
    {
        return -1;
    }

    snprintf(buffer, sizeof(buffer), "%s", formatted);
    sqlite3_free(formatted);


    return fprintf(stdout, "%s\n", buffer);
}

static int ui_readline(char *buffer, int size)
{
    if (!buffer || size <= 0)
    {
        return 0;
    }


    return fgets(buffer, size, stdin) != NULL;
}

/**
 * Permite la entrada de valores numericos por parte del usuario,
 * facilitando la configuracion de parametros enteros en el sistema.
 */
int input_int(const char *msg)
{
    char buffer[64];
    int v = 0;
    int attempts = 0;
    int eof_count = 0;

    while (attempts < 5)
    {
        ui_printf("%s", msg);

        if (!ui_readline(buffer, sizeof(buffer)))
        {
            eof_count++;
            if (eof_count >= 2)
            {
                ui_printf("Entrada cerrada (EOF).\n");
                return -1;
            }
            attempts++;
            continue;
        }

        size_t len = strlen_s(buffer, sizeof(buffer));
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
        {
            buffer[--len] = '\0';
        }

        if (len == 0)
        {
            ui_printf("Entrada vacia. Intente nuevamente.\n");
            continue;
        }

        char extra = '\0';
#if defined(_WIN32) && defined(_MSC_VER)
        if (sscanf_s(buffer, "%d %c", &v, &extra, 1) == 1)
#else
        if (sscanf(buffer, "%d %c", &v, &extra) == 1)
#endif
            return v;

        ui_printf("Entrada invalida. Intente nuevamente.\n");
        attempts++;
    }

    ui_printf("Se alcanzo el maximo de intentos.\n");
    return -1;
}

/* Implementacion portable de safe_strnlen */
size_t safe_strnlen(const char *s, size_t maxlen)
{
    if (s == NULL)
        return 0;
    size_t i;
    for (i = 0; i < maxlen; i++)
    {
        if (s[i] == '\0')
            break;
    }
    return i;
}

#if !defined(__STDC_LIB_EXT1__)
size_t strlen_s(const char *s, size_t maxlen)
{
    return safe_strnlen(s, maxlen);
}
#endif

/**
 * Determina si un punto en la posicion dada es un separador de miles o decimal.
 * Un punto se considera separador de miles si hay al menos 3 digitos despues de
 * el.
 */
static int is_thousands_separator(const char *buffer, int position)
{
    int remaining_digits = 0;

    // Contar digitos despues del punto actual
    for (int k = position + 1; buffer[k] != '\0'; k++)
    {
        if (isdigit(buffer[k]))
        {
            remaining_digits++;
        }
        else if (buffer[k] == ',' || buffer[k] == '.')
        {
            break;
        }
    }

    // Si hay al menos 3 digitos despues, es separador de miles
    return remaining_digits >= 3;
}

/**
 * Procesa un caracter individual y lo agrega al buffer de salida si es valido.
 * Devuelve 1 si se agrego un caracter, 0 si se omitio, -1 si se alcanzo el
 * limite.
 */
static int process_character(char c, char *output, size_t *j,
                             size_t output_size, int *has_decimal,
                             const char *input, int i)
{
    // Procesar coma como separador decimal
    if (c == ',' && !*has_decimal)
    {
        if (*j >= output_size - 1)
            return -1;
        output[(*j)++] = '.';
        *has_decimal = 1;
        return 1;
    }

    // Procesar punto (puede ser separador de miles o decimal)
    if (c == '.' && !*has_decimal)
    {
        if (is_thousands_separator(input, i))
            return 0; // Omitir separador de miles
        if (*j >= output_size - 1)
            return -1;
        output[(*j)++] = '.';
        *has_decimal = 1;
        return 1;
    }

    // Procesar digitos
    if (isdigit(c))
    {
        if (*j >= output_size - 1)
            return -1;
        output[(*j)++] = c;
        return 1;
    }

    // Ignorar otros caracteres
    return 0;
}

/**
 * Procesa un buffer de entrada para convertir separadores de miles y decimales
 * a formato estandar. Convierte comas a puntos decimales y elimina separadores
 * de miles.
 */
static void process_numeric_input(const char *input, char *output,
                                  size_t output_size)
{
    size_t j = 0;
    int has_decimal = 0;
    int ok = 1;

    if (input == NULL || output == NULL || output_size == 0)
    {
        if (output && output_size > 0)
            output[0] = '\0';
        return;
    }

    int i = 0;
    while (input[i] != '\0' && j < output_size - 1 && ok)
    {
        char c = input[i];

        ok = (process_character(c, output, &j, output_size, &has_decimal, input,
                                i) != -1);
        i++;
    }

    output[j] = '\0';
}

/**
 * Permite la entrada de valores de punto flotante por parte del usuario,
 * facilitando la configuracion de parametros decimales en el sistema.
 * Acepta tanto punto como coma como separador decimal, y maneja separadores de
 * miles.
 */
double input_double(const char *msg)
{
    char buffer[100];
    char processed[100];
    double v = 0.0;

    while (1)
    {
        ui_printf("%s", msg);

        if (!ui_readline(buffer, sizeof(buffer)))
            continue;
        buffer[strcspn(buffer, "\n")] = 0;

        process_numeric_input(buffer, processed, sizeof(processed));

        if (sscanf_s(processed, "%lf", &v) == 1)
            return v;
        ui_printf("Entrada invalida. Ingrese un numero valido (ej: 250, 1.500, "
                  "12.500, 250.000): ");
    }
}

/**
 * Valida la entrada de texto para asegurar la integridad de los datos y
 * prevenir errores en el procesamiento posterior, aceptando solo caracteres
 * alfanumericos y espacios.
 */
void input_string(const char *msg, char *buffer, int size)
{
    while (1)
    {
        ui_printf("%s", msg);

        if (!ui_readline(buffer, size))
            continue;
        buffer[strcspn(buffer, "\n")] = 0;

        int valid = 1;
        for (int i = 0; buffer[i] != '\0'; i++)
        {
            if (!isalpha(buffer[i]) && !isspace(buffer[i]) && !isdigit(buffer[i]))
            {
                valid = 0;
                break;
            }
        }

        if (valid)
            return;
        ui_printf("Entrada invalida. Solo se permiten letras, espacios y numeros.\n");
    }
}

void input_string_extended(const char *msg, char *buffer, int size)
{
    while (1)
    {
        ui_printf("%s", msg);

        if (!ui_readline(buffer, size))
            continue;
        buffer[strcspn(buffer, "\n")] = 0;

        int valid = 1;
        for (int i = 0; buffer[i] != '\0'; i++)
        {
            unsigned char c = (unsigned char)buffer[i];
            if (!isalnum(c) && !isspace(c) && c != '+' && c != '-' && c != '.'
                    && c != ',' && c != '(' && c != ')' && c != ':')
            {
                valid = 0;
                break;
            }
        }

        if (valid)
            return;
        ui_printf("Entrada invalida. Caracteres no permitidos.\n");
    }
}

void convert_display_date_to_storage(const char *display_date,
                                     char *storage_buffer, int buffer_size);

static void get_current_date(char *buffer, int size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%d/%m/%Y", &tm_struct);
}

static void get_current_time(char *buffer, int size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%H:%M", &tm_struct);
}

static void get_current_datetime(char *buffer, int size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%d/%m/%Y %H:%M", &tm_struct);
}

static int es_hora_sola(const char *buffer)
{
    return strchr(buffer, ':') != NULL && strchr(buffer, '/') == NULL && strchr(buffer, '-') == NULL;
}

static int msg_pide_datetime(const char *msg)
{
    if (!msg)
        return 0;
    return (strstr(msg, "HH:MM") || strstr(msg, "hh:mm")) &&
           (strstr(msg, "DD") || strstr(msg, "dd") || strstr(msg, "YYYY") || strstr(msg, "AAAA") || strstr(msg, "/"));
}

static int msg_pide_hora(const char *msg)
{
    if (!msg)
        return 0;
    return (strstr(msg, "HH:MM") || strstr(msg, "hh:mm")) && !msg_pide_datetime(msg);
}

static void completar_fecha_por_defecto(const char *msg, char *buffer, int size)
{
    if (msg_pide_datetime(msg))
    {
        get_current_datetime(buffer, size);
        return;
    }

    if (msg_pide_hora(msg))
    {
        get_current_time(buffer, size);
        return;
    }

    get_current_date(buffer, size);
}

static int validar_fecha_chars(const char *buffer)
{
    for (int i = 0; buffer[i] != '\0'; i++)
    {
        if (!isdigit(buffer[i]) && buffer[i] != '/' && buffer[i] != ':' && buffer[i] != ' ' && buffer[i] != '-')
        {
            return 0;
        }
    }
    return 1;
}

static int procesar_input_date(const char *msg, char *buffer, int size)
{
    if (safe_strnlen(buffer, (size_t)size) == 0)
    {
        completar_fecha_por_defecto(msg, buffer, size);
    }

    if (!validar_fecha_chars(buffer))
    {
        ui_printf("Entrada invalida. Solo se permiten digitos, barras diagonales (/), "
                  "guiones (-) y dos puntos (:).\n");
        return 0;
    }

    if (!es_hora_sola(buffer) && strchr(buffer, '/') != NULL)
    {
        char storage[64];
        convert_display_date_to_storage(buffer, storage, sizeof(storage));
        strncpy_s(buffer, (size_t)size, storage, (size_t)size - 1);
    }

    return 1;
}

/**
 * Valida la entrada de fecha para asegurar el formato correcto,
 * aceptando solo digitos, barras diagonales (/), guiones (-) y dos puntos (:).
 */
void input_date(const char *msg, char *buffer, int size)
{
    while (1)
    {
        ui_printf("%s", msg);

        if (!ui_readline(buffer, size))
            continue;
        buffer[strcspn(buffer, "\n")] = 0;

        if (procesar_input_date(msg, buffer, size))
        {
            return;
        }
    }
}

/**
 * Proporciona una representacion legible de la fecha y hora actual para mostrar
 * en interfaces de usuario, mejorando la experiencia al contextualizar acciones
 * con el tiempo.
 */
void get_datetime(char *buffer, int size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%d/%m/%Y %H:%M", &tm_struct);
}

/**
 * Genera un identificador temporal compacto para usar en nombres de archivos,
 * asegurando unicidad y orden cronologico en exportaciones y backups.
 */
void get_timestamp(char *buffer, int size)
{
    time_t t = time(NULL);
    struct tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    strftime(buffer, size, "%Y%m%d_%H%M", &tm_struct);
}

/**
 * Verifica la existencia de registros en la base de datos para mantener la
 * integridad referencial y evitar operaciones invalidas que puedan corromper
 * los datos.
 */
int existe_id(const char *tabla, int id)
{
    sqlite3_stmt *stmt;
    char sql[128];

    snprintf(sql, sizeof(sql), "SELECT 1 FROM %s WHERE id=?", tabla);
    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, id);

    int existe = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return existe;
}

/**
 * Limpia la pantalla de la consola para proporcionar una interfaz limpia y
 * organizada, mejorando la legibilidad de la informacion mostrada.
 */
void clear_screen()
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

static void free_nombre_usuario_if_needed(char *nombre_usuario)
{
    if (nombre_usuario && strcmp(nombre_usuario, "Usuario Desconocido") != 0)
    {
        free(nombre_usuario);
    }
}

static void print_header_stdout(const char *titulo_display,
                                const char *nombre_usuario, const char *fecha,
                                int mostrar_datos)
{
    printf("%s\n", titulo_display);
    if (mostrar_datos)
    {
        printf(" Usuario: %s\n", nombre_usuario);
        printf(" Fecha  : %s\n", fecha);
    }
    printf("========================================\n\n");
}

void print_header(const char *titulo)
{
    char fecha[20];
    get_datetime(fecha, sizeof(fecha));
    char titulo_buf[128];
    const char *titulo_display;

    char *nombre_usuario = get_user_name();
    if (!nombre_usuario)
    {
        nombre_usuario = "Usuario Desconocido";
    }

    uppercase_ascii(titulo, titulo_buf, sizeof(titulo_buf));
    titulo_display = titulo ? titulo_buf : "";
    int mostrar_datos = 1;
    if (titulo && strstr(titulo, "LISTADO") != NULL)
    {
        mostrar_datos = 0;
    }

    print_header_stdout(titulo_display, nombre_usuario, fecha,
                        mostrar_datos);
    free_nombre_usuario_if_needed(nombre_usuario);
}

/**
 * Pausa la ejecucion para permitir al usuario revisar informacion antes de
 * continuar, mejorando la interaccion controlada.
 */
void pause_console()
{
    ui_printf("\nPresione ENTER para continuar...");
    getchar();
}

/**
 * Solicita confirmacion binaria del usuario para operaciones criticas,
 * previniendo acciones accidentales que puedan afectar datos.
 */
int confirmar(const char *msg)
{
    int c;
    ui_printf("%s (S/N): ", msg);
    c = getchar();
    getchar();

    return (c == 's' || c == 'S');
}

static void leer_nombre_no_vacio(const char *prompt, const char *prompt_vacio,
                                 char *nombre, int size)
{
    ui_printf("%s", prompt);

    ui_readline(nombre, size);
    nombre[strcspn(nombre, "\n")] = 0;

    while (safe_strnlen(nombre, (size_t)size) == 0)
    {
        ui_printf("%s", prompt_vacio);
        ui_readline(nombre, size);
        nombre[strcspn(nombre, "\n")] = 0;
    }
}

static int leer_contrasena_no_vacia(const char *prompt, char *buffer, int size)
{
    if (!buffer || size <= 1)
    {
        return 0;
    }

    while (1)
    {
        ui_printf("%s", prompt);
        int pos = 0;
        int done = 0;

        memset(buffer, 0, (size_t)size);

        while (!done)
        {
#ifdef _WIN32
            int ch = _getch();
#else
            struct termios oldt;
            struct termios newt;
            int ch;

            if (tcgetattr(STDIN_FILENO, &oldt) != 0)
            {
                return 0;
            }
            newt = oldt;
            newt.c_lflag &= (unsigned int)~(ECHO | ICANON);
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);
            ch = getchar();
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif

            if (ch == '\r' || ch == '\n')
            {
                done = 1;
                ui_printf("\n");
            }
            else if ((ch == 8 || ch == 127) && pos > 0)
            {
                pos--;
                buffer[pos] = '\0';
                ui_printf("\b \b");
            }
            else if (ch >= 32 && ch <= 126 && pos < (size - 1))
            {
                buffer[pos++] = (char)ch;
                buffer[pos] = '\0';
                ui_printf("*");
            }
        }

        if (safe_strnlen(buffer, (size_t)size) >= 4)
        {
            return 1;
        }

        ui_printf("La contrasena debe tener al menos 4 caracteres.\n");
    }
}

static int password_symbols_enabled(void)
{
    return 1;
}

static int es_simbolo_password_permitido(unsigned char c)
{
    const char *allowed = "!@#$%^&*()-_=+[]{};:,.?";
    for (int i = 0; allowed[i] != '\0'; i++)
    {
        if ((unsigned char)allowed[i] == c)
        {
            return 1;
        }
    }
    return 0;
}

static int password_es_alfanumerica_con_reglas(const char *password)
{
    int has_upper = 0;
    int has_lower = 0;
    int has_digit = 0;

    if (!password)
    {
        return 0;
    }

    for (int i = 0; password[i] != '\0'; i++)
    {
        unsigned char c = (unsigned char)password[i];
        if (!isalnum(c) && !(password_symbols_enabled() && es_simbolo_password_permitido(c)))
        {
            return 0;
        }
        if (isupper(c))
        {
            has_upper = 1;
        }
        else if (islower(c))
        {
            has_lower = 1;
        }
        else if (isdigit(c))
        {
            has_digit = 1;
        }
    }

    return has_upper && has_lower && has_digit;
}

static const char *fortaleza_password(const char *password)
{
    int has_upper = 0;
    int has_lower = 0;
    int has_digit = 0;
    int score = 0;
    size_t len = safe_strnlen(password, 256);

    for (size_t i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char)password[i];
        if (isupper(c))
        {
            has_upper = 1;
        }
        else if (islower(c))
        {
            has_lower = 1;
        }
        else if (isdigit(c))
        {
            has_digit = 1;
        }
    }

    if (len >= 8)
    {
        score++;
    }
    if (len >= 12)
    {
        score++;
    }
    score += has_upper + has_lower + has_digit;

    if (score >= 5)
    {
        return "ALTA";
    }
    if (score >= 3)
    {
        return "MEDIA";
    }
    return "BAJA";
}

static int flujo_configurar_password(int requiere_actual)
{
    char nueva[128];
    char confirmar_pwd[128];

    (void)requiere_actual;

    if (!leer_contrasena_no_vacia("Ingresa tu nueva contrasena: ", nueva, (int)sizeof(nueva)))
    {
        return 0;
    }

    if (!password_es_alfanumerica_con_reglas(nueva))
    {
        if (password_symbols_enabled())
        {
            ui_printf("La contrasena debe incluir mayuscula, minuscula y numero. Puede usar letras, numeros y simbolos permitidos (!@#$%^&*()-_=+[]{};:,.?).\n");
        }
        else
        {
            ui_printf("La contrasena debe usar solo letras y numeros, e incluir mayuscula, minuscula y numero.\n");
        }
        return 0;
    }

    ui_printf("Fortaleza estimada: %s\n", fortaleza_password(nueva));

    if (!leer_contrasena_no_vacia("Confirma tu nueva contrasena: ", confirmar_pwd, (int)sizeof(confirmar_pwd)))
    {
        return 0;
    }

    if (strcmp(nueva, confirmar_pwd) != 0)
    {
        ui_printf("Las contrasenas no coinciden.\n");
        return 0;
    }

    if (!set_user_password(nueva))
    {
        ui_printf("No se pudo guardar la contrasena.\n");
        return 0;
    }

    ui_printf("Contrasena guardada correctamente.\n");
    return 1;
}

static int auth_username_exists(sqlite3 *auth_db, const char *username)
{
    sqlite3_stmt *stmt = NULL;
    int exists = 0;

    if (sqlite3_prepare_v2(auth_db, "SELECT 1 FROM local_users WHERE username = ? LIMIT 1;", -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        exists = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
    }
    return exists;
}

static int auth_upsert_user(sqlite3 *auth_db, const char *username, const char *plain_password)
{
    sqlite3_stmt *stmt = NULL;
    char salt[33] = "";
    char hash[17] = "";
    const char *sql = "INSERT INTO local_users(username, password_salt, password_hash) VALUES(?, ?, ?);";

    if (plain_password && plain_password[0] != '\0')
    {
        auth_generate_salt_hex(salt, sizeof(salt));
        auth_build_password_hash(plain_password, salt, hash, sizeof(hash));
    }

    if (sqlite3_prepare_v2(auth_db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, salt, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, hash, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    return 1;
}

static int auth_get_password_fields(sqlite3 *auth_db, const char *username,
                                    char *salt_out, size_t salt_size,
                                    char *hash_out, size_t hash_size)
{
    sqlite3_stmt *stmt = NULL;
    int ok = 0;

    if (sqlite3_prepare_v2(auth_db,
                           "SELECT password_salt, password_hash FROM local_users WHERE username = ? LIMIT 1;",
                           -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *salt = (const char *)sqlite3_column_text(stmt, 0);
        const char *hash = (const char *)sqlite3_column_text(stmt, 1);
        if (salt_out && salt_size > 0)
        {
            strncpy_s(salt_out, salt_size, salt ? salt : "", salt_size - 1);
        }
        if (hash_out && hash_size > 0)
        {
            strncpy_s(hash_out, hash_size, hash ? hash : "", hash_size - 1);
        }
        ok = 1;
    }

    sqlite3_finalize(stmt);
    return ok;
}

static int auth_verify_user_password(sqlite3 *auth_db, const char *username, const char *plain_password)
{
    char salt[64];
    char hash[64];
    char computed[32];

    if (!auth_get_password_fields(auth_db, username, salt, sizeof(salt), hash, sizeof(hash)))
    {
        return 0;
    }

    if (hash[0] == '\0')
    {
        return 1;
    }

    auth_build_password_hash(plain_password, salt, computed, sizeof(computed));
    return strcmp(hash, computed) == 0;
}

static int auth_user_requires_password(sqlite3 *auth_db, const char *username)
{
    char salt[64];
    char hash[64];
    if (!auth_get_password_fields(auth_db, username, salt, sizeof(salt), hash, sizeof(hash)))
    {
        return 0;
    }
    return hash[0] != '\0';
}

static int auth_prompt_password_login(sqlite3 *auth_db, const char *username)
{
    char intento[128];

    if (!auth_user_requires_password(auth_db, username))
    {
        return 1;
    }

    for (int i = 0; i < 3; i++)
    {
        if (!leer_contrasena_no_vacia("Contrasena: ", intento, (int)sizeof(intento)))
        {
            continue;
        }

        if (auth_verify_user_password(auth_db, username, intento))
        {
            return 1;
        }

        ui_printf("Contrasena incorrecta. Intentos restantes: %d\n", 2 - i);
    }

    return 0;
}

static int auth_registrar_usuario_interactivo(sqlite3 *auth_db)
{
    char username[64];
    char respuesta[16];
    char nueva[128] = "";
    char confirmar_pwd[128] = "";

    leer_nombre_no_vacio("Nuevo usuario (3-32, letras/numeros/_/-): ",
                         "Usuario obligatorio: ", username, (int)sizeof(username));

    if (!auth_username_valido(username))
    {
        ui_printf("Usuario invalido. Usa 3-32 caracteres: letras, numeros, '_' o '-'.\n");
        return 0;
    }

    if (auth_username_exists(auth_db, username))
    {
        ui_printf("Ese usuario ya existe.\n");
        return 0;
    }

    ui_printf("Deseas poner contrasena para este usuario? (S/N): ");
    if (!fgets(respuesta, sizeof(respuesta), stdin))
    {
        return 0;
    }

    if (respuesta[0] == 's' || respuesta[0] == 'S')
    {
        if (!leer_contrasena_no_vacia("Ingresa contrasena: ", nueva, (int)sizeof(nueva)))
        {
            return 0;
        }
        if (!password_es_alfanumerica_con_reglas(nueva))
        {
            ui_printf("La contrasena debe incluir mayuscula, minuscula y numero.\n");
            return 0;
        }
        ui_printf("Fortaleza estimada: %s\n", fortaleza_password(nueva));
        if (!leer_contrasena_no_vacia("Confirma contrasena: ", confirmar_pwd, (int)sizeof(confirmar_pwd)))
        {
            return 0;
        }
        if (strcmp(nueva, confirmar_pwd) != 0)
        {
            ui_printf("Las contrasenas no coinciden.\n");
            return 0;
        }
    }

    if (!auth_upsert_user(auth_db, username, nueva))
    {
        ui_printf("No se pudo crear el usuario.\n");
        return 0;
    }

    ui_printf("Usuario '%s' creado correctamente.\n", username);
    return 1;
}

static int auth_seleccionar_usuario(sqlite3 *auth_db, char *username_out, size_t out_size)
{
    sqlite3_stmt *stmt = NULL;
    char users[64][64];
    int count = 0;

    if (!username_out || out_size == 0)
    {
        return 0;
    }

    if (sqlite3_prepare_v2(auth_db, "SELECT username FROM local_users ORDER BY username;", -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW && count < 64)
    {
        const char *u = (const char *)sqlite3_column_text(stmt, 0);
        strncpy_s(users[count], sizeof(users[count]), u ? u : "", sizeof(users[count]) - 1);
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0)
    {
        return 0;
    }

    ui_printf("\nUsuarios locales:\n");
    for (int i = 0; i < count; i++)
    {
        ui_printf("%d. %s\n", i + 1, users[i]);
    }
    ui_printf("0. Volver\n");

    int op = input_int("> ");
    if (op <= 0 || op > count)
    {
        return 0;
    }

    strncpy_s(username_out, out_size, users[op - 1], out_size - 1);
    return 1;
}

static int auth_get_single_username(sqlite3 *auth_db, char *username_out, size_t out_size)
{
    sqlite3_stmt *stmt = NULL;
    int ok = 0;

    if (!username_out || out_size == 0)
    {
        return 0;
    }

    if (sqlite3_prepare_v2(auth_db,
                           "SELECT username FROM local_users ORDER BY username LIMIT 1;",
                           -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *u = (const char *)sqlite3_column_text(stmt, 0);
        strncpy_s(username_out, out_size, u ? u : "", out_size - 1);
        ok = (username_out[0] != '\0');
    }

    sqlite3_finalize(stmt);
    return ok;
}

static int auth_activar_usuario_y_cerrar(sqlite3 *auth_db, const char *username)
{
    db_set_active_user(username);
    sqlite3_close(auth_db);
    return 1;
}

static int auth_flujo_sin_usuarios(sqlite3 *auth_db, char *selected_user, size_t selected_user_size)
{
    if (auth_importar_usuario_legado_si_existe(auth_db))
    {
        return 0;
    }

    ui_printf("\nNo hay usuarios locales. Crea tu primer usuario para continuar.\n");
    ui_printf("Con un solo usuario es suficiente; agregar mas usuarios es opcional.\n");

    if (!auth_registrar_usuario_interactivo(auth_db))
    {
        return 0;
    }

    if (!auth_seleccionar_usuario(auth_db, selected_user, selected_user_size))
    {
        return 0;
    }

    return 1;
}

static int auth_flujo_un_usuario(sqlite3 *auth_db, char *selected_user, size_t selected_user_size)
{
    if (!auth_get_single_username(auth_db, selected_user, selected_user_size))
    {
        ui_printf("No se pudo cargar el usuario local.\n");
        return 0;
    }

    if (!auth_prompt_password_login(auth_db, selected_user))
    {
        ui_printf("Acceso denegado.\n");
        return 0;
    }

    return 1;
}

static int auth_menu_multiusuario(sqlite3 *auth_db, char *selected_user, size_t selected_user_size)
{
    ui_printf("\n=== MULTIUSUARIO LOCAL ===\n");
    ui_printf("1. Iniciar sesion\n");
    ui_printf("2. Agregar usuario local (opcional)\n");
    ui_printf("0. Salir\n");

    int op = input_int("> ");
    if (op == 0)
    {
        return 0;
    }

    if (op == 2)
    {
        auth_registrar_usuario_interactivo(auth_db);
        return -1;
    }

    if (op != 1)
    {
        ui_printf("Opcion invalida.\n");
        return -1;
    }

    if (!auth_seleccionar_usuario(auth_db, selected_user, selected_user_size))
    {
        return -1;
    }

    if (!auth_prompt_password_login(auth_db, selected_user))
    {
        ui_printf("Acceso denegado.\n");
        return -1;
    }

    return 1;
}

int iniciar_sesion_multiusuario_local(void)
{
    sqlite3 *auth_db = NULL;
    char selected_user[64];

    if (!auth_open(&auth_db))
    {
        ui_printf("No se pudo abrir el registro local de usuarios.\n");
        return 0;
    }

    while (1)
    {
        int total = auth_user_count(auth_db);
        if (total == 0)
        {
            if (auth_flujo_sin_usuarios(auth_db, selected_user, sizeof(selected_user)))
            {
                return auth_activar_usuario_y_cerrar(auth_db, selected_user);
            }
            continue;
        }

        if (total == 1)
        {
            if (auth_flujo_un_usuario(auth_db, selected_user, sizeof(selected_user)))
            {
                return auth_activar_usuario_y_cerrar(auth_db, selected_user);
            }
            continue;
        }

        int action = auth_menu_multiusuario(auth_db, selected_user, sizeof(selected_user));
        if (action == 0)
        {
            sqlite3_close(auth_db);
            return 0;
        }
        if (action < 0)
        {
            continue;
        }

        return auth_activar_usuario_y_cerrar(auth_db, selected_user);
    }
}

void configurar_password_inicial_opcional()
{
    char respuesta[16];

    if (user_has_password())
    {
        return;
    }

    ui_printf("\nDeseas configurar una contrasena para tu usuario? (S/N): ");
    if (!fgets(respuesta, sizeof(respuesta), stdin))
    {
        return;
    }

    if (respuesta[0] == 's' || respuesta[0] == 'S')
    {
        flujo_configurar_password(0);
    }
    else
    {
        ui_printf("Contrasena omitida. Puedes configurarla luego en Ajustes -> Usuario.\n");
    }
}

int autenticar_usuario_si_tiene_password()
{
    char intento[128];

    if (!user_has_password())
    {
        return 1;
    }

    ui_printf("\nEste perfil tiene contrasena.\n");
    for (int i = 0; i < 3; i++)
    {
        if (!leer_contrasena_no_vacia("Ingresa tu contrasena: ", intento, (int)sizeof(intento)))
        {
            continue;
        }

        if (verify_user_password(intento))
        {
            return 1;
        }

        ui_printf("Contrasena incorrecta. Intentos restantes: %d\n", 2 - i);
    }

    return 0;
}

static void configurar_o_cambiar_password_usuario()
{
    sqlite3 *auth_db = NULL;
    char actual[128];
    char nueva[128];
    char confirmar_pwd[128];
    sqlite3_stmt *stmt = NULL;
    const char *username = db_get_active_user();
    char salt[33];
    char hash[17];

    if (!username || username[0] == '\0')
    {
        ui_printf("No hay sesion activa.\n");
        pause_console();
        return;
    }

    if (!auth_open(&auth_db))
    {
        ui_printf("No se pudo abrir el registro de usuarios.\n");
        pause_console();
        return;
    }

    if (auth_user_requires_password(auth_db, username))
    {
        if (!leer_contrasena_no_vacia("Ingresa tu contrasena actual: ", actual, (int)sizeof(actual)))
        {
            ui_printf("Contrasena actual incorrecta.\n");
            sqlite3_close(auth_db);
            pause_console();
            return;
        }

        if (!auth_verify_user_password(auth_db, username, actual))
        {
            ui_printf("Contrasena actual incorrecta.\n");
            sqlite3_close(auth_db);
            pause_console();
            return;
        }
    }

    if (!leer_contrasena_no_vacia("Ingresa tu nueva contrasena: ", nueva, (int)sizeof(nueva)))
    {
        sqlite3_close(auth_db);
        pause_console();
        return;
    }
    if (!password_es_alfanumerica_con_reglas(nueva))
    {
        ui_printf("La contrasena debe incluir mayuscula, minuscula y numero.\n");
        sqlite3_close(auth_db);
        pause_console();
        return;
    }
    ui_printf("Fortaleza estimada: %s\n", fortaleza_password(nueva));
    if (!leer_contrasena_no_vacia("Confirma tu nueva contrasena: ", confirmar_pwd, (int)sizeof(confirmar_pwd)) ||
            strcmp(nueva, confirmar_pwd) != 0)
    {
        ui_printf("Las contrasenas no coinciden.\n");
        sqlite3_close(auth_db);
        pause_console();
        return;
    }

    auth_generate_salt_hex(salt, sizeof(salt));
    auth_build_password_hash(nueva, salt, hash, sizeof(hash));

    if (sqlite3_prepare_v2(auth_db,
                           "UPDATE local_users SET password_salt = ?, password_hash = ? WHERE username = ?;",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, salt, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, username, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            ui_printf("Contrasena actualizada correctamente.\n");
        }
        else
        {
            ui_printf("No se pudo actualizar la contrasena.\n");
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(auth_db);
    pause_console();
}

static void quitar_password_usuario()
{
    sqlite3 *auth_db = NULL;
    sqlite3_stmt *stmt = NULL;
    const char *username = db_get_active_user();
    char actual[128];

    if (!username || username[0] == '\0')
    {
        ui_printf("No hay sesion activa.\n");
        pause_console();
        return;
    }

    if (!auth_open(&auth_db))
    {
        ui_printf("No se pudo abrir el registro de usuarios.\n");
        pause_console();
        return;
    }

    if (auth_user_requires_password(auth_db, username))
    {
        if (!leer_contrasena_no_vacia("Para quitarla, ingresa tu contrasena actual: ", actual, (int)sizeof(actual)))
        {
            ui_printf("Contrasena incorrecta.\n");
            sqlite3_close(auth_db);
            pause_console();
            return;
        }

        if (!auth_verify_user_password(auth_db, username, actual))
        {
            ui_printf("Contrasena incorrecta.\n");
            sqlite3_close(auth_db);
            pause_console();
            return;
        }
    }

    if (sqlite3_prepare_v2(auth_db,
                           "UPDATE local_users SET password_salt = '', password_hash = '' WHERE username = ?;",
                           -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            ui_printf("Contrasena eliminada correctamente.\n");
        }
        else
        {
            ui_printf("No se pudo eliminar la contrasena.\n");
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(auth_db);
    pause_console();
}

/**
 * Recopila la identidad del usuario en el inicio para personalizar la
 * aplicacion y mantener un registro de uso.
 */
void pedir_nombre_usuario()
{
    char nombre[100];
    clear_screen();
    leer_nombre_no_vacio("Por favor, ingresa tu Nombre: ",
                         "El nombre no puede estar vacio. Ingresa tu nombre: ",
                         nombre, (int)sizeof(nombre));

    if (set_user_name(nombre))
    {
        ui_printf("!Bienvenido, %s!\n", nombre);
    }
    else
    {
        ui_printf("Error al guardar el nombre. Intenta nuevamente.\n");
    }
    pause_console();
}

/**
 * Permite al usuario verificar su identidad actual almacenada,
 * facilitando la gestion de su perfil.
 */
void mostrar_nombre_usuario()
{
    char *nombre = get_user_name();
    if (nombre)
    {
        ui_printf("Tu nombre actual es: %s\n", nombre);
        free(nombre);
    }
    else
    {
        ui_printf("No se pudo obtener el nombre del usuario.\n");
    }
    pause_console();
}

/**
 * Habilita la actualizacion de la identidad del usuario para mantener la
 * informacion actualizada y personalizada.
 */
void editar_nombre_usuario()
{
    char nombre[100];
    leer_nombre_no_vacio("Ingresa tu nuevo nombre: ",
                         "El nombre no puede estar vacio. Ingresa tu nuevo nombre: ",
                         nombre, (int)sizeof(nombre));

    if (set_user_name(nombre))
    {
        ui_printf("Nombre actualizado exitosamente a: %s\n", nombre);
    }
    else
    {
        ui_printf("Error al actualizar el nombre.\n");
    }
    pause_console();
}

static void agregar_usuario_local()
{
    sqlite3 *auth_db = NULL;
    if (!auth_open(&auth_db))
    {
        ui_printf("No se pudo abrir el registro de usuarios.\n");
        pause_console();
        return;
    }

    auth_registrar_usuario_interactivo(auth_db);
    sqlite3_close(auth_db);
    pause_console();
}

static void eliminar_mi_cuenta_local()
{
    sqlite3 *auth_db = NULL;
    sqlite3_stmt *stmt = NULL;
    const char *username = db_get_active_user();
    char actual[128];
    char user_db_path[1024];
    char user_log_path[1024];

    if (!username || username[0] == '\0')
    {
        ui_printf("No hay sesion activa.\n");
        pause_console();
        return;
    }

    ui_printf("ATENCION: Esta accion es IRREVERSIBLE y eliminara tu cuenta local.\n");
    if (!confirmar("Estas seguro de continuar"))
    {
        ui_printf("Operacion cancelada.\n");
        pause_console();
        return;
    }

    if (!auth_open(&auth_db))
    {
        ui_printf("No se pudo abrir el registro de usuarios.\n");
        pause_console();
        return;
    }

    if (auth_user_requires_password(auth_db, username))
    {
        if (!leer_contrasena_no_vacia("Confirma tu contrasena actual: ", actual, (int)sizeof(actual)))
        {
            ui_printf("Contrasena incorrecta.\n");
            sqlite3_close(auth_db);
            pause_console();
            return;
        }

        if (!auth_verify_user_password(auth_db, username, actual))
        {
            ui_printf("Contrasena incorrecta.\n");
            sqlite3_close(auth_db);
            pause_console();
            return;
        }
    }

    if (sqlite3_prepare_v2(auth_db, "DELETE FROM local_users WHERE username = ?;", -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            ui_printf("Cuenta eliminada. Cerrando aplicacion...\n");
        }
        else
        {
            ui_printf("No se pudo eliminar la cuenta.\n");
            sqlite3_finalize(stmt);
            sqlite3_close(auth_db);
            pause_console();
            return;
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(auth_db);

    auth_get_user_data_paths(username,
                             user_db_path, sizeof(user_db_path),
                             user_log_path, sizeof(user_log_path));
    db_close();
    remove(user_db_path);
    remove(user_log_path);
    exit(0);
}

/**
 * Proporciona una interfaz estructurada para gestionar opciones relacionadas
 * con el perfil del usuario.
 */
void menu_usuario()
{
    MenuItem items[] = {{1, "Mostrar Nombre", mostrar_nombre_usuario},
        {2, "Editar Nombre Visible", editar_nombre_usuario},
        {3, "Agregar Usuario Local", agregar_usuario_local},
        {4, "Modificar Mi Contrasena", configurar_o_cambiar_password_usuario},
        {5, "Quitar Mi Contrasena", quitar_password_usuario},
        {6, "Eliminar Mi Cuenta (Irreversible)", eliminar_mi_cuenta_local},
        {0, "Volver", NULL}
    };

    ejecutar_menu_estandar("USUARIO", items, 7);
}

/**
 * Adapta fechas del almacenamiento interno a un formato amigable para la
 * visualizacion, permitiendo flexibilidad en formatos futuros.
 */
void format_date_for_display(const char *input_date, char *output_buffer,
                             int buffer_size)
{
    if (!input_date || buffer_size <= 0)
        return;

    if (safe_strnlen(input_date, (size_t)buffer_size) >= 10 && input_date[4] == '-' && input_date[7] == '-')
    {
        char fecha[16];
        fecha[0] = input_date[8];
        fecha[1] = input_date[9];
        fecha[2] = '/';
        fecha[3] = input_date[5];
        fecha[4] = input_date[6];
        fecha[5] = '/';
        fecha[6] = input_date[0];
        fecha[7] = input_date[1];
        fecha[8] = input_date[2];
        fecha[9] = input_date[3];
        fecha[10] = '\0';

        if (input_date[10] == ' ')
        {
            snprintf(output_buffer, (size_t)buffer_size, "%s%s", fecha, input_date + 10);
        }
        else
        {
            strncpy_s(output_buffer, buffer_size, fecha, buffer_size - 1);
        }
        return;
    }

    strncpy_s(output_buffer, buffer_size, input_date, buffer_size - 1);
}

/**
 * Convierte fechas ingresadas por el usuario a un formato interno consistente,
 * facilitando el almacenamiento y procesamiento uniforme.
 */
void convert_display_date_to_storage(const char *display_date,
                                     char *storage_buffer, int buffer_size)
{
    if (!display_date || buffer_size <= 0)
        return;

    if (strchr(display_date, '/') != NULL && safe_strnlen(display_date, (size_t)buffer_size) >= 10)
    {
        char yyyy[5] = {display_date[6], display_date[7], display_date[8], display_date[9], '\0'};
        char mm[3] = {display_date[3], display_date[4], '\0'};
        char dd[3] = {display_date[0], display_date[1], '\0'};

        if (display_date[10] == ' ')
        {
            snprintf(storage_buffer, (size_t)buffer_size, "%s-%s-%s%s", yyyy, mm, dd, display_date + 10);
        }
        else
        {
            snprintf(storage_buffer, (size_t)buffer_size, "%s-%s-%s", yyyy, mm, dd);
        }
        return;
    }

    strncpy_s(storage_buffer, buffer_size, display_date, buffer_size - 1);
}

/**
 * Normaliza cadenas de texto removiendo caracteres acentuados para asegurar
 * compatibilidad con sistemas que no los soportan y mejorar la consistencia en
 * busquedas.
 */
char *remover_tildes(const char *str)
{
    static char buffer[256];
    size_t j = 0;

    const char *p = str;
    while (*p != '\0' && j < sizeof(buffer) - 1)
    {
        unsigned char c = (unsigned char)*p++;
        if (c == 0xE1 || c == 0xC1)
            buffer[j++] = 'a'; // a, a
        else if (c == 0xE9 || c == 0xC9)
            buffer[j++] = 'e'; // e, e
        else if (c == 0xED || c == 0xCD)
            buffer[j++] = 'i'; // i, i
        else if (c == 0xF3 || c == 0xD3)
            buffer[j++] = 'o'; // o, o
        else if (c == 0xFA || c == 0xDA)
            buffer[j++] = 'u'; // u, u
        else if (c == 0xF1 || c == 0xD1)
            buffer[j++] = 'n'; // n, n
        else if (c == 0xFC || c == 0xDC)
            buffer[j++] = 'u'; // u, u
        else
            buffer[j++] = (char)c;
    }
    buffer[j] = '\0';
    return buffer;
}

/**
 * Convierte un valor de resultado a texto
 *
 * @param resultado El valor numerico del resultado
 * @return La representacion textual del resultado
 */
const char *resultado_to_text(int resultado)
{
    switch (resultado)
    {
    case 1:
        return "VICTORIA";
    case 2:
        return "EMPATE";
    case 3:
        return "DERROTA";
    default:
        return "DESCONOCIDO";
    }
}

/**
 * Convierte un valor de clima a texto
 *
 * @param clima El valor numerico del clima
 * @return La representacion textual del clima
 */
const char *clima_to_text(int clima)
{
    switch (clima)
    {
    case 1:
        return "Despejado";
    case 2:
        return "Nublado";
    case 3:
        return "Lluvia";
    case 4:
        return "Ventoso";
    case 5:
        return "Mucho Calor";
    case 6:
        return "Mucho Frio";
    default:
        return "DESCONOCIDO";
    }
}

/**
 * Convierte un valor de dia a texto
 *
 * @param dia El valor numerico del dia
 * @return La representacion textual del dia
 */
const char *dia_to_text(int dia)
{
    switch (dia)
    {
    case 1:
        return "Dia";
    case 2:
        return "Tarde";
    case 3:
        return "Noche";
    default:
        return "DESCONOCIDO";
    }
}

/**
 * Obtiene el nombre de una entidad por su ID desde la base de datos.
 * Funcion generica para evitar duplicacion de codigo en consultas SQL comunes.
 *
 * @param tabla Nombre de la tabla (ej: "camiseta", "torneo", "cancha")
 * @param id ID de la entidad a buscar
 * @param buffer Buffer donde se almacenara el nombre encontrado
 * @param size Tamano maximo del buffer
 * @return 1 si se encontro la entidad, 0 si no se encontro
 */
int obtener_nombre_entidad(const char *tabla, int id, char *buffer, size_t size)
{
    sqlite3_stmt *stmt;
    char sql[256];

    if (!tabla || !buffer || size == 0)
    {
        return 0;
    }

    snprintf(sql, sizeof(sql), "SELECT nombre FROM %s WHERE id = ?", tabla);

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, id);

    int found = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *nombre = (const char *)sqlite3_column_text(stmt, 0);
        if (nombre)
        {
            snprintf(buffer, size, "%s", nombre);
            found = 1;
        }
    }

    sqlite3_finalize(stmt);
    return found;
}

/**
 * @brief Obtiene el siguiente ID disponible para una tabla
 * Implementa el patron usado en camiseta, cancha, lesion, etc.
 */
long long obtener_siguiente_id(const char *tabla)
{
    sqlite3_stmt *stmt;
    char sql[512];

    if (!tabla) return 1;

    /*
     * Esta consulta utiliza una CTE (Common Table Expression) recursiva para generar
     * una secuencia de numeros y encontrar el primer "hueco" (ID faltante) en la tabla.
     * Esto permite reutilizar IDs de registros eliminados manteniendo la secuencia compacta.
     */
    snprintf(sql, sizeof(sql),
             "WITH RECURSIVE seq(id) AS (VALUES(1) UNION ALL SELECT id+1 FROM seq WHERE id < (SELECT COALESCE(MAX(id),0)+1 FROM %s)) "
             "SELECT MIN(id) FROM seq WHERE id NOT IN (SELECT id FROM %s)",
             tabla, tabla);

    if (!preparar_stmt(sql, &stmt))
    {
        return 1;
    }

    int id = 1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

/**
 * @brief Verifica si hay registros en una tabla
 */
int hay_registros(const char *tabla)
{
    sqlite3_stmt *stmt;
    char sql[256];

    if (!tabla) return 0;

    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s", tabla);
    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return count > 0;
}

/**
 * @brief Obtiene el ID de una entidad por nombre
 */
int obtener_id_por_nombre(const char *tabla, const char *nombre)
{
    sqlite3_stmt *stmt;
    char sql[256];

    if (!tabla || !nombre) return -1;

    snprintf(sql, sizeof(sql), "SELECT id FROM %s WHERE nombre = ?", tabla);
    if (!preparar_stmt(sql, &stmt))
    {
        return -1;
    }
    sqlite3_bind_text(stmt, 1, nombre, -1, SQLITE_TRANSIENT);

    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return id;
}

/**
 * @brief Lista todas las entidades de una tabla
 */
void listar_entidades(const char *tabla, const char *titulo, const char *mensaje_vacio)
{
    if (!tabla || !titulo || !mensaje_vacio) return;

    clear_screen();
    print_header(titulo);

    sqlite3_stmt *stmt;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT id, nombre FROM %s ORDER BY id", tabla);
    if (!preparar_stmt(sql, &stmt))
    {
        pause_console();
        return;
    }

    int hay = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ui_printf_centered_line("%d - %s",
                                sqlite3_column_int(stmt, 0),
                                sqlite3_column_text(stmt, 1));
        hay = 1;
    }

    if (!hay)
        ui_printf_centered_line("%s", mensaje_vacio);

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Solicita un entero validado en rango
 */
int input_int_rango(const char *msg, int min, int max)
{
    int valor;
    do
    {
        valor = input_int(msg);
        if (valor < min || valor > max)
            printf("Ingrese un valor entre %d y %d.\n", min, max);
    }
    while (valor < min || valor > max);
    return valor;
}

/**
 * @brief Muestra "no hay registros"
 */
void mostrar_no_hay_registros(const char *entidad)
{
    if (!entidad) return;
    size_t len = safe_strnlen(entidad, SIZE_MAX);
    printf("No hay %s registrad%s.\n", entidad,
           (entidad[len-1] == 'a' || entidad[len-1] == 'o') ? "o" : "os");
}

/**
 * @brief Muestra "entidad no existe"
 */
void mostrar_no_existe(const char *entidad)
{
    if (!entidad) return;
    printf("El %s no existe.\n", entidad);
}

/**
 * @brief Muestra error de operacion
 */
void mostrar_error_operacion(const char *entidad, const char *operacion)
{
    if (!entidad || !operacion) return;
    printf("Error al %s el %s.\n", operacion, entidad);
}

/**
 * @brief Atajo para clear_screen + print_header
 */
void mostrar_pantalla(const char *titulo)
{
    clear_screen();
    print_header(titulo);
}

/**
 * @brief Elimina espacios en blanco al final de una cadena
 */
char* trim_trailing_spaces(char *str)
{
    if (!str) return str;

    size_t len = safe_strnlen(str, SIZE_MAX);
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t' || str[len-1] == '\n' || str[len-1] == '\r'))
    {
        str[--len] = '\0';
    }
    return str;
}

/**
 * @brief Ejecuta una consulta SQL y devuelve el statement preparado
 * Funcion comun para centralizar la ejecucion de consultas
 */
sqlite3_stmt* execute_query(const char *sql)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(sql, &stmt))
    {
        return NULL;
    }
    return stmt;
}

/**
 * @brief Lista equipos disponibles para seleccion
 * Funcion comun usada en multiples modulos para mostrar equipos
 */
int list_available_teams(const char *no_records_msg, int pause_on_empty)
{
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, nombre FROM equipo ORDER BY id;";

    if (preparar_stmt(sql, &stmt))
    {
        ui_printf_centered_line("=== EQUIPOS DISPONIBLES ===");
        ui_printf("\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int id = sqlite3_column_int(stmt, 0);
            const char *nombre = (const char*)sqlite3_column_text(stmt, 1);
            ui_printf_centered_line("%d. %s", id, nombre);
        }

        if (!found)
        {
            mostrar_no_hay_registros(no_records_msg);
            sqlite3_finalize(stmt);
            if (pause_on_empty)
            {
                pause_console();
            }
            return 0;
        }
        sqlite3_finalize(stmt);
        return 1;
    }

    if (pause_on_empty)
    {
        pause_console();
    }
    return 0;
}

/**
 * @brief Obtiene el ID de un equipo seleccionado por el usuario
 * Funcion comun para seleccion de equipos con validacion
 */
int select_team_id(const char *prompt, const char *no_records_msg, int pause_on_error)
{
    if (!list_available_teams(no_records_msg, pause_on_error))
    {
        return 0;
    }
    int equipo_id = input_int(prompt);
    if (equipo_id == 0)
    {
        return 0;
    }
    if (!existe_id("equipo", equipo_id))
    {
        printf("ID de equipo invalido.\n");
        if (pause_on_error)
        {
            pause_console();
        }
        return 0;
    }
    return equipo_id;
}

/**
 * @brief Escribe encabezado CSV para exportaciones
 * Funcion comun para formato consistente en exportaciones CSV
 */
void write_csv_header(FILE *f, const char *header)
{
    fprintf(f, "%s\n", header);
}

static char *get_trimmed_cancha_from_stmt(sqlite3_stmt *stmt)
{
    char *cancha_trimmed = strdup((const char *)sqlite3_column_text(stmt, 0));
    trim_trailing_spaces(cancha_trimmed);
    return cancha_trimmed;
}

/**
 * @brief Escribe fila CSV con datos de partido
 * Funcion comun para exportar datos de partidos a CSV
 */
void write_partido_csv_row(FILE *f, sqlite3_stmt *stmt)
{
    char *cancha_trimmed = get_trimmed_cancha_from_stmt(stmt);
    fprintf(f, "%s,%s,%d,%d,%s,%s,%s,%s,%d,%d,%d,%s\n",
            cancha_trimmed,
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_text(stmt, 4),
            resultado_to_text(sqlite3_column_int(stmt, 5)),
            clima_to_text(sqlite3_column_int(stmt, 6)),
            dia_to_text(sqlite3_column_int(stmt, 7)),
            sqlite3_column_int(stmt, 8),
            sqlite3_column_int(stmt, 9),
            sqlite3_column_int(stmt, 10),
            sqlite3_column_text(stmt, 11));
    free(cancha_trimmed);
}

/**
 * @brief Escribe fila TXT con datos de partido
 * Funcion comun para exportar datos de partidos a TXT
 */
void write_partido_txt_row(FILE *f, sqlite3_stmt *stmt)
{
    char *cancha_trimmed = get_trimmed_cancha_from_stmt(stmt);
    fprintf(f, "%s | %s | G:%d A:%d | %s | Res:%s Cli:%s Dia:%s RG:%d Can:%d EA:%d | %s\n",
            cancha_trimmed,
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_text(stmt, 4),
            resultado_to_text(sqlite3_column_int(stmt, 5)),
            clima_to_text(sqlite3_column_int(stmt, 6)),
            dia_to_text(sqlite3_column_int(stmt, 7)),
            sqlite3_column_int(stmt, 8),
            sqlite3_column_int(stmt, 9),
            sqlite3_column_int(stmt, 10),
            sqlite3_column_text(stmt, 11));
    free(cancha_trimmed);
}

/**
 * @brief Escribe objeto JSON con datos de partido
 * Funcion comun para exportar datos de partidos a JSON
 */
void write_partido_json_object(cJSON *item, sqlite3_stmt *stmt)
{
    char *cancha_trimmed = get_trimmed_cancha_from_stmt(stmt);

    cJSON_AddStringToObject(item, "cancha", cancha_trimmed);
    cJSON_AddStringToObject(item, "fecha", (const char *)sqlite3_column_text(stmt, 1));
    cJSON_AddNumberToObject(item, "goles", sqlite3_column_int(stmt, 2));
    cJSON_AddNumberToObject(item, "asistencias", sqlite3_column_int(stmt, 3));
    cJSON_AddStringToObject(item, "camiseta", (const char *)sqlite3_column_text(stmt, 4));
    cJSON_AddStringToObject(item, "resultado", resultado_to_text(sqlite3_column_int(stmt, 5)));
    cJSON_AddStringToObject(item, "clima", clima_to_text(sqlite3_column_int(stmt, 6)));
    cJSON_AddStringToObject(item, "dia", dia_to_text(sqlite3_column_int(stmt, 7)));
    cJSON_AddNumberToObject(item, "rendimiento_general", sqlite3_column_int(stmt, 8));
    cJSON_AddNumberToObject(item, "cansancio", sqlite3_column_int(stmt, 9));
    cJSON_AddNumberToObject(item, "estado_animo", sqlite3_column_int(stmt, 10));
    cJSON_AddStringToObject(item, "comentario_personal", (const char *)sqlite3_column_text(stmt, 11));

    free(cancha_trimmed);
}

/**
 * @brief Escribe fila HTML con datos de partido
 * Funcion comun para exportar datos de partidos a HTML
 */
void write_partido_html_row(FILE *f, sqlite3_stmt *stmt)
{
    char *cancha_trimmed = get_trimmed_cancha_from_stmt(stmt);
    fprintf(f,
            "<tr><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td></tr>",
            cancha_trimmed,
            sqlite3_column_text(stmt, 1),
            sqlite3_column_int(stmt, 2),
            sqlite3_column_int(stmt, 3),
            sqlite3_column_text(stmt, 4),
            resultado_to_text(sqlite3_column_int(stmt, 5)),
            clima_to_text(sqlite3_column_int(stmt, 6)),
            dia_to_text(sqlite3_column_int(stmt, 7)),
            sqlite3_column_int(stmt, 8),
            sqlite3_column_int(stmt, 9),
            sqlite3_column_int(stmt, 10),
            sqlite3_column_text(stmt, 11));
    free(cancha_trimmed);
}

/**
 * @brief Funcion comun para mostrar records simples
 * Centraliza la logica de mostrar records con formato consistente
 */
void mostrar_record_simple(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt = execute_query(sql);
    if (!stmt) return;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Valor: %d\n", sqlite3_column_int(stmt, 0));
        if (sqlite3_column_count(stmt) > 1)
        {
            printf("Camiseta: %s\n", sqlite3_column_text(stmt, 1));
        }
        if (sqlite3_column_count(stmt) > 2)
        {
            printf("Fecha: %s\n", sqlite3_column_text(stmt, 2));
        }
    }
    else
    {
        mostrar_no_hay_registros("datos disponibles");
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Funcion comun para mostrar combinaciones cancha-camiseta
 * Centraliza la logica de mostrar combinaciones con formato consistente
 */
void mostrar_combinacion_simple(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt = execute_query(sql);
    if (!stmt) return;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        printf("Cancha: %s\n", sqlite3_column_text(stmt, 0));
        printf("Camiseta: %s\n", sqlite3_column_text(stmt, 1));
        printf("Rendimiento Promedio: %.2f\n", sqlite3_column_double(stmt, 2));
        printf("Partidos Jugados: %d\n", sqlite3_column_int(stmt, 3));
    }
    else
    {
        mostrar_no_hay_registros("datos disponibles");
    }
    sqlite3_finalize(stmt);
}

/**
 * @brief Funcion comun para mostrar temporadas
 * Centraliza la logica de mostrar temporadas con formato consistente
 */
void mostrar_temporada_simple(const char *titulo, const char *sql)
{
    sqlite3_stmt *stmt = execute_query(sql);
    if (!stmt) return;

    printf("\n%s\n", titulo);
    printf("----------------------------------------\n");

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char* year = (const char*)sqlite3_column_text(stmt, 0);
        if (year)
        {
            printf("Ano: %s\n", year);
        }
        else
        {
            printf("Ano: Desconocido\n");
        }
        printf("Rendimiento Promedio: %.2f\n", sqlite3_column_double(stmt, 1));
        printf("Partidos Jugados: %d\n", sqlite3_column_int(stmt, 2));
    }
    else
    {
        mostrar_no_hay_registros("datos disponibles");
    }
    sqlite3_finalize(stmt);
}



/**
 * @brief Funcion generica para exportar records a CSV
 * Centraliza la logica de exportacion CSV para records individuales
 */
void exportar_record_simple_csv(const char *titulo, const char *sql, const char *filename)
{
    FILE *file = NULL;
    errno_t err = fopen_s(&file, get_export_path(filename), "w");
    if (err != 0 || !file)
    {
        printf("Error al crear el archivo\n");
        return;
    }

    fprintf(file, "%s\n", titulo);
    fprintf(file, "Valor,Camiseta,Fecha\n");

    sqlite3_stmt *stmt = execute_query(sql);
    if (stmt && sqlite3_step(stmt) == SQLITE_ROW)
    {
        fprintf(file, "%d,%s,%s\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2));
    }

    if (stmt) sqlite3_finalize(stmt);
    printf("Exportado a %s\n", get_export_path(filename));
    fclose(file);
}

/**
 * @brief Funcion generica para exportar un partido especifico a CSV
 * Centraliza la logica comun de exportacion de partidos especificos
 */
void exportar_partido_especifico_csv(const char *order_by, const char *filename)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = NULL;
    errno_t err = fopen_s(&f, get_export_path(filename), "w");
    if (err != 0 || !f) return;

    write_csv_header(f, "Cancha,Fecha,Goles,Asistencias,Camiseta,Resultado,Clima,Dia,Rendimiento_General,Cansancio,Estado_Animo,Comentario_Personal");

    sqlite3_stmt *stmt;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
             "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
             "JOIN cancha can ON p.cancha_id = can.id %s",
             order_by);

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            write_partido_csv_row(f, stmt);
        }
        sqlite3_finalize(stmt);
    }

    printf("Archivo exportado a: %s\n", get_export_path(filename));
    fclose(f);
}

/**
 * @brief Funcion generica para exportar un partido especifico a TXT
 * Centraliza la logica comun de exportacion de partidos especificos
 */
void exportar_partido_especifico_txt(const char *order_by, const char *filename, const char *title)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = NULL;
    errno_t err = fopen_s(&f, get_export_path(filename), "w");
    if (err != 0 || !f) return;

    fprintf(f, "%s\n\n", title);

    sqlite3_stmt *stmt;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
             "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
             "JOIN cancha can ON p.cancha_id = can.id %s",
             order_by);

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            write_partido_txt_row(f, stmt);
        }
        sqlite3_finalize(stmt);
    }

    printf("Archivo exportado a: %s\n", get_export_path(filename));
    fclose(f);
}

/**
 * @brief Funcion generica para exportar un partido especifico a JSON
 * Centraliza la logica comun de exportacion de partidos especificos
 */
void exportar_partido_especifico_json(const char *order_by, const char *filename)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = NULL;
    errno_t err = fopen_s(&f, get_export_path(filename), "w");
    if (err != 0 || !f) return;

    cJSON *root = cJSON_CreateObject();

    sqlite3_stmt *stmt;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
             "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
             "JOIN cancha can ON p.cancha_id = can.id %s",
             order_by);

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            write_partido_json_object(root, stmt);
        }
        sqlite3_finalize(stmt);
    }

    char *json_string = cJSON_Print(root);
    fprintf(f, "%s", json_string);
    free(json_string);
    cJSON_Delete(root);

    printf("Archivo exportado a: %s\n", get_export_path(filename));
    fclose(f);
}

/**
 * @brief Funcion generica para exportar un partido especifico a HTML
 * Centraliza la logica comun de exportacion de partidos especificos
 */
void exportar_partido_especifico_html(const char *order_by, const char *filename, const char *title)
{
    if (!has_records("partido"))
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = NULL;
    errno_t err = fopen_s(&f, get_export_path(filename), "w");
    if (err != 0 || !f) return;

    fprintf(f, "<html><body><h1>%s</h1><table border='1'>"
            "<tr><th>Cancha</th><th>Fecha</th><th>Goles</th><th>Asistencias</th><th>Camiseta</th><th>Resultado</th><th>Clima</th><th>Dia</th><th>Rendimiento General</th><th>Cansancio</th><th>Estado Animo</th><th>Comentario Personal</th></tr>",
            title);

    sqlite3_stmt *stmt;
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
             "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
             "JOIN cancha can ON p.cancha_id = can.id %s",
             order_by);

    if (preparar_stmt(sql, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            write_partido_html_row(f, stmt);
        }
        sqlite3_finalize(stmt);
    }

    fprintf(f, "</table></body></html>");
    printf("Archivo exportado a: %s\n", get_export_path(filename));
    fclose(f);
}

/* ===================== FUNCIONES DE ESTADiSTICAS COMPARTIDAS ===================== */

void reset_estadisticas(Estadisticas *stats)
{
    memset(stats, 0, sizeof(*stats));
}

void calcular_estadisticas(Estadisticas *stats, const char *sql)
{
    sqlite3_stmt *stmt;
    reset_estadisticas(stats);
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        stats->total_partidos = sqlite3_column_int(stmt, 0);
        stats->avg_goles = sqlite3_column_double(stmt, 1);
        stats->avg_asistencias = sqlite3_column_double(stmt, 2);
        stats->avg_rendimiento = sqlite3_column_double(stmt, 3);
        stats->avg_cansancio = sqlite3_column_double(stmt, 4);
        stats->avg_animo = sqlite3_column_double(stmt, 5);
    }
    sqlite3_finalize(stmt);
}

void actualizar_rachas(int resultado, int *racha_actual_v, int *max_racha_v,
                       int *racha_actual_d, int *max_racha_d)
{
    if (resultado == 1)
    {
        (*racha_actual_v)++;
        if (*racha_actual_v > *max_racha_v)
            *max_racha_v = *racha_actual_v;
        *racha_actual_d = 0;
        return;
    }

    if (resultado == 3)
    {
        (*racha_actual_d)++;
        if (*racha_actual_d > *max_racha_d)
            *max_racha_d = *racha_actual_d;
        *racha_actual_v = 0;
        return;
    }

    *racha_actual_v = 0;
    *racha_actual_d = 0;
}

int preparar_stmt_export(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

int preparar_consulta_con_verificacion(sqlite3_stmt **stmt, const char *tabla,
                                       const char *mensaje, const char *sql, int *count)
{
    sqlite3_stmt *check_stmt;
    *count = 0;

    // Construir consulta de conteo
    char count_sql[256];
    snprintf(count_sql, sizeof(count_sql), "SELECT COUNT(*) FROM %s", tabla);

    // Verificar si hay registros
    if (!preparar_stmt_export(&check_stmt, count_sql))
    {
        return 0;
    }

    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        *count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);

    if (*count == 0)
    {
        mostrar_no_hay_registros(mensaje);
        return 0;
    }

    // Preparar la consulta principal
    if (!preparar_stmt_export(stmt, sql))
    {
        return 0;
    }

    return 1;
}

FILE *abrir_archivo_exportacion(const char *filename, const char *error_msg)
{
    FILE *file;
    const char *path = get_export_path(filename);
    errno_t err = fopen_s(&file, path, "w");
    if (err != 0 || file == NULL)
    {
        printf("%s\n", error_msg);
        return NULL;
    }
    return file;
}

int has_records(const char *table_name)
{
    sqlite3_stmt *stmt;
    char sql[256];
    int count = 0;
    int result = 0;

    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s", table_name);

    if (preparar_stmt_export(&stmt, sql))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        result = count > 0;
    }

    return result;
}

void trim_whitespace(char *str)
{
    if (!str)
        return;

    size_t total = strlen_s(str, SIZE_MAX);
    if (total == 0)
        return;

    char const *start = str;
    while (*start && isspace((unsigned char)*start))
        start++;

    char const *end = start + strlen_s(start, SIZE_MAX);
    while (end > start && isspace((unsigned char)*(end - 1)))
        end--;

    size_t len = (size_t)(end - start);
    if (start != str)
        memmove(str, start, len + 1);
    else
        str[len] = '\0';
}

void extraer_estadistica_anio(sqlite3_stmt *stmt, EstadisticaAnio *stats)
{
    stats->anio = (const char *)sqlite3_column_text(stmt, 0);
    stats->camiseta = (const char *)sqlite3_column_text(stmt, 1);
    stats->partidos = sqlite3_column_int(stmt, 2);
    stats->total_goles = sqlite3_column_int(stmt, 3);
    stats->total_asistencias = sqlite3_column_int(stmt, 4);
    stats->avg_goles = sqlite3_column_double(stmt, 5);
    stats->avg_asistencias = sqlite3_column_double(stmt, 6);
}

static void app_escape_single_quotes_ps(const char *src, char *dst, size_t dst_size)
{
    if (!src || !dst || dst_size == 0)
    {
        return;
    }

    size_t j = 0;
    const char *p = src;
    while (*p != '\0' && j + 1 < dst_size)
    {
        if (*p == '\'')
        {
            if (j + 2 >= dst_size)
            {
                break;
            }
            dst[j++] = '\'';
            dst[j++] = '\'';
        }
        else
        {
            dst[j++] = *p;
        }
        p++;
    }
    dst[j] = '\0';
}

int app_command_exists(const char *cmd)
{
    if (!cmd || cmd[0] == '\0')
    {
        return 0;
    }

    char check_cmd[256];
#ifdef _WIN32
    snprintf(check_cmd, sizeof(check_cmd), "where %s >nul 2>nul", cmd);
#else
    snprintf(check_cmd, sizeof(check_cmd), "command -v %s >/dev/null 2>&1", cmd);
#endif
    return system(check_cmd) == 0;
}

void app_build_path(char *dest, size_t size, const char *dir, const char *file_name)
{
    if (!dest || size == 0)
    {
        return;
    }

    if (!dir)
    {
        dir = "";
    }

    if (!file_name)
    {
        file_name = "";
    }

#ifdef _WIN32
    snprintf(dest, size, "%s\\%s", dir, file_name);
#else
    snprintf(dest, size, "%s/%s", dir, file_name);
#endif
}

int app_copy_binary_file(const char *source_path, const char *dest_path)
{
    FILE *src = NULL;
    FILE *dst = NULL;

    if (fopen_s(&src, source_path, "rb") != 0 || !src)
    {
        return 0;
    }

    if (fopen_s(&dst, dest_path, "wb") != 0 || !dst)
    {
        fclose(src);
        return 0;
    }

    char buffer[8192];
    size_t bytes = 0;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0)
    {
        if (fwrite(buffer, 1, bytes, dst) != bytes)
        {
            fclose(src);
            fclose(dst);
            return 0;
        }
    }

    fclose(src);
    fclose(dst);
    return 1;
}

int app_optimize_image_file(const char *source_path, const char *dest_path)
{
    if (!source_path || !dest_path)
    {
        return 0;
    }

#ifdef _WIN32
    if (app_command_exists("magick"))
    {
        char cmd_magick[2600];
        snprintf(cmd_magick,
                 sizeof(cmd_magick),
                 "magick \"%s\" -auto-orient -resize \"1280x1280>\" -strip -quality 92 \"%s\"",
                 source_path,
                 dest_path);
        if (system(cmd_magick) == 0)
        {
            return 1;
        }
    }

    char src_ps[2200] = {0};
    char dst_ps[2200] = {0};
    app_escape_single_quotes_ps(source_path, src_ps, sizeof(src_ps));
    app_escape_single_quotes_ps(dest_path, dst_ps, sizeof(dst_ps));

    char cmd_ps[9000];
    snprintf(cmd_ps,
             sizeof(cmd_ps),
             "powershell -NoProfile -Command \"$ErrorActionPreference='Stop';"
             "Add-Type -AssemblyName System.Drawing;"
             "$src='%s';$dst='%s';"
             "$img=[System.Drawing.Image]::FromFile($src);"
             "try{"
             "$max=1280;$w=$img.Width;$h=$img.Height;"
             "if($w -gt $h){$nw=[Math]::Min($w,$max);$nh=[int]($h*$nw/$w)}"
             "else{$nh=[Math]::Min($h,$max);$nw=[int]($w*$nh/$h)};"
             "$bmp=New-Object System.Drawing.Bitmap $nw,$nh;"
             "$g=[System.Drawing.Graphics]::FromImage($bmp);"
             "$g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic;"
             "$g.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::HighQuality;"
             "$g.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality;"
             "$g.DrawImage($img,0,0,$nw,$nh);"
             "$enc=[System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders()|Where-Object{$_.MimeType -eq 'image/jpeg'}|Select-Object -First 1;"
             "$ep=New-Object System.Drawing.Imaging.EncoderParameters 1;"
             "$ep.Param[0]=New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality,92L);"
             "$bmp.Save($dst,$enc,$ep);"
             "$g.Dispose();$bmp.Dispose();"
             "}finally{$img.Dispose()}\"",
             src_ps,
             dst_ps);

    return system(cmd_ps) == 0;
#else
    char cmd[2600];
    if (app_command_exists("magick"))
    {
        snprintf(cmd,
                 sizeof(cmd),
                 "magick \"%s\" -auto-orient -resize \"1280x1280>\" -strip -quality 92 \"%s\"",
                 source_path,
                 dest_path);
        return system(cmd) == 0;
    }

    if (app_command_exists("convert"))
    {
        snprintf(cmd,
                 sizeof(cmd),
                 "convert \"%s\" -auto-orient -resize \"1280x1280>\" -strip -quality 92 \"%s\"",
                 source_path,
                 dest_path);
        return system(cmd) == 0;
    }

    return 0;
#endif
}

const char *app_get_file_extension(const char *path)
{
    if (!path)
    {
        return NULL;
    }

    const char *dot = strrchr(path, '.');
    if (!dot || dot == path)
    {
        return NULL;
    }
    return dot;
}

int app_is_supported_image_extension(const char *ext)
{
    if (!ext)
    {
        return 0;
    }

#ifdef _WIN32
    return _stricmp(ext, ".jpg") == 0 ||
           _stricmp(ext, ".jpeg") == 0 ||
           _stricmp(ext, ".png") == 0 ||
           _stricmp(ext, ".bmp") == 0 ||
           _stricmp(ext, ".webp") == 0;
#else
    return strcasecmp(ext, ".jpg") == 0 ||
           strcasecmp(ext, ".jpeg") == 0 ||
           strcasecmp(ext, ".png") == 0 ||
           strcasecmp(ext, ".bmp") == 0 ||
           strcasecmp(ext, ".webp") == 0;
#endif
}

int app_select_image_from_user(char *ruta_origen, size_t size, const char *temp_filename)
{
    if (!ruta_origen || size == 0)
    {
        return 0;
    }

#ifdef _WIN32
    const char *archivo_temp = temp_filename ? temp_filename : "mifutbol_imagen_sel.txt";
    remove(archivo_temp);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "powershell -NoProfile -Command \"Add-Type -AssemblyName System.Windows.Forms; "
             "$dlg = New-Object System.Windows.Forms.OpenFileDialog; "
             "$dlg.InitialDirectory = [System.IO.Path]::Combine($env:USERPROFILE, 'Downloads'); "
             "$dlg.Filter = 'Imagenes|*.jpg;*.jpeg;*.png;*.bmp;*.webp|Todos|*.*'; "
             "if ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { [System.IO.File]::WriteAllText('%s', $dlg.FileName) }\"",
             archivo_temp);

    int rc = system(cmd);
    (void)rc;

    FILE *f = NULL;
    if (fopen_s(&f, archivo_temp, "r") != 0 || !f)
    {
        return 0;
    }

    if (!fgets(ruta_origen, (int)size, f))
    {
        fclose(f);
        remove(archivo_temp);
        return 0;
    }

    fclose(f);
    remove(archivo_temp);
    trim_whitespace(ruta_origen);
    return ruta_origen[0] != '\0';
#else
    (void)temp_filename;
    input_string("Ruta de imagen: ", ruta_origen, (int)size);
    trim_whitespace(ruta_origen);
    return ruta_origen[0] != '\0';
#endif
}

int app_get_file_name_from_path(const char *path, char *nombre, size_t size)
{
    if (!path || !nombre || size == 0)
    {
        return 0;
    }

    const char *last_slash = strrchr(path, '/');
    const char *last_backslash = strrchr(path, '\\');
    const char *base = path;

    if (last_slash && last_backslash)
    {
        base = (last_slash > last_backslash) ? last_slash + 1 : last_backslash + 1;
    }
    else if (last_slash)
    {
        base = last_slash + 1;
    }
    else if (last_backslash)
    {
        base = last_backslash + 1;
    }

    return strncpy_s(nombre, size, base, _TRUNCATE) == 0;
}

int app_seleccionar_y_copiar_imagen(const char *selector_filename, const char *prefijo_base,
                                    char *ruta_relativa_db, size_t ruta_size)
{
    if (!selector_filename || !prefijo_base || !ruta_relativa_db || ruta_size == 0)
    {
        return 0;
    }

    char ruta_origen[1024] = {0};
    printf("\nSe abrira el selector de archivos en Descargas.\n");
    if (!app_select_image_from_user(ruta_origen, sizeof(ruta_origen), selector_filename))
    {
        printf("No se selecciono ninguna imagen.\n");
        return 0;
    }

    const char *ext = app_get_file_extension(ruta_origen);
    if (!app_is_supported_image_extension(ext))
    {
        printf("Formato no soportado. Usa: JPG, JPEG, PNG, BMP o WEBP.\n");
        return 0;
    }

    const char *images_dir = get_images_dir();
    if (!images_dir)
    {
        printf("No se pudo preparar la carpeta Imagenes.\n");
        return 0;
    }

    char ts[32] = {0};
    get_timestamp(ts, (int)sizeof(ts));

    char base_destino[220] = {0};
    snprintf(base_destino, sizeof(base_destino), "%s_%s", prefijo_base, ts);

    char nombre_destino_opt[256] = {0};
    snprintf(nombre_destino_opt, sizeof(nombre_destino_opt), "%s.jpg", base_destino);

    char nombre_destino_original[256] = {0};
    snprintf(nombre_destino_original, sizeof(nombre_destino_original), "%s%s", base_destino, ext);

    char ruta_destino_opt[1200] = {0};
    char ruta_destino_original[1200] = {0};
    app_build_path(ruta_destino_opt, sizeof(ruta_destino_opt), images_dir, nombre_destino_opt);
    app_build_path(ruta_destino_original, sizeof(ruta_destino_original), images_dir, nombre_destino_original);

    int optimizada = app_optimize_image_file(ruta_origen, ruta_destino_opt);
    const char *nombre_final = NULL;

    if (optimizada)
    {
        nombre_final = nombre_destino_opt;
    }
    else
    {
        if (!app_copy_binary_file(ruta_origen, ruta_destino_original))
        {
            printf("No se pudo mover/copiar la imagen a la carpeta Imagenes.\n");
            return 0;
        }
        nombre_final = nombre_destino_original;
    }

    snprintf(ruta_relativa_db, ruta_size, "Imagenes/%s", nombre_final);
    return 1;
}

int app_cargar_imagen_entidad(int id, const char *tabla, const char *selector_filename)
{
    if (id <= 0 || !tabla)
    {
        return 0;
    }

    char prefijo[220] = {0};
    snprintf(prefijo, sizeof(prefijo), "%s_%d", tabla, id);

    char ruta_relativa_db[300] = {0};
    if (!app_seleccionar_y_copiar_imagen(selector_filename, prefijo, ruta_relativa_db, sizeof(ruta_relativa_db)))
    {
        return 0;
    }

    char sql[200] = {0};
    snprintf(sql, sizeof(sql), "UPDATE %s SET imagen_ruta=? WHERE id=?", tabla);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        printf("Error al guardar ruta de imagen en DB.\n");
        return 0;
    }

    sqlite3_bind_text(stmt, 1, ruta_relativa_db, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        printf("Error al guardar ruta de imagen en DB.\n");
        return 0;
    }

    printf("\nImagen cargada correctamente.\n");
    return 1;
}
