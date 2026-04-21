/**
 * @file financiamiento.c
 * @brief Implementacion de funciones para la gestion financiera en MiFutbolC
 */

#include "financiamiento.h"
#include "db.h"
#include "utils.h"
#include "menu.h"
#include "cJSON.h"
#include "sqlite3.h"
#include "partido.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <ctype.h>

#ifndef UNIT_TEST
#include "estadisticas_gui_capture.h"
#endif

#ifdef _WIN32
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

#define ARRAY_COUNT(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))
#define FIN_ITEM(numero, texto, accion) {(numero), (texto), (accion), MENU_CATEGORY_ADMIN}
#define FIN_BACK_ITEM {0, "Volver", NULL, MENU_CATEGORY_ADMIN}

#define FIN_TEXT_MAX 256

#ifndef UNIT_TEST
typedef enum
{
    FIN_INPUT_ALNUM = 0,
    FIN_INPUT_NUMERIC,
    FIN_INPUT_DATE,
    FIN_INPUT_GENERIC
} FinInputMode;

static int fin_modal_input_texto_gui(const char *titulo,
                                     const char *etiqueta,
                                     const char *hint,
                                     char *out,
                                     size_t out_size,
                                     int permitir_vacio,
                                     FinInputMode mode);
static int fin_modal_input_entero_gui(const char *titulo,
                                      const char *etiqueta,
                                      const char *hint,
                                      int min_value,
                                      int *valor_out);
#endif


/**
 * @brief Convierte un tipo de transaccion enumerado a su nombre textual
 */
const char *get_nombre_tipo_transaccion(TipoTransaccion tipo)
{
    switch (tipo)
    {
    case INGRESO:
        return "Ingreso";
    case GASTO:
        return "Gasto";
    default:
        return "Desconocido";
    }
}

/**
 * @brief Convierte una categoria financiera enumerada a su nombre textual
 */
const char *get_nombre_categoria(CategoriaFinanciera categoria)
{
    switch (categoria)
    {
    case TRANSPORTE:
        return "Transporte";
    case EQUIPAMIENTO:
        return "Equipamiento";
    case CUOTAS:
        return "Cuotas";
    case TORNEOS:
        return "Torneos";
    case ARBITRAJE:
        return "Arbitraje";
    case CANCHAS:
        return "Canchas";
    case MEDICINA:
        return "Medicina";
    case OTROS:
        return "Otros";
    default:
        return "Desconocido";
    }
}

/**
 * @brief Retorna un monto entero formateado como string con puntos como separadores
 */
char *formato_monto(int monto)
{
    static char buf[20];
    char temp[20];
    int len = snprintf(temp, sizeof(temp), "%d", monto);

    int cont = 0;
    int idx = 0;

    for (int i = len - 1; i >= 0; i--)
    {
        buf[idx++] = temp[i];
        cont++;
        if (cont == 3 && i != 0)
        {
            buf[idx++] = '.';
            cont = 0;
        }
    }
    buf[idx] = '\0';

    // Reverse the string
    for (int i = 0; i < idx / 2; i++)
    {
        char t = buf[i];
        buf[i] = buf[idx - 1 - i];
        buf[idx - 1 - i] = t;
    }

    return buf;
}

/**
 * @brief Preparar un statement y reportar error
 */
static int preparar_stmt(const char *sql, sqlite3_stmt **stmt)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, 0) != SQLITE_OK)
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

/**
 * @brief Mostrar fecha en formato DD/MM/YYYY desde YYYY-MM-DD
 */
static void mostrar_fecha_display(const char *fecha_db, char *fecha_display, size_t size)
{
    int year = 0;
    int month = 0;
    int day = 0;

    if (sscanf_s(fecha_db, "%4d-%2d-%2d", &year, &month, &day) == 3)
    {
        snprintf(fecha_display, size, "%02d/%02d/%04d", day, month, year);
    }
    else
    {
        strncpy_s(fecha_display, size, fecha_db, size - 1);
    }
}

/**
 * @brief Obtener mes y ano actual en formato YYYY-MM
 */
static void obtener_mes_anio_actual(char *mes_anio)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_s(&tm, &t);
    snprintf(mes_anio, 32, "%04d-%02d", tm.tm_year + 1900, tm.tm_mon + 1);
}

/**
 * @brief Ejecutar update simple con entero
 */
static void ejecutar_update_int(const char *sql, int valor, int id_transaccion)
{
    sqlite3_stmt *stmt;
    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, valor);
        sqlite3_bind_int(stmt, 2, id_transaccion);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Ejecutar update simple con texto
 */
static void ejecutar_update_texto(const char *sql, const char *valor, int id_transaccion)
{
    sqlite3_stmt *stmt;
    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_text(stmt, 1, valor, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, id_transaccion);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

static void solicitar_texto_no_vacio(const char *prompt, char *buffer, int size)
{
#ifdef UNIT_TEST
    while (1)
    {
        input_string(prompt, buffer, size);
        if (safe_strnlen(buffer, (size_t)size) > 0)
            return;
        printf("El campo no puede estar vacio.\n");
    }
#else
    const char *etiqueta = (prompt && prompt[0] != '\0') ? prompt : "Ingrese un texto";
    (void)fin_modal_input_texto_gui("INGRESAR TEXTO",
                                    etiqueta,
                                    "Solo letras, numeros y espacios",
                                    buffer,
                                    (size_t)size,
                                    0,
                                    FIN_INPUT_ALNUM);
#endif
}

static int solicitar_monto_no_negativo(const char *prompt)
{
#ifdef UNIT_TEST
    int monto = input_int(prompt);
    while (monto < 0)
    {
        monto = input_int("Monto invalido. Ingrese 0 o mas: ");
    }
    return monto;
#else
    int monto = 0;
    const char *etiqueta = (prompt && prompt[0] != '\0') ? prompt : "Monto";
    if (!fin_modal_input_entero_gui("INGRESAR MONTO", etiqueta, "Ingrese un entero (0 o mayor)", 0, &monto))
    {
        return -1;
    }
    return monto;
#endif
}

/**
 * @brief Muestra un monto entero con formato de miles (puntos como separadores)
 */
void mostrar_monto(int monto)
{
    printf("%s\n", formato_monto(monto));
}

/**
 * @brief Muestra por pantalla toda la informacion detallada de una transaccion financiera
 */
void mostrar_transaccion(TransaccionFinanciera *transaccion)
{
    // Verificar que el puntero no sea nulo
    if (transaccion == NULL)
    {
        printf("Error: Puntero de transaccion nulo.\n");
        return;
    }

    printf("ID: %d\n", transaccion->id);

    // Convertir fecha de YYYY-MM-DD a DD/MM/YYYY para mostrar
    char fecha_display[11] = {0};
    mostrar_fecha_display(transaccion->fecha, fecha_display, sizeof(fecha_display));
    printf("Fecha: %s\n", fecha_display);

    printf("Tipo: %s\n", get_nombre_tipo_transaccion(transaccion->tipo));
    printf("Categoria: %s\n", get_nombre_categoria(transaccion->categoria));
    printf("Descripcion: %s\n", transaccion->descripcion);
    printf("Monto: $%s\n", formato_monto(transaccion->monto));
    if (transaccion->item_especifico[0] != '\0')
    {
        printf("Item Especifico: %s\n", transaccion->item_especifico);
    }
    printf("\n");
}

/**
 * @brief Obtiene la fecha actual en formato YYYY-MM-DD
 */
void obtener_fecha_actual(char *fecha)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_s(&tm, &t);
    strftime(fecha, 11, "%Y-%m-%d", &tm);
}

/**
 * @brief Convierte una fecha de formato DD/MM/YYYY a YYYY-MM-DD
 * @param fecha_ddmmyyyy Fecha en formato DD/MM/YYYY
 * @param fecha_yyyymmdd Buffer donde se almacenara la fecha convertida (YYYY-MM-DD)
 * @return 1 si la conversion fue exitosa, 0 si hubo error
 */
int convertir_fecha_ddmmyyyy_a_yyyymmdd(const char *fecha_ddmmyyyy, char *fecha_yyyymmdd)
{
    int dia = 0;
    int mes = 0;
    int anio = 0;

    // Intentar parsear la fecha en formato DD/MM/YYYY
    if (sscanf_s(fecha_ddmmyyyy, "%d/%d/%d", &dia, &mes, &anio) != 3)
    {
        return 0; // Error en el formato
    }

    // Validar rangos basicos
    if (dia < 1 || dia > 31 || mes < 1 || mes > 12 || anio < 1900 || anio > 2100)
    {
        return 0; // Fecha invalida
    }

    // Formatear a YYYY-MM-DD
    snprintf(fecha_yyyymmdd, 11, "%04d-%02d-%02d", anio, mes, dia);
    return 1; // exito
}

/**
 * @brief Obtiene el siguiente ID disponible para una nueva transaccion financiera
 *
 * Busca el ID mas pequeno disponible reutilizando espacios de IDs eliminados.
 * Utiliza una consulta SQL que encuentra el primer hueco en la secuencia de IDs.
 *
 * @return El ID disponible mas pequeno (comenzando desde 1 si la tabla esta vacia)
 */
// FUNCIoN ELIMINADA: ahora usa obtener_siguiente_id("financiamiento") de utils.c

#ifndef UNIT_TEST
typedef struct
{
    int id;
    char resumen[FIN_TEXT_MAX];
} FinTransaccionRow;

typedef struct
{
    int id;
    char resumen[FIN_TEXT_MAX];
} FinPartidoRow;

static int fin_input_char_valido(unsigned int codepoint, FinInputMode mode)
{
    unsigned char ch = (unsigned char)codepoint;

    if (codepoint < 32 || codepoint > 126)
    {
        return 0;
    }

    if (mode == FIN_INPUT_NUMERIC)
    {
        return isdigit(ch);
    }

    if (mode == FIN_INPUT_DATE)
    {
        return isdigit(ch) || ch == '/' || ch == '-';
    }

    if (mode == FIN_INPUT_ALNUM)
    {
        return isalnum(ch) || isspace(ch);
    }

    return isprint(ch);
}

static void fin_draw_action_button(Rectangle rect, const char *label, int primary)
{
    const GuiTheme *theme = gui_get_active_theme();
    Color fill = primary ? (Color){34, 132, 80, 255} : theme->bg_list;
    Color border = primary ? (Color){57, 178, 110, 255} : theme->card_border;
    Color text = primary ? (Color){244, 255, 248, 255} : theme->text_primary;
    Vector2 tm = gui_text_measure(label, 18.0f);

    DrawRectangleRounded(rect, 0.22f, 8, fill);
    DrawRectangleRoundedLines(rect, 0.22f, 8, border);
    gui_text(label,
             rect.x + (rect.width - tm.x) * 0.5f,
             rect.y + (rect.height - tm.y) * 0.5f,
             18.0f,
             text);
}

static int fin_modal_confirmar_gui(const char *titulo,
                                   const char *mensaje,
                                   const char *accion)
{
    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 760 : sw - 40;
        int panel_h = 240;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle btn_ok = {(float)panel_x + panel_w - 362.0f, (float)panel_y + panel_h - 54.0f, 168.0f, 38.0f};
        Rectangle btn_cancel = {(float)panel_x + panel_w - 184.0f, (float)panel_y + panel_h - 54.0f, 168.0f, 38.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header(titulo ? titulo : "CONFIRMAR", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text_wrapped(mensaje ? mensaje : "Confirma la accion",
                         (Rectangle){(float)panel_x + 24.0f, (float)panel_y + 52.0f, (float)panel_w - 48.0f, 86.0f},
                         20.0f,
                         gui_get_active_theme()->text_primary);

        fin_draw_action_button(btn_ok, accion ? accion : "Confirmar", 1);
        fin_draw_action_button(btn_cancel, "Cancelar", 0);
        gui_draw_footer_hint("ENTER: confirmar | ESC: cancelar", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_ok)) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        {
            return 1;
        }
        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_cancel)) || IsKeyPressed(KEY_ESCAPE))
        {
            return 0;
        }
    }

    return 0;
}

static void fin_mostrar_info_gui(const char *titulo, const char *mensaje)
{
    (void)fin_modal_confirmar_gui(titulo ? titulo : "INFORMACION",
                                  mensaje ? mensaje : "Operacion finalizada.",
                                  "Entendido");
}

static int fin_modal_input_texto_gui(const char *titulo,
                                     const char *etiqueta,
                                     const char *hint,
                                     char *out,
                                     size_t out_size,
                                     int permitir_vacio,
                                     FinInputMode mode)
{
    char texto[FIN_TEXT_MAX] = {0};
    int cursor = 0;
    int error_vacio = 0;

    if (!out || out_size == 0)
    {
        return 0;
    }

    if (out[0] != '\0')
    {
        strncpy_s(texto, sizeof(texto), out, _TRUNCATE);
        cursor = (int)strlen_s(texto, sizeof(texto));
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 1080 ? 860 : sw - 40;
        int panel_h = 280;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle input_rect = {(float)panel_x + 26.0f, (float)panel_y + 102.0f, (float)panel_w - 52.0f, 44.0f};
        Rectangle btn_ok = {(float)panel_x + panel_w - 362.0f, (float)panel_y + panel_h - 54.0f, 168.0f, 38.0f};
        Rectangle btn_cancel = {(float)panel_x + panel_w - 184.0f, (float)panel_y + panel_h - 54.0f, 168.0f, 38.0f};
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        int key = GetCharPressed();

        while (key > 0)
        {
            if (fin_input_char_valido((unsigned int)key, mode) &&
                cursor < (int)(out_size - 1) &&
                cursor < (int)(sizeof(texto) - 1))
            {
                texto[cursor++] = (char)key;
                texto[cursor] = '\0';
            }
            key = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE) && cursor > 0)
        {
            texto[--cursor] = '\0';
        }

        BeginDrawing();
        ClearBackground(gui_get_active_theme()->bg_main);
        gui_draw_module_header(titulo ? titulo : "ENTRADA", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, gui_get_active_theme()->card_border);

        gui_text(etiqueta ? etiqueta : "Ingrese un valor:",
                 (float)panel_x + 26.0f,
                 (float)panel_y + 56.0f,
                 20.0f,
                 gui_get_active_theme()->text_primary);

        DrawRectangleRounded(input_rect, 0.18f, 8, gui_get_active_theme()->bg_list);
        DrawRectangleRoundedLines(input_rect, 0.18f, 8, gui_get_active_theme()->card_border);
        gui_text_truncated(texto, input_rect.x + 10.0f, input_rect.y + 11.0f, 18.0f, input_rect.width - 20.0f, gui_get_active_theme()->text_primary);

        if (error_vacio)
        {
            gui_text("El campo no puede estar vacio.",
                     (float)panel_x + 26.0f,
                     (float)panel_y + 154.0f,
                     17.0f,
                     (Color){238, 121, 121, 255});
        }
        else if (hint && hint[0] != '\0')
        {
            gui_text(hint,
                     (float)panel_x + 26.0f,
                     (float)panel_y + 154.0f,
                     16.0f,
                     gui_get_active_theme()->text_muted);
        }

        fin_draw_action_button(btn_ok, "Guardar", 1);
        fin_draw_action_button(btn_cancel, "Cancelar", 0);
        gui_draw_footer_hint("ENTER: confirmar | ESC: cancelar | BACKSPACE: borrar", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_ok)) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        {
            if (!permitir_vacio && texto[0] == '\0')
            {
                error_vacio = 1;
                continue;
            }

            strncpy_s(out, out_size, texto, _TRUNCATE);
            return 1;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_cancel)) || IsKeyPressed(KEY_ESCAPE))
        {
            return 0;
        }
    }

    return 0;
}

static int fin_parse_int(const char *text, int *out)
{
    char *end = NULL;
    long v;

    if (!text || text[0] == '\0' || !out)
    {
        return 0;
    }

    v = strtol(text, &end, 10);
    if (!end || *end != '\0')
    {
        return 0;
    }

    if (v < INT_MIN || v > INT_MAX)
    {
        return 0;
    }

    *out = (int)v;
    return 1;
}

static int fin_modal_input_entero_gui(const char *titulo,
                                      const char *etiqueta,
                                      const char *hint,
                                      int min_value,
                                      int *valor_out)
{
    char texto[32] = {0};
    int valor = 0;

    if (!valor_out)
    {
        return 0;
    }

    while (1)
    {
        if (!fin_modal_input_texto_gui(titulo,
                                       etiqueta,
                                       hint,
                                       texto,
                                       sizeof(texto),
                                       0,
                                       FIN_INPUT_NUMERIC))
        {
            return 0;
        }

        if (fin_parse_int(texto, &valor) && valor >= min_value)
        {
            *valor_out = valor;
            return 1;
        }

        (void)fin_modal_confirmar_gui("VALOR INVALIDO",
                                      "Ingrese un numero valido dentro del rango permitido.",
                                      "Entendido");
    }
}

static int fin_modal_selector_opcion_gui(const char *titulo,
                                         const char *columna,
                                         const char **opciones,
                                         int cantidad,
                                         int seleccion_inicial,
                                         int *seleccion_out)
{
    int selected;
    int scroll = 0;
    const int row_h = 34;

    if (!seleccion_out || !opciones || cantidad <= 0)
    {
        return 0;
    }

    selected = seleccion_inicial;
    if (selected < 0 || selected >= cantidad)
    {
        selected = 0;
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = 72;
        int panel_y = 120;
        int panel_w = sw - 144;
        int panel_h = sh - 190;
        int content_y = panel_y + 32;
        int content_h = panel_h - 32;
        int visible_rows;
        int max_scroll;

        if (panel_w < 560)
        {
            panel_w = 560;
            panel_x = (sw - panel_w) / 2;
        }

        visible_rows = content_h / row_h;
        if (visible_rows < 1)
        {
            visible_rows = 1;
        }
        max_scroll = (cantidad > visible_rows) ? (cantidad - visible_rows) : 0;

        if (GetMouseWheelMove() > 0.01f) scroll -= 3;
        if (GetMouseWheelMove() < -0.01f) scroll += 3;
        if (IsKeyPressed(KEY_UP)) selected--;
        if (IsKeyPressed(KEY_DOWN)) selected++;

        if (selected < 0) selected = 0;
        if (selected >= cantidad) selected = cantidad - 1;

        if (selected < scroll) scroll = selected;
        if (selected >= scroll + visible_rows) scroll = selected - visible_rows + 1;
        if (scroll < 0) scroll = 0;
        if (scroll > max_scroll) scroll = max_scroll;

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header(titulo, sw);

        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                            "ID", 12.0f,
                            columna ? columna : "OPCION", 80.0f);

        BeginScissorMode(panel_x, content_y, panel_w, content_h);
        for (int i = scroll; i < cantidad; i++)
        {
            int row = i - scroll;
            int y = content_y + row * row_h;
            Rectangle fila;
            int hovered;
            int is_selected;

            if (row >= visible_rows)
            {
                break;
            }

            fila = (Rectangle){(float)(panel_x + 6), (float)y, (float)(panel_w - 12), (float)(row_h - 2)};
            hovered = CheckCollisionPointRec(GetMousePosition(), fila) ? 1 : 0;
            is_selected = (i == selected) ? 1 : 0;

            gui_draw_list_row_bg(fila, row, hovered || is_selected);
            if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                *seleccion_out = i;
                EndScissorMode();
                EndDrawing();
                return 1;
            }

            gui_text(TextFormat("%2d", i + 1), (float)(panel_x + 12), (float)(y + 7), 18.0f, (Color){220, 238, 225, 255});
            gui_text(opciones[i], (float)(panel_x + 80), (float)(y + 7), 18.0f,
                     is_selected ? (Color){183, 247, 206, 255} : (Color){233, 247, 236, 255});
        }
        EndScissorMode();

        gui_draw_footer_hint("Click o ENTER para confirmar | Flechas: mover | ESC: cancelar", (float)panel_x, sh);
        EndDrawing();

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        {
            *seleccion_out = selected;
            return 1;
        }

        if (IsKeyPressed(KEY_ESCAPE))
        {
            return 0;
        }
    }

    return 0;
}

static int fin_cargar_transacciones_gui_rows(FinTransaccionRow **rows_out, int *count_out, int limit)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql_all = "SELECT id, fecha, tipo, categoria, descripcion, monto FROM financiamiento ORDER BY fecha DESC, id DESC;";
    const char *sql_limit = "SELECT id, fecha, tipo, categoria, descripcion, monto FROM financiamiento ORDER BY fecha DESC, id DESC LIMIT ?;";
    FinTransaccionRow *rows = NULL;
    int count = 0;
    int cap = 0;

    if (!rows_out || !count_out)
    {
        return 0;
    }
    *rows_out = NULL;
    *count_out = 0;

    if (!preparar_stmt((limit > 0) ? sql_limit : sql_all, &stmt))
    {
        return 0;
    }

    if (limit > 0)
    {
        sqlite3_bind_int(stmt, 1, limit);
    }

    cap = 16;
    rows = (FinTransaccionRow *)calloc((size_t)cap, sizeof(FinTransaccionRow));
    if (!rows)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id;
        const char *fecha_db;
        int tipo;
        int categoria;
        const char *descripcion;
        int monto;
        char fecha_display[20] = {0};

        if (count >= cap)
        {
            int new_cap = cap * 2;
            FinTransaccionRow *tmp = (FinTransaccionRow *)realloc(rows, (size_t)new_cap * sizeof(FinTransaccionRow));
            if (!tmp)
            {
                free(rows);
                sqlite3_finalize(stmt);
                return 0;
            }
            rows = tmp;
            cap = new_cap;
        }

        id = sqlite3_column_int(stmt, 0);
        fecha_db = (const char *)sqlite3_column_text(stmt, 1);
        tipo = sqlite3_column_int(stmt, 2);
        categoria = sqlite3_column_int(stmt, 3);
        descripcion = (const char *)sqlite3_column_text(stmt, 4);
        monto = sqlite3_column_int(stmt, 5);

        if (fecha_db && fecha_db[0] != '\0')
        {
            mostrar_fecha_display(fecha_db, fecha_display, sizeof(fecha_display));
        }
        else
        {
            strcpy_s(fecha_display, sizeof(fecha_display), "(sin fecha)");
        }

        rows[count].id = id;
        snprintf(rows[count].resumen,
                 sizeof(rows[count].resumen),
                 "%s | %s | %s | %s | $%s",
                 fecha_display,
                 get_nombre_tipo_transaccion(tipo),
                 get_nombre_categoria(categoria),
                 (descripcion && descripcion[0] != '\0') ? descripcion : "(sin descripcion)",
                 formato_monto(monto));
        count++;
    }

    sqlite3_finalize(stmt);

    if (count == 0)
    {
        free(rows);
        rows = NULL;
    }

    *rows_out = rows;
    *count_out = count;
    return 1;
}

static int fin_seleccionar_transaccion_gui(const char *titulo,
                                           const char *ayuda,
                                           int limit,
                                           int *id_out)
{
    FinTransaccionRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    int selected = 0;
    const int row_h = 34;

    if (!id_out)
    {
        return 0;
    }
    *id_out = 0;

    if (!fin_cargar_transacciones_gui_rows(&rows, &count, limit))
    {
        return 0;
    }

    if (count == 0)
    {
        free(rows);
        fin_mostrar_info_gui(titulo ? titulo : "TRANSACCIONES",
                             "No hay transacciones registradas.");
        return 0;
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = 60;
        int panel_y = 110;
        int panel_w = sw - 120;
        int panel_h = sh - 180;
        int content_y = panel_y + 32;
        int content_h = panel_h - 32;
        int visible_rows;
        int max_scroll;

        visible_rows = content_h / row_h;
        if (visible_rows < 1)
        {
            visible_rows = 1;
        }
        max_scroll = (count > visible_rows) ? (count - visible_rows) : 0;

        if (GetMouseWheelMove() > 0.01f) scroll -= 3;
        if (GetMouseWheelMove() < -0.01f) scroll += 3;
        if (IsKeyPressed(KEY_UP)) selected--;
        if (IsKeyPressed(KEY_DOWN)) selected++;

        if (selected < 0) selected = 0;
        if (selected >= count) selected = count - 1;

        if (selected < scroll) scroll = selected;
        if (selected >= scroll + visible_rows) scroll = selected - visible_rows + 1;
        if (scroll < 0) scroll = 0;
        if (scroll > max_scroll) scroll = max_scroll;

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header(titulo ? titulo : "SELECCIONAR TRANSACCION", sw);

        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                            "ID", 12.0f,
                            "DETALLE", 80.0f);

        BeginScissorMode(panel_x, content_y, panel_w, content_h);
        for (int i = scroll; i < count; i++)
        {
            int row = i - scroll;
            int y = content_y + row * row_h;
            Rectangle fila;
            int hovered;
            int is_selected;

            if (row >= visible_rows)
            {
                break;
            }

            fila = (Rectangle){(float)(panel_x + 6), (float)y, (float)(panel_w - 12), (float)(row_h - 2)};
            hovered = CheckCollisionPointRec(GetMousePosition(), fila) ? 1 : 0;
            is_selected = (i == selected) ? 1 : 0;
            gui_draw_list_row_bg(fila, row, hovered || is_selected);

            if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                *id_out = rows[i].id;
                EndScissorMode();
                EndDrawing();
                free(rows);
                return 1;
            }

            gui_text(TextFormat("%3d", rows[i].id), (float)(panel_x + 12), (float)(y + 7), 18.0f, (Color){220, 238, 225, 255});
            gui_text_truncated(rows[i].resumen,
                               (float)(panel_x + 80),
                               (float)(y + 7),
                               18.0f,
                               (float)panel_w - 96.0f,
                               is_selected ? (Color){183, 247, 206, 255} : (Color){233, 247, 236, 255});
        }
        EndScissorMode();

        gui_draw_footer_hint(ayuda ? ayuda : "Click o ENTER para confirmar | Flechas: mover | ESC: cancelar", (float)panel_x, sh);
        EndDrawing();

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        {
            *id_out = rows[selected].id;
            free(rows);
            return 1;
        }
        if (IsKeyPressed(KEY_ESCAPE))
        {
            break;
        }
    }

    free(rows);
    return 0;
}

static int fin_cargar_partidos_gui_rows(FinPartidoRow **rows_out, int *count_out)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT p.id, p.fecha_hora, can.nombre, p.goles, p.asistencias, c.nombre, p.resultado "
        "FROM partido p "
        "LEFT JOIN camiseta c ON p.camiseta_id = c.id "
        "LEFT JOIN cancha can ON p.cancha_id = can.id "
        "ORDER BY p.fecha_hora DESC, p.id DESC;";
    FinPartidoRow *rows = NULL;
    int count = 0;
    int cap = 0;

    if (!rows_out || !count_out)
    {
        return 0;
    }

    *rows_out = NULL;
    *count_out = 0;

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    cap = 16;
    rows = (FinPartidoRow *)calloc((size_t)cap, sizeof(FinPartidoRow));
    if (!rows)
    {
        sqlite3_finalize(stmt);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id;
        const char *fecha_hora;
        const char *cancha;
        int goles;
        int asistencias;
        const char *camiseta;
        int resultado;
        char fecha_display[32] = {0};

        if (count >= cap)
        {
            int new_cap = cap * 2;
            FinPartidoRow *tmp = (FinPartidoRow *)realloc(rows, (size_t)new_cap * sizeof(FinPartidoRow));
            if (!tmp)
            {
                free(rows);
                sqlite3_finalize(stmt);
                return 0;
            }
            rows = tmp;
            cap = new_cap;
        }

        id = sqlite3_column_int(stmt, 0);
        fecha_hora = (const char *)sqlite3_column_text(stmt, 1);
        cancha = (const char *)sqlite3_column_text(stmt, 2);
        goles = sqlite3_column_int(stmt, 3);
        asistencias = sqlite3_column_int(stmt, 4);
        camiseta = (const char *)sqlite3_column_text(stmt, 5);
        resultado = sqlite3_column_int(stmt, 6);

        if (fecha_hora && fecha_hora[0] != '\0')
        {
            format_date_for_display(fecha_hora, fecha_display, sizeof(fecha_display));
        }
        else
        {
            strcpy_s(fecha_display, sizeof(fecha_display), "(sin fecha)");
        }

        rows[count].id = id;
        snprintf(rows[count].resumen,
                 sizeof(rows[count].resumen),
                 "%s | Cancha:%s | G:%d A:%d | Camiseta:%s | %s",
                 fecha_display,
                 (cancha && cancha[0] != '\0') ? cancha : "(sin cancha)",
                 goles,
                 asistencias,
                 (camiseta && camiseta[0] != '\0') ? camiseta : "(sin camiseta)",
                 resultado_to_text(resultado));
        count++;
    }

    sqlite3_finalize(stmt);

    if (count == 0)
    {
        free(rows);
        rows = NULL;
    }

    *rows_out = rows;
    *count_out = count;
    return 1;
}

static int fin_seleccionar_partido_gui(const char *titulo,
                                       const char *ayuda,
                                       int *id_out)
{
    FinPartidoRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    int selected = 0;
    const int row_h = 34;

    if (!id_out)
    {
        return 0;
    }
    *id_out = 0;

    if (!fin_cargar_partidos_gui_rows(&rows, &count))
    {
        return 0;
    }

    if (count == 0)
    {
        free(rows);
        fin_mostrar_info_gui("SIN PARTIDOS",
                             "No hay partidos registrados. Cree uno en el modulo Partidos.");
        return 0;
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = 60;
        int panel_y = 110;
        int panel_w = sw - 120;
        int panel_h = sh - 180;
        int content_y = panel_y + 32;
        int content_h = panel_h - 32;
        int visible_rows;
        int max_scroll;

        visible_rows = content_h / row_h;
        if (visible_rows < 1)
        {
            visible_rows = 1;
        }
        max_scroll = (count > visible_rows) ? (count - visible_rows) : 0;

        if (GetMouseWheelMove() > 0.01f) scroll -= 3;
        if (GetMouseWheelMove() < -0.01f) scroll += 3;
        if (IsKeyPressed(KEY_UP)) selected--;
        if (IsKeyPressed(KEY_DOWN)) selected++;

        if (selected < 0) selected = 0;
        if (selected >= count) selected = count - 1;

        if (selected < scroll) scroll = selected;
        if (selected >= scroll + visible_rows) scroll = selected - visible_rows + 1;
        if (scroll < 0) scroll = 0;
        if (scroll > max_scroll) scroll = max_scroll;

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header(titulo ? titulo : "SELECCIONAR PARTIDO", sw);

        gui_draw_list_shell((Rectangle){(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
                            "ID", 12.0f,
                            "PARTIDO", 80.0f);

        BeginScissorMode(panel_x, content_y, panel_w, content_h);
        for (int i = scroll; i < count; i++)
        {
            int row = i - scroll;
            int y = content_y + row * row_h;
            Rectangle fila;
            int hovered;
            int is_selected;

            if (row >= visible_rows)
            {
                break;
            }

            fila = (Rectangle){(float)(panel_x + 6), (float)y, (float)(panel_w - 12), (float)(row_h - 2)};
            hovered = CheckCollisionPointRec(GetMousePosition(), fila) ? 1 : 0;
            is_selected = (i == selected) ? 1 : 0;
            gui_draw_list_row_bg(fila, row, hovered || is_selected);

            if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                *id_out = rows[i].id;
                EndScissorMode();
                EndDrawing();
                free(rows);
                return 1;
            }

            gui_text(TextFormat("%3d", rows[i].id), (float)(panel_x + 12), (float)(y + 7), 18.0f, (Color){220, 238, 225, 255});
            gui_text_truncated(rows[i].resumen,
                               (float)(panel_x + 80),
                               (float)(y + 7),
                               18.0f,
                               (float)panel_w - 96.0f,
                               is_selected ? (Color){183, 247, 206, 255} : (Color){233, 247, 236, 255});
        }
        EndScissorMode();

        gui_draw_footer_hint(ayuda ? ayuda : "Click o ENTER para confirmar | Flechas: mover | ESC: cancelar", (float)panel_x, sh);
        EndDrawing();

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        {
            *id_out = rows[selected].id;
            free(rows);
            return 1;
        }
        if (IsKeyPressed(KEY_ESCAPE))
        {
            break;
        }
    }

    free(rows);
    return 0;
}

static int fin_obtener_transaccion_por_id(int id_transaccion, TransaccionFinanciera *transaccion)
{
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT fecha, tipo, categoria, descripcion, monto, item_especifico FROM financiamiento WHERE id = ?;";

    if (!transaccion)
    {
        return 0;
    }

    memset(transaccion, 0, sizeof(*transaccion));
    transaccion->id = id_transaccion;

    if (!preparar_stmt(sql, &stmt))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, id_transaccion);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        strncpy_s(transaccion->fecha, sizeof(transaccion->fecha),
                  sqlite3_column_text(stmt, 0) ? (const char *)sqlite3_column_text(stmt, 0) : "",
                  _TRUNCATE);
        transaccion->tipo = sqlite3_column_int(stmt, 1);
        transaccion->categoria = sqlite3_column_int(stmt, 2);
        strncpy_s(transaccion->descripcion, sizeof(transaccion->descripcion),
                  sqlite3_column_text(stmt, 3) ? (const char *)sqlite3_column_text(stmt, 3) : "",
                  _TRUNCATE);
        transaccion->monto = sqlite3_column_int(stmt, 4);
        strncpy_s(transaccion->item_especifico, sizeof(transaccion->item_especifico),
                  sqlite3_column_text(stmt, 5) ? (const char *)sqlite3_column_text(stmt, 5) : "",
                  _TRUNCATE);
        sqlite3_finalize(stmt);
        return 1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

static int fin_normalizar_fecha_input(const char *input, char *fecha_out, size_t out_size)
{
    if (!input || !fecha_out || out_size < 11)
    {
        return 0;
    }

    if (input[0] == '\0')
    {
        obtener_fecha_actual(fecha_out);
        return 1;
    }

    if (strchr(input, '/') != NULL)
    {
        return convertir_fecha_ddmmyyyy_a_yyyymmdd(input, fecha_out);
    }

    if (strlen_s(input, 32) == 10 &&
        isdigit((unsigned char)input[0]) && isdigit((unsigned char)input[1]) &&
        isdigit((unsigned char)input[2]) && isdigit((unsigned char)input[3]) &&
        input[4] == '-' &&
        isdigit((unsigned char)input[5]) && isdigit((unsigned char)input[6]) &&
        input[7] == '-' &&
        isdigit((unsigned char)input[8]) && isdigit((unsigned char)input[9]))
    {
        strncpy_s(fecha_out, out_size, input, _TRUNCATE);
        return 1;
    }

    return 0;
}
#endif

static int seleccionar_tipo_transaccion(TransaccionFinanciera *transaccion)
{
#ifdef UNIT_TEST
    printf("\nSeleccione el tipo de transaccion:\n");
    printf("1. Ingreso\n");
    printf("2. Gasto\n");
    printf("0. Volver\n");

    int opcion_tipo = input_int(">");
    switch (opcion_tipo)
    {
    case 1:
        transaccion->tipo = INGRESO;
        return 1;
    case 2:
        transaccion->tipo = GASTO;
        return 1;
    case 0:
        printf("Operacion cancelada.\n");
        pause_console();
        return 0;
    default:
        printf("Opcion invalida. Cancelando.\n");
        pause_console();
        return 0;
    }
#else
    const char *opciones[] = {"Ingreso", "Gasto"};
    int seleccion = 0;

    if (!transaccion)
    {
        return 0;
    }

    if (!fin_modal_selector_opcion_gui("TIPO DE TRANSACCION",
                                       "TIPO",
                                       opciones,
                                       ARRAY_COUNT(opciones),
                                       (int)transaccion->tipo,
                                       &seleccion))
    {
        return 0;
    }

    transaccion->tipo = (seleccion == 0) ? INGRESO : GASTO;
    return 1;
#endif
}

static int seleccionar_categoria_transaccion(TransaccionFinanciera *transaccion)
{
#ifdef UNIT_TEST
    printf("\nSeleccione la categoria:\n");
    printf("1. Transporte\n");
    printf("2. Equipamiento\n");
    printf("3. Cuotas\n");
    printf("4. Torneos\n");
    printf("5. Arbitraje\n");
    printf("6. Canchas\n");
    printf("7. Medicina\n");
    printf("8. Otros\n");

    int opcion_categoria = input_int(">");
    switch (opcion_categoria)
    {
    case 1:
        transaccion->categoria = TRANSPORTE;
        return 1;
    case 2:
        transaccion->categoria = EQUIPAMIENTO;
        return 1;
    case 3:
        transaccion->categoria = CUOTAS;
        return 1;
    case 4:
        transaccion->categoria = TORNEOS;
        return 1;
    case 5:
        transaccion->categoria = ARBITRAJE;
        return 1;
    case 6:
        transaccion->categoria = CANCHAS;
        return 1;
    case 7:
        transaccion->categoria = MEDICINA;
        return 1;
    case 8:
        transaccion->categoria = OTROS;
        return 1;
    default:
        printf("Opcion invalida. Cancelando.\n");
        pause_console();
        return 0;
    }
#else
    const char *opciones[] =
    {
        "Transporte",
        "Equipamiento",
        "Cuotas",
        "Torneos",
        "Arbitraje",
        "Canchas",
        "Medicina",
        "Otros"
    };
    int seleccion = 0;

    if (!transaccion)
    {
        return 0;
    }

    if (!fin_modal_selector_opcion_gui("CATEGORIA FINANCIERA",
                                       "CATEGORIA",
                                       opciones,
                                       ARRAY_COUNT(opciones),
                                       (int)transaccion->categoria,
                                       &seleccion))
    {
        return 0;
    }

    transaccion->categoria = (CategoriaFinanciera)seleccion;
    return 1;
#endif
}

static int verificar_partido_precio_asignado(int id_partido)
{
    sqlite3_stmt *stmt_check_precio;
    const char *sql_check_precio = "SELECT precio FROM partido WHERE id = ?";

    if (!preparar_stmt(sql_check_precio, &stmt_check_precio))
    {
        return 1;
    }

    sqlite3_bind_int(stmt_check_precio, 1, id_partido);
    if (sqlite3_step(stmt_check_precio) == SQLITE_ROW)
    {
        int precio = sqlite3_column_int(stmt_check_precio, 0);
        if (precio > 0)
        {
#ifdef UNIT_TEST
            printf("El partido ya tiene un precio asignado ($%s).\n", formato_monto(precio));
            printf("No se puede agregar otra transaccion para este partido.\n");
            sqlite3_finalize(stmt_check_precio);
            pause_console();
            return 0;
#else
            char mensaje[256];
            snprintf(mensaje,
                     sizeof(mensaje),
                     "El partido ya tiene un precio asignado ($%s). No se puede agregar otra transaccion para este partido.",
                     formato_monto(precio));
            sqlite3_finalize(stmt_check_precio);
            fin_mostrar_info_gui("PARTIDO CON PRECIO", mensaje);
            return 0;
#endif
        }
    }
    sqlite3_finalize(stmt_check_precio);
    return 1;
}

static int verificar_partido_transaccion_asociada(int id_partido)
{
    sqlite3_stmt *stmt_check;
    const char *sql_check = "SELECT COUNT(*) FROM financiamiento WHERE tipo = 1 AND categoria = 6 AND item_especifico LIKE ?";
    char item_pattern[256];
    snprintf(item_pattern, sizeof(item_pattern), "Partido ID: %d%%", id_partido);

    if (!preparar_stmt(sql_check, &stmt_check))
    {
        return 1;
    }

    sqlite3_bind_text(stmt_check, 1, item_pattern, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt_check) == SQLITE_ROW)
    {
        int count = sqlite3_column_int(stmt_check, 0);
        if (count > 0)
        {
#ifdef UNIT_TEST
            printf("El partido ya tiene una transaccion financiera asociada (precio ya asignado).\n");
            printf("No se puede agregar otra transaccion para este partido.\n");
            sqlite3_finalize(stmt_check);
            pause_console();
            return 0;
#else
            sqlite3_finalize(stmt_check);
            fin_mostrar_info_gui("PARTIDO YA ASOCIADO",
                                 "El partido ya tiene una transaccion financiera asociada. No se puede agregar otra.");
            return 0;
#endif
        }
    }
    sqlite3_finalize(stmt_check);
    return 1;
}

static void construir_item_partido(TransaccionFinanciera *transaccion, int id_partido)
{
    sqlite3_stmt *stmt_partido;
    const char *sql_partido = "SELECT p.id, can.nombre, fecha_hora, goles, asistencias, c.nombre, resultado, clima, dia "
                              "FROM partido p JOIN camiseta c ON p.camiseta_id = c.id "
                              "JOIN cancha can ON p.cancha_id = can.id WHERE p.id = ?";

    if (!preparar_stmt(sql_partido, &stmt_partido))
    {
        snprintf(transaccion->item_especifico, sizeof(transaccion->item_especifico), "Partido ID: %d", id_partido);
        return;
    }

    sqlite3_bind_int(stmt_partido, 1, id_partido);
    if (sqlite3_step(stmt_partido) == SQLITE_ROW)
    {
        char fecha_formateada[20];
        format_date_for_display((const char *)sqlite3_column_text(stmt_partido, 2), fecha_formateada, sizeof(fecha_formateada));

        snprintf(transaccion->item_especifico, sizeof(transaccion->item_especifico),
                 "(%d |Cancha:%s |Fecha:%s | G:%d A:%d |Camiseta:%s | %s |Clima:%s |Dia:%s)",
                 sqlite3_column_int(stmt_partido, 0),
                 sqlite3_column_text(stmt_partido, 1),
                 fecha_formateada,
                 sqlite3_column_int(stmt_partido, 3),
                 sqlite3_column_int(stmt_partido, 4),
                 sqlite3_column_text(stmt_partido, 5),
                 resultado_to_text(sqlite3_column_int(stmt_partido, 6)),
                 clima_to_text(sqlite3_column_int(stmt_partido, 7)),
                 dia_to_text(sqlite3_column_int(stmt_partido, 8)));
    }
    else
    {
        snprintf(transaccion->item_especifico, sizeof(transaccion->item_especifico), "Partido ID: %d (no encontrado)", id_partido);
    }
    sqlite3_finalize(stmt_partido);
}

static int completar_item_especifico(TransaccionFinanciera *transaccion)
{
    if (transaccion->tipo == GASTO && transaccion->categoria == CANCHAS)
    {
#ifdef UNIT_TEST
        printf("\n=== PARTIDOS DISPONIBLES ===\n");
        listar_partidos();
        printf("\n");

        int id_partido = 0;
        while (1)
        {
            id_partido = input_int("Ingrese el ID del partido (0 para cancelar): ");
            if (id_partido == 0)
                return 0;
            if (existe_id("partido", id_partido))
                break;
            printf("ID de partido invalido. Intente nuevamente.\n");
        }

        if (!verificar_partido_precio_asignado(id_partido))
        {
            return 0;
        }

        if (!verificar_partido_transaccion_asociada(id_partido))
        {
            return 0;
        }

        construir_item_partido(transaccion, id_partido);
        return 1;
#else
        int id_partido = 0;

        if (!fin_seleccionar_partido_gui("GASTO DE CANCHA",
                                         "Seleccione el partido al que desea asignar el gasto",
                                         &id_partido))
        {
            return 0;
        }

        if (!verificar_partido_precio_asignado(id_partido))
        {
            return 0;
        }

        if (!verificar_partido_transaccion_asociada(id_partido))
        {
            return 0;
        }

        construir_item_partido(transaccion, id_partido);
        return 1;
#endif
    }

#ifdef UNIT_TEST
    printf("Item especifico (opcional, ej: 'Botines Nike', 'Cuota enero'): ");
    input_string("", transaccion->item_especifico, sizeof(transaccion->item_especifico));
#else
    if (!fin_modal_input_texto_gui("ITEM ESPECIFICO",
                                   "Item especifico (opcional)",
                                   "Ej: Botines Nike, Cuota enero",
                                   transaccion->item_especifico,
                                   sizeof(transaccion->item_especifico),
                                   1,
                                   FIN_INPUT_ALNUM))
    {
        transaccion->item_especifico[0] = '\0';
    }
#endif
    return 1;
}

static void guardar_transaccion_db(const TransaccionFinanciera *transaccion)
{
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO financiamiento (id, fecha, tipo, categoria, descripcion, monto, item_especifico) VALUES (?, ?, ?, ?, ?, ?, ?);";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_int(stmt, 1, transaccion->id);
        sqlite3_bind_text(stmt, 2, transaccion->fecha, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, transaccion->tipo);
        sqlite3_bind_int(stmt, 4, transaccion->categoria);
        sqlite3_bind_text(stmt, 5, transaccion->descripcion, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 6, transaccion->monto);
        sqlite3_bind_text(stmt, 7, transaccion->item_especifico, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Transaccion guardada exitosamente con ID: %lld\n", sqlite3_last_insert_rowid(db));
        }
        else
        {
            printf("Error al guardar la transaccion: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
    }
}

static void truncar_resultado_partido(char const *item_especifico)
{
    if (!item_especifico || item_especifico[0] == '\0')
    {
        return;
    }

    char *pos = strstr(item_especifico, "VICTORIA");
    if (pos)
    {
        *(pos + strlen("VICTORIA")) = '\0';
        return;
    }

    pos = strstr(item_especifico, "EMPATE");
    if (pos)
    {
        *(pos + strlen("EMPATE")) = '\0';
        return;
    }

    pos = strstr(item_especifico, "DERROTA");
    if (pos)
    {
        *(pos + strlen("DERROTA")) = '\0';
    }
}


/**
 * @brief Agregar una nueva transaccion financiera
 */
void agregar_transaccion()
{
    clear_screen();
    print_header("AGREGAR TRANSACCION FINANCIERA");

    TransaccionFinanciera transaccion;

    // Fecha - usar fecha actual automaticamente (formato YYYY-MM-DD para BD)
    time_t t = time(NULL);
    struct tm tm;
    localtime_s(&tm, &t);
    strftime(transaccion.fecha, sizeof(transaccion.fecha), "%Y-%m-%d", &tm);

    if (!seleccionar_tipo_transaccion(&transaccion))
    {
        return;
    }

    if (!seleccionar_categoria_transaccion(&transaccion))
    {
        return;
    }

    // Descripcion
    solicitar_texto_no_vacio("Descripcion: ", transaccion.descripcion, sizeof(transaccion.descripcion));

    // Monto
    transaccion.monto = solicitar_monto_no_negativo("Monto: ");
    if (transaccion.monto < 0)
    {
        return;
    }

    if (!completar_item_especifico(&transaccion))
    {
        return;
    }

    // Obtener el ID y asignarlo a la transaccion
    transaccion.id = (int)obtener_siguiente_id("financiamiento");

    // Mostrar resumen y confirmar
    clear_screen();
    print_header("CONFIRMAR TRANSACCION");
    mostrar_transaccion(&transaccion);

    #ifdef UNIT_TEST
    if (confirmar("Desea guardar esta transaccion?"))
    #else
    if (fin_modal_confirmar_gui("CONFIRMAR TRANSACCION",
                                "Desea guardar esta transaccion?",
                                "Guardar"))
    #endif
    {
        guardar_transaccion_db(&transaccion);
    }
    else
    {
        printf("Transaccion cancelada.\n");
    }

    pause_console();
}

/**
 * @brief Gestion de presupuestos mensuales - menu principal
 */
void menu_presupuestos_mensuales()
{
#ifdef UNIT_TEST
    MenuItem items[] =
    {
        {1, "Configurar Presupuesto Mensual", configurar_presupuesto_mensual},
        {2, "Ver Estado del Presupuesto", ver_estado_presupuesto},
        {3, "Verificar Alertas de Gasto", verificar_alertas_presupuesto},
        {0, "Volver", NULL}
    };

    ejecutar_menu_estandar("PRESUPUESTOS MENSUALES", items, 4);
    return;
#endif
    clear_screen();
    print_header("PRESUPUESTOS MENSUALES");

    // Verificar alertas automaticamente al entrar al menu
    verificar_alertas_presupuesto();

    MenuItem items[] =
    {
        {1, "Configurar Presupuesto Mensual", configurar_presupuesto_mensual},
        {2, "Ver Estado del Presupuesto", ver_estado_presupuesto},
        {3, "Verificar Alertas de Gasto", verificar_alertas_presupuesto},
        {0, "Volver", NULL}
    };

    ejecutar_menu_estandar("PRESUPUESTOS MENSUALES", items, 4);
}

/**
 * @brief Recopilar datos del presupuesto desde la entrada del usuario
 */
static int recopilar_datos_presupuesto(PresupuestoMensual *presupuesto)
{
#ifdef UNIT_TEST
    presupuesto->presupuesto_total = input_int("Presupuesto total mensual: ");
    presupuesto->limite_gasto = input_int("Limite maximo de gasto mensual: ");

    printf("\nHabilitar alertas automaticas? (1=Si, 0=No): ");
    presupuesto->alertas_habilitadas = input_int(">");
    if (presupuesto->alertas_habilitadas != 0 && presupuesto->alertas_habilitadas != 1)
    {
        presupuesto->alertas_habilitadas = 1; // Default a habilitado
    }
    return 1;
#else
    const char *opciones_alerta[] = {"Si", "No"};
    int seleccion_alerta = 0;

    if (!presupuesto)
    {
        return 0;
    }

    if (!fin_modal_input_entero_gui("PRESUPUESTO MENSUAL",
                                    "Presupuesto total mensual",
                                    "Ingrese un entero (0 o mayor)",
                                    0,
                                    &presupuesto->presupuesto_total))
    {
        return 0;
    }

    if (!fin_modal_input_entero_gui("PRESUPUESTO MENSUAL",
                                    "Limite maximo de gasto mensual",
                                    "Ingrese un entero (0 o mayor)",
                                    0,
                                    &presupuesto->limite_gasto))
    {
        return 0;
    }

    if (fin_modal_selector_opcion_gui("ALERTAS AUTOMATICAS",
                                      "HABILITAR",
                                      opciones_alerta,
                                      ARRAY_COUNT(opciones_alerta),
                                      0,
                                      &seleccion_alerta))
    {
        presupuesto->alertas_habilitadas = (seleccion_alerta == 0) ? 1 : 0;
    }
    else
    {
        presupuesto->alertas_habilitadas = 1;
    }
    return 1;
#endif
}

/**
 * @brief Verificar si existe un presupuesto para el mes dado
 */
static int presupuesto_existe(const char *mes_anio)
{
    sqlite3_stmt *stmt;
    const char *sql_check = "SELECT id FROM presupuesto_mensual WHERE mes_anio = ?;";

    if (!preparar_stmt(sql_check, &stmt))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, mes_anio, -1, SQLITE_STATIC);
    int existe = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return existe;
}

/**
 * @brief Actualizar presupuesto existente
 */
static void actualizar_presupuesto(const PresupuestoMensual *presupuesto)
{
    sqlite3_stmt *stmt;
    const char *sql_update = "UPDATE presupuesto_mensual SET presupuesto_total = ?, limite_gasto = ?, alertas_habilitadas = ?, fecha_modificacion = ? WHERE mes_anio = ?;";

    if (preparar_stmt(sql_update, &stmt))
    {
        sqlite3_bind_int(stmt, 1, presupuesto->presupuesto_total);
        sqlite3_bind_int(stmt, 2, presupuesto->limite_gasto);
        sqlite3_bind_int(stmt, 3, presupuesto->alertas_habilitadas);
        sqlite3_bind_text(stmt, 4, presupuesto->fecha_modificacion, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, presupuesto->mes_anio, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Presupuesto actualizado exitosamente.\n");
        }
        else
        {
            printf("Error al actualizar presupuesto: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Insertar nuevo presupuesto
 */
static void insertar_presupuesto(const PresupuestoMensual *presupuesto)
{
    sqlite3_stmt *stmt;
    const char *sql_insert = "INSERT INTO presupuesto_mensual (mes_anio, presupuesto_total, limite_gasto, alertas_habilitadas, fecha_creacion, fecha_modificacion) VALUES (?, ?, ?, ?, ?, ?);";

    if (preparar_stmt(sql_insert, &stmt))
    {
        sqlite3_bind_text(stmt, 1, presupuesto->mes_anio, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, presupuesto->presupuesto_total);
        sqlite3_bind_int(stmt, 3, presupuesto->limite_gasto);
        sqlite3_bind_int(stmt, 4, presupuesto->alertas_habilitadas);
        sqlite3_bind_text(stmt, 5, presupuesto->fecha_creacion, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, presupuesto->fecha_modificacion, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            printf("Presupuesto configurado exitosamente.\n");
        }
        else
        {
            printf("Error al guardar presupuesto: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Configurar presupuesto mensual
 */
void configurar_presupuesto_mensual()
{
    clear_screen();
    print_header("CONFIGURAR PRESUPUESTO MENSUAL");

    char mes_anio[32];
    obtener_mes_anio_actual(mes_anio);
    printf("Configurando presupuesto para: %s\n\n", mes_anio);

    PresupuestoMensual presupuesto;
    strcpy_s(presupuesto.mes_anio, sizeof(presupuesto.mes_anio), mes_anio);
    if (!recopilar_datos_presupuesto(&presupuesto))
    {
        return;
    }

    // Fechas
    obtener_fecha_actual(presupuesto.fecha_creacion);
    strcpy_s(presupuesto.fecha_modificacion, sizeof(presupuesto.fecha_modificacion), presupuesto.fecha_creacion);

    if (presupuesto_existe(mes_anio))
    {
        printf("\nYa existe un presupuesto para este mes. Desea actualizarlo?\n");
#ifdef UNIT_TEST
        if (confirmar(""))
#else
        if (fin_modal_confirmar_gui("ACTUALIZAR PRESUPUESTO",
                                    "Ya existe un presupuesto para este mes. Desea actualizarlo?",
                                    "Actualizar"))
#endif
        {
            actualizar_presupuesto(&presupuesto);
        }
    }
    else
    {
        insertar_presupuesto(&presupuesto);
    }

    pause_console();
}

/**
 * @brief Ver estado actual del presupuesto mensual
 */
void ver_estado_presupuesto()
{
    clear_screen();
    print_header("ESTADO DEL PRESUPUESTO MENSUAL");

    // Obtener mes actual
    char mes_anio[32];
    obtener_mes_anio_actual(mes_anio);

    printf("Mes actual: %s\n\n", mes_anio);

    PresupuestoMensual presupuesto;
    if (!obtener_presupuesto_mes_actual(&presupuesto))
    {
        printf("No hay presupuesto configurado para este mes.\n");
        printf("Use la opcion 'Configurar Presupuesto Mensual' para crear uno.\n");
        pause_console();
        return;
    }

    // Obtener gastos actuales
    int gastos_actuales = obtener_gastos_mes_actual();
    int ingresos_actuales = 0;

    // Obtener ingresos del mes
    sqlite3_stmt *stmt;
    const char *sql_ingresos = "SELECT SUM(monto) FROM financiamiento WHERE tipo = 0 AND strftime('%Y-%m', fecha) = ?;";

    if (preparar_stmt(sql_ingresos, &stmt))
    {
        sqlite3_bind_text(stmt, 1, mes_anio, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            ingresos_actuales = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    printf("=== ESTADO DEL PRESUPUESTO ===\n");
    printf("Presupuesto total: $");
    mostrar_monto(presupuesto.presupuesto_total);
    printf("Limite de gasto: $");
    mostrar_monto(presupuesto.limite_gasto);
    printf("Alertas habilitadas: %s\n\n", presupuesto.alertas_habilitadas ? "Si" : "No");

    printf("=== ESTADO ACTUAL ===\n");
    printf("Ingresos del mes: $");
    mostrar_monto(ingresos_actuales);
    printf("Gastos del mes: $");
    mostrar_monto(gastos_actuales);
    printf("Balance del mes: $");
    mostrar_monto(ingresos_actuales - gastos_actuales);

    // Calcular porcentajes
    double porcentaje_gasto = 0.0;
    if (presupuesto.limite_gasto > 0)
    {
        porcentaje_gasto = (gastos_actuales / (double)presupuesto.limite_gasto) * 100.0;
    }

    double porcentaje_presupuesto = 0.0;
    if (presupuesto.presupuesto_total > 0)
    {
        porcentaje_presupuesto = ((ingresos_actuales - gastos_actuales) / (double)presupuesto.presupuesto_total) * 100.0;
    }

    printf("\n=== INDICADORES ===\n");
    printf("Uso del limite de gasto: %.1f%%\n", porcentaje_gasto);
    printf("Cumplimiento del presupuesto: %.1f%%\n", porcentaje_presupuesto);

    // Alertas
    if (gastos_actuales > presupuesto.limite_gasto)
    {
        printf("\n");
        mostrar_alerta_exceso_gasto(gastos_actuales, presupuesto.limite_gasto);
    }
    else if (porcentaje_gasto > 80.0)
    {
        printf("\n¡ADVERTENCIA! Ha utilizado mas del 80%% del limite de gasto.\n");
    }

    pause_console();
}

/**
 * @brief Verificar limites de gasto y mostrar alertas
 */
void verificar_alertas_presupuesto()
{
    clear_screen();
    print_header("VERIFICACION DE ALERTAS DE PRESUPUESTO");

    PresupuestoMensual presupuesto;
    if (!obtener_presupuesto_mes_actual(&presupuesto))
    {
        printf("No hay presupuesto configurado para este mes.\n");
        pause_console();
        return;
    }

    if (!presupuesto.alertas_habilitadas)
    {
        printf("Las alertas de presupuesto estan deshabilitadas.\n");
        pause_console();
        return;
    }

    int gastos_actuales = obtener_gastos_mes_actual();

    printf("Limite de gasto configurado: $");
    mostrar_monto(presupuesto.limite_gasto);
    printf("Gastos actuales del mes: $");
    mostrar_monto(gastos_actuales);
    printf("\n");

    if (gastos_actuales > presupuesto.limite_gasto)
    {
        mostrar_alerta_exceso_gasto(gastos_actuales, presupuesto.limite_gasto);
    }
    else if (gastos_actuales > (presupuesto.limite_gasto * 0.9))
    {
        printf("¡ADVERTENCIA! Esta cerca del limite de gasto.\n");
        printf("Ha gastado el %.1f%% del limite permitido.\n",
               (gastos_actuales / (double)presupuesto.limite_gasto) * 100.0);
    }
    else if (gastos_actuales > (presupuesto.limite_gasto * 0.75))
    {
        printf("¡AVISO! Ha utilizado mas del 75%% del limite de gasto.\n");
        printf("Gastos actuales: %.1f%% del limite.\n",
               (gastos_actuales / (double)presupuesto.limite_gasto) * 100.0);
    }
    else
    {
        printf("✓ Los gastos actuales estan dentro de los limites permitidos.\n");
        printf("Ha utilizado el %.1f%% del limite de gasto.\n",
               (gastos_actuales / (double)presupuesto.limite_gasto) * 100.0);
    }

    pause_console();
}

/**
 * @brief Obtener gastos totales del mes actual
 */
int obtener_gastos_mes_actual()
{
    char mes_anio[32];
    obtener_mes_anio_actual(mes_anio);

    sqlite3_stmt *stmt;
    const char *sql = "SELECT SUM(monto) FROM financiamiento WHERE tipo = 1 AND strftime('%Y-%m', fecha) = ?;";
    int gastos = 0;

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_text(stmt, 1, mes_anio, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            gastos = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return gastos;
}

/**
 * @brief Obtener presupuesto y limite del mes actual
 */
int obtener_presupuesto_mes_actual(PresupuestoMensual *presupuesto)
{
    char mes_anio[32];
    obtener_mes_anio_actual(mes_anio);

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, presupuesto_total, limite_gasto, alertas_habilitadas, fecha_creacion, fecha_modificacion FROM presupuesto_mensual WHERE mes_anio = ?;";

    if (preparar_stmt(sql, &stmt))
    {
        sqlite3_bind_text(stmt, 1, mes_anio, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            presupuesto->id = sqlite3_column_int(stmt, 0);
            strcpy_s(presupuesto->mes_anio, sizeof(presupuesto->mes_anio), mes_anio);
            presupuesto->presupuesto_total = sqlite3_column_int(stmt, 1);
            presupuesto->limite_gasto = sqlite3_column_int(stmt, 2);
            presupuesto->alertas_habilitadas = sqlite3_column_int(stmt, 3);
            strcpy_s(presupuesto->fecha_creacion, sizeof(presupuesto->fecha_creacion), (const char *)sqlite3_column_text(stmt, 4));
            strcpy_s(presupuesto->fecha_modificacion, sizeof(presupuesto->fecha_modificacion), (const char *)sqlite3_column_text(stmt, 5));

            sqlite3_finalize(stmt);
            return 1; // exito
        }
        sqlite3_finalize(stmt);
    }

    return 0; // No encontrado
}

/**
 * @brief Mostrar alertas cuando se exceden limites
 */
void mostrar_alerta_exceso_gasto(int gastos_actuales, int limite)
{
    int exceso = gastos_actuales - limite;
    double porcentaje_exceso = (exceso / (double)limite) * 100.0;

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║                    ¡ALERTA DE GASTO!                    ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║ Limite de gasto excedido                               ║\n");
    printf("║                                                        ║\n");
    printf("║ Limite configurado: $%-10s                      ║\n", formato_monto(limite));
    printf("║ Gastos actuales:    $%-10s                      ║\n", formato_monto(gastos_actuales));
    printf("║ Exceso:             $%-10s                      ║\n", formato_monto(exceso));
    printf("║ Porcentaje de exceso: %6.1f%%                            ║\n", porcentaje_exceso);
    printf("║                                                        ║\n");
    printf("║ ¡RECOMENDACION: Reduzca los gastos inmediatamente!     ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

/**
 * @brief Obtener estadisticas generales de financiamiento
 */
static void obtener_estadisticas_generales(int *total_ingresos, int *total_gastos, int *num_transacciones)
{
    sqlite3_stmt *stmt;
    const char *sql_totales = "SELECT tipo, SUM(monto), COUNT(*) FROM financiamiento GROUP BY tipo;";

    if (preparar_stmt(sql_totales, &stmt))
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int tipo = sqlite3_column_int(stmt, 0);
            int suma = sqlite3_column_int(stmt, 1);
            int count = sqlite3_column_int(stmt, 2);

            if (tipo == INGRESO)
            {
                *total_ingresos = suma;
            }
            else
            {
                *total_gastos = suma;
            }
            *num_transacciones += count;
        }
        sqlite3_finalize(stmt);
    }
}

/**
 * @brief Imprimir resumen general
 */
static void imprimir_resumen_general(int num_transacciones, int total_ingresos, int total_gastos)
{
    printf("\n=== RESUMEN GENERAL ===\n");
    printf("Total de transacciones: %d\n", num_transacciones);
    printf("Total Ingresos: $");
    mostrar_monto(total_ingresos);
    printf("Total Gastos: $");
    mostrar_monto(total_gastos);
    printf("Balance Neto: $");
    mostrar_monto(total_ingresos - total_gastos);
}

/**
 * @brief Imprimir desglose por categorias de ingresos
 */
static void imprimir_ingresos_por_categoria()
{
    sqlite3_stmt *stmt;
    const char *sql_ingresos = "SELECT categoria, SUM(monto), COUNT(*) FROM financiamiento WHERE tipo = 0 GROUP BY categoria ORDER BY SUM(monto) DESC;";

    printf("\n=== INGRESOS POR CATEGORIA ===\n");
    if (preparar_stmt(sql_ingresos, &stmt))
    {
        int found_ingresos = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found_ingresos = 1;
            int categoria = sqlite3_column_int(stmt, 0);
            int suma = sqlite3_column_int(stmt, 1);
            int count = sqlite3_column_int(stmt, 2);

            printf("%s: $", get_nombre_categoria(categoria));
            mostrar_monto(suma);
            printf(" (%d transacciones)\n", count);
        }
        sqlite3_finalize(stmt);

        if (!found_ingresos)
        {
            mostrar_no_hay_registros("ingresos registrados");
        }
    }
}

/**
 * @brief Imprimir desglose por categorias de gastos
 */
static void imprimir_gastos_por_categoria()
{
    sqlite3_stmt *stmt;
    const char *sql_gastos = "SELECT categoria, SUM(monto), COUNT(*) FROM financiamiento WHERE tipo = 1 GROUP BY categoria ORDER BY SUM(monto) DESC;";

    printf("\n=== GASTOS POR CATEGORIA ===\n");
    if (preparar_stmt(sql_gastos, &stmt))
    {
        int found_gastos = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found_gastos = 1;
            int categoria = sqlite3_column_int(stmt, 0);
            int suma = sqlite3_column_int(stmt, 1);
            int count = sqlite3_column_int(stmt, 2);

            printf("%s: $", get_nombre_categoria(categoria));
            mostrar_monto(suma);
            printf(" (%d transacciones)\n", count);
        }
        sqlite3_finalize(stmt);

        if (!found_gastos)
        {
            printf("No hay gastos registrados.\n");
        }
    }
}

/**
 * @brief Imprimir estadisticas de equipamiento
 */
static void imprimir_estadisticas_equipamiento()
{
    sqlite3_stmt *stmt;
    const char *sql_equipamiento = "SELECT item_especifico, SUM(monto), COUNT(*) FROM financiamiento WHERE tipo = 1 AND categoria = 1 AND item_especifico != '' GROUP BY item_especifico ORDER BY SUM(monto) DESC LIMIT 10;";

    printf("\n=== TOP ITEMS DE EQUIPAMIENTO ===\n");
    if (preparar_stmt(sql_equipamiento, &stmt))
    {
        int found_equip = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found_equip = 1;
            const char *item = (const char *)sqlite3_column_text(stmt, 0);
            int suma = sqlite3_column_int(stmt, 1);
            int count = sqlite3_column_int(stmt, 2);

            printf("%s: $", item);
            mostrar_monto(suma);
            printf(" (%d compras)\n", count);
        }
        sqlite3_finalize(stmt);

        if (!found_equip)
        {
            mostrar_no_hay_registros("compras de equipamiento especificadas");
        }
    }
}

/**
 * @brief Imprimir balance mensual
 */
static void imprimir_balance_mensual()
{
    sqlite3_stmt *stmt;
    const char *sql_mensual = "SELECT strftime('%Y-%m', fecha) as mes, "
                              "SUM(CASE WHEN tipo = 0 THEN monto ELSE 0 END) as ingresos, "
                              "SUM(CASE WHEN tipo = 1 THEN monto ELSE 0 END) as gastos "
                              "FROM financiamiento "
                              "WHERE fecha >= date('now', '-12 months') "
                              "GROUP BY mes ORDER BY mes DESC;";

    printf("\n=== BALANCE MENSUAL (ULTIMOS 12 MESES) ===\n");
    if (preparar_stmt(sql_mensual, &stmt))
    {
        int found_mensual = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found_mensual = 1;
            const char *mes = (const char *)sqlite3_column_text(stmt, 0);
            int ingresos_mes = sqlite3_column_int(stmt, 1);
            int gastos_mes = sqlite3_column_int(stmt, 2);

            printf("%s: Ingresos $", mes);
            mostrar_monto(ingresos_mes);
            printf(", Gastos $");
            mostrar_monto(gastos_mes);
            printf(", Balance $");
            mostrar_monto(ingresos_mes - gastos_mes);
            printf("\n");
        }
        sqlite3_finalize(stmt);

        if (!found_mensual)
        {
            mostrar_no_hay_registros("datos suficientes para mostrar balance mensual");
        }
    }
}

/**
 * @brief Mostrar resumen financiero del equipo
 */
void mostrar_resumen_financiero()
{
    clear_screen();
    print_header("RESUMEN FINANCIERO DEL EQUIPO");

    int total_ingresos = 0;
    int total_gastos = 0;
    int num_transacciones = 0;

    obtener_estadisticas_generales(&total_ingresos, &total_gastos, &num_transacciones);
    imprimir_resumen_general(num_transacciones, total_ingresos, total_gastos);

    if (num_transacciones == 0)
    {
        mostrar_no_hay_registros("transacciones registradas");
        pause_console();
        return;
    }

    imprimir_ingresos_por_categoria();
    imprimir_gastos_por_categoria();
    imprimir_estadisticas_equipamiento();
    imprimir_balance_mensual();

    pause_console();
}

/**
 * @brief Mostrar balance general de gastos
 */
void ver_balance_gastos()
{
    clear_screen();
    print_header("BALANCE GENERAL DE GASTOS");

    sqlite3_stmt *stmt;

    // Obtener total de gastos
    int total_gastos = 0;
    int num_gastos = 0;

    const char *sql_total_gastos = "SELECT SUM(monto), COUNT(*) FROM financiamiento WHERE tipo = 1;";

    if (preparar_stmt(sql_total_gastos, &stmt))
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            total_gastos = sqlite3_column_int(stmt, 0);
            num_gastos = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    printf("\n=== BALANCE GENERAL DE GASTOS ===\n");
    printf("Total de gastos registrados: %d\n", num_gastos);
    printf("Monto total de gastos: $");
    mostrar_monto(total_gastos);
    printf("\n\n");

    if (num_gastos == 0)
    {
        printf("No hay gastos registrados.\n");
        pause_console();
        return;
    }

    // Desglose por categorias de gastos
    printf("=== DESGLOSE POR CATEGORIAS ===\n");
    const char *sql_gastos_categoria = "SELECT categoria, SUM(monto), COUNT(*) FROM financiamiento WHERE tipo = 1 GROUP BY categoria ORDER BY SUM(monto) DESC;";

    if (preparar_stmt(sql_gastos_categoria, &stmt))
    {
        printf("%-15s %-12s %-10s %-8s\n", "Categoria", "Total", "Cantidad", "Porcentaje");
        printf("--------------------------------------------------\n");

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int categoria = sqlite3_column_int(stmt, 0);
            int suma = sqlite3_column_int(stmt, 1);
            int count = sqlite3_column_int(stmt, 2);
            double porcentaje = (suma / (double)total_gastos) * 100.0;

            printf("%-15s $%s %-10d %-7.1f%%\n",
                   get_nombre_categoria(categoria), formato_monto(suma), count, porcentaje);
        }
        sqlite3_finalize(stmt);
    }

    // Top 5 gastos mas altos
    printf("\n=== TOP 5 GASTOS MAS ALTOS ===\n");
    const char *sql_top_gastos = "SELECT descripcion, monto, fecha, categoria FROM financiamiento WHERE tipo = 1 ORDER BY monto DESC LIMIT 5;";

    if (preparar_stmt(sql_top_gastos, &stmt))
    {
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count++;
            const char *descripcion = (const char *)sqlite3_column_text(stmt, 0);
            int monto = sqlite3_column_int(stmt, 1);
            const char *fecha = (const char *)sqlite3_column_text(stmt, 2);
            int categoria = sqlite3_column_int(stmt, 3);

            printf("%d. $", count);
            mostrar_monto(monto);
            printf(" - %s (%s - %s)\n", descripcion, fecha, get_nombre_categoria(categoria));
        }
        sqlite3_finalize(stmt);

        if (count == 0)
        {
            mostrar_no_hay_registros("gastos registrados");
        }
    }

    // Balance mensual de gastos (ultimos 6 meses)
    printf("\n=== BALANCE MENSUAL DE GASTOS (ULTIMOS 6 MESES) ===\n");
    const char *sql_mensual_gastos = "SELECT strftime('%Y-%m', fecha) as mes, SUM(monto), COUNT(*) FROM financiamiento WHERE tipo = 1 AND fecha >= date('now', '-6 months') GROUP BY mes ORDER BY mes DESC;";

    if (preparar_stmt(sql_mensual_gastos, &stmt))
    {
        printf("%-8s %-12s %-10s\n", "Mes", "Total", "Cantidad");
        printf("----------------------------\n");

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *mes = (const char *)sqlite3_column_text(stmt, 0);
            int suma = sqlite3_column_int(stmt, 1);
            int count = sqlite3_column_int(stmt, 2);

            printf("%-8s $", mes);
            mostrar_monto(suma);
            printf(" %-10d\n", count);
        }
        sqlite3_finalize(stmt);
    }

    printf("\n=== RESUMEN EJECUTIVO ===\n");
    printf("Total gastado por el equipo: $");
    mostrar_monto(total_gastos);
    float promedio = num_gastos > 0 ? (float)total_gastos / (float)num_gastos : 0.0f;
    printf("Promedio por gasto: $%.2f\n", promedio);

    pause_console();
}

/**
 * @brief Estructura para parametros de preparacion de exportacion
 */
typedef struct
{
    const char *export_dir;
    const char *timestamp;
    char *csv_filename;
    FILE **csv_file;
    char *txt_filename;
    FILE **txt_file;
    char *html_filename;
    FILE **html_file;
    char *json_filename;
    FILE **json_file;
    cJSON **json_array;
} ExportPrepParams;

/**
 * @brief Limpiar archivos de exportacion abiertos
 */
static void limpiar_archivos_exportacion(FILE *csv_file, FILE *txt_file, FILE *html_file, FILE *json_file, cJSON *json_array)
{
    if (csv_file)
    {
        fclose(csv_file);
        csv_file = NULL;
    }
    if (txt_file)
    {
        fclose(txt_file);
        txt_file = NULL;
    }
    if (html_file)
    {
        fclose(html_file);
        html_file = NULL;
    }
    if (json_file)
    {
        fclose(json_file);
        json_file = NULL;
    }
    if (json_array)
    {
        cJSON_Delete(json_array);
        json_array = NULL;
    }
}

/**
 * @brief Helper function to open a file and handle errors
 */
static void abrir_archivo(FILE **file, const char *filename, const char *mode, const char *tipo_archivo)
{
    errno_t err = fopen_s(file, filename, mode);
    if (err != 0)
    {
        printf("Error opening %s file: %s\n", tipo_archivo, filename);
        return;
    }
}

/**
 * @brief Helper function to close previously opened files
 */
static void cerrar_archivos_anteriores(FILE *csv_file, FILE *txt_file, FILE *html_file)
{
    if (csv_file) fclose(csv_file);
    if (txt_file) fclose(txt_file);
    if (html_file) fclose(html_file);
}

/**
 * @brief Preparar archivos de exportacion
 */
static void preparar_archivos_exportacion(ExportPrepParams *params)
{

    // CSV
    snprintf(params->csv_filename, 300, "%s%sfinanciamiento_%s.csv", params->export_dir, PATH_SEP, params->timestamp);
    abrir_archivo(params->csv_file, params->csv_filename, "w", "CSV");
    if (*params->csv_file)
    {
        fprintf(*params->csv_file, "ID,Fecha,Tipo,Categoria,Descripcion,Monto,Item_Especifico\n");
    }

    // TXT
    snprintf(params->txt_filename, 300, "%s%sfinanciamiento_%s.txt", params->export_dir, PATH_SEP, params->timestamp);
    if (*params->csv_file)
    {
        abrir_archivo(params->txt_file, params->txt_filename, "w", "TXT");
        if (*params->txt_file)
        {
            fprintf(*params->txt_file, "LISTADO DE TRANSACCIONES FINANCIERAS\n");
            fprintf(*params->txt_file, "=====================================\n\n");
        }
        else
        {
            cerrar_archivos_anteriores(*params->csv_file, NULL, NULL);
            *params->csv_file = NULL;
            return;
        }
    }

    // HTML
    snprintf(params->html_filename, 300, "%s%sfinanciamiento_%s.html", params->export_dir, PATH_SEP, params->timestamp);
    if (*params->csv_file && *params->txt_file)
    {
        abrir_archivo(params->html_file, params->html_filename, "w", "HTML");
        if (*params->html_file)
        {
            fprintf(*params->html_file, "<html><body><h1>Transacciones Financieras</h1>");
            fprintf(*params->html_file, "<table border='1'><tr><th>ID</th><th>Fecha</th><th>Tipo</th><th>Categoria</th><th>Descripcion</th><th>Monto</th><th>Item Especifico</th></tr>");
        }
        else
        {
            cerrar_archivos_anteriores(*params->csv_file, *params->txt_file, NULL);
            *params->csv_file = NULL;
            *params->txt_file = NULL;
            return;
        }
    }

    // JSON
    snprintf(params->json_filename, 300, "%s%sfinanciamiento_%s.json", params->export_dir, PATH_SEP, params->timestamp);
    if (*params->csv_file && *params->txt_file && *params->html_file)
    {
        abrir_archivo(params->json_file, params->json_filename, "w", "JSON");
        if (*params->json_file)
        {
            *params->json_array = cJSON_CreateArray();
        }
        else
        {
            cerrar_archivos_anteriores(*params->csv_file, *params->txt_file, *params->html_file);
            *params->csv_file = NULL;
            *params->txt_file = NULL;
            *params->html_file = NULL;
            *params->json_array = NULL;
        }
    }
}

/**
 * @brief Estructura para parametros de exportacion de transacciones
 */
typedef struct
{
    FILE *csv_file;
    FILE *txt_file;
    FILE *html_file;
    cJSON *json_array;
    int *count;
    int *total_ingresos;
    int *total_gastos;
} ExportParams;

/**
 * @brief Procesar transaccion para exportacion
 */
static void procesar_transaccion_exportacion(sqlite3_stmt *stmt, ExportParams *params)
{
    int id = sqlite3_column_int(stmt, 0);
    const char *fecha = (const char *)sqlite3_column_text(stmt, 1);
    int tipo = sqlite3_column_int(stmt, 2);
    int categoria = sqlite3_column_int(stmt, 3);
    const char *descripcion = (const char *)sqlite3_column_text(stmt, 4);
    int monto = sqlite3_column_int(stmt, 5);
    const char *item = (const char *)sqlite3_column_text(stmt, 6);

    (*params->count)++;
    if (tipo == INGRESO)
    {
        *params->total_ingresos += monto;
    }
    else
    {
        *params->total_gastos += monto;
    }

    // CSV
    if (params->csv_file)
    {
        fprintf(params->csv_file, "%d,%s,%s,%s,\"%s\",%d",
                id, fecha, get_nombre_tipo_transaccion(tipo),
                get_nombre_categoria(categoria), descripcion, monto);

        if (item && safe_strnlen(item, 65536) > 0)
        {
            fprintf(params->csv_file, ",\"%s\"", item);
        }
        else
        {
            fprintf(params->csv_file, ",");
        }
        fprintf(params->csv_file, "\n");
    }

    // TXT
    if (params->txt_file)
    {
        fprintf(params->txt_file, "ID: %d\n", id);
        fprintf(params->txt_file, "Fecha: %s\n", fecha);
        fprintf(params->txt_file, "Tipo: %s\n", get_nombre_tipo_transaccion(tipo));
        fprintf(params->txt_file, "Categoria: %s\n", get_nombre_categoria(categoria));
        fprintf(params->txt_file, "Descripcion: %s\n", descripcion);
        fprintf(params->txt_file, "Monto: $%s\n", formato_monto(monto));
        if (item && safe_strnlen(item, 65536) > 0)
        {
            fprintf(params->txt_file, "Item Especifico: %s\n", item);
        }
        fprintf(params->txt_file, "----------------------------------------\n");
    }

    // HTML
    if (params->html_file)
    {
        fprintf(params->html_file, "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>$%s</td><td>%s</td></tr>",
                id, fecha, get_nombre_tipo_transaccion(tipo),
                get_nombre_categoria(categoria), descripcion, formato_monto(monto),
                (item && safe_strnlen(item, 65536) > 0) ? item : "");
    }

    // JSON
    cJSON *item_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(item_obj, "id", id);
    cJSON_AddStringToObject(item_obj, "fecha", fecha);
    cJSON_AddStringToObject(item_obj, "tipo", get_nombre_tipo_transaccion(tipo));
    cJSON_AddStringToObject(item_obj, "categoria", get_nombre_categoria(categoria));
    cJSON_AddStringToObject(item_obj, "descripcion", descripcion);
    cJSON_AddNumberToObject(item_obj, "monto", monto);
    if (item && safe_strnlen(item, 65536) > 0)
    {
        cJSON_AddStringToObject(item_obj, "item_especifico", item);
    }
    else
    {
        cJSON_AddStringToObject(item_obj, "item_especifico", "");
    }
    cJSON_AddItemToArray(params->json_array, item_obj);
}

/**
 * @brief Estructura para parametros de finalizacion de exportacion
 */
typedef struct
{
    const char *csv_filename;
    FILE *csv_file;
    const char *txt_filename;
    FILE *txt_file;
    const char *html_filename;
    FILE *html_file;
    const char *json_filename;
    FILE *json_file;
    cJSON *json_array;
    int count;
    int total_ingresos;
    int total_gastos;
} ExportFinalizeParams;

/**
 * @brief Finalizar archivos de exportacion
 */
static void finalizar_archivos_exportacion(ExportFinalizeParams *params)
{
    // CSV
    if (params->csv_file)
    {
        fprintf(params->csv_file, "\n");
        fprintf(params->csv_file, "RESUMEN,,Total Transacciones:,%d\n", params->count);
        fprintf(params->csv_file, "RESUMEN,,Total Ingresos:,$%d\n", params->total_ingresos);
        fprintf(params->csv_file, "RESUMEN,,Total Gastos:,$%d\n", params->total_gastos);
        fprintf(params->csv_file, "RESUMEN,,Balance Neto:,$%d\n", params->total_ingresos - params->total_gastos);
        fclose(params->csv_file);
        printf("CSV exportado: %s\n", params->csv_filename);
    }

    // TXT
    if (params->txt_file)
    {
        fprintf(params->txt_file, "\nRESUMEN GENERAL\n");
        fprintf(params->txt_file, "================\n");
        fprintf(params->txt_file, "Total de transacciones: %d\n", params->count);
        fprintf(params->txt_file, "Total Ingresos: $%s\n", formato_monto(params->total_ingresos));
        fprintf(params->txt_file, "Total Gastos: $%s\n", formato_monto(params->total_gastos));
        fprintf(params->txt_file, "Balance Neto: $%s\n", formato_monto(params->total_ingresos - params->total_gastos));
        fclose(params->txt_file);
        printf("TXT exportado: %s\n", params->txt_filename);
    }

    // HTML
    if (params->html_file)
    {
        fprintf(params->html_file, "</table>");
        fprintf(params->html_file, "<h2>Resumen General</h2>");
        fprintf(params->html_file, "<table border='1'>");
        fprintf(params->html_file, "<tr><th>Total Transacciones</th><td>%d</td></tr>", params->count);
        fprintf(params->html_file, "<tr><th>Total Ingresos</th><td>$%s</td></tr>", formato_monto(params->total_ingresos));
        fprintf(params->html_file, "<tr><th>Total Gastos</th><td>$%s</td></tr>", formato_monto(params->total_gastos));
        fprintf(params->html_file, "<tr><th>Balance Neto</th><td>$%s</td></tr>", formato_monto(params->total_ingresos - params->total_gastos));
        fprintf(params->html_file, "</table></body></html>");
        fclose(params->html_file);
        printf("HTML exportado: %s\n", params->html_filename);
    }

    // JSON
    if (params->json_file != NULL)
    {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "transacciones", params->json_array);

        cJSON *resumen = cJSON_CreateObject();
        cJSON_AddNumberToObject(resumen, "total_transacciones", params->count);
        cJSON_AddNumberToObject(resumen, "total_ingresos", params->total_ingresos);
        cJSON_AddNumberToObject(resumen, "total_gastos", params->total_gastos);
        cJSON_AddNumberToObject(resumen, "balance_neto", params->total_ingresos - params->total_gastos);
        cJSON_AddItemToObject(root, "resumen", resumen);

        char *json_string = cJSON_Print(root);
        fprintf(params->json_file, "%s", json_string);
        free(json_string);
        cJSON_Delete(root);
        fclose(params->json_file);
        printf("JSON exportado: %s\n", params->json_filename);
    }
    else
    {
        cJSON_Delete(params->json_array);
    }
}

/**
 * @brief Exportar transacciones financieras a multiples formatos
 */
void exportar_financiamiento()
{
    clear_screen();
    print_header("EXPORTAR FINANCIAMIENTO");

    // Obtener directorio de exportacion
    const char *export_dir = get_export_dir();
    if (!export_dir)
    {
        printf("Error: No se pudo obtener el directorio de exportacion.\n");
        pause_console();
        return;
    }

    // Generar timestamp
    time_t t = time(NULL);
    struct tm tm;
    localtime_s(&tm, &t);
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%04d%02d%02d_%02d%02d%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);

    printf("Exportando datos de financiamiento en todos los formatos...\n\n");

    // Preparar archivos
    char csv_filename[300];
    char txt_filename[300];
    char html_filename[300];
    char json_filename[300];
    FILE *csv_file = NULL;
    FILE *txt_file = NULL;
    FILE *html_file = NULL;
    FILE *json_file = NULL; // Inicializar explicitamente a NULL
    cJSON *json_array = NULL;

    ExportPrepParams prep_params =
    {
        .export_dir = export_dir,
        .timestamp = timestamp,
        .csv_filename = csv_filename,
        .csv_file = &csv_file,
        .txt_filename = txt_filename,
        .txt_file = &txt_file,
        .html_filename = html_filename,
        .html_file = &html_file,
        .json_filename = json_filename,
        .json_file = &json_file,
        .json_array = &json_array
    };

    preparar_archivos_exportacion(&prep_params);

    // Obtener transacciones
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, fecha, tipo, categoria, descripcion, monto, item_especifico FROM financiamiento ORDER BY fecha DESC, id DESC;";

    int count = 0;
    int total_ingresos = 0;
    int total_gastos = 0;

    if (!preparar_stmt(sql, &stmt))
    {
        limpiar_archivos_exportacion(csv_file, txt_file, html_file, json_file, json_array);
        pause_console();
        return;
    }

    // Procesar transacciones
    ExportParams params =
    {
        .csv_file = csv_file,
        .txt_file = txt_file,
        .html_file = html_file,
        .json_array = json_array,
        .count = &count,
        .total_ingresos = &total_ingresos,
        .total_gastos = &total_gastos
    };

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        procesar_transaccion_exportacion(stmt, &params);
    }

    sqlite3_finalize(stmt);

    // Finalizar archivos
    ExportFinalizeParams finalize_params =
    {
        .csv_filename = csv_filename,
        .csv_file = csv_file,
        .txt_filename = txt_filename,
        .txt_file = txt_file,
        .html_filename = html_filename,
        .html_file = html_file,
        .json_filename = json_filename,
        .json_file = json_file,
        .json_array = json_array,
        .count = count,
        .total_ingresos = total_ingresos,
        .total_gastos = total_gastos
    };

    finalizar_archivos_exportacion(&finalize_params);

    printf("\nExportacion completada exitosamente!\n");
    printf("Total de transacciones exportadas: %d\n", count);
    printf("Balance neto: $");
    mostrar_monto(total_ingresos - total_gastos);

    pause_console();
}

/**
 * @brief Menu principal de gestion financiera
 */
void menu_financiamiento()
{
    static const MenuItem items[] =
    {
        FIN_ITEM(1, "Agregar Transaccion", agregar_transaccion),
        FIN_ITEM(2, "Listar Transacciones", listar_transacciones),
        FIN_ITEM(3, "Modificar Transaccion", modificar_transaccion),
        FIN_ITEM(4, "Eliminar Transaccion", eliminar_transaccion),
        FIN_ITEM(5, "Ver Resumen Financiero", mostrar_resumen_financiero),
        FIN_ITEM(6, "Balance General de Gastos", ver_balance_gastos),
        FIN_ITEM(7, "Exportar Datos", exportar_financiamiento),
        FIN_ITEM(8, "Presupuestos Mensuales", menu_presupuestos_mensuales),
        FIN_BACK_ITEM
    };

    ejecutar_menu_estandar("FINANCIAMIENTO", items, ARRAY_COUNT(items));
}

/**
 * @brief Modificar la fecha de una transaccion financiera
 */
static void modificar_fecha_transaccion(int id_transaccion)
{
#ifdef UNIT_TEST
    printf("Nueva fecha (DD/MM/AAAA, Enter=hoy): ");
    char nueva_fecha[20] = "";
    while (1)
    {
        input_date("", nueva_fecha, sizeof(nueva_fecha));
        if (safe_strnlen(nueva_fecha, sizeof(nueva_fecha)) > 0)
            break;
        printf("La fecha no puede estar vacia.\n");
    }
    const char *sql = "UPDATE financiamiento SET fecha = ? WHERE id = ?;";
    ejecutar_update_texto(sql, nueva_fecha, id_transaccion);
    printf("Fecha actualizada exitosamente.\n");
#else
    char fecha_ingresada[20] = "";
    char fecha_normalizada[11] = "";
    const char *sql = "UPDATE financiamiento SET fecha = ? WHERE id = ?;";

    if (!fin_modal_input_texto_gui("MODIFICAR FECHA",
                                   "Nueva fecha (DD/MM/AAAA o YYYY-MM-DD)",
                                   "Deje vacio para usar hoy",
                                   fecha_ingresada,
                                   sizeof(fecha_ingresada),
                                   1,
                                   FIN_INPUT_DATE))
    {
        return;
    }

    if (!fin_normalizar_fecha_input(fecha_ingresada, fecha_normalizada, sizeof(fecha_normalizada)))
    {
        fin_mostrar_info_gui("FORMATO INVALIDO",
                             "La fecha debe ser DD/MM/AAAA o YYYY-MM-DD.");
        return;
    }

    ejecutar_update_texto(sql, fecha_normalizada, id_transaccion);
    printf("Fecha actualizada exitosamente.\n");
#endif
}

/**
 * @brief Modificar el tipo de una transaccion financiera
 */
static void modificar_tipo_transaccion(int id_transaccion)
{
#ifdef UNIT_TEST
    printf("Nuevo tipo:\n1. Ingreso\n2. Gasto\n");
    int nuevo_tipo = input_int(">") - 1;
    if (nuevo_tipo < 0 || nuevo_tipo > 1)
    {
        printf("Opcion invalida.\n");
        return;
    }
    const char *sql = "UPDATE financiamiento SET tipo = ? WHERE id = ?;";
    ejecutar_update_int(sql, nuevo_tipo, id_transaccion);
    printf("Tipo actualizado exitosamente.\n");
#else
    const char *opciones[] = {"Ingreso", "Gasto"};
    int seleccion = 0;
    const char *sql = "UPDATE financiamiento SET tipo = ? WHERE id = ?;";

    if (!fin_modal_selector_opcion_gui("MODIFICAR TIPO",
                                       "TIPO",
                                       opciones,
                                       ARRAY_COUNT(opciones),
                                       0,
                                       &seleccion))
    {
        return;
    }

    ejecutar_update_int(sql, seleccion, id_transaccion);
    printf("Tipo actualizado exitosamente.\n");
#endif
}

/**
 * @brief Modificar la categoria de una transaccion financiera
 */
static void modificar_categoria_transaccion(int id_transaccion)
{
#ifdef UNIT_TEST
    printf("Nueva categoria:\n");
    printf("1. Transporte\n2. Equipamiento\n3. Cuotas\n4. Torneos\n");
    printf("5. Arbitraje\n6. Canchas\n7. Medicina\n8. Otros\n");
    int nueva_categoria = input_int(">") - 1;
    if (nueva_categoria < 0 || nueva_categoria > 7)
    {
        printf("Opcion invalida.\n");
        return;
    }
    const char *sql = "UPDATE financiamiento SET categoria = ? WHERE id = ?;";
    ejecutar_update_int(sql, nueva_categoria, id_transaccion);
    printf("Categoria actualizada exitosamente.\n");
#else
    const char *opciones[] =
    {
        "Transporte",
        "Equipamiento",
        "Cuotas",
        "Torneos",
        "Arbitraje",
        "Canchas",
        "Medicina",
        "Otros"
    };
    int seleccion = 0;
    const char *sql = "UPDATE financiamiento SET categoria = ? WHERE id = ?;";

    if (!fin_modal_selector_opcion_gui("MODIFICAR CATEGORIA",
                                       "CATEGORIA",
                                       opciones,
                                       ARRAY_COUNT(opciones),
                                       0,
                                       &seleccion))
    {
        return;
    }

    ejecutar_update_int(sql, seleccion, id_transaccion);
    printf("Categoria actualizada exitosamente.\n");
#endif
}

/**
 * @brief Modificar la descripcion de una transaccion financiera
 */
static void modificar_descripcion_transaccion(int id_transaccion)
{
#ifdef UNIT_TEST
    printf("Nueva descripcion: ");
    char nueva_descripcion[200] = "";
    solicitar_texto_no_vacio("", nueva_descripcion, sizeof(nueva_descripcion));
    const char *sql = "UPDATE financiamiento SET descripcion = ? WHERE id = ?;";
    ejecutar_update_texto(sql, nueva_descripcion, id_transaccion);
    printf("Descripcion actualizada exitosamente.\n");
#else
    char nueva_descripcion[200] = "";
    const char *sql = "UPDATE financiamiento SET descripcion = ? WHERE id = ?;";

    if (!fin_modal_input_texto_gui("MODIFICAR DESCRIPCION",
                                   "Nueva descripcion",
                                   "Solo letras, numeros y espacios",
                                   nueva_descripcion,
                                   sizeof(nueva_descripcion),
                                   0,
                                   FIN_INPUT_ALNUM))
    {
        return;
    }

    ejecutar_update_texto(sql, nueva_descripcion, id_transaccion);
    printf("Descripcion actualizada exitosamente.\n");
#endif
}

/**
 * @brief Modificar el monto de una transaccion financiera
 */
static void modificar_monto_transaccion(int id_transaccion)
{
    int nuevo_monto = solicitar_monto_no_negativo("Nuevo monto: ");
    if (nuevo_monto < 0)
    {
        return;
    }
    const char *sql = "UPDATE financiamiento SET monto = ? WHERE id = ?;";
    ejecutar_update_int(sql, nuevo_monto, id_transaccion);
    printf("Monto actualizado exitosamente.\n");
}

/**
 * @brief Modificar el item especifico de una transaccion financiera
 */
static void modificar_item_especifico_transaccion(int id_transaccion)
{
#ifdef UNIT_TEST
    printf("Nuevo item especifico: ");
    char nuevo_item[100] = "";
    input_string("", nuevo_item, sizeof(nuevo_item));
    const char *sql = "UPDATE financiamiento SET item_especifico = ? WHERE id = ?;";
    ejecutar_update_texto(sql, nuevo_item, id_transaccion);
    printf("Item especifico actualizado exitosamente.\n");
#else
    char nuevo_item[100] = "";
    const char *sql = "UPDATE financiamiento SET item_especifico = ? WHERE id = ?;";

    if (!fin_modal_input_texto_gui("MODIFICAR ITEM",
                                   "Nuevo item especifico (opcional)",
                                   "Ej: Botines Nike",
                                   nuevo_item,
                                   sizeof(nuevo_item),
                                   1,
                                   FIN_INPUT_ALNUM))
    {
        return;
    }

    ejecutar_update_texto(sql, nuevo_item, id_transaccion);
    printf("Item especifico actualizado exitosamente.\n");
#endif
}

/**
 * @brief Modificar una transaccion financiera existente
 */
void modificar_transaccion()
{
    clear_screen();
    print_header("MODIFICAR TRANSACCION FINANCIERA");

#ifdef UNIT_TEST
    // Mostrar lista de todas las transacciones
    sqlite3_stmt *stmt;
    const char *sql_lista = "SELECT id, fecha, tipo, categoria, descripcion, monto FROM financiamiento ORDER BY fecha DESC, id DESC;";

    if (preparar_stmt(sql_lista, &stmt))
    {
        printf("\n=== TODAS LAS TRANSACCIONES ===\n\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int id = sqlite3_column_int(stmt, 0);
            const char *fecha_db = (const char *)sqlite3_column_text(stmt, 1);
            int tipo = sqlite3_column_int(stmt, 2);
            int categoria = sqlite3_column_int(stmt, 3);
            const char *descripcion = (const char *)sqlite3_column_text(stmt, 4);
            int monto = sqlite3_column_int(stmt, 5);

            // Convertir fecha de YYYY-MM-DD a DD/MM/YYYY para mostrar
            char fecha_display[11] = "";
            mostrar_fecha_display(fecha_db, fecha_display, sizeof(fecha_display));

            printf("ID: %d | %s | %s | %s | %s | $", id, fecha_display, get_nombre_tipo_transaccion(tipo), get_nombre_categoria(categoria), descripcion);
            mostrar_monto(monto);
            printf("\n");
        }
        sqlite3_finalize(stmt);

        if (!found)
        {
            printf("No hay transacciones registradas.\n");
            pause_console();
            return;
        }
    }

    int id_transaccion = input_int("\nIngrese el ID de la transaccion a modificar (0 para cancelar): ");

    if (id_transaccion == 0)
        return;

    // Verificar que existe
    if (!existe_id("financiamiento", id_transaccion))
    {
        printf("ID de transaccion invalido.\n");
        pause_console();
        return;
    }

    // Obtener datos actuales
    TransaccionFinanciera transaccion = {0};
    const char *sql_obtener = "SELECT fecha, tipo, categoria, descripcion, monto, item_especifico FROM financiamiento WHERE id = ?;";

    if (preparar_stmt(sql_obtener, &stmt))
    {
        sqlite3_bind_int(stmt, 1, id_transaccion);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            transaccion.id = id_transaccion;
            strncpy_s(transaccion.fecha, sizeof(transaccion.fecha), (const char *)sqlite3_column_text(stmt, 0), sizeof(transaccion.fecha) - 1);
            transaccion.tipo = sqlite3_column_int(stmt, 1);
            transaccion.categoria = sqlite3_column_int(stmt, 2);
            strncpy_s(transaccion.descripcion, sizeof(transaccion.descripcion), (const char *)sqlite3_column_text(stmt, 3), sizeof(transaccion.descripcion) - 1);
            transaccion.monto = sqlite3_column_int(stmt, 4);
            const char *item = (const char *)sqlite3_column_text(stmt, 5);
            if (item)
            {
                strncpy_s(transaccion.item_especifico, sizeof(transaccion.item_especifico), item, sizeof(transaccion.item_especifico) - 1);
            }
            else
            {
                transaccion.item_especifico[0] = '\0';
            }
        }
        sqlite3_finalize(stmt);
    }

    // Mostrar datos actuales y opciones de modificacion
    clear_screen();
    print_header("MODIFICAR TRANSACCION");
    printf("Datos actuales:\n");
    mostrar_transaccion(&transaccion);

    printf("Seleccione que desea modificar:\n");
    printf("1. Fecha\n");
    printf("2. Tipo\n");
    printf("3. Categoria\n");
    printf("4. Descripcion\n");
    printf("5. Monto\n");
    printf("6. Item especifico\n");
    printf("7. Volver\n");

    int opcion = input_int(">");

    switch (opcion)
    {
    case 1:
        modificar_fecha_transaccion(id_transaccion);
        break;
    case 2:
        modificar_tipo_transaccion(id_transaccion);
        break;
    case 3:
        modificar_categoria_transaccion(id_transaccion);
        break;
    case 4:
        modificar_descripcion_transaccion(id_transaccion);
        break;
    case 5:
        modificar_monto_transaccion(id_transaccion);
        break;
    case 6:
        modificar_item_especifico_transaccion(id_transaccion);
        break;
    case 7:
        return;
    default:
        printf("Opcion invalida.\n");
    }

    pause_console();
#else
    int id_transaccion = 0;
    int opcion = 0;
    TransaccionFinanciera transaccion;
    const char *opciones[] =
    {
        "Fecha",
        "Tipo",
        "Categoria",
        "Descripcion",
        "Monto",
        "Item especifico",
        "Volver"
    };

    if (!fin_seleccionar_transaccion_gui("MODIFICAR TRANSACCION",
                                         "Click o ENTER para seleccionar | ESC: cancelar",
                                         0,
                                         &id_transaccion))
    {
        return;
    }

    if (!fin_obtener_transaccion_por_id(id_transaccion, &transaccion))
    {
        fin_mostrar_info_gui("ERROR",
                             "No se pudo cargar la transaccion seleccionada.");
        return;
    }

    clear_screen();
    print_header("MODIFICAR TRANSACCION");
    printf("Datos actuales:\n");
    mostrar_transaccion(&transaccion);

    if (!fin_modal_selector_opcion_gui("QUE DESEA MODIFICAR?",
                                       "CAMPO",
                                       opciones,
                                       ARRAY_COUNT(opciones),
                                       0,
                                       &opcion))
    {
        return;
    }

    switch (opcion)
    {
    case 0:
        modificar_fecha_transaccion(id_transaccion);
        break;
    case 1:
        modificar_tipo_transaccion(id_transaccion);
        break;
    case 2:
        modificar_categoria_transaccion(id_transaccion);
        break;
    case 3:
        modificar_descripcion_transaccion(id_transaccion);
        break;
    case 4:
        modificar_monto_transaccion(id_transaccion);
        break;
    case 5:
        modificar_item_especifico_transaccion(id_transaccion);
        break;
    default:
        return;
    }

    pause_console();
#endif
}

/**
 * @brief Eliminar una transaccion financiera
 */
void eliminar_transaccion()
{
    clear_screen();
    print_header("ELIMINAR TRANSACCION FINANCIERA");

#ifdef UNIT_TEST
    // Mostrar lista de transacciones recientes
    sqlite3_stmt *stmt;
    const char *sql_lista = "SELECT id, fecha, tipo, categoria, descripcion, monto FROM financiamiento ORDER BY fecha DESC, id DESC LIMIT 10;";

    if (preparar_stmt(sql_lista, &stmt))
    {
        printf("\n=== ULTIMAS 10 TRANSACCIONES ===\n\n");

        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            found = 1;
            int id = sqlite3_column_int(stmt, 0);
            const char *fecha = (const char *)sqlite3_column_text(stmt, 1);
            int tipo = sqlite3_column_int(stmt, 2);
            int categoria = sqlite3_column_int(stmt, 3);
            const char *descripcion = (const char *)sqlite3_column_text(stmt, 4);
            int monto = sqlite3_column_int(stmt, 5);

            printf("ID: %d | %s | %s | %s | %s | $", id, fecha, get_nombre_tipo_transaccion(tipo), get_nombre_categoria(categoria), descripcion);
            mostrar_monto(monto);
            printf("\n");
        }
        sqlite3_finalize(stmt);

        if (!found)
        {
            printf("No hay transacciones registradas.\n");
            pause_console();
            return;
        }
    }

    int id_transaccion = input_int("\nIngrese el ID de la transaccion a eliminar (0 para cancelar): ");

    if (id_transaccion == 0)
        return;

    // Verificar que existe
    if (!existe_id("financiamiento", id_transaccion))
    {
        printf("ID de transaccion invalido.\n");
        pause_console();
        return;
    }

    // Mostrar la transaccion antes de eliminar
    const char *sql_obtener = "SELECT fecha, tipo, categoria, descripcion, monto, item_especifico FROM financiamiento WHERE id = ?;";

    if (preparar_stmt(sql_obtener, &stmt))
    {
        sqlite3_bind_int(stmt, 1, id_transaccion);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            TransaccionFinanciera transaccion;
            transaccion.id = id_transaccion;
            strncpy_s(transaccion.fecha, sizeof(transaccion.fecha), (const char *)sqlite3_column_text(stmt, 0), sizeof(transaccion.fecha) - 1);
            transaccion.tipo = sqlite3_column_int(stmt, 1);
            transaccion.categoria = sqlite3_column_int(stmt, 2);
            strncpy_s(transaccion.descripcion, sizeof(transaccion.descripcion), (const char *)sqlite3_column_text(stmt, 3), sizeof(transaccion.descripcion) - 1);
            transaccion.monto = sqlite3_column_int(stmt, 4);
            const char *item = (const char *)sqlite3_column_text(stmt, 5);
            if (item)
            {
                strncpy_s(transaccion.item_especifico, sizeof(transaccion.item_especifico), item, sizeof(transaccion.item_especifico) - 1);
            }
            else
            {
                transaccion.item_especifico[0] = '\0';
            }

            printf("\nTransaccion a eliminar:\n");
            mostrar_transaccion(&transaccion);
        }
        sqlite3_finalize(stmt);
    }

    if (confirmar("Esta seguro que desea eliminar esta transaccion? Esta accion no se puede deshacer."))
    {
        const char *sql_delete = "DELETE FROM financiamiento WHERE id = ?;";
        if (preparar_stmt(sql_delete, &stmt))
        {
            sqlite3_bind_int(stmt, 1, id_transaccion);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                printf("Transaccion eliminada exitosamente.\n");
            }
            else
            {
                printf("Error al eliminar la transaccion: %s\n", sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);
        }
    }
    else
    {
        printf("Eliminacion cancelada.\n");
    }

    pause_console();
#else
    int id_transaccion = 0;
    TransaccionFinanciera transaccion;
    sqlite3_stmt *stmt = NULL;

    if (!fin_seleccionar_transaccion_gui("ELIMINAR TRANSACCION",
                                         "Click o ENTER para seleccionar | ESC: cancelar",
                                         10,
                                         &id_transaccion))
    {
        return;
    }

    if (!fin_obtener_transaccion_por_id(id_transaccion, &transaccion))
    {
        fin_mostrar_info_gui("ERROR",
                             "No se pudo cargar la transaccion seleccionada.");
        return;
    }

    printf("\nTransaccion a eliminar:\n");
    mostrar_transaccion(&transaccion);

    if (fin_modal_confirmar_gui("CONFIRMAR ELIMINACION",
                                "Esta seguro que desea eliminar esta transaccion? Esta accion no se puede deshacer.",
                                "Eliminar"))
    {
        const char *sql_delete = "DELETE FROM financiamiento WHERE id = ?;";
        if (preparar_stmt(sql_delete, &stmt))
        {
            sqlite3_bind_int(stmt, 1, id_transaccion);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                printf("Transaccion eliminada exitosamente.\n");
            }
            else
            {
                printf("Error al eliminar la transaccion: %s\n", sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);
        }
    }
    else
    {
        printf("Eliminacion cancelada.\n");
    }

    pause_console();
#endif
}

/**
 * @brief Listar todas las transacciones financieras
 */
void listar_transacciones()
{
    clear_screen();
    print_header("LISTAR TRANSACCIONES FINANCIERAS");

    // Listar todas las transacciones sin filtros
    const char *sql = "SELECT id, fecha, tipo, categoria, descripcion, monto, item_especifico FROM financiamiento ORDER BY fecha DESC, id DESC;";

    sqlite3_stmt *stmt;
    if (preparar_stmt(sql, &stmt))
    {
        printf("=== TODAS LAS TRANSACCIONES FINANCIERAS ===\n\n");

        int total_ingresos = 0;
        int total_gastos = 0;
        int count = 0;

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count++;
            TransaccionFinanciera transaccion;

            transaccion.id = sqlite3_column_int(stmt, 0);
            strncpy_s(transaccion.fecha, sizeof(transaccion.fecha), (const char *)sqlite3_column_text(stmt, 1), sizeof(transaccion.fecha) - 1);
            transaccion.tipo = sqlite3_column_int(stmt, 2);
            transaccion.categoria = sqlite3_column_int(stmt, 3);
            strncpy_s(transaccion.descripcion, sizeof(transaccion.descripcion), (const char *)sqlite3_column_text(stmt, 4), sizeof(transaccion.descripcion) - 1);
            transaccion.monto = sqlite3_column_int(stmt, 5);
            const char *item = (const char *)sqlite3_column_text(stmt, 6);
            if (item)
            {
                strncpy_s(transaccion.item_especifico, sizeof(transaccion.item_especifico), item, sizeof(transaccion.item_especifico) - 1);
                // Truncate after the result to avoid showing Clima and Dia
                truncar_resultado_partido(transaccion.item_especifico);
            }
            else
            {
                transaccion.item_especifico[0] = '\0';
            }

            // Acumuladores para resumen
            if (transaccion.tipo == INGRESO)
            {
                total_ingresos += transaccion.monto;
            }
            else
            {
                total_gastos += transaccion.monto;
            }

            // Mostrar transaccion
            printf("----------------------------------------\n");
            mostrar_transaccion(&transaccion);
        }

        sqlite3_finalize(stmt);

        if (count == 0)
        {
            printf("No hay transacciones registradas.\n");
        }
        else
        {
            printf("========================================\n");
            printf("RESUMEN GENERAL:\n");
            printf("Total Ingresos: $%s\n", formato_monto(total_ingresos));
            printf("Total Gastos: $%s\n", formato_monto(total_gastos));
            printf("Balance: $%s\n", formato_monto(total_ingresos - total_gastos));
            printf("Total de transacciones: %d\n", count);
        }
    }
    else
    {
        printf("Error al preparar la consulta: %s\n", sqlite3_errmsg(db));
        pause_console();
        return;
    }

    pause_console();
}
