/**
 * @file import.c
 * @brief Modulo para importar datos desde archivos JSON a la base de datos.
 */

#include "import.h"
#include "cJSON.h"
#include "db.h"
#include "menu.h"
#include "settings.h"
#include "sqlite3.h"
#include "utils.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

// Forward declaration - Estructura para agrupar datos de un partido
typedef struct
{
    int partido_id;
    sqlite3_int64 cancha_id;
    const char *fecha;
    int goles;
    int asistencias;
    int camiseta_id;
    int resultado;
    int clima;
    int dia;
    int rendimiento_general;
    int cansancio;
    int estado_animo;
    const char *comentario_personal;
} PartidoData;

typedef int (*PartidoLineParser)(const char *line);

// Estructura para entrada de datos de partido (para reducir parametros)
typedef struct
{
    const char *cancha;
    const char *fecha;
    int goles;
    int asistencias;
    const char *camiseta;
    int resultado;
    int clima;
    int dia;
    int rendimiento_general;
    int cansancio;
    int estado_animo;
    const char *comentario;
} PartidoInput;

typedef struct
{
    const char *cancha;
    const char *fecha;
    int goles;
    int asistencias;
    const char *camiseta;
    const char *resultado_str;
    const char *clima_str;
    const char *dia_str;
    int rendimiento_general;
    int cansancio;
    int estado_animo;
    const char *comentario;
} PartidoRawInput;

typedef struct CamisetaData CamisetaData;
typedef int (*CamisetaLineParser)(const char *line, CamisetaData *out);
typedef struct LesionData LesionData;
typedef int (*LesionLineParser)(const char *line, LesionData *out);
typedef struct EstadisticaData EstadisticaData;
typedef int (*EstadisticaLineParser)(const char *line, EstadisticaData *out);

// Macro para obtener camiseta ID (debe definirse antes de las declaraciones forward)
#define obtener_camiseta_id(nombre) obtener_id_por_nombre("camiseta", nombre)

// Forward declaration
static char *read_file_content(const char *filename);
static int preparar_stmt(const char *sql, sqlite3_stmt **stmt);
static int id_existe_en_tabla(const char *tabla, int id);
static void insertar_camiseta(int id, const char *nombre);
static void insertar_lesion(int id, const char *jugador, const char *tipo,
                            const char *descripcion, const char *fecha);
static void importar_con_pausa(const char *inicio, const char *fin,
                               void (*func)());
static int procesar_camiseta_importada(const CamisetaData *camiseta);
static int parse_camiseta_txt_line(const char *line, CamisetaData *out);
static int parse_camiseta_csv_line(const char *line, CamisetaData *out);
static void importar_camisetas_desde_archivo(const char *filename, const char *formato,
        CamisetaLineParser parser);
static bool crear_tabla_estadisticas();
static int obtener_camiseta_id_estadistica(const char *camiseta_nombre);
static bool estadistica_existe(int camiseta_id);
static void insertar_estadistica(int camiseta_id, int goles, int asistencias,
                                 int partidos, int victorias, int empates,
                                 int derrotas);
static sqlite3_int64 obtener_o_crear_cancha_id(const char *cancha_nombre);
static int partido_existe(sqlite3_int64 cancha_id, const char *fecha, int camiseta_id);
static int obtener_siguiente_partido_id(void);
static void insertar_partido(PartidoData data);
static int procesar_e_insertar_partido(const PartidoInput *input);
static void importar_partidos_desde_archivo(const char *filename, const char *formato,
        PartidoLineParser parser);
static int procesar_lesion_importada(const LesionData *lesion);
static int parse_lesion_txt_line(const char *line, LesionData *out);
static int parse_lesion_csv_line(const char *line, LesionData *out);
static void importar_lesiones_desde_archivo(const char *filename, const char *formato,
        LesionLineParser parser);
static int procesar_estadistica_importada(const EstadisticaData *estadistica);
static int parse_estadistica_txt_line(const char *line, EstadisticaData *out);
static int parse_estadistica_csv_line(const char *line, EstadisticaData *out);
static void importar_estadisticas_desde_archivo(const char *filename, const char *formato,
        EstadisticaLineParser parser, int log_parse_error);

// trim_trailing_spaces() fue movido a utils.c como funci\u00f3n gen\u00e9rica
// Se puede usar directamente desde utils.h

/**
 * @brief Estructura para almacenar datos de una camiseta.
 */
struct CamisetaData
{
    int id;
    char nombre[256];
};

/**
 * @brief Estructura para almacenar datos de una lesion.
 */
struct LesionData
{
    int id;
    char jugador[256];
    char tipo[256];
    char descripcion[512];
    char fecha[256];
};

/**
 * @brief Estructura para almacenar datos de estadisticas.
 */
struct EstadisticaData
{
    char camiseta[256];
    int goles;
    int asistencias;
    int partidos;
    int victorias;
    int empates;
    int derrotas;
};

/**
 * @brief Construye el nombre del archivo completo.
 *
 * @param extension Extension del archivo (e.g., "json").
 * @param filename Buffer para almacenar el nombre completo.
 * @param size Tamano del buffer.
 */
static void build_filename(const char *extension, char *filename, size_t size)
{
    strcpy_s(filename, size, get_import_dir());
    size_t filename_len = safe_strnlen(filename, size);
    strncat_s(filename, size, PATH_SEP, size - filename_len - 1);
    strncat_s(filename, size, extension, size - filename_len - 1);
}

/**
 * @brief Lee el contenido completo de un archivo de texto.
 *
 * Para permitir el analisis eficiente del contenido sin multiples lecturas de
 * disco.
 *
 * @param filename Ruta del archivo a leer.
 * @return Puntero al contenido del archivo o NULL si hay error.
 */
static char *read_file_content(const char *filename)
{
    FILE *file = NULL;
    errno_t err = fopen_s(&file, filename, "r");
    if (err != 0 || !file)
    {
        printf("Error: No se pudo abrir el archivo %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (length <= 0 ||
            length > 10485760) // Limit to 10MB to prevent excessive memory usage
    {
        fclose(file);
        return NULL;
    }

    char *content = (char *)malloc(length + 1);
    if (!content)
    {
        printf("Error: No se pudo asignar memoria\n");
        fclose(file);
        return NULL;
    }

    fread(content, 1, length, file);
    content[length] = '\0';
    fclose(file);
    return content;
}

/**
 * @brief Preparar statement y reportar errores.
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
 * @brief Funcion generica para cargar y validar un archivo JSON
 * @param json_filename Nombre del archivo JSON (ej: "partidos.json")
 * @param entity_name Nombre de la entidad para mensajes (ej: "partidos")
 * @return puntero a cJSON si es valido y es un array, NULL en caso contrario
 */
static cJSON *cargar_json_array(const char *json_filename, const char *entity_name, int *out_count)
{
    char filename[1024];
    build_filename(json_filename, filename, sizeof(filename));

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return NULL;

    cJSON *json = cJSON_Parse(content);
    free(content);

    if (!json)
    {
        printf("Error: JSON de %s invalido\n", entity_name);
        return NULL;
    }

    if (!cJSON_IsArray(json))
    {
        printf("Error: El JSON de %s debe ser un array\n", entity_name);
        cJSON_Delete(json);
        return NULL;
    }

    int count = cJSON_GetArraySize(json);
    printf("Importando %d %s...\n", count, entity_name);

    if (out_count)
        *out_count = count;

    return json;
}

/**
 * @brief Funcion generica para abrir archivos de texto (CSV/TXT) para importacion
 * @param txt_filename Nombre del archivo (ej: "lesiones.csv")
 * @param skip_header Si es true, salta la primera linea
 * @return FILE* abierto o NULL si hay error
 */
static FILE *abrir_archivo_texto_importacion(const char *txt_filename, bool skip_header)
{
    char filename[1024];
    build_filename(txt_filename, filename, sizeof(filename));

    printf("Importando desde: %s\n", filename);

    FILE *file = NULL;
    errno_t err = fopen_s(&file, filename, "r");
    if (err != 0 || !file)
    {
        printf("Error: No se pudo abrir el archivo %s\n", filename);
        return NULL;
    }

    if (skip_header)
    {
        char line[1024];
        if (fgets(line, sizeof(line), file) == NULL)
        {
            printf("Error: Archivo vacio o formato incorrecto\n");
            fclose(file);
            return NULL;
        }
    }

    return file;
}

/**
 * @brief Procesa e inserta un partido validando cancha, camiseta y duplicados
 *
 * Esta funcion centraliza la logica comun de validacion e insercion de partidos
 * que se usa en todas las funciones de importacion (TXT, CSV, HTML).
 *
 * @param input Puntero a estructura con los datos del partido a procesar
 * @return 1 si el partido fue insertado, 0 si hubo error o ya existe
 */
static int procesar_e_insertar_partido(const PartidoInput *input)
{
    // Obtener ID de cancha
    sqlite3_int64 cancha_id = obtener_o_crear_cancha_id(input->cancha);
    if (cancha_id == -1)
    {
        printf("Error al obtener ID de cancha para '%s', omitiendo partido...\n",
               input->cancha);
        return 0;
    }

    // Obtener ID de camiseta
    int camiseta_id = obtener_camiseta_id(input->camiseta);

    if (camiseta_id == -1)
    {
        printf("Camiseta '%s' no encontrada, omitiendo partido...\n", input->camiseta);
        return 0;
    }

    // Verificar si ya existe un partido con los mismos datos
    if (partido_existe(cancha_id, input->fecha, camiseta_id))
    {
        printf("Partido ya existe, omitiendo...\n");
        return 0;
    }

    // Obtener siguiente ID para partido
    int partido_id = obtener_siguiente_partido_id();

    // Insertar partido
    PartidoData partido_data =
    {
        partido_id, cancha_id, input->fecha, input->goles, input->asistencias,
        camiseta_id, input->resultado, input->clima, input->dia, input->rendimiento_general,
        input->cansancio, input->estado_animo, input->comentario
    };
    insertar_partido(partido_data);

    printf("Partido en '%s' importado correctamente\n", input->cancha);
    return 1;
}

/**
 * @brief Verifica si existe un registro por ID en la tabla dada.
 */
static int id_existe_en_tabla(const char *tabla, int id)
{
    sqlite3_stmt *stmt;
    char sql[128];
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s WHERE id = ?", tabla);
    if (!preparar_stmt(sql, &stmt))
        return 0;

    sqlite3_bind_int(stmt, 1, id);
    int exists = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        exists = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return exists;
}

/**
 * @brief Inserta una camiseta en la base de datos.
 */
static void insertar_camiseta(int id, const char *nombre)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "INSERT INTO camiseta(id, nombre, sorteada) VALUES(?, ?, 0)",
                &stmt))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/**
 * @brief Inserta una lesion en la base de datos.
 */
static void insertar_lesion(int id, const char *jugador, const char *tipo,
                            const char *descripcion, const char *fecha)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "INSERT INTO lesion(id, jugador, tipo, descripcion, fecha) "
                "VALUES(?, ?, ?, ?, ?)",
                &stmt))
    {
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, jugador, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, tipo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, descripcion, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/**
 * @brief Ejecuta importacion con mensajes y pausa.
 */
static void importar_con_pausa(const char *inicio, const char *fin,
                               void (*func)())
{
    printf("%s\n", inicio);
    func();
    printf("%s\n", fin);
    pause_console();
}

static void importar_con_pausa_formato(const char *entidad, const char *formato,
                                       void (*func)())
{
    char inicio[128];
    char fin[128];

    snprintf(inicio, sizeof(inicio), "Importando %s desde %s...", entidad, formato);
    snprintf(fin, sizeof(fin), "Importacion de %s completada.", entidad);

    importar_con_pausa(inicio, fin, func);
}

#define DEFINE_IMPORT_CON_PAUSA(func_name, entidad, formato, func) \
    static void func_name(void)                                   \
    {                                                            \
        importar_con_pausa_formato(entidad, formato, func);       \
    }

typedef struct
{
    const char *inicio;
    const char *fin;
    void (*importar_camisetas)();
    void (*importar_partidos)();
    void (*importar_lesiones)();
    void (*importar_estadisticas)();
} ImportTodoConfig;

static void importar_todo_con_config(const ImportTodoConfig *config)
{
    printf("%s\n", config->inicio);
    config->importar_camisetas();
    config->importar_partidos();
    config->importar_lesiones();
    config->importar_estadisticas();
    printf("%s\n", config->fin);
    pause_console();
}

static void importar_partidos_desde_archivo(const char *filename, const char *formato,
        PartidoLineParser parser)
{
    FILE *file = abrir_archivo_texto_importacion(filename, true);
    if (!file)
        return;

    printf("Importando partidos desde %s...\n", formato);
    char line[2048];
    int count = 0;

    while (fgets(line, sizeof(line), file))
    {
        if (parser(line))
            count++;
    }

    fclose(file);
    printf("Importacion de partidos desde %s completada. %d partidos importados\n",
           formato, count);
}

static int procesar_camiseta_importada(const CamisetaData *camiseta)
{
    if (id_existe_en_tabla("camiseta", camiseta->id))
    {
        printf("Camiseta ID %d ya existe, omitiendo...\n", camiseta->id);
        return 0;
    }

    insertar_camiseta(camiseta->id, camiseta->nombre);
    printf("Camiseta '%s' importada correctamente\n", camiseta->nombre);
    return 1;
}

static int parse_camiseta_line_format(const char *line, CamisetaData *out,
                                      const char *format)
{
    if (!line || !out)
        return 0;

    int id;
    char nombre[256];

    if (sscanf_s(line, format, &id, nombre, (unsigned)_countof(nombre)) != 2)
        return 0;

    trim_trailing_spaces(nombre);
    out->id = id;
    strcpy_s(out->nombre, sizeof(out->nombre), nombre);
    return 1;
}

static int parse_camiseta_txt_line(const char *line, CamisetaData *out)
{
    return parse_camiseta_line_format(line, out, "%d - %s");
}

static int parse_camiseta_csv_line(const char *line, CamisetaData *out)
{
    return parse_camiseta_line_format(line, out, "%d,%s");
}

static void importar_camisetas_desde_archivo(const char *filename, const char *formato,
        CamisetaLineParser parser)
{
    FILE *file = abrir_archivo_texto_importacion(filename, true);
    if (!file)
        return;

    printf("Importando camisetas desde %s...\n", formato);
    char line[1024];
    int count = 0;

    while (fgets(line, sizeof(line), file))
    {
        CamisetaData data;
        if (parser(line, &data))
            count += procesar_camiseta_importada(&data);
    }

    fclose(file);
    printf("Importacion de camisetas desde %s completada. %d camisetas importadas\n",
           formato, count);
}

static int procesar_lesion_importada(const LesionData *lesion)
{
    if (id_existe_en_tabla("lesion", lesion->id))
    {
        printf("Lesion ID %d ya existe, omitiendo...\n", lesion->id);
        return 0;
    }

    insertar_lesion(lesion->id, lesion->jugador, lesion->tipo, lesion->descripcion,
                    lesion->fecha);
    printf("Lesion de '%s' importada correctamente\n", lesion->jugador);
    return 1;
}

static int parse_lesion_line_format(const char *line, LesionData *out,
                                    const char *format)
{
    if (!line || !out)
        return 0;

    int id;
    char jugador[256];
    char tipo[256];
    char descripcion[512];
    char fecha[256];

    if (sscanf_s(line, format, &id, jugador, (unsigned)_countof(jugador), tipo,
                 (unsigned)_countof(tipo), descripcion,
                 (unsigned)_countof(descripcion), fecha,
                 (unsigned)_countof(fecha)) != 5)
    {
        return 0;
    }

    out->id = id;
    strcpy_s(out->jugador, sizeof(out->jugador), jugador);
    strcpy_s(out->tipo, sizeof(out->tipo), tipo);
    strcpy_s(out->descripcion, sizeof(out->descripcion), descripcion);
    strcpy_s(out->fecha, sizeof(out->fecha), fecha);
    return 1;
}

static int parse_lesion_txt_line(const char *line, LesionData *out)
{
    return parse_lesion_line_format(line, out,
                                    "%d - %[^|] | %[^|] | %[^|] | %[^\n]");
}

static int parse_lesion_csv_line(const char *line, LesionData *out)
{
    return parse_lesion_line_format(line, out, "%d,%[^,],%[^,],%[^,],%s");
}

static void importar_lesiones_desde_archivo(const char *filename, const char *formato,
        LesionLineParser parser)
{
    FILE *file = abrir_archivo_texto_importacion(filename, true);
    if (!file)
        return;

    printf("Importando lesiones desde %s...\n", formato);
    char line[1024];
    int count = 0;

    while (fgets(line, sizeof(line), file))
    {
        LesionData data;
        if (parser(line, &data) && procesar_lesion_importada(&data))
            count++;
    }

    fclose(file);
    printf("Importacion de lesiones desde %s completada. %d lesiones importadas\n",
           formato, count);
}

static int procesar_estadistica_importada(const EstadisticaData *estadistica)
{
    int camiseta_id = obtener_camiseta_id_estadistica(estadistica->camiseta);

    if (camiseta_id == -1)
    {
        printf("Camiseta '%s' no encontrada, omitiendo estadistica...\n",
               estadistica->camiseta);
        return 0;
    }

    if (estadistica_existe(camiseta_id))
    {
        printf("Estadistica para camiseta '%s' ya existe, omitiendo...\n",
               estadistica->camiseta);
        return 0;
    }

    insertar_estadistica(camiseta_id, estadistica->goles, estadistica->asistencias,
                         estadistica->partidos, estadistica->victorias,
                         estadistica->empates, estadistica->derrotas);

    printf("Estadistica de '%s' importada correctamente\n", estadistica->camiseta);
    return 1;
}

static int parse_estadistica_line_format(const char *line, EstadisticaData *out,
        const char *format)
{
    if (!line || !out)
        return 0;

    char camiseta[256];
    int goles;
    int asistencias;
    int partidos;
    int victorias;
    int empates;
    int derrotas;

    if (sscanf_s(line, format, camiseta, (unsigned)_countof(camiseta), &goles,
                 &asistencias, &partidos, &victorias, &empates,
                 &derrotas) != 7)
    {
        return 0;
    }

    strcpy_s(out->camiseta, sizeof(out->camiseta), camiseta);
    out->goles = goles;
    out->asistencias = asistencias;
    out->partidos = partidos;
    out->victorias = victorias;
    out->empates = empates;
    out->derrotas = derrotas;
    return 1;
}

static int parse_estadistica_txt_line(const char *line, EstadisticaData *out)
{
    return parse_estadistica_line_format(line, out,
                                         "%[^|] | G:%d A:%d P:%d V:%d E:%d D:%d");
}

static int parse_estadistica_csv_line(const char *line, EstadisticaData *out)
{
    return parse_estadistica_line_format(line, out, "%[^,],%d,%d,%d,%d,%d,%d");
}

static void importar_estadisticas_desde_archivo(const char *filename, const char *formato,
        EstadisticaLineParser parser, int log_parse_error)
{
    if (!crear_tabla_estadisticas())
        return;

    FILE *file = abrir_archivo_texto_importacion(filename, true);
    if (!file)
        return;

    printf("Importando estadisticas desde %s...\n", formato);
    char line[1024];
    int count = 0;

    while (fgets(line, sizeof(line), file))
    {
        EstadisticaData data;
        if (parser(line, &data) && procesar_estadistica_importada(&data))
            count++;
        else if (log_parse_error)
        {
            printf("Error parsing line: %s", line);
        }
    }

    fclose(file);
    printf("Importacion de estadisticas desde %s completada. %d estadisticas importadas\n",
           formato, count);
}

/**
 * @brief Parsea e inserta una camiseta desde JSON.
 *
 * @param item Objeto JSON de la camiseta.
 * @return 1 si se inserto correctamente, 0 si no.
 */
static int parse_camiseta_json(cJSON const *item)
{
    if (!cJSON_IsObject(item))
        return 0;

    cJSON const *id_json = cJSON_GetObjectItem(item, "id");
    cJSON const *nombre_json = cJSON_GetObjectItem(item, "nombre");

    if (!cJSON_IsNumber(id_json) || !cJSON_IsString(nombre_json))
        return 0;

    int id = id_json->valueint;
    const char *nombre = nombre_json->valuestring;

    // Verificar si ya existe
    if (id_existe_en_tabla("camiseta", id))
    {
        printf("Camiseta ID %d ya existe, omitiendo...\n", id);
        return 0;
    }

    // Insertar
    insertar_camiseta(id, nombre);

    printf("Camiseta '%s' importada correctamente\n", nombre);
    return 1;
}

/**
 * @brief Funcion generica para importar desde JSON.
 *
 * @param extension Extension del archivo.
 * @param parser Funcion para parsear un item JSON.
 */
static void import_json_generic(const char *extension,
                                int (*parser)(cJSON const *))
{
    char filename[1024];
    build_filename(extension, filename, sizeof(filename));

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    cJSON *json = cJSON_Parse(content);
    free(content);

    if (!json)
    {
        printf("Error: JSON invalido\n");
        return;
    }

    if (!cJSON_IsArray(json))
    {
        printf("Error: El JSON debe ser un array\n");
        cJSON_Delete(json);
        return;
    }

    int total = cJSON_GetArraySize(json);
    printf("Importando %d items...\n", total);

    int imported = 0;
    for (int i = 0; i < total; i++)
    {
        cJSON const *item = cJSON_GetArrayItem(json, i);
        if (parser(item))
            imported++;
    }

    cJSON_Delete(json);
    printf("Importacion completada. %d items importados\n", imported);
}
/**
 * @brief Importa camisetas desde archivo JSON.
 *
 * Lee el archivo JSON de camisetas y las inserta en la base de datos.
 */
void importar_camisetas_json()
{
    import_json_generic("camisetas.json", parse_camiseta_json);
}

/**
 * @brief Obtiene el ID de una cancha por nombre, creando una nueva si no
 * existe.
 *
 * @param cancha_nombre Nombre de la cancha.
 * @return ID de la cancha o -1 si hay error.
 */
static sqlite3_int64 obtener_o_crear_cancha_id(const char *cancha_nombre)
{
    // Buscar cancha existente
    sqlite3_stmt *cancha_stmt;
    if (!preparar_stmt("SELECT id FROM cancha WHERE nombre = ?", &cancha_stmt))
    {
        return -1;
    }
    sqlite3_bind_text(cancha_stmt, 1, cancha_nombre, -1, SQLITE_TRANSIENT);
    sqlite3_int64 cancha_id = -1;
    if (sqlite3_step(cancha_stmt) == SQLITE_ROW)
    {
        cancha_id = sqlite3_column_int64(cancha_stmt, 0);
    }
    sqlite3_finalize(cancha_stmt);

    if (cancha_id == -1)
    {
        printf("Cancha '%s' no encontrada, creando...\n", cancha_nombre);
        // Crear cancha si no existe
        sqlite3_stmt *insert_cancha;
        if (!preparar_stmt("INSERT INTO cancha(nombre) VALUES(?)", &insert_cancha))
        {
            return -1;
        }
        sqlite3_bind_text(insert_cancha, 1, cancha_nombre, -1, SQLITE_TRANSIENT);
        sqlite3_step(insert_cancha);
        cancha_id = sqlite3_last_insert_rowid(db);
        sqlite3_finalize(insert_cancha);
    }

    return cancha_id;
}

/**
 * @brief Verifica si ya existe un partido con los mismos datos.
 *
 * @param cancha_id ID de la cancha.
 * @param fecha Fecha y hora del partido.
 * @param camiseta_id ID de la camiseta.
 * @return 1 si existe, 0 si no existe.
 */
static int partido_existe(sqlite3_int64 cancha_id, const char *fecha,
                          int camiseta_id)
{
    sqlite3_stmt *dup_stmt;
    if (!preparar_stmt("SELECT COUNT(*) FROM partido WHERE cancha_id = ? AND "
                       "fecha_hora = ? AND camiseta_id = ?",
                       &dup_stmt))
    {
        return 0;
    }
    sqlite3_bind_int64(dup_stmt, 1, cancha_id);
    sqlite3_bind_text(dup_stmt, 2, fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(dup_stmt, 3, camiseta_id);
    sqlite3_step(dup_stmt);
    int exists = sqlite3_column_int(dup_stmt, 0);
    sqlite3_finalize(dup_stmt);

    return exists;
}

/**
 * @brief Obtiene el siguiente ID disponible para un partido.
 *
 * @return ID del partido.
 */
static int obtener_siguiente_partido_id()
{
    int partido_id = 1;
    sqlite3_stmt *max_stmt;
    if (!preparar_stmt("SELECT COALESCE(MAX(id), 0) + 1 FROM partido", &max_stmt))
    {
        return partido_id;
    }
    if (sqlite3_step(max_stmt) == SQLITE_ROW)
    {
        partido_id = sqlite3_column_int(max_stmt, 0);
    }
    sqlite3_finalize(max_stmt);

    return partido_id;
}

// Estructura PartidoData ya definida en forward declarations

/**
 * @brief Inserta un partido en la base de datos.
 *
 * @param data Estructura con los datos del partido.
 */
static void insertar_partido(PartidoData data)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "INSERT INTO partido(id, cancha_id, fecha_hora, goles, asistencias, "
                "camiseta_id, resultado, clima, dia, rendimiento_general, cansancio, "
                "estado_animo, comentario_personal) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                "?, ?, ?)",
                &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, data.partido_id);
    sqlite3_bind_int64(stmt, 2, data.cancha_id);
    sqlite3_bind_text(stmt, 3, data.fecha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, data.goles);
    sqlite3_bind_int(stmt, 5, data.asistencias);
    sqlite3_bind_int(stmt, 6, data.camiseta_id);
    sqlite3_bind_int(stmt, 7, data.resultado);
    sqlite3_bind_int(stmt, 8, data.clima);
    sqlite3_bind_int(stmt, 9, data.dia);
    sqlite3_bind_int(stmt, 10, data.rendimiento_general);
    sqlite3_bind_int(stmt, 11, data.cansancio);
    sqlite3_bind_int(stmt, 12, data.estado_animo);
    sqlite3_bind_text(stmt, 13, data.comentario_personal, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/**
 * @brief Procesa un item de partido desde JSON y lo inserta en la base de
 * datos.
 *
 * @param item El objeto JSON del partido.
 * @return 1 si se importo correctamente, 0 si no.
 */
static int procesar_partido_json_item(cJSON const *item)
{
    if (!cJSON_IsObject(item))
        return 0;

    cJSON const *cancha_json = cJSON_GetObjectItem(item, "cancha");
    cJSON const *fecha_json = cJSON_GetObjectItem(item, "fecha");
    cJSON const *goles_json = cJSON_GetObjectItem(item, "goles");
    cJSON const *asistencias_json = cJSON_GetObjectItem(item, "asistencias");
    cJSON const *camiseta_json = cJSON_GetObjectItem(item, "camiseta");
    cJSON const *resultado_json = cJSON_GetObjectItem(item, "resultado");
    cJSON const *clima_json = cJSON_GetObjectItem(item, "clima");
    cJSON const *dia_json = cJSON_GetObjectItem(item, "dia");
    cJSON const *rendimiento_general_json =
        cJSON_GetObjectItem(item, "rendimiento_general");
    cJSON const *cansancio_json = cJSON_GetObjectItem(item, "cansancio");
    cJSON const *estado_animo_json = cJSON_GetObjectItem(item, "estado_animo");
    cJSON const *comentario_personal_json =
        cJSON_GetObjectItem(item, "comentario_personal");

    if (!cJSON_IsString(cancha_json) || !cJSON_IsString(fecha_json) ||
            !cJSON_IsNumber(goles_json) || !cJSON_IsNumber(asistencias_json) ||
            !cJSON_IsString(camiseta_json))
        return 0;

    const char *cancha_nombre = cancha_json->valuestring;
    const char *fecha = fecha_json->valuestring;
    int goles = goles_json->valueint;
    int asistencias = asistencias_json->valueint;
    const char *camiseta_nombre = camiseta_json->valuestring;
    int resultado = resultado_json ? resultado_json->valueint : 0;
    int clima = clima_json ? clima_json->valueint : 0;
    int dia = dia_json ? dia_json->valueint : 0;
    int rendimiento_general =
        rendimiento_general_json ? rendimiento_general_json->valueint : 0;
    int cansancio = cansancio_json ? cansancio_json->valueint : 0;
    int estado_animo = estado_animo_json ? estado_animo_json->valueint : 0;
    const char *comentario_personal =
        comentario_personal_json ? comentario_personal_json->valuestring : "";

    PartidoInput input =
    {
        cancha_nombre, fecha, goles, asistencias, camiseta_nombre,
        resultado, clima, dia, rendimiento_general, cansancio, estado_animo,
        comentario_personal
    };

    return procesar_e_insertar_partido(&input);
}

/**
 * @brief Importa partidos desde archivo JSON.
 *
 * Lee el archivo JSON de partidos y los inserta en la base de datos.
 */
void importar_partidos_json()
{
    int count;
    cJSON *json = cargar_json_array("partidos.json", "partidos", &count);
    if (!json)
        return;

    int imported = 0;
    for (int i = 0; i < count; i++)
    {
        cJSON const *item = cJSON_GetArrayItem(json, i);
        if (procesar_partido_json_item(item))
            imported++;
    }

    cJSON_Delete(json);
    printf("Importacion de partidos completada. %d partidos importados\n",
           imported);
}

/**
 * @brief Importa lesiones desde archivo JSON.
 *
 * Lee el archivo JSON de lesiones y las inserta en la base de datos.
 */
void importar_lesiones_json()
{
    int count;
    cJSON *json = cargar_json_array("lesiones.json", "lesiones", &count);
    if (!json)
        return;

    for (int i = 0; i < count; i++)
    {
        cJSON const *item = cJSON_GetArrayItem(json, i);
        if (!cJSON_IsObject(item))
            continue;

        cJSON const *id_json = cJSON_GetObjectItem(item, "id");
        cJSON const *jugador_json = cJSON_GetObjectItem(item, "jugador");
        cJSON const *tipo_json = cJSON_GetObjectItem(item, "tipo");
        cJSON const *descripcion_json = cJSON_GetObjectItem(item, "descripcion");
        cJSON const *fecha_json = cJSON_GetObjectItem(item, "fecha");
        if (!cJSON_IsNumber(id_json) || !cJSON_IsString(jugador_json) ||
                !cJSON_IsString(tipo_json) || !cJSON_IsString(descripcion_json) ||
                !cJSON_IsString(fecha_json))
            continue;

        int id = id_json->valueint;
        const char *jugador = jugador_json->valuestring;
        const char *tipo = tipo_json->valuestring;
        const char *descripcion = descripcion_json->valuestring;
        const char *fecha = fecha_json->valuestring;

        // Verificar si ya existe
        if (id_existe_en_tabla("lesion", id))
        {
            printf("Lesion ID %d ya existe, omitiendo...\n", id);
            continue;
        }

        // Insertar lesion
        insertar_lesion(id, jugador, tipo, descripcion, fecha);

        printf("Lesion de '%s' importada correctamente\n", jugador);
    }

    cJSON_Delete(json);
    printf("Importacion de lesiones completada\n");
}

/**
 * @brief Importa estadisticas desde archivo JSON.
 *
 * Lee el archivo JSON de estadisticas y las inserta en la base de datos.
 */
void importar_estadisticas_json()
{
    if (!crear_tabla_estadisticas())
        return;

    int count;
    cJSON *json = cargar_json_array("estadisticas.json", "estadisticas", &count);
    if (!json)
        return;

    for (int i = 0; i < count; i++)
    {
        cJSON const *item = cJSON_GetArrayItem(json, i);
        if (!cJSON_IsObject(item))
            continue;

        cJSON const *camiseta_json = cJSON_GetObjectItem(item, "camiseta");
        cJSON const *goles_json = cJSON_GetObjectItem(item, "goles");
        cJSON const *asistencias_json = cJSON_GetObjectItem(item, "asistencias");
        cJSON const *partidos_json = cJSON_GetObjectItem(item, "partidos");
        cJSON const *victorias_json = cJSON_GetObjectItem(item, "victorias");
        cJSON const *empates_json = cJSON_GetObjectItem(item, "empates");
        cJSON const *derrotas_json = cJSON_GetObjectItem(item, "derrotas");

        if (!cJSON_IsString(camiseta_json) || !cJSON_IsNumber(goles_json) ||
                !cJSON_IsNumber(asistencias_json) || !cJSON_IsNumber(partidos_json))
            continue;

        const char *camiseta = camiseta_json->valuestring;
        int goles = goles_json->valueint;
        int asistencias = asistencias_json->valueint;
        int partidos = partidos_json->valueint;
        int victorias = victorias_json ? victorias_json->valueint : 0;
        int empates = empates_json ? empates_json->valueint : 0;
        int derrotas = derrotas_json ? derrotas_json->valueint : 0;

        // Obtener ID de camiseta
        int camiseta_id = obtener_camiseta_id_estadistica(camiseta);

        if (camiseta_id == -1)
        {
            printf("Camiseta '%s' no encontrada, omitiendo estadistica...\n",
                   camiseta);
            continue;
        }

        // Verificar si ya existe estadistica para esta camiseta
        if (estadistica_existe(camiseta_id))
        {
            printf("Estadistica para camiseta '%s' ya existe, omitiendo...\n",
                   camiseta);
            continue;
        }

        // Insertar estadistica
        insertar_estadistica(camiseta_id, goles, asistencias, partidos, victorias,
                             empates, derrotas);

        printf("Estadistica de '%s' importada correctamente\n", camiseta);
    }

    cJSON_Delete(json);
    printf("Importacion de estadisticas completada\n");
}

/**
 * @brief Importa camisetas desde archivo JSON con pausa.
 */
DEFINE_IMPORT_CON_PAUSA(importar_camisetas_json_con_pausa, "camisetas", "JSON", importar_camisetas_json)

/**
 * @brief Importa partidos desde archivo JSON con pausa.
 */
DEFINE_IMPORT_CON_PAUSA(importar_partidos_json_con_pausa, "partidos", "JSON", importar_partidos_json)

/**
 * @brief Importa lesiones desde archivo JSON con pausa.
 */
DEFINE_IMPORT_CON_PAUSA(importar_lesiones_json_con_pausa, "lesiones", "JSON", importar_lesiones_json)

/**
 * @brief Importa estadisticas desde archivo JSON con pausa.
 */
DEFINE_IMPORT_CON_PAUSA(importar_estadisticas_json_con_pausa, "estadisticas", "JSON", importar_estadisticas_json)

/**
 * @brief Importa camisetas desde archivo TXT con pausa.
 */
DEFINE_IMPORT_CON_PAUSA(importar_camisetas_txt_con_pausa, "camisetas", "TXT", importar_camisetas_txt)

/**
 * @brief Importa partidos desde archivo TXT con pausa.
 */
DEFINE_IMPORT_CON_PAUSA(importar_partidos_txt_con_pausa, "partidos", "TXT", importar_partidos_txt)

/**
 * @brief Importa lesiones desde archivo TXT con pausa.
 */
DEFINE_IMPORT_CON_PAUSA(importar_lesiones_txt_con_pausa, "lesiones", "TXT", importar_lesiones_txt)

/**
 * @brief Importa estadisticas desde archivo TXT con pausa.
 */
DEFINE_IMPORT_CON_PAUSA(importar_estadisticas_txt_con_pausa, "estadisticas", "TXT", importar_estadisticas_txt)

/**
 * @brief Importa camisetas desde archivo CSV con pausa.
 */
DEFINE_IMPORT_CON_PAUSA(importar_camisetas_csv_con_pausa, "camisetas", "CSV", importar_camisetas_csv)

/**
 * @brief Importa partidos desde archivo CSV con pausa.
 */
DEFINE_IMPORT_CON_PAUSA(importar_partidos_csv_con_pausa, "partidos", "CSV", importar_partidos_csv)

/**
 * @brief Importa lesiones desde archivo CSV con pausa.
 */
DEFINE_IMPORT_CON_PAUSA(importar_lesiones_csv_con_pausa, "lesiones", "CSV", importar_lesiones_csv)

/**
 * @brief Importa estadisticas desde archivo CSV con pausa.
 */
DEFINE_IMPORT_CON_PAUSA(importar_estadisticas_csv_con_pausa, "estadisticas", "CSV", importar_estadisticas_csv)

/**
 * @brief Importa todos los datos desde archivos CSV con pausa.
 */
static void importar_todo_csv_con_pausa()
{
    ImportTodoConfig config =
    {
        "Importando todo desde CSV...",
        "Importacion de todo desde CSV completada.",
        importar_camisetas_csv,
        importar_partidos_csv,
        importar_lesiones_csv,
        importar_estadisticas_csv
    };
    importar_todo_con_config(&config);
}

/**
 * @brief Importa camisetas desde archivo HTML con pausa.
 */
DEFINE_IMPORT_CON_PAUSA(importar_camisetas_html_con_pausa, "camisetas", "HTML", importar_camisetas_html)

/**
 * @brief Importa partidos desde archivo HTML con pausa.
 */
DEFINE_IMPORT_CON_PAUSA(importar_partidos_html_con_pausa, "partidos", "HTML", importar_partidos_html)

/**
 * @brief Importa lesiones desde archivo HTML con pausa.
 */
DEFINE_IMPORT_CON_PAUSA(importar_lesiones_html_con_pausa, "lesiones", "HTML", importar_lesiones_html)

/**
 * @brief Importa estadisticas desde archivo HTML con pausa.
 */
DEFINE_IMPORT_CON_PAUSA(importar_estadisticas_html_con_pausa, "estadisticas", "HTML", importar_estadisticas_html)

/**
 * @brief Importa todos los datos desde archivos HTML con pausa.
 */
static void importar_todo_html_con_pausa()
{
    ImportTodoConfig config =
    {
        "Importando todo desde HTML...",
        "Importacion de todo desde HTML completada.",
        importar_camisetas_html,
        importar_partidos_html,
        importar_lesiones_html,
        importar_estadisticas_html
    };
    importar_todo_con_config(&config);
}

/**
 * @brief Importa todos los datos desde archivos TXT con pausa.
 */
static void importar_todo_txt_con_pausa()
{
    ImportTodoConfig config =
    {
        "Importando todo desde TXT...",
        "Importacion de todo desde TXT completada.",
        importar_camisetas_txt,
        importar_partidos_txt,
        importar_lesiones_txt,
        importar_estadisticas_txt
    };
    importar_todo_con_config(&config);
}

/* ===================== IMPORTACIoN DESDE TXT ===================== */

/**
 * @brief Importa camisetas desde archivo TXT.
 *
 * Lee el archivo TXT de camisetas y las inserta en la base de datos.
 * El formato esperado es: ID - NOMBRE
 */
void importar_camisetas_txt()
{
    importar_camisetas_desde_archivo("camisetas.txt", "TXT", parse_camiseta_txt_line);
}

/**
 * @brief Convierte una cadena de resultado a numero.
 *
 * @param resultado_str Cadena del resultado.
 * @return Numero correspondiente al resultado.
 */
static int convertir_resultado(const char *resultado_str)
{
    if (strcmp(resultado_str, "VICTORIA") == 0)
        return 1;
    else if (strcmp(resultado_str, "EMPATE") == 0)
        return 2;
    else if (strcmp(resultado_str, "DERROTA") == 0)
        return 3;
    return 0;
}

/**
 * @brief Convierte una cadena de clima a numero.
 *
 * @param clima_str Cadena del clima.
 * @return Numero correspondiente al clima.
 */
static int convertir_clima(const char *clima_str)
{
    if (strcmp(clima_str, "Despejado") == 0)
        return 1;
    else if (strcmp(clima_str, "Nublado") == 0)
        return 2;
    else if (strcmp(clima_str, "Lluvia") == 0)
        return 3;
    else if (strcmp(clima_str, "Ventoso") == 0)
        return 4;
    else if (strcmp(clima_str, "Mucho") == 0)
        return 5; // Mucho Calor o Mucho Frio
    else if (strcmp(clima_str, "Frio") == 0)
        return 6;
    return 0;
}

/**
 * @brief Convierte una cadena de dia a numero.
 *
 * @param dia_str Cadena del dia.
 * @return Numero correspondiente al dia.
 */
static int convertir_dia(const char *dia_str)
{
    if (strcmp(dia_str, "Dia") == 0)
        return 1;
    else if (strcmp(dia_str, "Tarde") == 0)
        return 2;
    else if (strcmp(dia_str, "Noche") == 0)
        return 3;
    return 0;
}

static int procesar_partido_desde_raw(const PartidoRawInput *raw)
{
    int resultado = convertir_resultado(raw->resultado_str);
    int clima = convertir_clima(raw->clima_str);
    int dia = convertir_dia(raw->dia_str);

    PartidoInput input =
    {
        raw->cancha, raw->fecha, raw->goles, raw->asistencias, raw->camiseta,
        resultado, clima, dia, raw->rendimiento_general,
        raw->cansancio, raw->estado_animo, raw->comentario
    };

    return procesar_e_insertar_partido(&input);
}

/**
 * @brief Procesa una linea de partido desde TXT y la inserta en la base de
 * datos.
 *
 * @param line Linea a procesar.
 * @return 1 si se proceso correctamente, 0 si no.
 */
static int procesar_partido_txt_line(const char *line)
{
    char cancha[256];
    char fecha[256];
    char camiseta[256];
    char resultado_str[32];
    char clima_str[32];
    char dia_str[32];
    char comentario[512];
    int goles;
    int asistencias;
    int rendimiento_general;
    int cansancio;
    int estado_animo;

    // Formato: CANCHA | FECHA | G:Goles A:Asistencias | CAMISETA | Res:Resultado
    // Cli:Clima Dia:Dia RG:Rendimiento Can:Cansancio EA:EstadoAnimo | Comentario
#if defined(_WIN32) && defined(_MSC_VER)
    if (sscanf_s(line,
                 " %255[^|] | %255[^|] | G:%d A:%d | %255[^|] | Res:%31[^ ] Cli:%31[^ ] "
                 "Dia:%31[^ ] RG:%d Can:%d EA:%d | %511[^\n]",
                 cancha, (unsigned)sizeof(cancha),
                 fecha, (unsigned)sizeof(fecha),
                 &goles, &asistencias,
                 camiseta, (unsigned)sizeof(camiseta),
                 resultado_str, (unsigned)sizeof(resultado_str),
                 clima_str, (unsigned)sizeof(clima_str),
                 dia_str, (unsigned)sizeof(dia_str),
                 &rendimiento_general, &cansancio, &estado_animo,
                 comentario, (unsigned)sizeof(comentario)) != 12)
#else
    if (sscanf(line,
               " %255[^|] | %255[^|] | G:%d A:%d | %255[^|] | Res:%31[^ ] Cli:%31[^ ] "
               "Dia:%31[^ ] RG:%d Can:%d EA:%d | %511[^\n]",
               cancha, fecha, &goles, &asistencias, camiseta, resultado_str,
               clima_str, dia_str, &rendimiento_general, &cansancio,
               &estado_animo, comentario) != 12)
#endif
        return 0;

    PartidoRawInput raw =
    {
        cancha, fecha, goles, asistencias, camiseta,
        resultado_str, clima_str, dia_str,
        rendimiento_general, cansancio, estado_animo, comentario
    };

    return procesar_partido_desde_raw(&raw);
}

/**
 * @brief Importa partidos desde archivo TXT.
 *
 * Lee el archivo TXT de partidos y los inserta en la base de datos.
 * El formato esperado es complejo con multiples campos separados por |
 */
void importar_partidos_txt()
{
    importar_partidos_desde_archivo("partidos.txt", "TXT", procesar_partido_txt_line);
}

/**
 * @brief Importa lesiones desde archivo TXT.
 *
 * Lee el archivo TXT de lesiones y las inserta en la base de datos.
 * El formato esperado es: ID - JUGADOR | TIPO | DESCRIPCION | FECHA
 */
void importar_lesiones_txt()
{
    importar_lesiones_desde_archivo("lesiones.txt", "TXT", parse_lesion_txt_line);
}

/**
 * @brief Importa estadisticas desde archivo TXT.
 *
 * Lee el archivo TXT de estadisticas y las inserta en la base de datos.
 * El formato esperado es: CAMISETA | G:Goles A:Asistencias P:Partidos
 * V:Victorias E:Empates D:Derrotas
 */
void importar_estadisticas_txt()
{
    importar_estadisticas_desde_archivo("estadisticas.txt", "TXT", parse_estadistica_txt_line, 1);
}

/* ===================== IMPORTACIoN DESDE CSV ===================== */

/**
 * @brief Importa camisetas desde archivo CSV.
 *
 * Lee el archivo CSV de camisetas y las inserta en la base de datos.
 * El formato esperado es: id,nombre
 */
void importar_camisetas_csv()
{
    importar_camisetas_desde_archivo("camisetas.csv", "CSV", parse_camiseta_csv_line);
}

/**
 * @brief Procesa una linea de partido desde CSV y la inserta en la base de
 * datos.
 *
 * @param line Linea a procesar.
 * @return 1 si se proceso correctamente, 0 si no.
 */
static int procesar_partido_csv_line(const char *line)
{
    char cancha[256];
    char fecha[256];
    char camiseta[256];
    char resultado_str[32];
    char clima_str[32];
    char dia_str[32];
    char comentario[512];
    int goles;
    int asistencias;
    int rendimiento_general;
    int cansancio;
    int estado_animo;

    // Formato:
    // cancha,fecha,goles,asistencias,camiseta,resultado,clima,dia,rendimiento_general,cansancio,estado_animo,comentario
#if defined(_WIN32) && defined(_MSC_VER)
    if (sscanf_s(line,
                 "%255[^,],%255[^,],%d,%d,%255[^,],%31[^,],%31[^,],%31[^,],%d,%d,%d,%511[^\n]",
                 cancha, (unsigned)sizeof(cancha),
                 fecha, (unsigned)sizeof(fecha),
                 &goles, &asistencias,
                 camiseta, (unsigned)sizeof(camiseta),
                 resultado_str, (unsigned)sizeof(resultado_str),
                 clima_str, (unsigned)sizeof(clima_str),
                 dia_str, (unsigned)sizeof(dia_str),
                 &rendimiento_general, &cansancio, &estado_animo,
                 comentario, (unsigned)sizeof(comentario)) != 12)
#else
    if (sscanf(line,
               "%255[^,],%255[^,],%d,%d,%255[^,],%31[^,],%31[^,],%31[^,],%d,%d,%d,%511[^\n]",
               cancha, fecha, &goles, &asistencias, camiseta, resultado_str,
               clima_str, dia_str, &rendimiento_general, &cansancio,
               &estado_animo, comentario) != 12)
#endif
        return 0;

    PartidoRawInput raw =
    {
        cancha, fecha, goles, asistencias, camiseta,
        resultado_str, clima_str, dia_str,
        rendimiento_general, cansancio, estado_animo, comentario
    };

    return procesar_partido_desde_raw(&raw);
}

/**
 * @brief Importa partidos desde archivo CSV.
 *
 * Lee el archivo CSV de partidos y los inserta en la base de datos.
 * El formato esperado es complejo con multiples campos separados por coma.
 */
void importar_partidos_csv()
{
    importar_partidos_desde_archivo("partidos.csv", "CSV", procesar_partido_csv_line);
}

/**
 * @brief Importa lesiones desde archivo CSV.
 *
 * Lee el archivo CSV de lesiones y las inserta en la base de datos.
 * El formato esperado es: id,jugador,tipo,descripcion,fecha
 */
void importar_lesiones_csv()
{
    importar_lesiones_desde_archivo("lesiones.csv", "CSV", parse_lesion_csv_line);
}

/**
 * @brief Importa estadisticas desde archivo CSV.
 *
 * Lee el archivo CSV de estadisticas y las inserta en la base de datos.
 * El formato esperado es:
 * camiseta,goles,asistencias,partidos,victorias,empates,derrotas
 */
void importar_estadisticas_csv()
{
    importar_estadisticas_desde_archivo("estadisticas.csv", "CSV", parse_estadistica_csv_line, 0);
}

/* ===================== IMPORTACIoN DESDE HTML ===================== */

/**
 * @brief Importa camisetas desde archivo HTML.
 *
 * Lee el archivo HTML de camisetas y las inserta en la base de datos.
 * Asume un formato simple de tabla HTML con <td> para id y nombre.
 */
void importar_camisetas_html()
{
    char filename[1024];
    build_filename("camisetas.html", filename, sizeof(filename));

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    printf("Importando camisetas desde HTML...\n");
    int count = 0;
    char const *ptr = content;

    // Buscar <td> tags
    int continue_parsing = 1;
    while (continue_parsing)
    {
        ptr = strstr(ptr, "<td>");
        if (ptr == NULL)
        {
            continue_parsing = 0;
            continue;
        }
        ptr += 4; // Saltar <td>
        char *end = strstr(ptr, "</td>");
        if (!end)
        {
            continue_parsing = 0;
            continue;
        }

        *end = '\0';
        int id = atoi(ptr);

        // Siguiente <td> para nombre
        ptr = strstr(end + 5, "<td>");
        if (!ptr)
        {
            continue_parsing = 0;
            continue;
        }
        ptr += 4;
        end = strstr(ptr, "</td>");
        if (!end)
        {
            continue_parsing = 0;
            continue;
        }
        *end = '\0';
        char nombre[256];
        strcpy_s(nombre, sizeof(nombre), ptr);

        // Verificar si ya existe
        if (id_existe_en_tabla("camiseta", id))
        {
            printf("Camiseta ID %d ya existe, omitiendo...\n", id);
            ptr = end + 5;
            continue;
        }

        // Insertar
        insertar_camiseta(id, nombre);

        printf("Camiseta '%s' importada correctamente\n", nombre);
        count++;
        ptr = end + 5;
    }

    free(content);
    printf("Importacion de camisetas desde HTML completada. %d camisetas "
           "importadas\n",
           count);
}

/**
 * @brief Procesa una fila de partido desde HTML y la inserta en la base de
 * datos.
 *
 * @param ptr Puntero a la fila <tr> en el contenido HTML.
 * @return 1 si se proceso correctamente, 0 si no.
 */
static int procesar_partido_html_row(char **ptr)
{
    char cancha[256];
    char fecha[256];
    char camiseta[256];
    char resultado_str[32];
    char clima_str[32];
    char dia_str[32];
    char comentario[512];
    int goles = 0;
    int asistencias = 0;
    int rendimiento_general = 0;
    int cansancio = 0;
    int estado_animo = 0;

    // Extraer celdas
    for (int i = 0; i < 12; i++)
    {
        char const *td = strstr(*ptr, "<td>");
        if (!td)
            return 0;
        td += 4;
        char *end = strstr(td, "</td>");
        if (!end)
            return 0;
        *end = '\0';

        if (i == 0)
            strcpy_s(cancha, sizeof(cancha), td);
        else if (i == 1)
            strcpy_s(fecha, sizeof(fecha), td);
        else if (i == 2)
            goles = atoi(td);
        else if (i == 3)
            asistencias = atoi(td);
        else if (i == 4)
            strcpy_s(camiseta, sizeof(camiseta), td);
        else if (i == 5)
            strcpy_s(resultado_str, sizeof(resultado_str), td);
        else if (i == 6)
            strcpy_s(clima_str, sizeof(clima_str), td);
        else if (i == 7)
            strcpy_s(dia_str, sizeof(dia_str), td);
        else if (i == 8)
            rendimiento_general = atoi(td);
        else if (i == 9)
            cansancio = atoi(td);
        else if (i == 10)
            estado_animo = atoi(td);
        else if (i == 11)
            strcpy_s(comentario, sizeof(comentario), td);
        *ptr = end + 5;
    }

    PartidoRawInput raw =
    {
        cancha, fecha, goles, asistencias, camiseta,
        resultado_str, clima_str, dia_str,
        rendimiento_general, cansancio, estado_animo, comentario
    };

    return procesar_partido_desde_raw(&raw);
}

/**
 * @brief Importa partidos desde archivo HTML.
 *
 * Lee el archivo HTML de partidos y los inserta en la base de datos.
 * Asume un formato simple de tabla HTML.
 */
void importar_partidos_html()
{
    char filename[1024];
    build_filename("partidos.html", filename, sizeof(filename));

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    printf("Importando partidos desde HTML...\n");
    int count = 0;
    char *ptr = content;

    // Buscar filas de tabla
    while ((ptr = strstr(ptr, "<tr>")) != NULL)
    {
        ptr += 4; // Saltar <tr>
        if (procesar_partido_html_row(&ptr))
            count++;
    }

    free(content);
    printf(
        "Importacion de partidos desde HTML completada. %d partidos importados\n",
        count);
}

/**
 * @brief Importa lesiones desde archivo HTML.
 *
 * Lee el archivo HTML de lesiones y las inserta en la base de datos.
 * Asume un formato simple de tabla HTML.
 */
void importar_lesiones_html()
{
    char filename[1024];
    build_filename("lesiones.html", filename, sizeof(filename));

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    printf("Importando lesiones desde HTML...\n");
    int count = 0;
    char const *ptr = content;

    // Buscar filas de tabla
    while ((ptr = strstr(ptr, "<tr>")) != NULL)
    {
        ptr += 4; // Saltar <tr>
        int id;
        char jugador[256];
        char tipo[256];
        char descripcion[512];
        char fecha[256];

        // Extraer celdas
        char const *td = strstr(ptr, "<td>");
        if (!td)
            continue;
        td += 4;
        char *end = strstr(td, "</td>");
        if (!end)
            continue;
        *end = '\0';
        id = atoi(td);
        ptr = end + 5;

        td = strstr(ptr, "<td>");
        if (!td)
            continue;
        td += 4;
        end = strstr(td, "</td>");
        if (!end)
            continue;
        *end = '\0';
        strcpy_s(jugador, sizeof(jugador), td);
        ptr = end + 5;

        td = strstr(ptr, "<td>");
        if (!td)
            continue;
        td += 4;
        end = strstr(td, "</td>");
        if (!end)
            continue;
        *end = '\0';
        strcpy_s(tipo, sizeof(tipo), td);
        ptr = end + 5;

        td = strstr(ptr, "<td>");
        if (!td)
            continue;
        td += 4;
        end = strstr(td, "</td>");
        if (!end)
            continue;
        *end = '\0';
        strcpy_s(descripcion, sizeof(descripcion), td);
        ptr = end + 5;

        td = strstr(ptr, "<td>");
        if (!td)
            continue;
        td += 4;
        end = strstr(td, "</td>");
        if (!end)
            continue;
        *end = '\0';
        strcpy_s(fecha, sizeof(fecha), td);
        ptr = end + 5;

        // Verificar si ya existe
        if (id_existe_en_tabla("lesion", id))
        {
            printf("Lesion ID %d ya existe, omitiendo...\n", id);
            continue;
        }

        // Insertar lesion
        insertar_lesion(id, jugador, tipo, descripcion, fecha);

        printf("Lesion de '%s' importada correctamente\n", jugador);
        count++;
    }

    free(content);
    printf(
        "Importacion de lesiones desde HTML completada. %d lesiones importadas\n",
        count);
}

/**
 * @brief Crea la tabla de estadisticas si no existe.
 *
 * @return true si la tabla se creo o ya existe, false si hubo error.
 */
static bool crear_tabla_estadisticas()
{
    const char *create_table_sql =
        "CREATE TABLE IF NOT EXISTS estadistica ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "camiseta_id INTEGER,"
        "goles INTEGER,"
        "asistencias INTEGER,"
        "partidos INTEGER,"
        "victorias INTEGER,"
        "empates INTEGER,"
        "derrotas INTEGER,"
        "FOREIGN KEY (camiseta_id) REFERENCES camiseta(id));";
    char *err_msg = NULL;
    if (sqlite3_exec(db, create_table_sql, NULL, NULL, &err_msg) != SQLITE_OK)
    {
        printf("Error creando tabla estadistica: %s\n", err_msg);
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

/**
 * @brief Estructura para almacenar datos de estadisticas.
 */
typedef struct
{
    char camiseta[256];
    int goles;
    int asistencias;
    int partidos;
    int victorias;
    int empates;
    int derrotas;
} EstadisticasData;

/**
 * @brief Extrae datos de estadisticas de una fila HTML.
 *
 * @param ptr Puntero al contenido HTML.
 * @param data Puntero a la estructura EstadisticasData para almacenar los
 * datos.
 * @return true si la extraccion fue exitosa, false si no.
 */
static bool extraer_datos_estadisticas_html(char **ptr,
        EstadisticasData *data)
{
    // Extraer celdas
    for (int i = 0; i < 7; i++)
    {
        char const *td = strstr(*ptr, "<td>");
        if (!td)
            return false;
        td += 4;
        char *end = strstr(td, "</td>");
        if (!end)
            return false;
        *end = '\0';

        if (i == 0)
            strcpy_s(data->camiseta, 256, td);
        else if (i == 1)
            data->goles = atoi(td);
        else if (i == 2)
            data->asistencias = atoi(td);
        else if (i == 3)
            data->partidos = atoi(td);
        else if (i == 4)
            data->victorias = atoi(td);
        else if (i == 5)
            data->empates = atoi(td);
        else if (i == 6)
            data->derrotas = atoi(td);
        *ptr = end + 5;
    }
    return true;
}

/**
 * @brief Obtiene el ID de una camiseta por nombre.
 *
 * @param camiseta_nombre Nombre de la camiseta.
 * @return ID de la camiseta o -1 si no existe.
 */
static int obtener_camiseta_id_estadistica(const char *camiseta_nombre)
{
    sqlite3_stmt *camiseta_stmt;
    if (!preparar_stmt("SELECT id FROM camiseta WHERE nombre = ?", &camiseta_stmt))
    {
        return -1;
    }
    sqlite3_bind_text(camiseta_stmt, 1, camiseta_nombre, -1, SQLITE_TRANSIENT);
    int camiseta_id = -1;
    if (sqlite3_step(camiseta_stmt) == SQLITE_ROW)
    {
        camiseta_id = sqlite3_column_int(camiseta_stmt, 0);
    }
    sqlite3_finalize(camiseta_stmt);
    return camiseta_id;
}

/**
 * @brief Verifica si ya existe una estadistica para una camiseta.
 *
 * @param camiseta_id ID de la camiseta.
 * @return true si existe, false si no existe.
 */
static bool estadistica_existe(int camiseta_id)
{
    sqlite3_stmt *check_stmt;
    if (!preparar_stmt("SELECT COUNT(*) FROM estadistica WHERE camiseta_id = ?",
                       &check_stmt))
    {
        return false;
    }
    sqlite3_bind_int(check_stmt, 1, camiseta_id);
    sqlite3_step(check_stmt);
    int exists = sqlite3_column_int(check_stmt, 0);
    sqlite3_finalize(check_stmt);
    return exists > 0;
}

/**
 * @brief Inserta una estadistica en la base de datos.
 *
 * @param camiseta_id ID de la camiseta.
 * @param goles Goles.
 * @param asistencias Asistencias.
 * @param partidos Partidos.
 * @param victorias Victorias.
 * @param empates Empates.
 * @param derrotas Derrotas.
 */
static void insertar_estadistica(int camiseta_id, int goles, int asistencias,
                                 int partidos, int victorias, int empates,
                                 int derrotas)
{
    sqlite3_stmt *stmt;
    if (!preparar_stmt(
                "INSERT INTO estadistica(camiseta_id, goles, asistencias, partidos, "
                "victorias, empates, derrotas) VALUES(?, ?, ?, ?, ?, ?, ?)",
                &stmt))
    {
        return;
    }
    sqlite3_bind_int(stmt, 1, camiseta_id);
    sqlite3_bind_int(stmt, 2, goles);
    sqlite3_bind_int(stmt, 3, asistencias);
    sqlite3_bind_int(stmt, 4, partidos);
    sqlite3_bind_int(stmt, 5, victorias);
    sqlite3_bind_int(stmt, 6, empates);
    sqlite3_bind_int(stmt, 7, derrotas);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/**
 * @brief Importa estadisticas desde archivo HTML.
 *
 * Lee el archivo HTML de estadisticas y las inserta en la base de datos.
 * Asume un formato simple de tabla HTML.
 */
void importar_estadisticas_html()
{
    if (!crear_tabla_estadisticas())
        return;

    char filename[1024];
    build_filename("estadisticas.html", filename, sizeof(filename));

    printf("Importando desde: %s\n", filename);

    char *content = read_file_content(filename);
    if (!content)
        return;

    printf("Importando estadisticas desde HTML...\n");
    int count = 0;
    char *ptr = content;

    // Buscar filas de tabla
    while ((ptr = strstr(ptr, "<tr>")) != NULL)
    {
        ptr += 4; // Saltar <tr>

        EstadisticasData data;

        if (!extraer_datos_estadisticas_html(&ptr, &data))
            continue;

        int camiseta_id = obtener_camiseta_id_estadistica(data.camiseta);
        if (camiseta_id == -1)
        {
            printf("Camiseta '%s' no encontrada, omitiendo estadistica...\n",
                   data.camiseta);
            continue;
        }

        if (estadistica_existe(camiseta_id))
        {
            printf("Estadistica para camiseta '%s' ya existe, omitiendo...\n",
                   data.camiseta);
            continue;
        }

        insertar_estadistica(camiseta_id, data.goles, data.asistencias,
                             data.partidos, data.victorias, data.empates,
                             data.derrotas);
        printf("Estadistica de '%s' importada correctamente\n", data.camiseta);
        count++;
    }

    free(content);
    printf("Importacion de estadisticas desde HTML completada. %d estadisticas "
           "importadas\n",
           count);
}

/**
 * @brief Importa todos los datos desde archivos JSON con pausa.
 */
static void importar_todo_con_pausa()
{
    ImportTodoConfig config =
    {
        "Importando todo...",
        "Importacion de todo completada.",
        importar_camisetas_json,
        importar_partidos_json,
        importar_lesiones_json,
        importar_estadisticas_json
    };
    importar_todo_con_config(&config);
}

/**
 * @brief Submenu para importar datos desde archivos JSON.
 */
static void submenu_importar_json()
{
    MenuItem items[] = {{1, get_text("import_camisetas"), &importar_camisetas_json_con_pausa},
        {2, get_text("import_partidos"), &importar_partidos_json_con_pausa},
        {3, get_text("import_lesiones"), &importar_lesiones_json_con_pausa},
        {4, get_text("import_estadisticas"), &importar_estadisticas_json_con_pausa},
        {5, get_text("import_todo"), &importar_todo_con_pausa},
        {0, get_text("menu_back"), NULL}
    };
    ejecutar_menu_estandar(get_text("import_menu_json_title"), items, 6);
}

/**
 * @brief Submenu para importar datos desde archivos TXT.
 */
static void submenu_importar_txt()
{
    MenuItem items[] = {{1, get_text("import_camisetas"), &importar_camisetas_txt_con_pausa},
        {2, get_text("import_partidos"), &importar_partidos_txt_con_pausa},
        {3, get_text("import_lesiones"), &importar_lesiones_txt_con_pausa},
        {4, get_text("import_estadisticas"), &importar_estadisticas_txt_con_pausa},
        {5, get_text("import_todo"), &importar_todo_txt_con_pausa},
        {0, get_text("menu_back"), NULL}
    };
    ejecutar_menu_estandar(get_text("import_menu_txt_title"), items, 6);
}

/**
 * @brief Submenu para importar datos desde archivos CSV.
 */
static void submenu_importar_csv()
{
    MenuItem items[] = {{1, get_text("import_camisetas"), &importar_camisetas_csv_con_pausa},
        {2, get_text("import_partidos"), &importar_partidos_csv_con_pausa},
        {3, get_text("import_lesiones"), &importar_lesiones_csv_con_pausa},
        {4, get_text("import_estadisticas"), &importar_estadisticas_csv_con_pausa},
        {5, get_text("import_todo"), &importar_todo_csv_con_pausa},
        {0, get_text("menu_back"), NULL}
    };
    ejecutar_menu_estandar(get_text("import_menu_csv_title"), items, 6);
}

/**
 * @brief Submenu para importar datos desde archivos HTML.
 */
static void submenu_importar_html()
{
    MenuItem items[] = {{1, get_text("import_camisetas"), &importar_camisetas_html_con_pausa},
        {2, get_text("import_partidos"), &importar_partidos_html_con_pausa},
        {3, get_text("import_lesiones"), &importar_lesiones_html_con_pausa},
        {4, get_text("import_estadisticas"), &importar_estadisticas_html_con_pausa},
        {5, get_text("import_todo"), &importar_todo_html_con_pausa},
        {0, get_text("menu_back"), NULL}
    };
    ejecutar_menu_estandar(get_text("import_menu_html_title"), items, 6);
}

static void menu_importar_json_con_backup()
{
    backup_base_datos_automatico("import_json");
    submenu_importar_json();
}

static void menu_importar_txt_con_backup()
{
    backup_base_datos_automatico("import_txt");
    submenu_importar_txt();
}

static void menu_importar_csv_con_backup()
{
    backup_base_datos_automatico("import_csv");
    submenu_importar_csv();
}

static void menu_importar_html_con_backup()
{
    backup_base_datos_automatico("import_html");
    submenu_importar_html();
}

static void importar_todo_json_rapido()
{
    backup_base_datos_automatico("import_json");
    importar_todo_con_pausa();
}

static void importar_todo_csv_rapido()
{
    backup_base_datos_automatico("import_csv");
    importar_todo_csv_con_pausa();
}

static void importar_base_datos_con_backup()
{
    backup_base_datos_automatico("import_db");
    importar_base_datos();
}

/**
 * @brief Menu principal para importar datos desde archivos segun seleccion del
 * usuario.
 *
 * Esta funcion muestra un menu principal para que el usuario seleccione el
 * formato de archivo desde el cual importar: JSON, TXT, CSV o HTML. Cada opcion
 * lleva a un submenu especifico para ese formato.
 */
void menu_importar()
{
    MenuItem items[] = {{1, get_text("import_from_json"),&menu_importar_json_con_backup},
        {2, get_text("import_from_txt"), &menu_importar_txt_con_backup},
        {3, get_text("import_from_csv"), &menu_importar_csv_con_backup},
        {4, get_text("import_from_html"), &menu_importar_html_con_backup},
        {5, get_text("import_todo_json_rapido"), &importar_todo_json_rapido},
        {6, get_text("import_todo_csv_rapido"), &importar_todo_csv_rapido},
        {7, get_text("import_from_db"), &importar_base_datos_con_backup},
        {0, get_text("menu_back"), NULL}
    };
    ejecutar_menu_estandar(get_text("import_menu_title"), items, 8);
}
