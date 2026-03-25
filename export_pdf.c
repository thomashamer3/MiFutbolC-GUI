/**
 * @file export_pdf.c
 * @brief Generacion de informes en PDF con libharu.
 */

#include "export_pdf.h"
#include "export_camisetas.h"
#include "export_camisetas_mejorado.h"
#include "export_partidos.h"
#include "export_lesiones.h"
#include "export_lesiones_mejorado.h"
#include "export_estadisticas.h"
#include "export_estadisticas_generales.h"
#include "export_records_rankings.h"
#include "export.h"
#include "utils.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "pdfgen.h"

#ifdef _WIN32
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

#define PDF_MARGIN 40.0f
#define PDF_BODY_SIZE 10.0f
#define PDF_SUBTITLE_SIZE 12.0f
#define PDF_TITLE_SIZE 16.0f
#define PDF_SECTION_SIZE 12.0f
#define PDF_FOOTER_SIZE 8.0f

static int export_pdf_strncpy_s(char *dest, size_t destsz, const char *src)
{
    if (!dest || !src || destsz == 0)
        return 1;

#if defined(__STDC_LIB_EXT1__)
    return strncpy_s(dest, destsz, src, _TRUNCATE);
#elif defined(_MSC_VER)
    return strncpy_s(dest, destsz, src, _TRUNCATE);
#else
    /* Use strlen_s (Annex K compatible) to avoid reading beyond `destsz` bytes if `src` is not NUL-terminated. */
    size_t len = strlen_s(src, destsz);
    size_t copy = (len >= destsz) ? destsz - 1 : len;
    for (size_t i = 0; i < copy; ++i)
    {
        dest[i] = src[i];
    }
    dest[copy] = '\0';
    return 0;
#endif
}

typedef struct
{
    struct pdf_doc *pdf;
    struct pdf_object *page;
    const char *font_regular;
    const char *font_bold;
    const char *font_italic;
    float margin;
    float y;
    struct pdf_object **pages;
    int page_count;
    int page_cap;
    int warned_pdf_error;
} PdfCtx;

static void sanitize_to_ascii(const char *src, char *dst, size_t dst_size)
{
    if (!dst || dst_size == 0)
        return;

    dst[0] = '\0';
    if (!src)
        return;

    size_t out = 0;
    const unsigned char *p = (const unsigned char *)src;
    while (*p != '\0')
    {
        if (out + 1 >= dst_size)
            break;

        unsigned char c = *p++;
        if (c >= 32 && c <= 126)
        {
            dst[out++] = (char)c;
            continue;
        }

        if (c == '\n' || c == '\r' || c == '\t')
        {
            dst[out++] = ' ';
            continue;
        }

        dst[out++] = '?';
    }

    dst[out] = '\0';
}

static int pdf_add_text_safe(PdfCtx *ctx, struct pdf_object *page, const char *text,
                             float size, float x, float y, uint32_t colour)
{
    int rc = pdf_add_text(ctx->pdf, page, text, size, x, y, colour);
    if (rc >= 0)
        return rc;

    char ascii_text[1024];
    sanitize_to_ascii(text, ascii_text, sizeof(ascii_text));
    rc = pdf_add_text(ctx->pdf, page, ascii_text, size, x, y, colour);
    if (rc >= 0)
        return rc;

    if (!ctx->warned_pdf_error)
    {
        int errval = 0;
        const char *err = pdf_get_err(ctx->pdf, &errval);
        printf("Advertencia PDF: no se pudo escribir texto (%d): %s\n", errval,
               err ? err : "error desconocido");
        ctx->warned_pdf_error = 1;
    }

    return rc;
}

static int pdf_get_text_width_safe(PdfCtx *ctx, const char *font, const char *text,
                                   float size, float *width)
{
    int rc = pdf_get_font_text_width(ctx->pdf, font, text, size, width);
    if (rc == 0)
        return 0;

    char ascii_text[1024];
    sanitize_to_ascii(text, ascii_text, sizeof(ascii_text));
    return pdf_get_font_text_width(ctx->pdf, font, ascii_text, size, width);
}

static void new_page(PdfCtx *ctx)
{
    ctx->page = pdf_append_page(ctx->pdf);
    pdf_page_set_size(ctx->pdf, ctx->page, PDF_A4_WIDTH, PDF_A4_HEIGHT);
    ctx->y = pdf_page_height(ctx->page) - ctx->margin;

    if (ctx->page_cap <= ctx->page_count)
    {
        int new_cap = ctx->page_cap == 0 ? 8 : ctx->page_cap * 2;
        struct pdf_object **new_pages = (struct pdf_object **)realloc(ctx->pages, sizeof(struct pdf_object *) * new_cap);
        if (new_pages)
        {
            ctx->pages = new_pages;
            ctx->page_cap = new_cap;
        }
    }

    if (ctx->pages && ctx->page_count < ctx->page_cap)
    {
        ctx->pages[ctx->page_count++] = ctx->page;
    }
}

static void ensure_space(PdfCtx *ctx, float needed)
{
    if (ctx->y - needed < ctx->margin)
        new_page(ctx);
}

static void write_text_line(PdfCtx *ctx, const char *text, const char *font, float size, float leading)
{
    if (!text)
        return;
    ensure_space(ctx, leading);
    pdf_set_font(ctx->pdf, font);
    pdf_add_text_safe(ctx, ctx->page, text, size, ctx->margin, ctx->y, PDF_BLACK);
    ctx->y -= leading;
}

static void write_blank_line(PdfCtx *ctx, float leading)
{
    ensure_space(ctx, leading);
    ctx->y -= leading;
}

static int preparar_stmt_pdf(sqlite3_stmt **stmt, const char *sql)
{
    if (sqlite3_prepare_v2(db, sql, -1, stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    return 1;
}

typedef void (*PdfRowHandler)(PdfCtx *ctx, sqlite3_stmt *stmt);

/**
 * @brief Ejecuta una consulta que devuelve una sola fila.
 *
 * Esta funcion prepara el statement, enlaza "mes_yyyy_mm" en todos los parametros
 * (para consultas que usan el mismo valor repetido) y ejecuta el handler si hay fila.
 */
static void ejecutar_consulta_una_fila(PdfCtx *ctx, const char *sql, const char *mes_yyyy_mm, int bind_count, PdfRowHandler row_handler)
{
    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt_pdf(&stmt, sql))
        return;

    for (int i = 1; i <= bind_count; ++i)
        sqlite3_bind_text(stmt, i, mes_yyyy_mm, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW && row_handler)
        row_handler(ctx, stmt);

    sqlite3_finalize(stmt);
}

static void format_mes_display(const char *mes_yyyy_mm, char *buffer, int size)
{
    if (!mes_yyyy_mm || safe_strnlen(mes_yyyy_mm, (size_t)size) < 7)
    {
        strncpy_s(buffer, (size_t)size, "N/A", (size_t)size - 1);
        return;
    }

    char yyyy[5] = {mes_yyyy_mm[0], mes_yyyy_mm[1], mes_yyyy_mm[2], mes_yyyy_mm[3], '\0'};
    char mm[3] = {mes_yyyy_mm[5], mes_yyyy_mm[6], '\0'};
    snprintf(buffer, (size_t)size, "%s/%s", mm, yyyy);
}

static void manejar_fila_resumen_partidos(PdfCtx *ctx, sqlite3_stmt *stmt)
{
    int total = sqlite3_column_int(stmt, 0);
    double avg_goles = sqlite3_column_double(stmt, 1);
    double avg_asist = sqlite3_column_double(stmt, 2);
    double avg_rend = sqlite3_column_double(stmt, 3);
    double avg_cans = sqlite3_column_double(stmt, 4);
    double avg_animo = sqlite3_column_double(stmt, 5);

    char line[256];
    snprintf(line, sizeof(line), "Partidos: %d", total);
    write_text_line(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 2.0f);
    snprintf(line, sizeof(line), "Promedio goles: %.2f | asistencias: %.2f", avg_goles, avg_asist);
    write_text_line(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 2.0f);
    snprintf(line, sizeof(line), "Rendimiento: %.2f | cansancio: %.2f | animo: %.2f", avg_rend, avg_cans, avg_animo);
    write_text_line(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 2.0f);
}

static void escribir_resumen_partidos(PdfCtx *ctx, const char *mes_yyyy_mm)
{
    write_text_line(ctx, "Resumen de Partidos", ctx->font_bold, PDF_SECTION_SIZE, PDF_SECTION_SIZE + 4.0f);

    const char *sql_partidos =
        "SELECT COUNT(*), AVG(goles), AVG(asistencias), AVG(rendimiento_general), AVG(cansancio), AVG(estado_animo) "
        "FROM partido WHERE strftime('%Y-%m', fecha_hora) = ?";

    ejecutar_consulta_una_fila(ctx, sql_partidos, mes_yyyy_mm, 1, manejar_fila_resumen_partidos);
}

static void manejar_fila_resumen_entrenamiento(PdfCtx *ctx, sqlite3_stmt *stmt)
{
    int total = sqlite3_column_int(stmt, 0);
    int total_min = sqlite3_column_int(stmt, 1);
    double avg_int = sqlite3_column_double(stmt, 2);
    int omitidos = sqlite3_column_int(stmt, 3);

    char line[256];
    snprintf(line, sizeof(line), "Sesiones: %d | minutos: %d | intensidad promedio: %.2f", total, total_min, avg_int);
    write_text_line(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 2.0f);
    snprintf(line, sizeof(line), "Entrenamientos omitidos: %d", omitidos);
    write_text_line(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 2.0f);
}

static void escribir_resumen_entrenamiento(PdfCtx *ctx, const char *mes_yyyy_mm)
{
    write_blank_line(ctx, PDF_BODY_SIZE + 4.0f);
    write_text_line(ctx, "Resumen de Entrenamiento", ctx->font_bold, PDF_SECTION_SIZE, PDF_SECTION_SIZE + 4.0f);

    const char *sql_entrenamiento =
        "SELECT COUNT(*), SUM(duracion_min), AVG(intensidad), SUM(omitido) "
        "FROM bienestar_entrenamiento WHERE substr(fecha, 1, 7) = ?";

    ejecutar_consulta_una_fila(ctx, sql_entrenamiento, mes_yyyy_mm, 1, manejar_fila_resumen_entrenamiento);
}

static void manejar_fila_resumen_alimentacion(PdfCtx *ctx, sqlite3_stmt *stmt)
{
    int total = sqlite3_column_int(stmt, 0);
    int buenas = sqlite3_column_int(stmt, 1);
    int regulares = sqlite3_column_int(stmt, 2);
    int malas = sqlite3_column_int(stmt, 3);
    double pct_buena = (total > 0) ? ((double)buenas / (double)total) * 100.0 : 0.0;

    char line[256];
    snprintf(line, sizeof(line), "Comidas: %d | Buenas: %d | Regulares: %d | Malas: %d", total, buenas, regulares, malas);
    write_text_line(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 2.0f);
    snprintf(line, sizeof(line), "%% de comidas buenas: %.1f%%", pct_buena);
    write_text_line(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 2.0f);
}

static void manejar_fila_resumen_hidratacion(PdfCtx *ctx, sqlite3_stmt *stmt)
{
    int baja = sqlite3_column_int(stmt, 0);
    int media = sqlite3_column_int(stmt, 1);
    int alta = sqlite3_column_int(stmt, 2);

    char line[256];
    snprintf(line, sizeof(line), "Hidratacion: baja %d | media %d | alta %d", baja, media, alta);
    write_text_line(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 2.0f);
}

static void escribir_resumen_alimentacion(PdfCtx *ctx, const char *mes_yyyy_mm)
{
    write_blank_line(ctx, PDF_BODY_SIZE + 4.0f);
    write_text_line(ctx, "Resumen de Alimentacion", ctx->font_bold, PDF_SECTION_SIZE, PDF_SECTION_SIZE + 4.0f);

    const char *sql_alimentacion =
        "SELECT COUNT(*), "
        "SUM(CASE WHEN calidad = 'Buena' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN calidad = 'Regular' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN calidad = 'Mala' THEN 1 ELSE 0 END) "
        "FROM bienestar_comida WHERE substr(fecha, 1, 7) = ?";

    ejecutar_consulta_una_fila(ctx, sql_alimentacion, mes_yyyy_mm, 1, manejar_fila_resumen_alimentacion);

    const char *sql_hidratacion =
        "SELECT "
        "SUM(CASE WHEN hidratacion = 'Baja' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN hidratacion = 'Media' THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN hidratacion = 'Alta' THEN 1 ELSE 0 END) "
        "FROM bienestar_dia_nutricional WHERE substr(fecha, 1, 7) = ?";

    ejecutar_consulta_una_fila(ctx, sql_hidratacion, mes_yyyy_mm, 1, manejar_fila_resumen_hidratacion);
}

static void manejar_fila_resumen_mental(PdfCtx *ctx, sqlite3_stmt *stmt)
{
    int total = sqlite3_column_int(stmt, 0);
    double conf = sqlite3_column_double(stmt, 1);
    double estres = sqlite3_column_double(stmt, 2);
    double mot = sqlite3_column_double(stmt, 3);
    double pres = sqlite3_column_double(stmt, 4);
    double conc = sqlite3_column_double(stmt, 5);

    char line[256];
    snprintf(line, sizeof(line), "Sesiones: %d | confianza %.2f | estres %.2f", total, conf, estres);
    write_text_line(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 2.0f);
    snprintf(line, sizeof(line), "Motivacion %.2f | presion %.2f | concentracion %.2f", mot, pres, conc);
    write_text_line(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 2.0f);
}

static void manejar_fila_resumen_tendencia(PdfCtx *ctx, sqlite3_stmt *stmt)
{
    double avg_alta = sqlite3_column_double(stmt, 0);
    double avg_baja = sqlite3_column_double(stmt, 1);

    char line[256];
    snprintf(line, sizeof(line), "Rendimiento con confianza alta: %.2f | baja: %.2f", avg_alta, avg_baja);
    write_text_line(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 2.0f);

    if (avg_alta > avg_baja)
    {
        write_text_line(ctx, "Tendencia: Jugas mejor cuando la confianza esta alta.", ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 2.0f);
    }
}

static void escribir_resumen_mental(PdfCtx *ctx, const char *mes_yyyy_mm)
{
    write_blank_line(ctx, PDF_BODY_SIZE + 4.0f);
    write_text_line(ctx, "Resumen de Mentalidad", ctx->font_bold, PDF_SECTION_SIZE, PDF_SECTION_SIZE + 4.0f);

    const char *sql_mental =
        "SELECT COUNT(*), AVG(confianza), AVG(estres), AVG(motivacion), AVG(presion), AVG(concentracion) "
        "FROM bienestar_sesion_mental WHERE substr(fecha, 1, 7) = ?";

    ejecutar_consulta_una_fila(ctx, sql_mental, mes_yyyy_mm, 1, manejar_fila_resumen_mental);

    const char *sql_tendencia =
        "SELECT "
        "(SELECT AVG(p.rendimiento_general) FROM partido p "
        " WHERE strftime('%Y-%m', p.fecha_hora) = ? AND strftime('%Y-%m-%d', p.fecha_hora) IN "
        " (SELECT DISTINCT fecha FROM bienestar_sesion_mental WHERE confianza >= 8 AND substr(fecha, 1, 7) = ?)) AS avg_alta, "
        "(SELECT AVG(p.rendimiento_general) FROM partido p "
        " WHERE strftime('%Y-%m', p.fecha_hora) = ? AND strftime('%Y-%m-%d', p.fecha_hora) IN "
        " (SELECT DISTINCT fecha FROM bienestar_sesion_mental WHERE confianza <= 4 AND substr(fecha, 1, 7) = ?)) AS avg_baja";

    ejecutar_consulta_una_fila(ctx, sql_tendencia, mes_yyyy_mm, 4, manejar_fila_resumen_tendencia);
}

static const char *skip_spaces(const char *p)
{
    while (p && (*p == ' ' || *p == '\t'))
        p++;
    return p;
}

static const char *read_word(const char *p, char *word, size_t max_len)
{
    size_t wlen = 0;
    if (!p || !word || max_len == 0)
        return p;

    while (*p && *p != ' ' && *p != '\t' && wlen + 1 < max_len)
    {
        word[wlen++] = *p++;
    }
    word[wlen] = '\0';
    return p;
}

static void flush_line(PdfCtx *ctx, char *line, const char *font, float size, float leading)
{
    if (line && line[0] != '\0')
    {
        write_text_line(ctx, line, font, size, leading);
        line[0] = '\0';
    }
}

static void write_long_word(PdfCtx *ctx, const char *word, const char *font, float size, float leading, float max_width)
{
    char chunk[256];
    size_t clen = 0;

    for (size_t i = 0; word && word[i] != '\0'; i++)
    {
        if (clen + 1 >= sizeof(chunk))
        {
            chunk[clen] = '\0';
            write_text_line(ctx, chunk, font, size, leading);
            clen = 0;
        }

        chunk[clen++] = word[i];
        chunk[clen] = '\0';

        float chunk_width = 0.0f;
        if (pdf_get_text_width_safe(ctx, font, chunk, size, &chunk_width) == 0 && chunk_width > max_width && clen > 1)
        {
            chunk[clen - 1] = '\0';
            write_text_line(ctx, chunk, font, size, leading);
            clen = 0;
            chunk[clen++] = word[i];
            chunk[clen] = '\0';
        }
    }

    if (clen > 0)
    {
        chunk[clen] = '\0';
        write_text_line(ctx, chunk, font, size, leading);
    }
}

static int try_append_word(PdfCtx *ctx, char *line, size_t line_size, const char *word, const char *font, float size, float max_width)
{
    if (!ctx || !line || !word || line_size == 0)
        return 0;

    size_t line_len = strlen_s(line, SIZE_MAX);
    size_t word_len = strlen_s(word, SIZE_MAX);

    // Guard against overflowing the temporary candidate buffer
    if (line_len + 1 + word_len >= sizeof(char[512]))
        return 0;

    int fits_in_buffer = 0;
    if (line_len == 0)
    {
        fits_in_buffer = (word_len + 1 <= line_size);
    }
    else
    {
        fits_in_buffer = (line_len + 1 + word_len + 1 <= line_size);
    }

    if (!fits_in_buffer)
        return 0;

    char candidate[512];
    if (line_len == 0)
    {
        memcpy(candidate, word, word_len + 1);
    }
    else
    {
        memcpy(candidate, line, line_len);
        candidate[line_len] = ' ';
        memcpy(candidate + line_len + 1, word, word_len + 1);
    }

    float candidate_width = 0.0f;
    if (pdf_get_text_width_safe(ctx, font, candidate, size, &candidate_width) != 0)
        return 0;

    if (candidate_width > max_width)
        return 0;

    size_t cand_len = strlen_s(candidate, SIZE_MAX);
    memcpy(line, candidate, cand_len + 1);
    return 1;
}

static void write_wrapped_text(PdfCtx *ctx, const char *text, const char *font, float size, float leading)
{
    if (!text || text[0] == '\0')
    {
        if (text)
            write_blank_line(ctx, leading);
        return;
    }

    pdf_set_font(ctx->pdf, font);
    float max_width = pdf_page_width(ctx->page) - (2.0f * ctx->margin);

    char line[512];
    line[0] = '\0';

    const char *p = text;
    while (p && *p)
    {
        p = skip_spaces(p);
        if (!p || *p == '\0')
            break;

        char word[256];
        p = read_word(p, word, sizeof(word));
        if (word[0] == '\0')
            continue;

        if (try_append_word(ctx, line, sizeof(line), word, font, size, max_width))
            continue;

        flush_line(ctx, line, font, size, leading);

        float word_width = 0.0f;
        if (pdf_get_text_width_safe(ctx, font, word, size, &word_width) != 0)
            word_width = max_width + 1;

        if (word_width <= max_width)
        {
            size_t word_len = strlen_s(word, SIZE_MAX);
            size_t copy_len = word_len < (sizeof(line) - 1) ? word_len : (sizeof(line) - 1);
            strncpy_s(line, sizeof(line), word, copy_len);
            line[copy_len] = '\0';
            continue;
        }

        write_long_word(ctx, word, font, size, leading, max_width);
    }

    flush_line(ctx, line, font, size, leading);
}

static void write_section_header(PdfCtx *ctx, const char *title)
{
    if (!title)
        return;

    float leading = PDF_SECTION_SIZE + 6.0f;
    ensure_space(ctx, leading + 10.0f);

    float page_width = pdf_page_width(ctx->page);
    float band_height = PDF_SECTION_SIZE + 6.0f;
    float band_y = ctx->y - PDF_SECTION_SIZE - 2.0f;

    pdf_add_filled_rectangle(ctx->pdf, ctx->page, ctx->margin, band_y, page_width - (2.0f * ctx->margin), band_height, 0.0f, PDF_RGB(235, 240, 250), PDF_RGB(235, 240, 250));

    pdf_set_font(ctx->pdf, ctx->font_bold);
    pdf_add_text_safe(ctx, ctx->page, title, PDF_SECTION_SIZE, ctx->margin + 6.0f, ctx->y, PDF_BLACK);

    ctx->y = band_y - 8.0f;
    write_blank_line(ctx, PDF_BODY_SIZE + 2.0f);
}

static void add_page_footers(PdfCtx *ctx)
{
    if (!ctx || !ctx->pdf || !ctx->font_regular || !ctx->pages)
        return;

    int pages = ctx->page_count;
    for (int i = 0; i < pages; i++)
    {
        struct pdf_object *page = ctx->pages[i];
        if (!page)
            continue;

        char footer[64];
        snprintf(footer, sizeof(footer), "MiFutbolC - Pagina %d de %d", i + 1, pages);

        float page_width = pdf_page_width(page);
        float y = ctx->margin * 0.5f;

        pdf_add_line(ctx->pdf, page, ctx->margin, y + 6.0f, page_width - ctx->margin, y + 6.0f, 0.3f, PDF_RGB(204, 204, 204));

        pdf_set_font(ctx->pdf, ctx->font_regular);
        float text_width = 0.0f;
        pdf_get_text_width_safe(ctx, ctx->font_regular, footer, PDF_FOOTER_SIZE, &text_width);
        pdf_add_text_safe(ctx, page, footer, PDF_FOOTER_SIZE, (page_width - text_width) / 2.0f, y, PDF_BLACK);
    }
}

static void draw_cover(PdfCtx *ctx, const char *title, const char *subtitle, const char *usuario,
                       const char *datetime, const char *export_dir, int total_sections)
{
    if (!ctx)
        return;

    new_page(ctx);

    float page_width = pdf_page_width(ctx->page);
    float page_height = pdf_page_height(ctx->page);

    pdf_add_filled_rectangle(ctx->pdf, ctx->page, 0, page_height - 140.0f, page_width, 140.0f, 0.0f, PDF_RGB(26, 89, 166), PDF_RGB(26, 89, 166));

    pdf_set_font(ctx->pdf, ctx->font_bold);
    float title_width = 0.0f;
    pdf_get_text_width_safe(ctx, ctx->font_bold, title, 24.0f, &title_width);
    pdf_add_text_safe(ctx, ctx->page, title, 24.0f, (page_width - title_width) / 2.0f, page_height - 85.0f, PDF_WHITE);

    pdf_set_font(ctx->pdf, ctx->font_regular);
    float sub_width = 0.0f;
    pdf_get_text_width_safe(ctx, ctx->font_regular, subtitle, PDF_SUBTITLE_SIZE, &sub_width);
    pdf_add_text_safe(ctx, ctx->page, subtitle, PDF_SUBTITLE_SIZE, (page_width - sub_width) / 2.0f, page_height - 110.0f, PDF_WHITE);

    ctx->y = page_height - 190.0f;

    write_text_line(ctx, "Resumen del informe", ctx->font_bold, PDF_SECTION_SIZE, PDF_SECTION_SIZE + 6.0f);
    write_blank_line(ctx, PDF_BODY_SIZE + 2.0f);

    char line[512];
    snprintf(line, sizeof(line), "Usuario: %s", usuario ? usuario : "Usuario Desconocido");
    write_text_line(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 4.0f);
    snprintf(line, sizeof(line), "Fecha y hora: %s", datetime ? datetime : "-");
    write_text_line(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 4.0f);
    snprintf(line, sizeof(line), "Directorio de exportacion: %s", export_dir ? export_dir : "-");
    write_wrapped_text(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 4.0f);
    snprintf(line, sizeof(line), "Secciones incluidas: %d", total_sections);
    write_text_line(ctx, line, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 4.0f);

    write_blank_line(ctx, PDF_BODY_SIZE + 10.0f);
    write_wrapped_text(ctx, "Documento generado automaticamente por MiFutbolC.", ctx->font_italic,
                       PDF_BODY_SIZE, PDF_BODY_SIZE + 4.0f);
}

static void format_datetime_filename(const char *src, char *dst, size_t size)
{
    if (!dst || size == 0)
        return;

    dst[0] = '\0';
    if (!src)
        return;

    char *out = dst;
    size_t remaining = size;
    const char *p = src;
    while (*p != '\0' && remaining > 1)
    {
        char c = *p++;
        if (c == '/')
            c = '-';
        else if (c == ' ')
            c = '-';
        else if (c == ':')
            c = '.';
        *out++ = c;
        remaining--;
    }
    *out = '\0';
}

static void procesar_archivo_txt(const char *path, const char *titulo, PdfCtx *ctx)
{
    if (!path || !titulo || !ctx)
        return;

    write_section_header(ctx, titulo);

    FILE *f = NULL;
    errno_t err = fopen_s(&f, path, "r");
    if (err != 0 || !f)
    {
        write_wrapped_text(ctx, "[Archivo no disponible]", ctx->font_italic, PDF_BODY_SIZE, PDF_BODY_SIZE + 3.0f);
        write_blank_line(ctx, PDF_BODY_SIZE + 3.0f);
        return;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), f))
    {
        size_t len = strlen_s(buffer, SIZE_MAX);
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
        {
            buffer[len - 1] = '\0';
            len--;
        }
        write_wrapped_text(ctx, buffer, ctx->font_regular, PDF_BODY_SIZE, PDF_BODY_SIZE + 3.0f);
    }

    fclose(f);
    write_blank_line(ctx, PDF_BODY_SIZE + 3.0f);
}

static int escribir_pdf(const char *filepath, PdfCtx *ctx)
{
    if (!filepath || !ctx || !ctx->pdf)
        return 0;

    return pdf_save(ctx->pdf, filepath) >= 0;
}

typedef struct
{
    const char *title;
    const char *subject;
    const char *cover_subtitle;
    const char *usuario;
    const char *fecha_hora;
    const char *export_dir;
    int total_sections;
} PdfExportOptions;

static int init_pdf_export(PdfCtx *ctx, const PdfExportOptions *opts)
{
    if (!ctx || !opts || !opts->export_dir || !opts->fecha_hora)
        return 0;

    const char *usuario_final = opts->usuario ? opts->usuario : "Usuario Desconocido";

    memset(ctx, 0, sizeof(*ctx));
    ctx->margin = PDF_MARGIN;

    struct pdf_info pdf_info = {0};
    export_pdf_strncpy_s(pdf_info.creator, sizeof(pdf_info.creator), "MiFutbolC");
    export_pdf_strncpy_s(pdf_info.producer, sizeof(pdf_info.producer), "MiFutbolC");
    export_pdf_strncpy_s(pdf_info.title, sizeof(pdf_info.title), opts->title);
    export_pdf_strncpy_s(pdf_info.author, sizeof(pdf_info.author), usuario_final);
    export_pdf_strncpy_s(pdf_info.subject, sizeof(pdf_info.subject), opts->subject);
    export_pdf_strncpy_s(pdf_info.date, sizeof(pdf_info.date), opts->fecha_hora);

    ctx->pdf = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT, &pdf_info);
    if (!ctx->pdf)
    {
        printf("No se pudo crear el documento PDF.\n");
        return 0;
    }

    ctx->font_regular = "Helvetica";
    ctx->font_bold = "Helvetica-Bold";
    ctx->font_italic = "Helvetica-Oblique";

    draw_cover(ctx, "MiFutbolC", opts->cover_subtitle, usuario_final, opts->fecha_hora,
               opts->export_dir, opts->total_sections);

    new_page(ctx);
    return 1;
}

static void cleanup_pdf_ctx(PdfCtx *ctx)
{
    if (!ctx)
        return;

    if (ctx->pdf)
        pdf_destroy(ctx->pdf);

    if (ctx->pages)
        free(ctx->pages);

    memset(ctx, 0, sizeof(*ctx));
}

int generar_informe_total_pdf(void)
{
    exportar_camisetas_txt_mejorado();
    exportar_partidos_txt();
    exportar_partido_mas_goles_txt();
    exportar_partido_mas_asistencias_txt();
    exportar_partido_menos_goles_reciente_txt();
    exportar_partido_menos_asistencias_reciente_txt();
    exportar_lesiones_txt();
    exportar_lesiones_txt_mejorado();
    exportar_estadisticas_txt();
    exportar_analisis_txt();
    exportar_estadisticas_generales_txt();
    exportar_estadisticas_por_mes_txt();
    exportar_estadisticas_por_anio_txt();
    exportar_records_rankings_txt();
    exportar_finanzas_resumen_txt();
    exportar_ranking_canchas_txt();
    exportar_partidos_por_clima_txt();
    exportar_lesiones_por_tipo_estado_txt();
    exportar_rachas_historial_txt();
    exportar_estado_animo_cansancio_txt();

    const char *export_dir = get_export_dir();
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));

    char fecha_hora[32];
    get_datetime(fecha_hora, sizeof(fecha_hora));

    char fecha_archivo[64];
    format_datetime_filename(fecha_hora, fecha_archivo, sizeof(fecha_archivo));

    char pdf_filename[128];
    snprintf(pdf_filename, sizeof(pdf_filename), "Informe Total %s.pdf", fecha_archivo);

    char pdf_path[512];
    snprintf(pdf_path, sizeof(pdf_path), "%s%s%s", export_dir, PATH_SEP, pdf_filename);

    const char *files[] =
    {
        "camisetas_mejorado.txt",
        "partidos.txt",
        "partido_mas_goles.txt",
        "partido_mas_asistencias.txt",
        "partido_menos_goles_reciente.txt",
        "partido_menos_asistencias_reciente.txt",
        "lesiones.txt",
        "lesiones_mejorado.txt",
        "estadisticas.txt",
        "analisis.txt",
        "estadisticas_generales.txt",
        "estadisticas_por_mes.txt",
        "estadisticas_por_anio.txt",
        "records_rankings.txt",
        "finanzas_resumen.txt",
        "ranking_canchas.txt",
        "partidos_por_clima.txt",
        "lesiones_por_tipo_estado.txt",
        "rachas_historial.txt",
        "estado_animo_cansancio.txt"
    };

    const char *titles[] =
    {
        "Camisetas (analisis avanzado)",
        "Partidos",
        "Partido con mas goles",
        "Partido con mas asistencias",
        "Partido menos goles reciente",
        "Partido menos asistencias reciente",
        "Lesiones",
        "Lesiones (analisis de impacto)",
        "Estadisticas",
        "Analisis",
        "Estadisticas generales",
        "Estadisticas por mes",
        "Estadisticas por anio",
        "Records y rankings",
        "Resumen financiero",
        "Ranking de canchas",
        "Partidos por clima",
        "Lesiones por tipo y estado",
        "Historial de rachas",
        "Estado de animo y cansancio"
    };

    char *usuario = get_user_name();

    PdfCtx ctx;
    PdfExportOptions opts =
    {
        .title = "Informe Total",
        .subject = "Informe de exportacion",
        .cover_subtitle = "Informe total de exportacion",
        .usuario = usuario,
        .fecha_hora = fecha_hora,
        .export_dir = export_dir,
        .total_sections = (int)(sizeof(files) / sizeof(files[0])),
    };

    if (!init_pdf_export(&ctx, &opts))
    {
        if (usuario)
            free(usuario);
        return 0;
    }

    char intro[256];
    snprintf(intro, sizeof(intro), "INFORME TOTAL MiFutbolC - %s", timestamp);
    write_text_line(&ctx, intro, ctx.font_bold, PDF_TITLE_SIZE, PDF_TITLE_SIZE + 6.0f);
    write_blank_line(&ctx, PDF_BODY_SIZE + 4.0f);

    for (int i = 0; i < (int)(sizeof(files) / sizeof(files[0])); i++)
    {
        char path[512];
        snprintf(path, sizeof(path), "%s%s%s", export_dir, PATH_SEP, files[i]);
        procesar_archivo_txt(path, titles[i], &ctx);
    }

    add_page_footers(&ctx);

    int ok = escribir_pdf(pdf_path, &ctx);
    cleanup_pdf_ctx(&ctx);

    if (usuario)
        free(usuario);

    if (!ok)
    {
        printf("No se pudo generar el informe PDF.\n");
        return 0;
    }

    printf("Informe PDF generado exitosamente: %s\n", pdf_path);
    return 1;
}

int generar_informe_personal_mensual_pdf(const char *mes_yyyy_mm)
{
    if (!mes_yyyy_mm || safe_strnlen(mes_yyyy_mm, 8) < 7)
    {
        printf("Mes invalido para informe mensual.\n");
        return 0;
    }

    const char *export_dir = get_export_dir();
    if (!export_dir)
    {
        printf("No se pudo obtener el directorio de exportacion.\n");
        return 0;
    }

    char mes_display[16];
    format_mes_display(mes_yyyy_mm, mes_display, sizeof(mes_display));

    char mes_filename[16];
    format_datetime_filename(mes_display, mes_filename, sizeof(mes_filename));

    char pdf_filename[128];
    snprintf(pdf_filename, sizeof(pdf_filename), "Informe Personal %s.pdf", mes_filename);

    char pdf_path[512];
    snprintf(pdf_path, sizeof(pdf_path), "%s%s%s", export_dir, PATH_SEP, pdf_filename);

    char fecha_hora[32];
    get_datetime(fecha_hora, sizeof(fecha_hora));

    char *usuario = get_user_name();

    PdfCtx ctx;
    PdfExportOptions opts =
    {
        .title = "Informe Personal Mensual",
        .subject = "Informe personal mensual",
        .cover_subtitle = "Informe personal mensual",
        .usuario = usuario,
        .fecha_hora = fecha_hora,
        .export_dir = export_dir,
        .total_sections = 1,
    };

    if (!init_pdf_export(&ctx, &opts))
    {
        if (usuario)
            free(usuario);
        return 0;
    }

    char titulo[128];
    snprintf(titulo, sizeof(titulo), "INFORME PERSONAL MENSUAL - %s", mes_display);
    write_text_line(&ctx, titulo, ctx.font_bold, PDF_TITLE_SIZE, PDF_TITLE_SIZE + 6.0f);
    write_blank_line(&ctx, PDF_BODY_SIZE + 4.0f);

    escribir_resumen_partidos(&ctx, mes_yyyy_mm);
    escribir_resumen_entrenamiento(&ctx, mes_yyyy_mm);
    escribir_resumen_alimentacion(&ctx, mes_yyyy_mm);
    escribir_resumen_mental(&ctx, mes_yyyy_mm);

    write_blank_line(&ctx, PDF_BODY_SIZE + 6.0f);
    write_text_line(&ctx, "Este informe es 100% local.", ctx.font_italic, PDF_BODY_SIZE, PDF_BODY_SIZE + 2.0f);

    int ok = escribir_pdf(pdf_path, &ctx);
    cleanup_pdf_ctx(&ctx);

    if (usuario)
        free(usuario);

    if (ok)
    {
        printf("Informe mensual generado en: %s\n", pdf_path);
        return 1;
    }

    printf("No se pudo guardar el informe mensual.\n");
    return 0;
}
