/**
 * @file utils.h
 * @brief Declaraciones de funciones utilitarias para entrada/salida y manejo de datos.
 *
 * Este archivo de cabecera declara funciones auxiliares para interactuar con el usuario,
 * manejar fechas y horas, limpiar la pantalla, verificar existencia de IDs en la base de datos,
 * y gestionar directorios de exportación.
 */

#ifndef UTILS_H
#define UTILS_H
#include "cJSON.h"
#include "sqlite3.h"
#include <stddef.h>
#include <stdio.h>

/**
 * @brief Solicita al usuario un número entero.
 *
 * Muestra el mensaje proporcionado y lee un entero desde la entrada estándar.
 *
 * @param msg El mensaje a mostrar al usuario.
 * @return El número entero ingresado por el usuario.
 */
int input_int(const char *msg);

/**
 * @brief Solicita al usuario un número de punto flotante.
 *
 * Muestra el mensaje proporcionado y lee un double desde la entrada estándar.
 *
 * @param msg El mensaje a mostrar al usuario.
 * @return El número double ingresado por el usuario.
 */
double input_double(const char *msg);

/**
 * @brief Solicita al usuario una cadena de texto.
 *
 * Muestra el mensaje proporcionado y lee una cadena desde la entrada estándar,
 * eliminando el carácter de nueva línea al final.
 *
 * @param msg El mensaje a mostrar al usuario.
 * @param buffer El buffer donde se almacenará la cadena leída.
 * @param size El tamaño máximo del buffer.
 */
void input_string(const char *msg, char *buffer, int size);

/**
 * @brief Solicita una cadena que acepta letras, numeros, espacios y caracteres +-.,:()
 */
void input_string_extended(const char *msg, char *buffer, int size);

/**
 * @brief Solicita al usuario una fecha con formato específico.
 *
 * Muestra el mensaje proporcionado y lee una cadena desde la entrada estándar,
 * validando que contenga solo dígitos, barras diagonales (/) y dos puntos (:).
 *
 * @param msg El mensaje a mostrar al usuario.
 * @param buffer El buffer donde se almacenará la cadena leída.
 * @param size El tamaño máximo del buffer.
 */
void input_date(const char *msg, char *buffer, int size);

/**
 * @brief Imprime texto en la UI (stdout).
 */
int ui_printf(const char *fmt, ...);

/**
 * @brief Imprime una línea en la UI (stdout).
 */
int ui_puts(const char *s);

/**
 * @brief Imprime un carácter en la UI (stdout).
 */
int ui_putchar(int c);

/**
 * @brief Imprime una línea centrada (stdout).
 */
int ui_printf_centered_line(const char *fmt, ...);


/**
 * @brief Obtiene la fecha y hora actual en formato legible.
 *
 * Formatea la fecha y hora actual en el formato "dd/mm/yyyy hh:mm".
 *
 * @param buffer El buffer donde se almacenará la cadena formateada.
 * @param size El tamaño máximo del buffer.
 */
void get_datetime(char *buffer, int size);

/**
 * @brief Obtiene un timestamp actual en formato compacto.
 *
 * Formatea la fecha y hora actual en el formato "yyyymmdd_hhmm" para usar en nombres de archivos.
 *
 * @param buffer El buffer donde se almacenará la cadena formateada.
 * @param size El tamaño máximo del buffer.
 */
void get_timestamp(char *buffer, int size);

/**
 * @brief Limpia la pantalla de la consola.
 */
void clear_screen();

/**
 * @brief Imprime un encabezado con el título proporcionado.
 *
 * @param titulo El título a mostrar en el encabezado.
 */
void print_header(const char *titulo);

/**
 * @brief Pausa la ejecución del programa hasta que el usuario presione una tecla.
 */
void pause_console();

/* Fuerza maximizar la consola en Windows y ajusta el buffer de pantalla */
void ensure_console_maximized_windows(void);

/**
 * @brief Verifica si existe un ID en una tabla de la base de datos.
 *
 * Ejecuta una consulta SQL para comprobar si un ID específico existe en la tabla indicada.
 *
 * @param tabla El nombre de la tabla a consultar.
 * @param id El ID a verificar.
 * @return 1 si el ID existe, 0 en caso contrario.
 */
int existe_id(const char *tabla, int id);

/**
 * @brief Solicita confirmación al usuario (Sí/No).
 *
 * Muestra el mensaje proporcionado seguido de "(S/N):" y lee la respuesta del usuario.
 * Acepta 's' o 'S' como afirmativo.
 *
 * @param msg El mensaje a mostrar al usuario.
 * @return 1 si el usuario confirma (sí), 0 en caso contrario.
 */
int confirmar(const char *msg);

/**
 * @brief Gestiona inicio de sesion/registro multiusuario local
 *
 * Debe ejecutarse antes de inicializar la base principal para definir
 * el perfil activo y enrutar el archivo de datos del usuario.
 *
 * @return 1 si se eligió un usuario válido, 0 si se cancela/sale
 */
int iniciar_sesion_multiusuario_local(void);

/**
 * @brief Pide el nombre del usuario en la primera ejecución
 */
void pedir_nombre_usuario();

/**
 * @brief Muestra el nombre actual del usuario
 */
void mostrar_nombre_usuario();

/**
 * @brief Permite editar el nombre del usuario
 */
void editar_nombre_usuario();

/**
 * @brief Solicita configurar contraseña opcional al crear usuario
 */
void configurar_password_inicial_opcional();

/**
 * @brief Solicita contraseña al iniciar si existe una configurada
 *
 * @return 1 si la autenticación fue exitosa o no hay contraseña, 0 si falla
 */
int autenticar_usuario_si_tiene_password();

/**
 * @brief Menú de gestión de usuario
 */
void menu_usuario();

/**
 * @brief Formatea una fecha para mostrar en el formato dd/mm/yyyy hh:mm
 *
 * Convierte una fecha en formato de almacenamiento a formato de visualización.
 * Actualmente ambos formatos son iguales (dd/mm/yyyy hh:mm), pero esta función
 * permite cambiar el formato de almacenamiento en el futuro sin afectar la visualización.
 *
 * @param input_date Fecha de entrada en formato de almacenamiento
 * @param output_buffer Buffer donde se almacenará la fecha formateada
 * @param buffer_size Tamaño máximo del buffer
 */
void format_date_for_display(const char *input_date, char *output_buffer, int buffer_size);

/**
 * @brief Convierte una fecha en formato de visualización a formato de almacenamiento
 *
 * Convierte una fecha en formato dd/mm/yyyy hh:mm a formato de almacenamiento.
 * Actualmente ambos formatos son iguales, pero esta función permite cambiar
 * el formato de almacenamiento en el futuro.
 *
 * @param display_date Fecha en formato de visualización (dd/mm/yyyy hh:mm)
 * @param storage_buffer Buffer donde se almacenará la fecha en formato de almacenamiento
 * @param buffer_size Tamaño máximo del buffer
 */
void convert_display_date_to_storage(const char *display_date, char *storage_buffer, int buffer_size);

/**
 * @brief Remueve tildes y caracteres acentuados de una cadena
 *
 * Convierte caracteres acentuados a su equivalente sin tilde.
 *
 * @param str La cadena a procesar
 * @return La cadena sin tildes
 */
char* remover_tildes(const char *str);

/**
 * @brief Versión segura y portátil de strnlen que limita la longitud máxima.
 *
 * @param s Cadena a medir
 * @param maxlen Máximo número de caracteres a examinar
 * @return Longitud de la cadena (máximo `maxlen`)
 */
size_t safe_strnlen(const char *s, size_t maxlen);

#if !defined(__STDC_LIB_EXT1__)
/**
 * @brief Implementación compatible de strlen_s cuando Annex K no está disponible.
 *
 * @param s Cadena a medir
 * @param maxlen Máximo número de caracteres a examinar
 * @return Longitud de la cadena (máximo `maxlen`)
 */
size_t strlen_s(const char *s, size_t maxlen);
#endif

/**
 * @brief Convierte un valor de resultado a texto
 *
 * @param resultado El valor numérico del resultado
 * @return La representación textual del resultado
 */
const char *resultado_to_text(int resultado);

/**
 * @brief Convierte un valor de clima a texto
 *
 * @param clima El valor numérico del clima
 * @return La representación textual del clima
 */
const char *clima_to_text(int clima);

/**
 * @brief Convierte un valor de día a texto
 *
 * @param dia El valor numérico del día
 * @return La representación textual del día
 */
const char *dia_to_text(int dia);

/**
 * @brief Obtiene el nombre de una entidad por su ID desde la base de datos
 *
 * Función genérica que obtiene el nombre de cualquier entidad (camiseta, torneo, etc.)
 * dada una tabla y un ID. Evita duplicación de código para consultas comunes.
 *
 * @param tabla Nombre de la tabla (ej: "camiseta", "torneo", "cancha")
 * @param id ID de la entidad a buscar
 * @param buffer Buffer donde se almacenará el nombre encontrado
 * @param size Tamaño máximo del buffer
 * @return 1 si se encontró la entidad, 0 si no se encontró
 */
int obtener_nombre_entidad(const char *tabla, int id, char *buffer, size_t size);

/**
 * @brief Lista equipos disponibles para selección
 *
 * @param no_records_msg Mensaje a mostrar si no hay equipos
 * @param pause_on_empty Pausar la consola si no hay equipos
 * @return 1 si hay equipos disponibles, 0 si no
 */
int list_available_teams(const char *no_records_msg, int pause_on_empty);

/**
 * @brief Obtiene el ID de un equipo seleccionado por el usuario
 *
 * @param prompt Mensaje para solicitar el ID
 * @param no_records_msg Mensaje si no hay equipos disponibles
 * @param pause_on_error Pausar la consola ante error de selección
 * @return ID del equipo seleccionado o 0 si se cancela o es inválido
 */
int select_team_id(const char *prompt, const char *no_records_msg, int pause_on_error);

/**
 * @brief Obtiene el siguiente ID disponible para una tabla (reutiliza espacios)
 *
 * Función genérica que calcula el siguiente ID disponible reutilizando IDs
 * eliminados. Implementa el patrón usado en camiseta, cancha, lesion, etc.
 *
 * @param tabla Nombre de la tabla (ej: "camiseta", "partido")
 * @return El ID disponible más pequeño (comenzando desde 1 si está vacía)
 */
long long obtener_siguiente_id(const char *tabla);

/**
 * @brief Verifica si hay registros en una tabla
 *
 * Función genérica para verificar si una tabla tiene al menos un registro.
 * Reemplaza múltiples funciones hay_camisetas(), hay_cancha(), etc.
 *
 * @param tabla Nombre de la tabla a verificar
 * @return 1 si hay registros, 0 si está vacía
 */
int hay_registros(const char *tabla);

/**
 * @brief Obtiene el ID de una entidad buscándola por nombre
 *
 * Función genérica para obtener un ID cuando se conoce el nombre.
 * Reemplaza obtener_camiseta_id(), etc.
 *
 * @param tabla Nombre de la tabla
 * @param nombre Nombre de la entidad a buscar
 * @return ID encontrado, o -1 si no existe
 */
int obtener_id_por_nombre(const char *tabla, const char *nombre);

/**
 * @brief Lista todas las entidades de una tabla (id, nombre)
 *
 * Función genérica para mostrar listado de entidades. Limpia pantalla,
 * muestra encabezado, lista entidades y pausa.
 *
 * @param tabla Nombre de la tabla a listar
 * @param titulo Título a mostrar en el encabezado
 * @param mensaje_vacio Mensaje si la tabla está vacía
 */
void listar_entidades(const char *tabla, const char *titulo, const char *mensaje_vacio);

/**
 * @brief Solicita un entero validado dentro de un rango
 *
 * Función que solicita al usuario un entero y valida que esté en [min, max].
 * Repite el mensaje de error mientras el valor esté fuera de rango.
 *
 * @param msg Mensaje a mostrar al usuario
 * @param min Valor mínimo permitido (inclusive)
 * @param max Valor máximo permitido (inclusive)
 * @return El entero validado dentro del rango
 */
int input_int_rango(const char *msg, int min, int max);

/**
 * @brief Muestra mensaje de "no hay registros" de manera consistente
 *
 * @param entidad Nombre de la entidad (ej: "camisetas", "canchas")
 */
void mostrar_no_hay_registros(const char *entidad);

/**
 * @brief Muestra mensaje de "entidad no existe" de manera consistente
 *
 * @param entidad Nombre de la entidad (ej: "Camiseta", "Cancha")
 */
void mostrar_no_existe(const char *entidad);

/**
 * @brief Muestra mensaje de error de manera consistente
 *
 * @param entidad Nombre de la entidad (ej: "Camiseta", "Cancha")
 * @param operacion Operación que falló (ej: "guardar", "eliminar")
 */
void mostrar_error_operacion(const char *entidad, const char *operacion);

/**
 * @brief Atajo para clear_screen + print_header
 *
 * @param titulo Título a mostrar
 */
void mostrar_pantalla(const char *titulo);

/**
 * @brief Elimina espacios en blanco al final de una cadena
 *
 * @param str Cadena a procesar
 * @return La misma cadena sin espacios finales
 */
char* trim_trailing_spaces(char *str);

/**
 * @brief Exporta un record simple a CSV
 *
 * @param titulo Título del record
 * @param sql Consulta SQL para obtener los datos
 * @param filename Nombre del archivo de salida
 */
void exportar_record_simple_csv(const char *titulo, const char *sql, const char *filename);

/**
 * @brief Muestra un record simple
 *
 * @param titulo Título del record
 * @param sql Consulta SQL para obtener los datos
 */
void mostrar_record_simple(const char *titulo, const char *sql);

/**
 * @brief Función genérica para exportar un partido específico a CSV
 * Centraliza la lógica común de exportación de partidos específicos
 */
void exportar_partido_especifico_csv(const char *order_by, const char *filename);

/**
 * @brief Función genérica para exportar un partido específico a TXT
 * Centraliza la lógica común de exportación de partidos específicos
 */
void exportar_partido_especifico_txt(const char *order_by, const char *filename, const char *title);

/**
 * @brief Escribe fila CSV con datos de partido
 * Función común para exportar datos de partidos a CSV
 */
void write_partido_csv_row(FILE *f, sqlite3_stmt *stmt);

/**
 * @brief Escribe fila TXT con datos de partido
 * Función común para exportar datos de partidos a TXT
 */
void write_partido_txt_row(FILE *f, sqlite3_stmt *stmt);

/**
 * @brief Escribe fila HTML con datos de partido
 * Función común para exportar datos de partidos a HTML
 */
void write_partido_html_row(FILE *f, sqlite3_stmt *stmt);

/**
 * @brief Escribe objeto JSON con datos de partido
 * Función común para exportar datos de partidos a JSON
 */
void write_partido_json_object(cJSON *item, sqlite3_stmt *stmt);

/**
 * @brief Estructura para almacenar estadísticas de partidos
 */
typedef struct
{
    double avg_goles;
    double avg_asistencias;
    double avg_rendimiento;
    double avg_cansancio;
    double avg_animo;
    int total_partidos;
} Estadisticas;

/**
 * @brief Reinicia una estructura de estadísticas a cero
 * @param stats Puntero a la estructura de estadísticas
 */
void reset_estadisticas(Estadisticas *stats);

/**
 * @brief Calcula estadísticas usando una consulta SQL
 * @param stats Puntero a la estructura donde almacenar las estadísticas
 * @param sql Consulta SQL para obtener las estadísticas
 */
void calcular_estadisticas(Estadisticas *stats, const char *sql);

/**
 * @brief Actualiza rachas basado en el resultado del partido
 * @param resultado Resultado del partido (1=Victoria, 2=Empate, 3=Derrota)
 * @param racha_actual_v Puntero a racha actual de victorias
 * @param max_racha_v Puntero a máxima racha de victorias
 * @param racha_actual_d Puntero a racha actual de derrotas
 * @param max_racha_d Puntero a máxima racha de derrotas
 */
void actualizar_rachas(int resultado, int *racha_actual_v, int *max_racha_v,
                       int *racha_actual_d, int *max_racha_d);

/**
 * @brief Prepara una sentencia SQL
 * @param stmt Doble puntero a la sentencia SQLite
 * @param sql Consulta SQL a preparar
 * @return 1 si la preparación fue exitosa, 0 en caso contrario
 */
int preparar_stmt_export(sqlite3_stmt **stmt, const char *sql);

/**
 * @brief Prepara una consulta verificando primero si hay registros
 * @param stmt Doble puntero a la sentencia SQLite
 * @param tabla Nombre de la tabla para verificar
 * @param mensaje Mensaje a mostrar si no hay registros
 * @param sql Consulta SQL principal a preparar
 * @param count Puntero para almacenar el número de registros encontrados
 * @return 1 si hay registros y la preparación fue exitosa, 0 en caso contrario
 */
int preparar_consulta_con_verificacion(sqlite3_stmt **stmt, const char *tabla,
                                       const char *mensaje, const char *sql, int *count);

/**
 * @brief Abre un archivo para exportación
 * @param filename Nombre del archivo a crear
 * @param error_msg Mensaje de error a mostrar si falla
 * @return Puntero al archivo abierto o NULL si falla
 */
FILE *abrir_archivo_exportacion(const char *filename, const char *error_msg);

/**
 * @brief Verifica si una tabla tiene registros
 * @param table_name Nombre de la tabla a verificar
 * @return 1 si tiene registros, 0 si no
 */
int has_records(const char *table_name);

/**
 * @brief Elimina espacios en blanco al inicio y final de una cadena
 * @param str Cadena a limpiar (se modifica in-place)
 */
void trim_whitespace(char *str);

/**
 * @brief Estructura para almacenar datos de estadísticas por año
 */
typedef struct
{
    const char *anio;
    const char *camiseta;
    int partidos;
    int total_goles;
    int total_asistencias;
    double avg_goles;
    double avg_asistencias;
} EstadisticaAnio;

/**
 * @brief Extrae datos de estadísticas por año desde un statement SQLite
 * @param stmt Statement preparado con la consulta de estadísticas por año
 * @param stats Puntero a estructura donde almacenar los datos extraídos
 * @note Asume que el statement tiene las columnas en el orden:
 *       anio, camiseta, partidos, total_goles, total_asistencias, avg_goles, avg_asistencias
 */
void extraer_estadistica_anio(sqlite3_stmt *stmt, EstadisticaAnio *stats);

int app_command_exists(const char *cmd);
int app_copy_binary_file(const char *source_path, const char *dest_path);
int app_optimize_image_file(const char *source_path, const char *dest_path);
const char *app_get_file_extension(const char *path);
int app_is_supported_image_extension(const char *ext);
int app_select_image_from_user(char *ruta_origen, size_t size, const char *temp_filename);
int app_get_file_name_from_path(const char *path, char *nombre, size_t size);
void app_build_path(char *dest, size_t size, const char *dir, const char *file_name);
int app_seleccionar_y_copiar_imagen(const char *selector_filename, const char *prefijo_base,
                                    char *ruta_relativa_db, size_t ruta_size);
int app_cargar_imagen_entidad(int id, const char *tabla, const char *selector_filename);

#endif
