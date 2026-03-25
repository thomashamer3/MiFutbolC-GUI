/**
 * @file db.h
 * @brief API de persistencia de datos relacional con SQLite3
 *
 * Define interfaz para operaciones de base de datos utilizando motor SQLite3,
 * implementando patrón Singleton para conexión global, configuración automática
 * de esquema relacional y gestión de directorios específicos del sistema operativo.
 * Soporta evolución de esquema mediante ALTER TABLE dinámicos.
 */

#ifndef DB_H
#define DB_H

#include <stddef.h>
#include "compat_port.h"
#include "sqlite3.h"

/**
 * @brief Instancia global de conexión SQLite3
 *
 * Puntero singleton que mantiene estado de conexión activa durante
 * ciclo de vida de la aplicación, utilizado por todas las operaciones
 * de consulta y modificación de datos.
 */
extern sqlite3 *db;

/**
 * @brief Define el usuario local activo para enrutar la base de datos
 *
 * Debe llamarse antes de db_init() para que la aplicación abra el archivo
 * de datos correspondiente al usuario seleccionado.
 *
 * @param username Nombre de usuario local
 * @return 1 si se asignó correctamente, 0 en caso de entrada inválida
 */
int db_set_active_user(const char *username);

/**
 * @brief Obtiene el usuario local activo
 *
 * @return Nombre de usuario activo o cadena vacía si no se definió
 */
const char *db_get_active_user(void);

/**
 * @brief Crea un backup automático de la base de datos
 *
 * Esta función genera una copia con timestamp en la carpeta de exportaciones
 * para prevenir pérdida de datos antes de importaciones.
 *
 * @param motivo Texto opcional para identificar el contexto del backup.
 * @return 1 si se creó el backup, 0 si falló.
 */
int backup_base_datos_automatico(const char *motivo);

/**
 * @brief Inicializa infraestructura completa de persistencia
 *
 * Ejecuta secuencia de configuración: rutas del SO, conexión SQLite,
 * creación de esquema relacional completo, adición de columnas faltantes
 * y preparación de directorios auxiliares para import/export.
 *
 * @return 1 si configuración completa exitosa, 0 en caso de error crítico
 */
int db_init();

/**
 * @brief Finaliza conexión y libera recursos del motor SQLite
 *
 * Cierra handle de base de datos de manera ordenada, asegurando
 * commit de transacciones pendientes y liberación de memoria.
 */
void db_close();

/**
 * @brief Recupera configuración de usuario desde tabla relacional
 *
 * Ejecuta consulta SELECT preparada sobre tabla 'usuario' con
 * límite de resultado único, retornando configuración personalizada.
 *
 * @return Puntero dinámico a string con nombre de usuario, NULL si no existe
 */
char* get_user_name();

/**
 * @brief Persiste configuración de usuario en base de datos
 *
 * Utiliza INSERT OR REPLACE para upsert de configuración personal,
 * permitiendo actualización atómica de preferencias del usuario.
 *
 * @param nombre String con identificador de usuario a almacenar
 * @return 1 si operación de escritura exitosa, 0 en caso de error de BD
 */
int set_user_name(const char* nombre);

/**
 * @brief Verifica si el usuario tiene contraseña configurada
 *
 * @return 1 si existe hash de contraseña, 0 si no existe
 */
int user_has_password(void);

/**
 * @brief Configura o actualiza la contraseña del usuario
 *
 * La contraseña se almacena como hash con salt en la base de datos.
 *
 * @param plain_password Contraseña en texto plano
 * @return 1 si la operación fue exitosa, 0 en caso de error
 */
int set_user_password(const char *plain_password);

/**
 * @brief Verifica una contraseña contra el hash almacenado
 *
 * @param plain_password Contraseña ingresada en texto plano
 * @return 1 si coincide, 0 si no coincide o no existe contraseña
 */
int verify_user_password(const char *plain_password);

/**
 * @brief Elimina la contraseña del usuario
 *
 * @return 1 si la operación fue exitosa, 0 en caso de error
 */
int clear_user_password(void);

/**
 * @brief Proporciona ruta absoluta al directorio de datos internos
 *
 * Retorna ubicación del sistema de archivos donde se almacenan
 * archivos persistentes de la aplicación (base de datos, configuración).
 *
 * @return Puntero constante a string con path del directorio de datos
 */
const char* get_data_dir();

/**
 * @brief Determina ubicación canónica para archivos de exportación
 *
 * Configura directorio visible al usuario (Documents en Windows)
 * para almacenamiento de datos exportados en formatos externos,
 * facilitando backup y portabilidad de información.
 *
 * @return Puntero constante a string con path del directorio de exportaciones
 */
const char* get_export_dir();

/**
 * @brief Determina ubicación canónica para archivos de importación
 *
 * Configura directorio accesible al usuario para colocación de
 * archivos fuente de datos, permitiendo carga masiva desde
 * formatos externos mediante operaciones de importación.
 *
 * @return Puntero constante a string con path del directorio de importaciones
 */
const char* get_import_dir();

/**
 * @brief Determina ubicacion canónica para archivos de imagen
 *
 * Configura directorio accesible al usuario para almacenar imagenes
 * procesadas por la aplicacion, al mismo nivel que Exportaciones e
 * Importaciones.
 *
 * @return Puntero constante a string con path del directorio de imagenes
 */
const char* get_images_dir();

/**
 * @brief Obtiene la ruta de imagen almacenada para una entidad por ID
 *
 * Consulta la columna `imagen_ruta` en la tabla indicada y copia
 * el valor en el buffer de salida.
 *
 * @param table_name Nombre de tabla (solo caracteres alfanumericos y '_')
 * @param id ID de registro a consultar
 * @param ruta Buffer de salida
 * @param size Tamano del buffer de salida
 * @return 1 si encontro una ruta valida, 0 en caso contrario
 */
int db_get_image_path_by_id(const char *table_name, int id, char *ruta, size_t size);

/**
 * @brief Resuelve ruta absoluta de imagen y valida que exista en disco
 *
 * Toma una ruta almacenada en DB (por ejemplo `Imagenes/archivo.jpg`),
 * extrae el nombre de archivo, construye la ruta absoluta usando el
 * directorio de imagenes de la aplicacion y verifica que el archivo exista.
 *
 * @param ruta_db Ruta almacenada en la base de datos
 * @param ruta_absoluta Buffer de salida para ruta absoluta
 * @param size Tamano del buffer de salida
 * @return 1 si la ruta fue resuelta y el archivo existe, 0 en caso contrario
 */
int db_resolve_image_absolute_path(const char *ruta_db, char *ruta_absoluta, size_t size);

/**
 * @brief Registra un evento informativo en el archivo de log de la aplicación
 *
 * Permite registrar acciones de usuario y eventos de flujo funcional
 * desde cualquier módulo de la aplicación.
 *
 * @param component Componente o módulo emisor (ej: "MENU", "CAMISETA")
 * @param message Mensaje del evento
 */
void app_log_event(const char *component, const char *message);

/**
 * @brief Copia la base de datos SQLite a la carpeta de documentos
 *
 * Esta función realiza una copia exacta del archivo de base de datos
 * desde la ubicación de datos internos a la carpeta de exportación
 * (normalmente Documentos del usuario).
 */
void exportar_base_datos();

/**
 * @brief Importa una base de datos SQLite desde un archivo externo
 *
 * Esta función permite al usuario importar una base de datos completa
 * desde un archivo externo, reemplazando la base de datos actual.
 * Se realiza una copia exacta del archivo de origen al directorio de datos.
 */
void importar_base_datos();

#endif
