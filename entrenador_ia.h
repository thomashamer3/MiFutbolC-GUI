/**
 * @file entrenador_ia.h
 * @brief Sistema de Inteligencia Artificial para asistencia técnica deportiva
 *
 * Implementa un entrenador virtual basado en IA que analiza el estado del jugador
 * y genera recomendaciones personalizadas sobre descanso, entrenamiento y gestión
 * de fatiga. Utiliza análisis de datos históricos para optimizar el rendimiento.
 */

#ifndef ENTRENADOR_IA_H
#define ENTRENADOR_IA_H

#include <time.h>

/**
 * @brief Estructura que representa el estado actual del jugador
 *
 * Contiene métricas clave para evaluar la condición física y mental del jugador,
 * permitiendo al sistema IA generar recomendaciones apropiadas.
 */
typedef struct
{
    float rendimiento_promedio;     /**< Rendimiento promedio reciente (0.0-10.0) */
    float cansancio_promedio;       /**< Nivel promedio de cansancio (0.0-100.0) */
    float estado_animo_promedio;    /**< Estado de ánimo promedio (0.0-10.0) */
    int partidos_consecutivos;      /**< Número de partidos jugados consecutivamente */
    float riesgo_lesion;            /**< Nivel de riesgo de lesión calculado (0.0-100.0) */
    int derrotas_consecutivas;      /**< Número de derrotas consecutivas */
    int dias_descanso;              /**< Días desde el último partido */
} EstadoJugador;

/**
 * @brief Niveles de importancia de los consejos generados
 *
 * Clasifica los consejos según su urgencia e importancia para el jugador.
 */
typedef enum
{
    CONSEJO_INFO,           /**< Información general, no urgente */
    CONSEJO_ADVERTENCIA,    /**< Advertencia que requiere atención */
    CONSEJO_CRITICO         /**< Crítico, requiere acción inmediata */
} NivelConsejo;

/**
 * @brief Categorías temáticas de los consejos
 *
 * Agrupa los consejos según el aspecto del rendimiento que abordan.
 */
typedef enum
{
    CATEGORIA_FISICO,       /**< Aspectos físicos y de condición */
    CATEGORIA_MENTAL,       /**< Aspectos psicológicos y motivacionales */
    CATEGORIA_DEPORTIVO,    /**< Aspectos técnicos y tácticos */
    CATEGORIA_SALUD,        /**< Salud y prevención de lesiones */
    CATEGORIA_GESTION       /**< Gestión de recursos y planificación */
} CategoriaConsejo;

/**
 * @brief Estructura que representa un consejo generado por la IA
 *
 * Contiene el mensaje del consejo junto con su clasificación por nivel y categoría.
 */
typedef struct
{
    char *mensaje;              /**< Texto del consejo generado */
    NivelConsejo nivel;         /**< Nivel de importancia del consejo */
    CategoriaConsejo categoria; /**< Categoría temática del consejo */
} Consejo;

/**
 * @brief Registro histórico de un consejo dado
 *
 * Permite rastrear qué consejos se dieron y si fueron seguidos por el usuario.
 */
typedef struct
{
    time_t fecha;       /**< Fecha y hora en que se dio el consejo */
    char *consejo;      /**< Texto del consejo dado */
    int seguido;        /**< 1 si el usuario siguió el consejo, 0 si no */
} HistorialConsejo;

/**
 * @brief Perfil de comportamiento del usuario
 *
 * Analiza cómo el usuario responde a los consejos para personalizar futuras recomendaciones.
 */
typedef struct
{
    int consejos_aceptados;     /**< Número de consejos que el usuario siguió */
    int consejos_ignorados;     /**< Número de consejos que el usuario ignoró */
    float indice_prudencia;     /**< Índice de prudencia del usuario (0.0-1.0) */
} PerfilUsuarioIA;

/**
 * @brief Evalúa el estado actual del jugador basándose en datos históricos
 *
 * Analiza partidos recientes, lesiones, cansancio y otros factores para
 * determinar el estado actual del jugador.
 *
 * @return Estructura EstadoJugador con las métricas calculadas
 */
EstadoJugador evaluar_estado_jugador();

/**
 * @brief Genera consejos personalizados basados en el estado del jugador
 *
 * Utiliza algoritmos de IA para generar recomendaciones apropiadas según
 * el estado actual del jugador.
 *
 * @param estado Estado actual del jugador a analizar
 * @param consejos Puntero a array de consejos (será asignado dinámicamente)
 * @param num_consejos Puntero donde se almacenará el número de consejos generados
 */
void generar_consejos(EstadoJugador estado, Consejo **consejos, int *num_consejos);

/**
 * @brief Muestra los consejos actuales generados por la IA
 *
 * Presenta al usuario los consejos más recientes de forma organizada por categoría.
 */
void mostrar_consejos_actuales();

/**
 * @brief Muestra el historial completo de consejos dados
 *
 * Presenta un registro de todos los consejos dados anteriormente y si fueron seguidos.
 */
void mostrar_historial_consejos();

/**
 * @brief Permite al usuario evaluar si siguió un consejo pasado
 *
 * Actualiza el historial de consejos con la retroalimentación del usuario.
 */
void evaluar_decision_pasada();

/**
 * @brief Configura el nivel de intervención de la IA
 *
 * Permite al usuario ajustar qué tan frecuentemente y con qué intensidad
 * la IA debe intervenir con consejos.
 */
void configurar_nivel_intervencion();

/**
 * @brief Guarda un consejo en el historial
 *
 * Registra un consejo dado y si fue seguido por el usuario.
 *
 * @param consejo Texto del consejo a guardar
 * @param seguido 1 si fue seguido, 0 si no
 */
void guardar_consejo_historial(const char *consejo, int seguido);

/**
 * @brief Obtiene el perfil de comportamiento del usuario
 *
 * Recupera estadísticas sobre cómo el usuario responde a los consejos.
 *
 * @return Estructura PerfilUsuarioIA con las métricas del usuario
 */
PerfilUsuarioIA obtener_perfil_usuario();

/**
 * @brief Actualiza el perfil del usuario según si siguió un consejo
 *
 * Modifica las estadísticas del perfil basándose en la acción del usuario.
 *
 * @param consejo_seguido 1 si el consejo fue seguido, 0 si no
 */
void actualizar_perfil_usuario(int consejo_seguido);

/**
 * @brief Convierte un nivel de consejo a su representación textual
 *
 * @param nivel Nivel del consejo a convertir
 * @return Cadena de texto representando el nivel
 */
const char *nivel_a_string(NivelConsejo nivel);

/**
 * @brief Convierte una categoría de consejo a su representación textual
 *
 * @param categoria Categoría del consejo a convertir
 * @return Cadena de texto representando la categoría
 */
const char *categoria_a_string(CategoriaConsejo categoria);

/**
 * @brief Activa la IA antes de un partido
 *
 * Genera consejos específicos para la preparación pre-partido.
 */
void activar_ia_antes_partido();

/**
 * @brief Activa la IA antes de un torneo
 *
 * Genera consejos estratégicos para la planificación de torneos.
 */
void activar_ia_antes_torneo();

/**
 * @brief Activa la IA durante la visualización de estadísticas
 *
 * Genera insights y recomendaciones basadas en las estadísticas mostradas.
 */
void activar_ia_estadisticas();

/**
 * @brief Muestra el menú principal del entrenador IA
 *
 * Presenta opciones para interactuar con el sistema de IA, ver consejos,
 * historial y configurar preferencias.
 */
void menu_entrenador_ia();

#endif
