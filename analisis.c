/**
 * @file analisis.c
 * @brief Modulo para el analisis de rendimiento en partidos de futbol.
 *
 * Este archivo contiene funciones para analizar el rendimiento comparando
 * los ultimos 5 partidos con promedios generales, y calculando rachas.
 */

#include "analisis.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "settings.h"
#include "entrenador_ia.h"
#include "partido.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int listar_y_seleccionar_dos_entidades(const char *tabla, const char *titulo, int *id1, int *id2);


static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

static int preparar_stmt_con_mensaje(sqlite3_stmt **stmt, const char *sql)
{
    if (preparar_stmt(stmt, sql))
    {
        return 1;
    }

    printf("Error al consultar la base de datos.\n");
    return 0;
}

static void iniciar_pantalla_analisis(const char *titulo)
{
    clear_screen();
    print_header(titulo);
}

static int normalizar_no_negativo(int valor)
{
    return (valor < 0) ? 0 : valor;
}

static int migrar_columna_duplicable(const char *sql_alter)
{
    char *err = NULL;
    int rc = sqlite3_exec(db, sql_alter, NULL, NULL, &err);

    if (rc != SQLITE_OK && (!err || strstr(err, "duplicate column name") == NULL))
    {
        printf("Error al actualizar tabla de quimica: %s\n", err ? err : "desconocido");
        sqlite3_free(err);
        return 0;
    }

    if (err)
        sqlite3_free(err);

    return 1;
}

static int existe_id_entidad(const char *tabla, int id)
{
    sqlite3_stmt *stmt;
    char sql[128];
    snprintf(sql, sizeof(sql), "SELECT 1 FROM %s WHERE id = ? LIMIT 1", tabla);

    if (!preparar_stmt(&stmt, sql))
        return 0;

    sqlite3_bind_int(stmt, 1, id);
    int existe = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return existe;
}

static void solicitar_fecha_yyyy_mm_dd(const char *prompt, char *buffer, int size)
{
    input_date(prompt, buffer, size);
}

/**
 * @brief Calcula estadisticas generales de todos los partidos
 *
 * Establece linea base de rendimiento historico para comparaciones.
 *
 * @param stats Puntero a la estructura donde almacenar las estadisticas
 */
static void calcular_estadisticas_generales(Estadisticas *stats)
{
    calcular_estadisticas(stats,
                          "SELECT COUNT(*), AVG(goles), AVG(asistencias), AVG(rendimiento_general), AVG(cansancio), AVG(estado_animo) "
                          "FROM partido");
}

/**
 * @brief Calcula estadisticas de los ultimos 5 partidos
 *
 * @param stats Puntero a la estructura donde almacenar las estadisticas
 */
static void calcular_estadisticas_ultimos5(Estadisticas *stats)
{
    calcular_estadisticas(stats,
                          "SELECT COUNT(*), AVG(goles), AVG(asistencias), AVG(rendimiento_general), AVG(cansancio), AVG(estado_animo) "
                          "FROM (SELECT * FROM partido ORDER BY fecha_hora DESC LIMIT 5)");
}

/**
 * @brief Calcula la racha mas larga de victorias y derrotas
 *
 * @param mejor_racha_victorias Puntero donde almacenar la mejor racha de victorias
 * @param peor_racha_derrotas Puntero donde almacenar la peor racha de derrotas
 */
static void calcular_rachas(int *mejor_racha_victorias, int *peor_racha_derrotas)
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
                       "SELECT resultado FROM partido ORDER BY fecha_hora",
                       -1, &stmt, NULL);

    int racha_actual_v = 0;
    int max_racha_v = 0;
    int racha_actual_d = 0;
    int max_racha_d = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int resultado = sqlite3_column_int(stmt, 0);
        actualizar_rachas(resultado, &racha_actual_v, &max_racha_v,
                          &racha_actual_d, &max_racha_d);
    }

    *mejor_racha_victorias = max_racha_v;
    *peor_racha_derrotas = max_racha_d;
    sqlite3_finalize(stmt);
}

/**
 * @brief Muestra los ultimos 5 partidos
 *
 * Facilita la visualizacion rapida del rendimiento reciente para contextualizar
 * las estadisticas comparativas.
 */
static void mostrar_ultimos5_partidos()
{
    printf("\nULTIMOS 5 PARTIDOS:\n");
    printf("----------------------------------------\n");

    sqlite3_stmt *stmt;
    if (!preparar_stmt(&stmt,
                       "SELECT id, fecha_hora, goles, asistencias, rendimiento_general, resultado "
                       "FROM partido ORDER BY id DESC LIMIT 5"))
    {
        return;
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 1);
        int goles = sqlite3_column_int(stmt, 2);
        int asistencias = sqlite3_column_int(stmt, 3);
        int rendimiento = sqlite3_column_int(stmt, 4);
        int resultado = sqlite3_column_int(stmt, 5);

        printf("%d | %s | G:%d A:%d | Rend:%d | %s\n",
               id, fecha, goles, asistencias, rendimiento, resultado_to_text(resultado));
        count++;
    }

    if (count == 0)
    {
        printf("No hay partidos registrados.\n");
    }

    sqlite3_finalize(stmt);
}

/**
 * @brief Muestra comparacion de estadisticas ultimos 5 vs general
 *
 * Permite identificar tendencias recientes respecto al rendimiento historico
 * para tomar decisiones de mejora.
 *
 * @param ultimos Estadisticas de ultimos 5 partidos
 * @param generales Estadisticas generales
 */
static void mostrar_comparacion_estadisticas(const Estadisticas *ultimos, const Estadisticas *generales)
{
    printf("\nCOMPARACION ULTIMOS 5 VS PROMEDIO GENERAL:\n");
    printf("----------------------------------------\n");
    printf("Goles:        %.1f vs %.1f\n", ultimos->avg_goles, generales->avg_goles);
    printf("Asistencias:  %.1f vs %.1f\n", ultimos->avg_asistencias, generales->avg_asistencias);
    printf("Rendimiento:  %.1f vs %.1f\n", ultimos->avg_rendimiento, generales->avg_rendimiento);
    printf("Cansancio:    %.1f vs %.1f\n", ultimos->avg_cansancio, generales->avg_cansancio);
    printf("Estado Animo: %.1f vs %.1f\n", ultimos->avg_animo, generales->avg_animo);
}

/**
 * @brief Muestra rachas de victorias y derrotas
 *
 * Ayuda a entender patrones de consistencia en el rendimiento competitivo.
 *
 * @param mejor_racha_v Mejor racha de victorias
 * @param peor_racha_d Peor racha de derrotas
 */
static void mostrar_rachas(int mejor_racha_v, int peor_racha_d)
{
    printf("\nRACHAS:\n");
    printf("----------------------------------------\n");
    printf("Mejor racha de victorias: %d partidos\n", mejor_racha_v);
    printf("Peor racha de derrotas: %d partidos\n", peor_racha_d);
}

/**
 * @brief Genera un mensaje motivacional basado en el rendimiento
 *
 * Proporciona retroalimentacion psicologica para mantener la motivacion
 * y enfoque en el desarrollo deportivo.
 *
 * @param ultimos Puntero a estadisticas de ultimos 5 partidos
 * @param generales Puntero a estadisticas generales
 */
static void mensaje_motivacional(const Estadisticas *ultimos, const Estadisticas *generales)
{
    printf("\nANALISIS MOTIVACIONAL:\n");
    printf("----------------------------------------\n");

    double diff_goles = ultimos->avg_goles - generales->avg_goles;
    double diff_rendimiento = ultimos->avg_rendimiento - generales->avg_rendimiento;

    if (diff_goles > 0.5 && diff_rendimiento > 0.5)
    {
        printf("Excelente Estas en racha ascendente. Sigue asi, tu esfuerzo esta dando frutos.\n");
        printf("Manten la consistencia y continua trabajando duro en los entrenamientos.\n");
    }
    else if (diff_goles < -0.5 || diff_rendimiento < -0.5)
    {
        printf("No te desanimes. Todos tenemos dias dificiles. Analiza que puedes mejorar:\n");
        printf("- Revisa tu preparacion fisica y tecnica.\n");
        printf("- Habla con tu entrenador sobre estrategias.\n");
        printf("- Recuerda: el futbol es un deporte de perseverancia.\n");
    }
    else
    {
        printf("Buen trabajo manteniendo el nivel. La consistencia es clave en el futbol.\n");
        printf("Sigue entrenando y manten la motivacion alta. Cada partido es una oportunidad!\n");
    }
}

/**
 * @brief Muestra el analisis basico de rendimiento
 */
static void mostrar_analisis_basico()
{
    iniciar_pantalla_analisis("ANALISIS DE RENDIMIENTO");

    Estadisticas generales = {0};
    Estadisticas ultimos5 = {0};
    int mejor_racha_v;
    int peor_racha_d;

    calcular_estadisticas_generales(&generales);
    calcular_estadisticas_ultimos5(&ultimos5);
    calcular_rachas(&mejor_racha_v, &peor_racha_d);

    if (generales.total_partidos == 0)
    {
        printf("No hay suficientes datos para realizar el analisis.\n");
        printf("Registra al menos algunos partidos para ver estadisticas.\n");
        pause_console();
        return;
    }

    mostrar_ultimos5_partidos();
    mostrar_comparacion_estadisticas(&ultimos5, &generales);
    mostrar_rachas(mejor_racha_v, peor_racha_d);
    mensaje_motivacional(&ultimos5, &generales);

    pause_console();
}

static int asegurar_tabla_quimica_jugador_estadistica(void)
{
    const char *sql =
        "CREATE TABLE IF NOT EXISTS quimica_jugador_estadistica ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " partido_id INTEGER NOT NULL,"
        " jugador TEXT NOT NULL,"
        " companero_asistido TEXT DEFAULT '',"
        " posicion TEXT DEFAULT '',"
        " goles INTEGER DEFAULT 0,"
        " asistencias INTEGER DEFAULT 0,"
        " asistencias_al_usuario INTEGER DEFAULT 0,"
        " asistencias_del_usuario INTEGER DEFAULT 0,"
        " comentario TEXT DEFAULT '',"
        " FOREIGN KEY(partido_id) REFERENCES partido(id));";

    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK)
    {
        printf("Error al preparar tabla de quimica: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    /* Migracion simple para bases ya existentes */
    if (!migrar_columna_duplicable("ALTER TABLE quimica_jugador_estadistica ADD COLUMN companero_asistido TEXT DEFAULT ''"))
        return 0;

    if (!migrar_columna_duplicable("ALTER TABLE quimica_jugador_estadistica ADD COLUMN asistencias_del_usuario INTEGER DEFAULT 0"))
        return 0;

    return 1;
}

static void listar_partidos_para_quimica(void)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT id, fecha_hora, goles, asistencias, resultado "
        "FROM partido "
        "ORDER BY id ASC";

    if (!preparar_stmt(&stmt, sql))
    {
        printf("No se pudo listar partidos.\n");
        return;
    }

    printf("\nPartidos disponibles:\n");
    printf("----------------------------------------\n");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 1);
        int goles = sqlite3_column_int(stmt, 2);
        int asistencias = sqlite3_column_int(stmt, 3);
        int resultado = sqlite3_column_int(stmt, 4);

        printf("ID:%d | %s | G:%d A:%d | %s\n",
               id,
               fecha ? fecha : "N/A",
               goles,
               asistencias,
               resultado_to_text(resultado));
    }

    sqlite3_finalize(stmt);
}

static int seleccionar_partido_para_quimica(const char *prompt)
{
    int partido_id;

    listar_partidos_para_quimica();

    do
    {
        partido_id = input_int(prompt);
        if (partido_id == 0)
        {
            return 0;
        }

        if (partido_id < 0 || !existe_id("partido", partido_id))
        {
            printf("ID de partido invalido.\n");
            partido_id = 0;
        }
    }
    while (partido_id == 0);

    return partido_id;
}

/**
 * @brief Muestra la mejor quimica entre dos jugadores
 *
 * Calcula la dupla con mejor winrate considerando partidos en los que
 * ambos jugadores estuvieron en el mismo equipo asignado a un partido.
 */
static void mostrar_mejor_quimica_jugadores()
{
    iniciar_pantalla_analisis("MEJOR COMBINACION DE JUGADORES");

    if (!asegurar_tabla_quimica_jugador_estadistica())
    {
        pause_console();
        return;
    }

    if (!hay_registros("partido"))
    {
        printf("No hay suficientes datos para analizar quimica entre jugadores.\n");
        printf("Necesitas al menos partidos registrados.\n");
        pause_console();
        return;
    }

    char *nombre_usuario = get_user_name();
    if (!nombre_usuario || nombre_usuario[0] == '\0')
    {
        free(nombre_usuario);
        nombre_usuario = (char *)malloc(8);
        if (nombre_usuario)
        {
            strcpy_s(nombre_usuario, 8, "Usuario");
        }
    }

    sqlite3_stmt *stmt;
    const char *sql =
        "WITH duplas_auto AS ("
        "  SELECT "
        "    j1.id AS jugador_a_id, "
        "    j1.nombre AS jugador_a, "
        "    j2.id AS jugador_b_id, "
        "    j2.nombre AS jugador_b, "
        "    p.resultado AS resultado "
        "  FROM equipo e "
        "  JOIN partido p ON p.id = e.partido_id "
        "  JOIN jugador j1 ON j1.equipo_id = e.id "
        "  JOIN jugador j2 ON j2.equipo_id = e.id AND j1.id < j2.id "
        "  WHERE e.partido_id > 0"
        "), duplas_manual AS ("
        "  SELECT DISTINCT "
        "    -1 AS jugador_a_id, "
        "    ? AS jugador_a, "
        "    -1 AS jugador_b_id, "
        "    q.jugador AS jugador_b, "
        "    p.resultado AS resultado "
        "  FROM quimica_jugador_estadistica q "
        "  JOIN partido p ON p.id = q.partido_id "
        "  WHERE q.jugador IS NOT NULL AND q.jugador <> ''"
        "), duplas AS ("
        "  SELECT * FROM duplas_auto "
        "  UNION ALL "
        "  SELECT * FROM duplas_manual"
        "), resumen AS ("
        "  SELECT "
        "    jugador_a, jugador_b, "
        "    SUM(CASE WHEN resultado = 1 THEN 1 ELSE 0 END) AS victorias, "
        "    SUM(CASE WHEN resultado = 3 THEN 1 ELSE 0 END) AS derrotas, "
        "    SUM(CASE WHEN resultado = 2 THEN 1 ELSE 0 END) AS empates "
        "  FROM duplas "
        "  GROUP BY jugador_a, jugador_b"
        ") "
        "SELECT "
        "  jugador_a, jugador_b, victorias, derrotas, empates, "
        "  CASE WHEN (victorias + derrotas) > 0 "
        "       THEN (CAST(victorias AS REAL) * 100.0) / (victorias + derrotas) "
        "       ELSE 0 END AS winrate "
        "FROM resumen "
        "ORDER BY winrate DESC, victorias DESC, (victorias + derrotas + empates) DESC "
        "LIMIT 1";

    if (!preparar_stmt_con_mensaje(&stmt, sql))
    {
        pause_console();
        free(nombre_usuario);
        return;
    }

    sqlite3_bind_text(stmt, 1, nombre_usuario, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        printf("No hay suficientes datos de duplas por partido.\n");
        printf("Asegurate de tener equipos asignados y/o estadisticas manuales cargadas.\n");
        sqlite3_finalize(stmt);
        free(nombre_usuario);
        pause_console();
        return;
    }

    const char *jugador_a = (const char *)sqlite3_column_text(stmt, 0);
    const char *jugador_b = (const char *)sqlite3_column_text(stmt, 1);
    int victorias = sqlite3_column_int(stmt, 2);
    int derrotas = sqlite3_column_int(stmt, 3);
    int empates = sqlite3_column_int(stmt, 4);
    double winrate = sqlite3_column_double(stmt, 5);

    printf("\nMejor combinacion de jugadores\n");
    printf("----------------------------------------\n");
    printf("%s + %s\n", jugador_a ? jugador_a : "N/A", jugador_b ? jugador_b : "N/A");
    printf("Victorias : %d\n", victorias);
    printf("Derrotas  : %d\n", derrotas);
    printf("Empates   : %d\n", empates);
    printf("Winrate   : %.1f%%\n", winrate);

    sqlite3_finalize(stmt);
    free(nombre_usuario);
    pause_console();
}

typedef struct
{
    char jugador[100];
    char companero_asistido[100];
    char posicion[40];
    char comentario[200];
    int goles;
    int asistencias;
    int asistencias_al_usuario;
    int asistencias_del_usuario;
} DatosQuimicaJugador;

static void capturar_datos_quimica_jugador(DatosQuimicaJugador *datos, int es_edicion)
{
    if (!es_edicion)
    {
        input_string("Nombre del companero que te asistio(Enter para Vacio): ", datos->jugador, sizeof(datos->jugador));
        input_string("Nombre del companero al que asististe(Enter para Vacio): ", datos->companero_asistido, sizeof(datos->companero_asistido));
        input_string("Posicion (ej: Mediocampista-Delantero): ", datos->posicion, sizeof(datos->posicion));

        datos->goles = normalizar_no_negativo(input_int("Goles del companero: "));
        datos->asistencias = normalizar_no_negativo(input_int("Asistencias totales del companero: "));
        datos->asistencias_al_usuario = normalizar_no_negativo(input_int("Asistencias del companero hacia ti: "));
        datos->asistencias_del_usuario = normalizar_no_negativo(input_int("Asistencias tuyas hacia ese companero: "));

        input_string("Comentario (opcional): ", datos->comentario, sizeof(datos->comentario));
        return;
    }

    input_string("Nuevo nombre del companero que te asistio: ", datos->jugador, sizeof(datos->jugador));
    input_string("Nuevo nombre del companero al que asististe: ", datos->companero_asistido, sizeof(datos->companero_asistido));
    input_string("Nueva posicion: ", datos->posicion, sizeof(datos->posicion));

    datos->goles = normalizar_no_negativo(input_int("Nuevos goles del companero: "));
    datos->asistencias = normalizar_no_negativo(input_int("Nuevas asistencias totales del companero: "));
    datos->asistencias_al_usuario = normalizar_no_negativo(input_int("Nuevas asistencias del companero hacia ti: "));
    datos->asistencias_del_usuario = normalizar_no_negativo(input_int("Nuevas asistencias tuyas hacia ese companero: "));

    input_string("Nuevo comentario (opcional): ", datos->comentario, sizeof(datos->comentario));
}

static void bind_datos_quimica_jugador(sqlite3_stmt *stmt, int partido_id, const DatosQuimicaJugador *datos)
{
    sqlite3_bind_int(stmt, 1, partido_id);
    sqlite3_bind_text(stmt, 2, datos->jugador, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, datos->companero_asistido, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, datos->posicion, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, datos->goles);
    sqlite3_bind_int(stmt, 6, datos->asistencias);
    sqlite3_bind_int(stmt, 7, datos->asistencias_al_usuario);
    sqlite3_bind_int(stmt, 8, datos->asistencias_del_usuario);
    sqlite3_bind_text(stmt, 9, datos->comentario, -1, SQLITE_TRANSIENT);
}

static void crear_estadistica_quimica_jugador(void)
{
    iniciar_pantalla_analisis("AGREGAR ESTADISTICA DE JUGADOR");

    if (!asegurar_tabla_quimica_jugador_estadistica())
    {
        pause_console();
        return;
    }

    if (!hay_registros("partido"))
    {
        mostrar_no_hay_registros("partidos");
        pause_console();
        return;
    }

    int partido_id = seleccionar_partido_para_quimica("Selecciona el ID del partido jugado: ");
    if (partido_id == 0)
        return;

    DatosQuimicaJugador datos = {0};
    capturar_datos_quimica_jugador(&datos, 0);

    sqlite3_stmt *stmt;
    const char *sql =
        "INSERT INTO quimica_jugador_estadistica "
        "(partido_id, jugador, companero_asistido, posicion, goles, asistencias, asistencias_al_usuario, asistencias_del_usuario, comentario) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";

    if (!preparar_stmt_con_mensaje(&stmt, sql))
    {
        pause_console();
        return;
    }

    bind_datos_quimica_jugador(stmt, partido_id, &datos);

    if (sqlite3_step(stmt) == SQLITE_DONE)
        printf("Estadistica guardada correctamente.\n");
    else
        printf("No se pudo guardar la estadistica.\n");

    sqlite3_finalize(stmt);
    pause_console();
}

static void listar_estadisticas_quimica_jugador(void)
{
    iniciar_pantalla_analisis("LISTADO DE ESTADISTICAS DE JUGADORES");

    if (!asegurar_tabla_quimica_jugador_estadistica())
    {
        pause_console();
        return;
    }

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT q.id, q.partido_id, p.fecha_hora, q.jugador, q.companero_asistido, q.posicion, "
        "q.goles, q.asistencias, q.asistencias_al_usuario, q.asistencias_del_usuario "
        "FROM quimica_jugador_estadistica q "
        "LEFT JOIN partido p ON p.id = q.partido_id "
        "ORDER BY q.id DESC";

    if (!preparar_stmt_con_mensaje(&stmt, sql))
    {
        pause_console();
        return;
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        int partido_id = sqlite3_column_int(stmt, 1);
        const char *fecha = (const char *)sqlite3_column_text(stmt, 2);
        const char *jugador = (const char *)sqlite3_column_text(stmt, 3);
        const char *companero_asistido = (const char *)sqlite3_column_text(stmt, 4);
        const char *posicion = (const char *)sqlite3_column_text(stmt, 5);
        int goles = sqlite3_column_int(stmt, 6);
        int asistencias = sqlite3_column_int(stmt, 7);
        int asist_usr = sqlite3_column_int(stmt, 8);
        int asist_del_usr = sqlite3_column_int(stmt, 9);

        printf("ID:%d | Partido:%d (%s) | Te asistio:%s | Asististe a:%s | %s | G:%d A:%d A->Tu:%d Tu->Comp:%d\n",
               id,
               partido_id,
               fecha ? fecha : "N/A",
               jugador ? jugador : "N/A",
               companero_asistido ? companero_asistido : "N/A",
               posicion ? posicion : "N/A",
               goles,
               asistencias,
               asist_usr,
               asist_del_usr);
        count++;
    }

    if (count == 0)
        mostrar_no_hay_registros("estadisticas de jugadores");

    sqlite3_finalize(stmt);
    pause_console();
}

static void editar_estadistica_quimica_jugador(void)
{
    iniciar_pantalla_analisis("EDITAR ESTADISTICA DE JUGADOR");

    if (!asegurar_tabla_quimica_jugador_estadistica())
    {
        pause_console();
        return;
    }

    listar_estadisticas_quimica_jugador();

    int id = input_int("ID de la estadistica a editar (0 cancelar): ");
    if (id <= 0)
        return;

    if (!existe_id("quimica_jugador_estadistica", id))
    {
        printf("ID invalido.\n");
        pause_console();
        return;
    }

    int partido_id = seleccionar_partido_para_quimica("Nuevo ID de partido jugado: ");
    if (partido_id == 0)
        return;

    DatosQuimicaJugador datos = {0};
    capturar_datos_quimica_jugador(&datos, 1);

    sqlite3_stmt *stmt;
    const char *sql =
        "UPDATE quimica_jugador_estadistica "
        "SET partido_id = ?, jugador = ?, companero_asistido = ?, posicion = ?, goles = ?, asistencias = ?, "
        "asistencias_al_usuario = ?, asistencias_del_usuario = ?, comentario = ? "
        "WHERE id = ?";

    if (!preparar_stmt_con_mensaje(&stmt, sql))
    {
        pause_console();
        return;
    }

    bind_datos_quimica_jugador(stmt, partido_id, &datos);
    sqlite3_bind_int(stmt, 10, id);

    if (sqlite3_step(stmt) == SQLITE_DONE)
        printf("Estadistica actualizada correctamente.\n");
    else
        printf("No se pudo actualizar la estadistica.\n");

    sqlite3_finalize(stmt);
    pause_console();
}

static void eliminar_estadistica_quimica_jugador(void)
{
    iniciar_pantalla_analisis("ELIMINAR ESTADISTICA DE JUGADOR");

    if (!asegurar_tabla_quimica_jugador_estadistica())
    {
        pause_console();
        return;
    }

    listar_estadisticas_quimica_jugador();

    int id = input_int("ID de la estadistica a eliminar (0 cancelar): ");
    if (id <= 0)
        return;

    if (!existe_id("quimica_jugador_estadistica", id))
    {
        printf("ID invalido.\n");
        pause_console();
        return;
    }

    if (!confirmar("Seguro que deseas eliminar esta estadistica?"))
        return;

    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM quimica_jugador_estadistica WHERE id = ?";

    if (!preparar_stmt_con_mensaje(&stmt, sql))
    {
        pause_console();
        return;
    }

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_DONE)
        printf("Estadistica eliminada correctamente.\n");
    else
        printf("No se pudo eliminar la estadistica.\n");

    sqlite3_finalize(stmt);
    pause_console();
}

static void analizar_quimica_jugadores()
{
    if (!asegurar_tabla_quimica_jugador_estadistica())
    {
        pause_console();
        return;
    }

    MenuItem items[] =
    {
        {1, "Mejor Combinacion de Jugadores", &mostrar_mejor_quimica_jugadores},
        {2, "Agregar Estadistica de Jugador", &crear_estadistica_quimica_jugador},
        {3, "Listar Estadisticas de Jugadores", &listar_estadisticas_quimica_jugador},
        {4, "Editar Estadistica de Jugador", &editar_estadistica_quimica_jugador},
        {5, "Eliminar Estadistica de Jugador", &eliminar_estadistica_quimica_jugador},
        {0, "Volver", NULL}
    };

    ejecutar_menu("ANALISIS DE QUIMICA", items, 6);
}

/**
 * @brief Estructura para metricas de comparacion
 */
typedef struct
{
    double goles;
    double asistencias;
    double rendimiento;
    int partidos;
} MetricasComparacion;

/**
 * @brief Determina el ganador basado en la diferencia
 *
 * @param diff Diferencia entre metricas
 * @param nombre1 Nombre del primer elemento
 * @param nombre2 Nombre del segundo elemento
 * @return Nombre del ganador o "Empate"
 */
static const char *determinar_ganador(double diff, const char *nombre1, const char *nombre2)
{
    if (diff > 0)
        return nombre1;
    if (diff < 0)
        return nombre2;
    return "Empate";
}

/**
 * @brief Calcula metricas para una condicion especifica
 */
static void calcular_metricas_por_condicion(MetricasComparacion *metricas, const char *condicion_sql)
{
    sqlite3_stmt *stmt;
    char sql[512];

    memset(metricas, 0, sizeof(*metricas));

    snprintf(sql, sizeof(sql),
             "SELECT COUNT(*), AVG(goles), AVG(asistencias), AVG(rendimiento_general) "
             "FROM partido WHERE %s", condicion_sql);

    if (!preparar_stmt(&stmt, sql))
    {
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        metricas->partidos = sqlite3_column_int(stmt, 0);
        metricas->goles = sqlite3_column_double(stmt, 1);
        metricas->asistencias = sqlite3_column_double(stmt, 2);
        metricas->rendimiento = sqlite3_column_double(stmt, 3);
    }

    sqlite3_finalize(stmt);
}

/**
 * @brief Muestra comparacion entre dos metricas
 */
static void mostrar_comparacion_dos_metricas(const MetricasComparacion *m1, const MetricasComparacion *m2,
        const char *nombre1, const char *nombre2)
{
    printf("\nCOMPARACION: %s vs %s\n", nombre1, nombre2);
    printf("----------------------------------------\n");

    if (m1->partidos == 0 && m2->partidos == 0)
    {
        printf("No hay datos suficientes para comparar.\n");
        return;
    }

    printf("%-15s %-15s %-15s %-15s %-15s\n", "Metrica", nombre1, nombre2, "Diferencia", "Porcentaje");
    printf("----------------------------------------\n");

    // Goles
    double diff_goles = m1->goles - m2->goles;
    double pct_goles = (m2->goles != 0) ? (diff_goles / m2->goles) * 100 : 0;
    printf("%-15s %-15.2f %-15.2f %-15.2f %-15.1f%%\n", "Goles", m1->goles, m2->goles, diff_goles, pct_goles);

    // Asistencias
    double diff_asist = m1->asistencias - m2->asistencias;
    double pct_asist = (m2->asistencias != 0) ? (diff_asist / m2->asistencias) * 100 : 0;
    printf("%-15s %-15.2f %-15.2f %-15.2f %-15.1f%%\n", "Asistencias", m1->asistencias, m2->asistencias, diff_asist, pct_asist);

    // Rendimiento
    double diff_rend = m1->rendimiento - m2->rendimiento;
    double pct_rend = (m2->rendimiento != 0) ? (diff_rend / m2->rendimiento) * 100 : 0;
    printf("%-15s %-15.2f %-15.2f %-15.2f %-15.1f%%\n", "Rendimiento", m1->rendimiento, m2->rendimiento, diff_rend, pct_rend);

    printf("----------------------------------------\n");

    // Determinar ganadores por metrica
    printf("GANADORES POR METRICA:\n");
    printf("  Goles: %s\n", determinar_ganador(diff_goles, nombre1, nombre2));
    printf("  Asistencias: %s\n", determinar_ganador(diff_asist, nombre1, nombre2));
    printf("  Rendimiento: %s\n", determinar_ganador(diff_rend, nombre1, nombre2));
}

static void comparar_y_mostrar_metricas(const char *condicion_sql_1,
                                        const char *condicion_sql_2,
                                        const char *nombre1,
                                        const char *nombre2)
{
    MetricasComparacion m1 = {0};
    MetricasComparacion m2 = {0};

    calcular_metricas_por_condicion(&m1, condicion_sql_1);
    calcular_metricas_por_condicion(&m2, condicion_sql_2);

    mostrar_comparacion_dos_metricas(&m1, &m2, nombre1, nombre2);
    pause_console();
}

static void construir_condicion_sql_id(char *destino,
                                       size_t destino_size,
                                       const char *condicion_prefijo,
                                       int id,
                                       const char *condicion_sufijo)
{
    char id_txt[32];
    sprintf_s(id_txt, sizeof(id_txt), "%d", id);

    strcpy_s(destino, destino_size, condicion_prefijo);
    strcat_s(destino, destino_size, id_txt);
    strcat_s(destino, destino_size, condicion_sufijo);
}

static void comparar_entidades_por_tabla(const char *titulo_pantalla,
        const char *tabla,
        const char *titulo_lista,
        const char *condicion_prefijo,
        const char *condicion_sufijo,
        const char *nombre_default_1,
        const char *nombre_default_2)
{
    int id1;
    int id2;
    char sql1[256];
    char sql2[256];
    char nombre1[256];
    char nombre2[256];

    iniciar_pantalla_analisis(titulo_pantalla);

    if (!listar_y_seleccionar_dos_entidades(tabla, titulo_lista, &id1, &id2))
        return;

    construir_condicion_sql_id(sql1, sizeof(sql1), condicion_prefijo, id1, condicion_sufijo);
    construir_condicion_sql_id(sql2, sizeof(sql2), condicion_prefijo, id2, condicion_sufijo);

    strcpy_s(nombre1, sizeof(nombre1), nombre_default_1);
    strcpy_s(nombre2, sizeof(nombre2), nombre_default_2);

    obtener_nombre_entidad(tabla, id1, nombre1, sizeof(nombre1));
    obtener_nombre_entidad(tabla, id2, nombre2, sizeof(nombre2));

    comparar_y_mostrar_metricas(sql1, sql2, nombre1, nombre2);
}

/**
 * @brief Funcion auxiliar para listar entidades y obtener dos IDs
 * Centraliza logica repetida en comparadores para evitar duplicacion
 *
 * @param tabla Nombre de la tabla a consultar
 * @param titulo Titulo a mostrar
 * @param id1 Puntero para almacenar primer ID
 * @param id2 Puntero para almacenar segundo ID
 * @return 1 si se obtuvieron dos IDs validos, 0 si hay menos de 2 entidades
 */
static int listar_y_seleccionar_dos_entidades(const char *tabla, const char *titulo, int *id1, int *id2)
{
    sqlite3_stmt *stmt;

    // Nota: Reemplazar %s en la consulta manualmente
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT id, nombre FROM %s ORDER BY id", tabla);

    if (!preparar_stmt(&stmt, sql))
    {
        return 0;
    }

    printf("%s disponibles:\n", titulo);
    printf("----------------------------------------\n");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *nombre = (const char *)sqlite3_column_text(stmt, 1);
        printf("%d. %s\n", id, nombre);
        count++;
    }
    sqlite3_finalize(stmt);

    if (count < 2)
    {
        printf("Se necesitan al menos 2 %s para comparar.\n", tabla);
        pause_console();
        return 0;
    }

    while (1)
    {
        *id1 = input_int("\nIngrese ID de la primera entidad (0 para cancelar): ");
        if (*id1 == 0)
            return 0;
        if (!existe_id_entidad(tabla, *id1))
        {
            printf("ID invalido.\n");
            continue;
        }

        *id2 = input_int("Ingrese ID de la segunda entidad (0 para cancelar): ");
        if (*id2 == 0)
            return 0;
        if (!existe_id_entidad(tabla, *id2))
        {
            printf("ID invalido.\n");
            continue;
        }
        if (*id1 == *id2)
        {
            printf("Los IDs deben ser diferentes.\n");
            continue;
        }
        return 1;
    }
}

/**
 * @brief Compara dos camisetas
 */
static void comparar_camisetas()
{
    comparar_entidades_por_tabla("COMPARADOR: CAMISETAS",
                                 "camiseta",
                                 "Camiseta",
                                 "camiseta_id = ",
                                 "",
                                 "Camiseta A",
                                 "Camiseta B");
}

/**
 * @brief Compara dos torneos
 */
static void comparar_torneos()
{
    comparar_entidades_por_tabla("COMPARADOR: TORNEOS",
                                 "torneo",
                                 "Torneo",
                                 "id IN (SELECT partido_id FROM partido_torneo WHERE torneo_id = ",
                                 ")",
                                 "Torneo A",
                                 "Torneo B");
}

/**
 * @brief Compara dos periodos
 */
static void comparar_periodos()
{
    iniciar_pantalla_analisis("COMPARADOR: PERIODOS");

    printf("Formatos de fecha: DD/MM/AAAA\n");
    printf("Ejemplo: 2024-01-01 al 2024-06-30\n\n");

    char fecha1_inicio[20];
    char fecha1_fin[20];
    char fecha2_inicio[20];
    char fecha2_fin[20];

    printf("PRIMER PERIODO:\n");
    solicitar_fecha_yyyy_mm_dd("Fecha inicio (DD/MM/AAAA, Enter=hoy): ", fecha1_inicio, sizeof(fecha1_inicio));
    solicitar_fecha_yyyy_mm_dd("Fecha fin (DD/MM/AAAA, Enter=hoy): ", fecha1_fin, sizeof(fecha1_fin));
    while (strcmp(fecha1_fin, fecha1_inicio) < 0)
    {
        printf("La fecha de fin no puede ser anterior a la de inicio.\n");
        solicitar_fecha_yyyy_mm_dd("Fecha fin (DD/MM/AAAA, Enter=hoy): ", fecha1_fin, sizeof(fecha1_fin));
    }

    printf("\nSEGUNDO PERIODO:\n");
    solicitar_fecha_yyyy_mm_dd("Fecha inicio (DD/MM/AAAA, Enter=hoy): ", fecha2_inicio, sizeof(fecha2_inicio));
    solicitar_fecha_yyyy_mm_dd("Fecha fin (DD/MM/AAAA, Enter=hoy): ", fecha2_fin, sizeof(fecha2_fin));
    while (strcmp(fecha2_fin, fecha2_inicio) < 0)
    {
        printf("La fecha de fin no puede ser anterior a la de inicio.\n");
        solicitar_fecha_yyyy_mm_dd("Fecha fin (DD/MM/AAAA, Enter=hoy): ", fecha2_fin, sizeof(fecha2_fin));
    }

    char sql1[256];
    char sql2[256];
    snprintf(sql1, sizeof(sql1), "fecha_hora BETWEEN '%s' AND '%s'", fecha1_inicio, fecha1_fin);
    snprintf(sql2, sizeof(sql2), "fecha_hora BETWEEN '%s' AND '%s'", fecha2_inicio, fecha2_fin);

    char nombre1[256];
    char nombre2[256];
    snprintf(nombre1, sizeof(nombre1), "Periodo %s a %s", fecha1_inicio, fecha1_fin);
    snprintf(nombre2, sizeof(nombre2), "Periodo %s a %s", fecha2_inicio, fecha2_fin);

    comparar_y_mostrar_metricas(sql1, sql2, nombre1, nombre2);
}

/**
 * @brief Compara dos condiciones
 */
static void comparar_condiciones()
{
    iniciar_pantalla_analisis("COMPARADOR: CONDICIONES");

    printf("Tipos de condicion:\n");
    printf("1. Clima (0=Soleado, 1=Lluvia, 2=Nublado)\n");
    printf("2. Dia de la semana (0=Lunes, 1=Martes, ..., 6=Domingo)\n");

    int tipo_condicion = 0;
    while (tipo_condicion < 1 || tipo_condicion > 2)
    {
        tipo_condicion = input_int("\nSeleccione tipo de condicion (1-2): ");
        if (tipo_condicion < 1 || tipo_condicion > 2)
            printf("Opcion invalida.\n");
    }

    int valor1;
    int valor2;
    const char *campo = (tipo_condicion == 1) ? "clima" : "dia";
    const char *tipo_texto = (tipo_condicion == 1) ? "Clima" : "Dia";

    int min_val = 0;
    int max_val = (tipo_condicion == 1) ? 2 : 6;

    while (1)
    {
        valor1 = input_int("\nIngrese primer valor: ");
        valor2 = input_int("Ingrese segundo valor: ");

        if (valor1 < min_val || valor1 > max_val || valor2 < min_val || valor2 > max_val)
        {
            printf("Valores invalidos. Rango permitido: %d a %d.\n", min_val, max_val);
            continue;
        }
        if (valor1 == valor2)
        {
            printf("Los valores deben ser diferentes.\n");
            continue;
        }
        break;
    }

    char sql1[128];
    char sql2[128];
    snprintf(sql1, sizeof(sql1), "%s = %d", campo, valor1);
    snprintf(sql2, sizeof(sql2), "%s = %d", campo, valor2);

    char nombre1[256];
    char nombre2[256];
    snprintf(nombre1, sizeof(nombre1), "%s %d", tipo_texto, valor1);
    snprintf(nombre2, sizeof(nombre2), "%s %d", tipo_texto, valor2);

    comparar_y_mostrar_metricas(sql1, sql2, nombre1, nombre2);
}

/**
 * @brief Muestra el menu del comparador avanzado
 */
static void mostrar_comparador_avanzado()
{
    iniciar_pantalla_analisis("COMPARADOR AVANZADO");

    MenuItem items[] =
    {
        {1, "Comparar Camisetas", comparar_camisetas},
        {2, "Comparar Torneos", comparar_torneos},
        {3, "Comparar Periodos", comparar_periodos},
        {4, "Comparar Condiciones", comparar_condiciones},
        {0, "Volver", NULL}
    };

    ejecutar_menu("COMPARADOR AVANZADO", items, 5);
}

/**
 * @brief Muestra el analisis completo de rendimiento
 */
void mostrar_analisis()
{
    iniciar_pantalla_analisis("ANALISIS Y COMPARADOR");

    MenuItem items[] =
    {
        {1, "Analisis Basico", mostrar_analisis_basico},
        {2, "Comparador Avanzado", mostrar_comparador_avanzado},
        {3, "Analisis Tactico (Diagramas)", &menu_tacticas_partido},
        {4, get_text("menu_entrenador_ia"), &menu_entrenador_ia},
        {5, "Quimica Entre Jugadores", analizar_quimica_jugadores},
        {0, "Volver", NULL}
    };

    ejecutar_menu("ANALISIS Y COMPARADOR", items, 6);
}

/**
 * @brief Estructura para almacenar estadisticas mensuales
 */
typedef struct
{
    int mes;
    int anio;
    double avg_valor;
    int total_partidos;
} EstadisticasMensuales;

/**
 * @brief Convierte numero de mes a nombre
 *
 * @param mes Numero del mes (1-12)
 * @return Nombre del mes en espanol
 */
static const char *mes_to_text(int mes)
{
    switch (mes)
    {
    case 1:
        return "Enero";
    case 2:
        return "Febrero";
    case 3:
        return "Marzo";
    case 4:
        return "Abril";
    case 5:
        return "Mayo";
    case 6:
        return "Junio";
    case 7:
        return "Julio";
    case 8:
        return "Agosto";
    case 9:
        return "Septiembre";
    case 10:
        return "Octubre";
    case 11:
        return "Noviembre";
    case 12:
        return "Diciembre";
    default:
        return "DESCONOCIDO";
    }
}

/**
 * @brief Calcula estadisticas mensuales para una metrica especifica
 *
 * @param stats Array donde almacenar las estadisticas mensuales
 * @param max_stats Tamano maximo del array
 * @param columna Nombre de la columna a promediar (goles, asistencias, rendimiento_general)
 * @return Numero de meses con datos
 */
static int calcular_estadisticas_mensuales(EstadisticasMensuales *stats, int max_stats, const char *columna)
{
    sqlite3_stmt *stmt;
    char sql[512];

    /*
     * Se utiliza strftime('%m', fecha_hora) y strftime('%Y', fecha_hora) para agrupar los datos
     * a nivel mensual, permitiendo calcular promedios (AVG) por cada periodo mes-ano.
     * El orden persistente es descendente para mostrar primero los datos mas recientes.
     */
    snprintf(sql, sizeof(sql),
             "SELECT strftime('%%m', fecha_hora) as mes, strftime('%%Y', fecha_hora) as anio, "
             "AVG(%s), COUNT(*) "
             "FROM partido "
             "GROUP BY strftime('%%Y', fecha_hora), strftime('%%m', fecha_hora) "
             "ORDER BY anio DESC, mes DESC",
             columna);

    if (!preparar_stmt(&stmt, sql))
    {
        return 0;
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_stats)
    {
        stats[count].mes = atoi((const char *)sqlite3_column_text(stmt, 0));
        stats[count].anio = atoi((const char *)sqlite3_column_text(stmt, 1));
        stats[count].avg_valor = sqlite3_column_double(stmt, 2);
        stats[count].total_partidos = sqlite3_column_int(stmt, 3);
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

/**
 * @brief Muestra la evolucion mensual de una metrica
 *
 * @param titulo Titulo a mostrar
 * @param columna Nombre de la columna de la base de datos
 */
static void mostrar_evolucion_mensual(const char *titulo, const char *columna)
{
    iniciar_pantalla_analisis(titulo);

    EstadisticasMensuales stats[120]; // Maximo 10 anos de datos
    int num_meses = calcular_estadisticas_mensuales(stats, 120, columna);

    if (num_meses == 0)
    {
        printf("No hay suficientes datos para mostrar la evolucion mensual.\n");
        printf("Registra al menos algunos partidos para ver estadisticas.\n");
        pause_console();
        return;
    }

    printf("EVOLUCION MENSUAL:\n");
    printf("----------------------------------------\n");

    for (int i = 0; i < num_meses; i++)
    {
        printf("%s %d: %.2f (%d partidos)\n",
               mes_to_text(stats[i].mes), stats[i].anio,
               stats[i].avg_valor, stats[i].total_partidos);
    }

    pause_console();
}

/**
 * @brief Encuentra el mejor o peor mes historico
 *
 * @param mejor 1 para mejor mes, 0 para peor mes
 */
static void encontrar_mes_historico(int mejor)
{
    iniciar_pantalla_analisis(mejor ? "MEJOR MES HISTORICO" : "PEOR MES HISTORICO");

    sqlite3_stmt *stmt;
    const char *sql = mejor ?
                      "SELECT strftime('%m', fecha_hora) as mes, strftime('%Y', fecha_hora) as anio, "
                      "AVG(rendimiento_general), COUNT(*) "
                      "FROM partido "
                      "GROUP BY strftime('%Y', fecha_hora), strftime('%m', fecha_hora) "
                      "ORDER BY AVG(rendimiento_general) DESC LIMIT 1" :
                      "SELECT strftime('%m', fecha_hora) as mes, strftime('%Y', fecha_hora) as anio, "
                      "AVG(rendimiento_general), COUNT(*) "
                      "FROM partido "
                      "GROUP BY strftime('%Y', fecha_hora), strftime('%m', fecha_hora) "
                      "ORDER BY AVG(rendimiento_general) ASC LIMIT 1";

    if (!preparar_stmt_con_mensaje(&stmt, sql))
    {
        pause_console();
        return;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int mes = atoi((const char *)sqlite3_column_text(stmt, 0));
        int anio = atoi((const char *)sqlite3_column_text(stmt, 1));
        double avg_rendimiento = sqlite3_column_double(stmt, 2);
        int partidos = sqlite3_column_int(stmt, 3);

        printf("%s MES HISTORICO:\n", mejor ? "MEJOR" : "PEOR");
        printf("----------------------------------------\n");
        printf("Mes: %s %d\n", mes_to_text(mes), anio);
        printf("Rendimiento promedio: %.2f\n", avg_rendimiento);
        printf("Partidos jugados: %d\n", partidos);
    }
    else
    {
        printf("No hay suficientes datos para determinar el %s mes historico.\n",
               mejor ? "mejor" : "peor");
    }

    sqlite3_finalize(stmt);
    pause_console();

}

/**
 * @brief Compara rendimiento al inicio vs fin de ano
 */
static void comparar_inicio_fin_anio()
{
    iniciar_pantalla_analisis("INICIO VS FIN DE ANIO");

    sqlite3_stmt *stmt;
    /*
     * Clasifica los partidos en dos grandes semestres usando CAST y strftime.
     * Esto permite un analisis comparativo de la evolucion del rendimiento entre
     * la primera y la segunda mitad del ano calendario.
     */
    const char *sql =
        "SELECT "
        "CASE WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) <= 6 THEN 'Inicio' ELSE 'Fin' END as periodo, "
        "AVG(goles), AVG(asistencias), AVG(rendimiento_general), COUNT(*) "
        "FROM partido "
        "GROUP BY CASE WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) <= 6 THEN 'Inicio' ELSE 'Fin' END";

    if (!preparar_stmt_con_mensaje(&stmt, sql))
    {
        pause_console();
        return;
    }

    printf("COMPARACION INICIO VS FIN DE ANIO:\n");
    printf("----------------------------------------\n");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *periodo = (const char *)sqlite3_column_text(stmt, 0);
        double avg_goles = sqlite3_column_double(stmt, 1);
        double avg_asistencias = sqlite3_column_double(stmt, 2);
        double avg_rendimiento = sqlite3_column_double(stmt, 3);
        int partidos = sqlite3_column_int(stmt, 4);

        printf("%s de ano (Ene-Jun):\n", strcmp(periodo, "Inicio") == 0 ? "Inicio" : "Fin");
        printf("  Goles: %.2f\n", avg_goles);
        printf("  Asistencias: %.2f\n", avg_asistencias);
        printf("  Rendimiento: %.2f\n", avg_rendimiento);
        printf("  Partidos: %d\n\n", partidos);
        count++;
    }

    if (count == 0)
    {
        printf("No hay suficientes datos para comparar inicio vs fin de ano.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Compara rendimiento en meses frios vs calidos
 */
static void comparar_meses_frios_calidos()
{
    iniciar_pantalla_analisis("MESES FRIOS VS CALIDOS");

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT "
        "CASE "
        "  WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) BETWEEN 6 AND 9 THEN 'Frios' "
        "  WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) IN (12,1,2,3,4) THEN 'Calidos' "
        "  ELSE 'Otros' "
        "END as temporada, "
        "AVG(goles), AVG(asistencias), AVG(rendimiento_general), COUNT(*) "
        "FROM partido "
        "GROUP BY CASE "
        "  WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) BETWEEN 6 AND 9 THEN 'Frios' "
        "  WHEN CAST(strftime('%m', fecha_hora) AS INTEGER) IN (12,1,2,3,4) THEN 'Calidos' "
        "  ELSE 'Otros' "
        "END";

    if (!preparar_stmt_con_mensaje(&stmt, sql))
    {
        pause_console();
        return;
    }

    printf("COMPARACION MESES FRIOS VS CALIDOS:\n");
    printf("----------------------------------------\n");
    printf("Meses frios: Junio, Julio, Agosto, Septiembre\n");
    printf("Meses calidos: Diciembre, Enero, Febrero, Marzo, Abril\n\n");

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *temporada = (const char *)sqlite3_column_text(stmt, 0);
        if (strcmp(temporada, "Otros") == 0)
            continue;

        double avg_goles = sqlite3_column_double(stmt, 1);
        double avg_asistencias = sqlite3_column_double(stmt, 2);
        double avg_rendimiento = sqlite3_column_double(stmt, 3);
        int partidos = sqlite3_column_int(stmt, 4);

        printf("Meses %s:\n", temporada);
        printf("  Goles: %.2f\n", avg_goles);
        printf("  Asistencias: %.2f\n", avg_asistencias);
        printf("  Rendimiento: %.2f\n", avg_rendimiento);
        printf("  Partidos: %d\n\n", partidos);
        count++;
    }

    if (count == 0)
    {
        printf("No hay suficientes datos en meses frios o calidos para comparar.\n");
    }

    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Calcula y muestra la tendencia de rendimiento
 *
 * @param stmt Statement preparado para calcular tendencia
 */
static void mostrar_tendencia(sqlite3_stmt *tend_stmt)
{
    double avg_primeros = 0;
    double avg_ultimos = 0;
    if (sqlite3_step(tend_stmt) == SQLITE_ROW)
        avg_primeros = sqlite3_column_double(tend_stmt, 0);
    if (sqlite3_step(tend_stmt) == SQLITE_ROW)
        avg_ultimos = sqlite3_column_double(tend_stmt, 0);

    double tendencia = avg_ultimos - avg_primeros;
    printf("\nTENDENCIA:\n");
    printf("Primeros 5 partidos: %.2f\n", avg_primeros);
    printf("ultimos 5 partidos: %.2f\n", avg_ultimos);

    const char *tendencia_texto;
    if (tendencia > 0.5)
    {
        tendencia_texto = "ASCENDENTE";
    }
    else if (tendencia < -0.5)
    {
        tendencia_texto = "DESCENDENTE";
    }
    else
    {
        tendencia_texto = "ESTABLE";
    }
    printf("Tendencia: %s (%.2f)\n", tendencia_texto, tendencia);

    sqlite3_finalize(tend_stmt);
}

/**
 * @brief Calcula y muestra el progreso total del jugador
 */
static void calcular_progreso_total()
{
    iniciar_pantalla_analisis("PROGRESO TOTAL DEL JUGADOR");

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT "
        "COUNT(*), "
        "AVG(goles), AVG(asistencias), AVG(rendimiento_general), "
        "MIN(fecha_hora), MAX(fecha_hora) "
        "FROM partido";

    if (!preparar_stmt_con_mensaje(&stmt, sql))
    {
        pause_console();
        return;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        printf("No hay datos suficientes para calcular el progreso total.\n");
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }

    int total_partidos = sqlite3_column_int(stmt, 0);
    double avg_goles = sqlite3_column_double(stmt, 1);
    double avg_asistencias = sqlite3_column_double(stmt, 2);
    double avg_rendimiento = sqlite3_column_double(stmt, 3);
    const char *fecha_inicio = (const char *)sqlite3_column_text(stmt, 4);
    const char *fecha_fin = (const char *)sqlite3_column_text(stmt, 5);

    printf("PROGRESO TOTAL DEL JUGADOR:\n");
    printf("----------------------------------------\n");
    printf("Periodo: %s - %s\n", fecha_inicio ? fecha_inicio : "N/A", fecha_fin ? fecha_fin : "N/A");
    printf("Total de partidos: %d\n", total_partidos);
    printf("Promedio de goles: %.2f\n", avg_goles);
    printf("Promedio de asistencias: %.2f\n", avg_asistencias);
    printf("Promedio de rendimiento: %.2f\n", avg_rendimiento);

    // Calcular tendencia (comparar primeros vs ultimos partidos)
    if (total_partidos < 10)
    {
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }

    /*
     * Algoritmo de Tendencia: Se comparan los promedios de los primeros 5 partidos vs los ultimos 5.
     * Se usa UNION ALL para obtener ambos promedios en una sola ejecucion de statement,
     * optimizando el acceso a la base de datos para el calculo del delta de rendimiento.
     */
    sqlite3_stmt *tend_stmt;
    const char *tend_sql =
        "SELECT AVG(rendimiento_general) FROM "
        "(SELECT rendimiento_general FROM partido ORDER BY fecha_hora ASC LIMIT 5) "
        "UNION ALL "
        "SELECT AVG(rendimiento_general) FROM "
        "(SELECT rendimiento_general FROM partido ORDER BY fecha_hora DESC LIMIT 5)";

    if (!preparar_stmt(&tend_stmt, tend_sql))
    {
        sqlite3_finalize(stmt);
        pause_console();
        return;
    }

    mostrar_tendencia(tend_stmt);
    sqlite3_finalize(stmt);
    pause_console();
}

/**
 * @brief Muestra el menu de evolucion temporal
 */
void mostrar_evolucion_temporal()
{
    iniciar_pantalla_analisis("EVOLUCION TEMPORAL");

    MenuItem items[] =
    {
        {1, "Evolucion Mensual de Goles", evolucion_mensual_goles},
        {2, "Evolucion Mensual de Asistencias", evolucion_mensual_asistencias},
        {3, "Evolucion Mensual de Rendimiento", evolucion_mensual_rendimiento},
        {4, "Mejor Mes Historico", mejor_mes_historico},
        {5, "Peor Mes Historico", peor_mes_historico},
        {6, "Inicio vs Fin de Anio", inicio_vs_fin_anio},
        {7, "Meses Frios vs Calidos", meses_frios_vs_calidos},
        {8, "Progreso Total del Jugador", progreso_total_jugador},
        {0, "Volver", NULL}
    };

    ejecutar_menu("EVOLUCION TEMPORAL", items, 9);
}

/**
 * @brief Muestra la evolucion mensual de goles
 */
void evolucion_mensual_goles()
{
    mostrar_evolucion_mensual("EVOLUCION MENSUAL DE GOLES", "goles");
}

/**
 * @brief Muestra la evolucion mensual de asistencias
 */
void evolucion_mensual_asistencias()
{
    mostrar_evolucion_mensual("EVOLUCION MENSUAL DE ASISTENCIAS", "asistencias");
}

/**
 * @brief Muestra la evolucion mensual de rendimiento
 */
void evolucion_mensual_rendimiento()
{
    mostrar_evolucion_mensual("EVOLUCION MENSUAL DE RENDIMIENTO", "rendimiento_general");
}

/**
 * @brief Muestra el mejor mes historico
 */
void mejor_mes_historico()
{
    encontrar_mes_historico(1);
}

/**
 * @brief Muestra el peor mes historico
 */
void peor_mes_historico()
{
    encontrar_mes_historico(0);
}

/**
 * @brief Compara el rendimiento al inicio vs fin de ano
 */
void inicio_vs_fin_anio()
{
    comparar_inicio_fin_anio();
}

/**
 * @brief Compara el rendimiento en meses frios vs calidos
 */
void meses_frios_vs_calidos()
{
    comparar_meses_frios_calidos();
}

/**
 * @brief Muestra el progreso total del jugador
 */
void progreso_total_jugador()
{
    calcular_progreso_total();
}
