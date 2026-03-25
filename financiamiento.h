/**
 * @file financiamiento.h
 * @brief API de gestión financiera para el equipo de fútbol
 *
 * Define interfaz para operaciones CRUD sobre transacciones financieras,
 * incluyendo ingresos y gastos del equipo, con categorías específicas
 * para transporte, equipamiento, cuotas y otros gastos relacionados.
 */

#ifndef FINANCIAMIENTO_H
#define FINANCIAMIENTO_H

#include <time.h>

/**
 * @brief Tipos de transacciones financieras disponibles
 */
typedef enum
{
    INGRESO,    /**< Ingreso al equipo (cuotas, sponsors, etc.) */
    GASTO       /**< Gasto del equipo (transporte, equipamiento, etc.) */
} TipoTransaccion;

/**
 * @brief Categorías específicas para gastos e ingresos
 */
typedef enum
{
    TRANSPORTE,         /**< Transporte del equipo (combis, buses, etc.) */
    EQUIPAMIENTO,       /**< Equipamiento deportivo (botines, remeras, etc.) */
    CUOTAS,            /**< Cuotas de socios/jugadores */
    TORNEOS,           /**< Inscripciones y pagos de torneos */
    ARBITRAJE,         /**< Pagos de árbitros */
    CANCHAS,           /**< Alquiler de canchas */
    MEDICINA,          /**< Gastos médicos y lesiones */
    OTROS              /**< Otras categorías */
} CategoriaFinanciera;

/**
 * @brief Estructura que representa una transacción financiera
 */
typedef struct
{
    int id;                             /**< Identificador único de la transacción */
    char fecha[11];                     /**< Fecha en formato YYYY-MM-DD */
    TipoTransaccion tipo;               /**< Tipo de transacción (ingreso/gasto) */
    CategoriaFinanciera categoria;      /**< Categoría específica */
    char descripcion[200];              /**< Descripción detallada de la transacción */
    int monto;                       /**< Monto de la transacción */
    char item_especifico[256];          /**< Item específico (ej: "Botines Nike", "Cuota enero") */
} TransaccionFinanciera;

/**
 * @brief Estructura que representa un presupuesto mensual
 */
typedef struct
{
    int id;                             /**< Identificador único del presupuesto */
    char mes_anio[8];                   /**< Mes y año en formato YYYY-MM */
    int presupuesto_total;              /**< Presupuesto total mensual */
    int limite_gasto;                   /**< Límite máximo de gasto mensual */
    int alertas_habilitadas;            /**< Si las alertas están habilitadas (1) o no (0) */
    char fecha_creacion[11];            /**< Fecha de creación del presupuesto */
    char fecha_modificacion[11];        /**< Fecha de última modificación */
} PresupuestoMensual;

/**
 * @brief Interfaz de menú para operaciones CRUD de financiamiento
 */
void menu_financiamiento();

/**
 * @brief Agregar una nueva transacción financiera
 */
void agregar_transaccion();

/**
 * @brief Listar todas las transacciones financieras
 */
void listar_transacciones();

/**
 * @brief Modificar una transacción financiera existente
 */
void modificar_transaccion();

/**
 * @brief Eliminar una transacción financiera
 */
void eliminar_transaccion();

/**
 * @brief Mostrar resumen financiero del equipo
 */
void mostrar_resumen_financiero();

/**
 * @brief Mostrar balance general de gastos
 */
void ver_balance_gastos();

/**
 * @brief Exportar transacciones financieras a archivo
 */
void exportar_financiamiento();

/**
 * @brief Convierte un tipo de transacción a su nombre textual
 */
const char* get_nombre_tipo_transaccion(TipoTransaccion tipo);

/**
 * @brief Convierte una categoría financiera a su nombre textual
 */
const char* get_nombre_categoria(CategoriaFinanciera categoria);

/**
 * @brief Mostrar una transacción financiera
 */
void mostrar_transaccion(TransaccionFinanciera *transaccion);

/**
 * @brief Gestión de presupuestos mensuales - menú principal
 */
void menu_presupuestos_mensuales();

/**
 * @brief Configurar presupuesto mensual
 */
void configurar_presupuesto_mensual();

/**
 * @brief Ver estado actual del presupuesto mensual
 */
void ver_estado_presupuesto();

/**
 * @brief Verificar límites de gasto y mostrar alertas
 */
void verificar_alertas_presupuesto();

/**
 * @brief Obtener gastos totales del mes actual
 */
int obtener_gastos_mes_actual();

/**
 * @brief Obtener presupuesto y límite del mes actual
 */
int obtener_presupuesto_mes_actual(PresupuestoMensual *presupuesto);

/**
 * @brief Mostrar alertas cuando se exceden límites
 */
void mostrar_alerta_exceso_gasto(int gastos_actuales, int limite);

#endif
