/**
 * @file temporada.h
 * @brief Sistema de Temporadas y Ciclos Deportivos para MiFutbolC
 *
 * Este módulo implementa un sistema completo de gestión de temporadas deportivas,
 * incluyendo fases de temporada, fatiga acumulada, evolución de rendimiento,
 * y resúmenes automáticos anuales.
 */

#ifndef TEMPORADA_H
#define TEMPORADA_H

#include <time.h>

// ========== ESTRUCTURAS DE DATOS ==========

/**
 * @brief Estructura que representa una temporada deportiva
 */
typedef struct
{
    int id;
    char nombre[100];
    int anio;
    char fecha_inicio[20];  // Formato YYYY-MM-DD
    char fecha_fin[20];     // Formato YYYY-MM-DD
    char estado[20];        // Planificada, Activa, Finalizada
    char descripcion[500];
} Temporada;

/**
 * @brief Fases dentro de una temporada
 */
typedef enum
{
    PRETEMPORADA,
    TEMPORADA_REGULAR,
    POSTTEMPORADA
} TipoFaseTemporada;

/**
 * @brief Estructura que representa una fase de temporada
 */
typedef struct
{
    int id;
    int temporada_id;
    char nombre[100];
    TipoFaseTemporada tipo_fase;
    char fecha_inicio[20];
    char fecha_fin[20];
    char descripcion[500];
} TemporadaFase;

/**
 * @brief Estructura para seguimiento de fatiga de equipos
 */
typedef struct
{
    int id;
    int equipo_id;
    int temporada_id;
    char fecha[20];
    float fatiga_acumulada;     // 0.0 a 100.0
    int partidos_jugados;
    float rendimiento_promedio; // 0.0 a 10.0
} EquipoTemporadaFatiga;

/**
 * @brief Estructura para seguimiento de fatiga de jugadores
 */
typedef struct
{
    int id;
    int jugador_id;
    int temporada_id;
    char fecha[20];
    float fatiga_acumulada;     // 0.0 a 100.0
    int minutos_jugados_total;
    float rendimiento_promedio; // 0.0 a 10.0
    int lesiones_acumuladas;
} JugadorTemporadaFatiga;

/**
 * @brief Estructura para evolución de rendimiento de equipos
 */
typedef struct
{
    int id;
    int equipo_id;
    int temporada_id;
    char fecha_medicion[20];
    float puntuacion_rendimiento; // 0.0 a 100.0
    char tendencia[20];          // Mejorando, Estable, Decayendo
    int partidos_ganados;
    int partidos_totales;
} EquipoTemporadaEvolucion;

/**
 * @brief Estructura para resumen automático de temporada
 */
typedef struct
{
    int id;
    int temporada_id;
    int total_partidos;
    int total_goles;
    float promedio_goles_partido;
    int equipo_campeon_id;
    int mejor_goleador_jugador_id;
    int mejor_goleador_goles;
    int total_lesiones;
    char fecha_generacion[20];
} TemporadaResumen;

/**
 * @brief Estructura para resumen mensual dentro de temporada
 */
typedef struct
{
    int id;
    int temporada_id;
    char mes_anio[8];  // Formato YYYY-MM
    int total_partidos;
    int total_goles;
    float promedio_goles_partido;
    int partidos_ganados;
    int partidos_empatados;
    int partidos_perdidos;
    int total_lesiones;
    float total_gastos;
    float total_ingresos;
    int mejor_equipo_mes;
    int peor_equipo_mes;
    char fecha_generacion[20];
} MensualResumen;

// ========== FUNCIONES PRINCIPALES ==========

/**
 * @brief Crea una nueva temporada
 */
void crear_temporada();

/**
 * @brief Lista todas las temporadas
 */
void listar_temporadas();

/**
 * @brief Modifica una temporada existente
 */
void modificar_temporada();

/**
 * @brief Elimina una temporada
 */
void eliminar_temporada();

/**
 * @brief Administra una temporada específica
 */
void administrar_temporada();

/**
 * @brief Crea las fases por defecto para una temporada
 * @param temporada_id ID de la temporada
 */
void crear_fases_temporada_defecto(int temporada_id);

/**
 * @brief Asocia un torneo a una temporada
 * @param torneo_id ID del torneo
 */
void asociar_torneo_temporada(int torneo_id);

/**
 * @brief Calcula y actualiza la fatiga de un equipo
 * @param equipo_id ID del equipo
 * @param temporada_id ID de la temporada
 * @param partido_id ID del partido jugado
 */
void actualizar_fatiga_equipo(int equipo_id, int temporada_id, int partido_id);

/**
 * @brief Calcula y actualiza la fatiga de un jugador
 * @param jugador_id ID del jugador
 * @param temporada_id ID de la temporada
 * @param minutos_jugados Minutos jugados en el partido
 * @param lesion_ocurrida Si ocurrió una lesión
 */
void actualizar_fatiga_jugador(int jugador_id, int temporada_id, int minutos_jugados, int lesion_ocurrida);

/**
 * @brief Actualiza la evolución de rendimiento de un equipo
 * @param equipo_id ID del equipo
 * @param temporada_id ID de la temporada
 */
void actualizar_evolucion_equipo(int equipo_id, int temporada_id);

/**
 * @brief Genera resumen automático de temporada
 * @param temporada_id ID de la temporada
 */
void generar_resumen_temporada(int temporada_id);

/**
 * @brief Compara dos temporadas
 * @param temporada1_id ID de la primera temporada
 * @param temporada2_id ID de la segunda temporada
 */
void comparar_temporadas(int temporada1_id, int temporada2_id);

/**
 * @brief Muestra dashboard de temporada
 * @param temporada_id ID de la temporada
 */
void mostrar_dashboard_temporada(int temporada_id);

/**
 * @brief Muestra estadísticas de fatiga por temporada
 * @param temporada_id ID de la temporada
 */
void mostrar_estadisticas_fatiga(int temporada_id);

/**
 * @brief Exporta resumen de temporada a archivo
 * @param temporada_id ID de la temporada
 */
void exportar_resumen_temporada(int temporada_id);

/**
 * @brief Genera resumen mensual automático
 * @param temporada_id ID de la temporada
 * @param mes_anio Mes a resumir (formato YYYY-MM)
 */
void generar_resumen_mensual(int temporada_id, const char* mes_anio);

/**
 * @brief Muestra resumen mensual
 * @param temporada_id ID de la temporada
 * @param mes_anio Mes a mostrar (formato YYYY-MM)
 */
void mostrar_resumen_mensual(int temporada_id, const char* mes_anio);

/**
 * @brief Lista todos los resúmenes mensuales de una temporada
 * @param temporada_id ID de la temporada
 */
void listar_resumenes_mensuales(int temporada_id);

/**
 * @brief Exporta resumen mensual a archivo
 * @param temporada_id ID de la temporada
 * @param mes_anio Mes a exportar (formato YYYY-MM)
 */
void exportar_resumen_mensual(int temporada_id, const char* mes_anio);

/**
 * @brief Permite seleccionar y comparar dos temporadas
 */
void seleccionar_comparar_temporadas();

// ========== FUNCIONES AUXILIARES ==========

/**
 * @brief Convierte enum TipoFaseTemporada a string
 * @param tipo Tipo de fase
 * @return String representativo
 */
const char* get_nombre_tipo_fase(TipoFaseTemporada tipo);

/**
 * @brief Calcula la fatiga basada en partidos recientes
 * @param equipo_id ID del equipo
 * @param dias_recientes Número de días a considerar
 * @return Nivel de fatiga (0.0-100.0)
 */
float calcular_fatiga_equipo(int equipo_id, int dias_recientes);

/**
 * @brief Calcula la fatiga de un jugador
 * @param jugador_id ID del jugador
 * @param dias_recientes Número de días a considerar
 * @return Nivel de fatiga (0.0-100.0)
 */
float calcular_fatiga_jugador(int jugador_id, int dias_recientes);

/**
 * @brief Obtiene el ID de la temporada actual
 * @return ID de la temporada actual o -1 si no hay
 */
int get_temporada_actual_id();

/**
 * @brief Verifica si una fecha está dentro de una temporada
 * @param fecha Fecha a verificar (YYYY-MM-DD)
 * @param temporada_id ID de la temporada
 * @return 1 si está dentro, 0 si no
 */
int fecha_en_temporada(const char* fecha, int temporada_id);

/**
 * @brief Obtiene la fase actual de la temporada para una fecha
 * @param temporada_id ID de la temporada
 * @param fecha Fecha actual (YYYY-MM-DD)
 * @return ID de la fase o -1 si no hay
 */
int get_fase_temporada_actual(int temporada_id, const char* fecha);

// ========== MENÚ PRINCIPAL ==========

/**
 * @brief Muestra el menú principal de gestión de temporadas
 */
void menu_temporadas();

#endif // TEMPORADA_H
