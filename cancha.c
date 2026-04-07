#include "cancha.h"
#include "menu.h"
#include "db.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "raylib.h"
#include "gui_components.h"
#include "input.h"
#ifdef _WIN32
#include <process.h>
#include <io.h>
#else
#include "process.h"
#include <strings.h>
#endif


static int preparar_stmt(sqlite3_stmt **stmt, const char *sql)
{
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}

typedef struct
{
    int id;
    char nombre[128];
} CanchaRow;

typedef struct
{
    int x;
    int y;
    int w;
    int h;
    int visible_rows;
} CanchaGuiPanel;

static int ampliar_capacidad_canchas_gui(CanchaRow **rows, int *cap)
{
    int new_cap = (*cap) * 2;
    CanchaRow *tmp = (CanchaRow *)realloc(*rows, (size_t)new_cap * sizeof(CanchaRow));
    if (!tmp)
    {
        return 0;
    }

    memset(tmp + (*cap), 0, (size_t)(new_cap - (*cap)) * sizeof(CanchaRow));
    *rows = tmp;
    *cap = new_cap;
    return 1;
}

static int cargar_canchas_gui_rows(CanchaRow **rows_out, int *count_out)
{
    sqlite3_stmt *stmt = NULL;
    int cap = 32;
    int count = 0;
    CanchaRow *rows = (CanchaRow *)calloc((size_t)cap, sizeof(CanchaRow));
    if (!rows)
    {
        return 0;
    }

    if (!preparar_stmt(&stmt, "SELECT id, nombre FROM cancha ORDER BY id"))
    {
        free(rows);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (count >= cap && !ampliar_capacidad_canchas_gui(&rows, &cap))
        {
            sqlite3_finalize(stmt);
            free(rows);
            return 0;
        }

        rows[count].id = sqlite3_column_int(stmt, 0);
        snprintf(rows[count].nombre,
                 sizeof(rows[count].nombre),
                 "%s",
                 (const char *)(sqlite3_column_text(stmt, 1) ? sqlite3_column_text(stmt, 1) : (const unsigned char *)"(sin nombre)"));
        count++;
    }

    sqlite3_finalize(stmt);
    *rows_out = rows;
    *count_out = count;
    return 1;
}

static int clamp_scroll_canchas_gui(int scroll, int count, int visible_rows)
{
    int max_scroll = count - visible_rows;
    if (max_scroll < 0)
    {
        max_scroll = 0;
    }

    if (scroll < 0)
    {
        return 0;
    }

    if (scroll > max_scroll)
    {
        return max_scroll;
    }

    return scroll;
}

static int actualizar_scroll_canchas_gui(int scroll, int count, int visible_rows, Rectangle area)
{
    float wheel = GetMouseWheelMove();
    if (CheckCollisionPointRec(GetMousePosition(), area))
    {
        if (wheel < 0.0f && scroll < count - visible_rows)
        {
            scroll++;
        }
        else if (wheel > 0.0f && scroll > 0)
        {
            scroll--;
        }
    }

    return clamp_scroll_canchas_gui(scroll, count, visible_rows);
}

static void calcular_panel_listado_canchas_gui(int sw, int sh, int row_h, CanchaGuiPanel *panel)
{
    panel->x = (sw > 900) ? 80 : 20;
    panel->y = 130;
    panel->w = sw - (panel->x * 2);
    panel->h = sh - 220;
    if (panel->h < 200)
    {
        panel->h = 200;
    }

    panel->visible_rows = panel->h / row_h;
    if (panel->visible_rows < 1)
    {
        panel->visible_rows = 1;
    }
}

typedef struct
{
    int id;
    char nombre[128];
    int partidos_jugados;
    int goles_totales;
    int asistencias_totales;
    char ruta_imagen[1200];
    Texture2D textura;
    int textura_cargada;
} CanchaListadoCard;

typedef struct
{
    int panel_x;
    int panel_y;
    int panel_w;
    int panel_h;
    int area_y;
    int area_h;
    int card_w;
    int card_h;
    int gap_x;
    int gap_y;
    int cols;
    int total_rows;
    int visible_rows;
} CanchaCardsLayout;

static int resolver_ruta_fallback_cancha(char *ruta, size_t size)
{
    const char *candidatas[] = {
        "./Icons/Canchas-2.png",
        "../../Icons/Canchas-2.png",
        "../Icons/Canchas-2.png",
        "Icons/Canchas-2.png",
        "./images/Canchas-2.png",
        "images/Canchas-2.png",
        NULL
    };

    if (!ruta || size == 0)
    {
        return 0;
    }

    ruta[0] = '\0';
    for (int i = 0; candidatas[i] != NULL; i++)
    {
        if (FileExists(candidatas[i]))
        {
            return strncpy_s(ruta, size, candidatas[i], _TRUNCATE) == 0;
        }
    }

    return 0;
}

static int cargar_textura_listado_cancha(const char *ruta, Texture2D *textura)
{
    Texture2D t;

    if (!ruta || ruta[0] == '\0' || !textura || !FileExists(ruta))
    {
        return 0;
    }

    t = LoadTexture(ruta);
    if (t.id == 0)
    {
        return 0;
    }

    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    *textura = t;
    return 1;
}

static int cargar_canchas_listado_cards(CanchaListadoCard **cards_out, int *count_out)
{
    sqlite3_stmt *stmt = NULL;
    int cap = 48;
    int count = 0;
    CanchaListadoCard *cards;

    if (!cards_out || !count_out)
    {
        return 0;
    }

    cards = (CanchaListadoCard *)calloc((size_t)cap, sizeof(CanchaListadoCard));
    if (!cards)
    {
        return 0;
    }

    if (!preparar_stmt(&stmt,
                       "SELECT c.id, c.nombre, c.imagen_ruta, "
                       "COUNT(p.id), IFNULL(SUM(p.goles),0), IFNULL(SUM(p.asistencias),0) "
                       "FROM cancha c "
                       "LEFT JOIN partido p ON p.cancha_id = c.id "
                       "GROUP BY c.id, c.nombre, c.imagen_ruta "
                       "ORDER BY c.id"))
    {
        free(cards);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (count >= cap)
        {
            int new_cap = cap * 2;
            CanchaListadoCard *tmp = (CanchaListadoCard *)realloc(cards,
                                                                   (size_t)new_cap * sizeof(CanchaListadoCard));
            if (!tmp)
            {
                sqlite3_finalize(stmt);
                free(cards);
                return 0;
            }
            memset(tmp + cap, 0, (size_t)(new_cap - cap) * sizeof(CanchaListadoCard));
            cards = tmp;
            cap = new_cap;
        }

        cards[count].id = sqlite3_column_int(stmt, 0);
        cards[count].partidos_jugados = sqlite3_column_int(stmt, 3);
        cards[count].goles_totales = sqlite3_column_int(stmt, 4);
        cards[count].asistencias_totales = sqlite3_column_int(stmt, 5);

        snprintf(cards[count].nombre,
                 sizeof(cards[count].nombre),
                 "%s",
                 (const char *)(sqlite3_column_text(stmt, 1)
                                    ? sqlite3_column_text(stmt, 1)
                                    : (const unsigned char *)"(sin nombre)"));

        {
            const unsigned char *ruta_db = sqlite3_column_text(stmt, 2);
            if (ruta_db && ruta_db[0] != '\0')
            {
                char ruta_db_buf[300] = {0};
                if (strncpy_s(ruta_db_buf,
                              sizeof(ruta_db_buf),
                              (const char *)ruta_db,
                              _TRUNCATE) == 0)
                {
                    (void)db_resolve_image_absolute_path(ruta_db_buf,
                                                         cards[count].ruta_imagen,
                                                         sizeof(cards[count].ruta_imagen));
                }
            }
        }

        if (cards[count].ruta_imagen[0] != '\0' &&
            cargar_textura_listado_cancha(cards[count].ruta_imagen, &cards[count].textura))
        {
            cards[count].textura_cargada = 1;
        }

        count++;
    }

    sqlite3_finalize(stmt);
    *cards_out = cards;
    *count_out = count;
    return 1;
}

static void liberar_canchas_listado_cards(CanchaListadoCard *cards, int count)
{
    if (!cards)
    {
        return;
    }

    for (int i = 0; i < count; i++)
    {
        if (cards[i].textura_cargada && cards[i].textura.id != 0)
        {
            UnloadTexture(cards[i].textura);
        }
    }

    free(cards);
}

static void calcular_layout_cards_canchas(int sw, int sh, int count, CanchaCardsLayout *layout)
{
    if (!layout)
    {
        return;
    }

    layout->panel_x = (sw > 1000) ? 54 : 16;
    layout->panel_y = 112;
    layout->panel_w = sw - (layout->panel_x * 2);
    layout->panel_h = sh - 190;
    if (layout->panel_w < 250)
    {
        layout->panel_w = 250;
    }
    if (layout->panel_h < 240)
    {
        layout->panel_h = 240;
    }

    layout->card_w = (sw > 1400) ? 276 : 248;
    layout->card_h = 312;
    layout->gap_x = 18;
    layout->gap_y = 18;

    layout->area_y = layout->panel_y + 16;
    layout->area_h = layout->panel_h - 32;
    if (layout->area_h < layout->card_h)
    {
        layout->area_h = layout->card_h;
    }

    layout->cols = (layout->panel_w + layout->gap_x) / (layout->card_w + layout->gap_x);
    if (layout->cols < 1)
    {
        layout->cols = 1;
    }

    layout->visible_rows = (layout->area_h + layout->gap_y) / (layout->card_h + layout->gap_y);
    if (layout->visible_rows < 1)
    {
        layout->visible_rows = 1;
    }

    layout->total_rows = (count + layout->cols - 1) / layout->cols;
}

static Rectangle calcular_dst_textura_en_caja_cancha(Texture2D tex, Rectangle caja)
{
    float sx = caja.width / (float)tex.width;
    float sy = caja.height / (float)tex.height;
    float scale = (sx < sy) ? sx : sy;
    float w = (float)tex.width * scale;
    float h = (float)tex.height * scale;
    float x = caja.x + (caja.width - w) * 0.5f;
    float y = caja.y + (caja.height - h) * 0.5f;

    return (Rectangle){x, y, w, h};
}

static void dibujar_preview_card_cancha(const CanchaListadoCard *card,
                                        const Texture2D *fallback_tex,
                                        int fallback_loaded)
{
    const GuiTheme *theme = gui_get_active_theme();
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    int panel_w;
    int panel_h;
    int panel_x;
    int panel_y;
    Rectangle image_box;

    if (!card)
    {
        return;
    }

    panel_w = (sw > 1200) ? 900 : (sw - 44);
    panel_h = (sh > 780) ? 680 : (sh - 56);
    if (panel_w < 320)
    {
        panel_w = 320;
    }
    if (panel_h < 260)
    {
        panel_h = 260;
    }

    panel_x = (sw - panel_w) / 2;
    panel_y = (sh - panel_h) / 2;
    image_box = (Rectangle){(float)panel_x + 18.0f,
                            (float)panel_y + 72.0f,
                            (float)panel_w - 36.0f,
                            (float)panel_h - 220.0f};

    DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 176});
    DrawRectangle(panel_x, panel_y, panel_w, panel_h, theme->card_bg);
    DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, theme->accent_primary);

    gui_text(TextFormat("CANCHA %d", card->id),
             (float)panel_x + 20.0f,
             (float)panel_y + 20.0f,
             24.0f,
             theme->text_primary);
    gui_text_truncated(card->nombre,
                       (float)panel_x + 20.0f,
                       (float)panel_y + 46.0f,
                       20.0f,
                       (float)panel_w - 40.0f,
                       theme->text_secondary);

    DrawRectangleRec(image_box, (Color){12, 24, 17, 255});
    DrawRectangleLinesEx(image_box, 1.0f, (Color){56, 96, 69, 255});

    {
        const Texture2D *tex = NULL;
        if (card->textura_cargada && card->textura.id != 0)
        {
            tex = &card->textura;
        }
        else if (fallback_loaded && fallback_tex && fallback_tex->id != 0)
        {
            tex = fallback_tex;
        }

        if (tex)
        {
            Rectangle src = {0.0f, 0.0f, (float)tex->width, (float)tex->height};
            Rectangle dst = calcular_dst_textura_en_caja_cancha(*tex, image_box);
            DrawTexturePro(*tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
        }
        else
        {
            gui_text("Sin imagen disponible",
                     image_box.x + 24.0f,
                     image_box.y + image_box.height * 0.5f - 10.0f,
                     20.0f,
                     theme->text_secondary);
        }
    }

    gui_text(TextFormat("Partidos Jugados: %d", card->partidos_jugados),
             (float)panel_x + 20.0f,
             (float)panel_y + (float)panel_h - 118.0f,
             20.0f,
             theme->text_primary);
    gui_text(TextFormat("Goles: %d", card->goles_totales),
             (float)panel_x + 20.0f,
             (float)panel_y + (float)panel_h - 88.0f,
             19.0f,
             theme->text_secondary);
    gui_text(TextFormat("Asistencias: %d", card->asistencias_totales),
             (float)panel_x + 20.0f,
             (float)panel_y + (float)panel_h - 60.0f,
             19.0f,
             theme->text_secondary);
    gui_text("Click / ESC / ENTER para cerrar",
             (float)panel_x + 20.0f,
             (float)panel_y + (float)panel_h - 32.0f,
             18.0f,
             theme->text_secondary);
}

static void dibujar_cards_listado_canchas(const CanchaListadoCard *cards,
                                          int count,
                                          int scroll_rows,
                                          const CanchaCardsLayout *layout,
                                          const Texture2D *fallback_tex,
                                          int fallback_loaded)
{
    const GuiTheme *theme = gui_get_active_theme();
    int panel_x;
    int panel_w;
    int area_y;
    int area_h;
    int x_start;

    if (!cards || !layout)
    {
        return;
    }

    panel_x = layout->panel_x;
    panel_w = layout->panel_w;
    area_y = layout->area_y;
    area_h = layout->area_h;
    x_start = panel_x + 12;

    BeginScissorMode(panel_x, area_y, panel_w, area_h);
    for (int i = 0; i < count; i++)
    {
        int row = i / layout->cols;
        int col = i % layout->cols;
        int row_on_screen = row - scroll_rows;
        int x = x_start + col * (layout->card_w + layout->gap_x);
        int y = area_y + row_on_screen * (layout->card_h + layout->gap_y);
        Rectangle card;
        Rectangle image_box;
        int hovered;

        if (row_on_screen < 0)
        {
            continue;
        }

        if (y >= area_y + area_h)
        {
            break;
        }

        card = (Rectangle){(float)x, (float)y, (float)layout->card_w, (float)layout->card_h};
        image_box = (Rectangle){card.x + 12.0f, card.y + 12.0f, card.width - 24.0f, 156.0f};
        hovered = CheckCollisionPointRec(GetMousePosition(), card);

        DrawRectangleRounded(card, 0.09f, 12, hovered ? (Color){28, 55, 39, 255} : theme->card_bg);
        DrawRectangleRoundedLines(card, 0.09f, 12,
                                  hovered ? theme->accent_primary : theme->card_border);

        DrawRectangleRec(image_box, (Color){12, 24, 17, 255});
        DrawRectangleLinesEx(image_box, 1.0f, (Color){56, 96, 69, 255});

        {
            const Texture2D *tex = NULL;
            if (cards[i].textura_cargada && cards[i].textura.id != 0)
            {
                tex = &cards[i].textura;
            }
            else if (fallback_loaded && fallback_tex && fallback_tex->id != 0)
            {
                tex = fallback_tex;
            }

            if (tex)
            {
                Rectangle src = {0.0f, 0.0f, (float)tex->width, (float)tex->height};
                Rectangle dst = calcular_dst_textura_en_caja_cancha(*tex, image_box);
                DrawTexturePro(*tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
            }
            else
            {
                gui_text("Sin imagen", image_box.x + 16.0f,
                         image_box.y + image_box.height * 0.5f - 10.0f,
                         18.0f, theme->text_secondary);
            }
        }

        gui_text(TextFormat("ID %d", cards[i].id), card.x + 14.0f, card.y + 176.0f,
                 16.0f, theme->text_secondary);
        gui_text_truncated(cards[i].nombre,
                           card.x + 14.0f,
                           card.y + 198.0f,
                           19.0f,
                           card.width - 28.0f,
                           theme->text_primary);
        gui_text(TextFormat("Goles: %d", cards[i].goles_totales),
                 card.x + 14.0f,
                 card.y + 230.0f,
                 17.0f,
                 theme->text_secondary);
        gui_text(TextFormat("Asistencias: %d", cards[i].asistencias_totales),
                 card.x + 14.0f,
                 card.y + 252.0f,
                 17.0f,
                 theme->text_secondary);
        gui_text(TextFormat("Partidos Jugados: %d", cards[i].partidos_jugados),
                 card.x + 14.0f,
                 card.y + 274.0f,
                 17.0f,
                 theme->text_secondary);
    }
    EndScissorMode();
}

static int listar_canchas_gui(void)
{
    CanchaListadoCard *cards = NULL;
    int count = 0;
    int scroll_rows = 0;
    int card_seleccionada = -1;
    Texture2D fallback_tex = (Texture2D){0};
    int fallback_loaded = 0;
    char fallback_path[512] = {0};

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    if (!cargar_canchas_listado_cards(&cards, &count))
    {
        return 0;
    }

    if (resolver_ruta_fallback_cancha(fallback_path, sizeof(fallback_path)) &&
        cargar_textura_listado_cancha(fallback_path, &fallback_tex))
    {
        fallback_loaded = 1;
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int accion = GUI_CARDS_FLOW_CONTINUE;
        int click_card = -1;
        CanchaCardsLayout layout;
        Rectangle area;

        calcular_layout_cards_canchas(sw, sh, count, &layout);
        area = (Rectangle){(float)layout.panel_x,
                           (float)layout.area_y,
                           (float)layout.panel_w,
                           (float)layout.area_h};
        scroll_rows = actualizar_scroll_canchas_gui(scroll_rows,
                                                    layout.total_rows,
                                                    layout.visible_rows,
                                                    area);

        if (card_seleccionada < 0)
        {
            GuiCardsHitTestConfig hit_cfg = {
                count,
                scroll_rows,
                layout.cols,
                layout.card_w,
                layout.card_h,
                layout.gap_x,
                layout.gap_y,
                layout.panel_x + 12,
                layout.area_y,
                layout.area_h};
            click_card = gui_cards_detect_click_index(&hit_cfg);
        }

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header("LISTADO DE CANCHAS", sw);

        DrawRectangle(layout.panel_x, layout.panel_y, layout.panel_w, layout.panel_h,
                      (Color){19, 40, 27, 255});
        DrawRectangleLines(layout.panel_x, layout.panel_y, layout.panel_w, layout.panel_h,
                           (Color){110, 161, 125, 255});

        if (count > 0)
        {
            dibujar_cards_listado_canchas(cards,
                                          count,
                                          scroll_rows,
                                          &layout,
                                          &fallback_tex,
                                          fallback_loaded);
        }
        else
        {
            gui_text("No hay canchas cargadas.",
                     (float)layout.panel_x + 24.0f,
                     (float)layout.panel_y + 28.0f,
                     24.0f,
                     (Color){233, 247, 236, 255});
        }

        if (card_seleccionada >= 0 && card_seleccionada < count)
        {
            dibujar_preview_card_cancha(&cards[card_seleccionada],
                                        &fallback_tex,
                                        fallback_loaded);
        }

        if (card_seleccionada >= 0)
        {
            gui_draw_footer_hint("Vista ampliada activa", (float)layout.panel_x, sh);
        }
        else
        {
            gui_draw_footer_hint("Click en una card: ampliar imagen | Rueda: scroll | ESC/Enter: volver",
                                (float)layout.panel_x,
                                sh);
        }
        EndDrawing();

        accion = gui_cards_handle_preview_selection(&card_seleccionada, click_card);
        if (accion == GUI_CARDS_FLOW_EXIT)
        {
            break;
        }

        continue;
    }

    if (fallback_loaded && fallback_tex.id != 0)
    {
        UnloadTexture(fallback_tex);
    }
    liberar_canchas_listado_cards(cards, count);
    return 1;
}

static int cargar_imagen_para_cancha_id(int id)
{
    return app_cargar_imagen_entidad(id, "cancha", "mifutbol_imagen_sel_cancha.txt");
}

static void cancha_gui_append_printable_ascii(char *buffer, int *cursor, size_t buffer_size)
{
    int key = GetCharPressed();
    while (key > 0)
    {
        if (key >= 32 && key <= 126 && *cursor < (int)buffer_size - 1)
        {
            buffer[*cursor] = (char)key;
            (*cursor)++;
            buffer[*cursor] = '\0';
        }
        key = GetCharPressed();
    }
}

static void cancha_gui_handle_backspace(char *buffer, int *cursor)
{
    if (!IsKeyPressed(KEY_BACKSPACE))
    {
        return;
    }

    size_t len = strlen_s(buffer, SIZE_MAX);
    if (len > 0)
    {
        buffer[len - 1] = '\0';
        *cursor = (int)len - 1;
    }
}

static void cancha_gui_draw_input_caret(const char *text, Rectangle input_rect)
{
    int blink_on = (((int)(GetTime() * 2.0)) % 2) == 0;
    if (!blink_on)
    {
        return;
    }

    Vector2 text_size = gui_text_measure(text ? text : "", 18.0f);
    float caret_x = input_rect.x + 8.0f + text_size.x + 1.0f;
    float caret_right_limit = input_rect.x + input_rect.width - 8.0f;
    caret_x = (caret_x > caret_right_limit) ? caret_right_limit : caret_x;

    DrawLineEx((Vector2){caret_x, input_rect.y + 7.0f},
               (Vector2){caret_x, input_rect.y + input_rect.height - 7.0f},
               2.0f,
               (Color){241, 252, 244, 255});
}

static void cancha_gui_draw_action_toast(int sw, int sh, const char *msg, int success)
{
    int toast_w = 520;
    int toast_h = 44;
    int toast_x = (sw - toast_w) / 2;
    int toast_y = sh - 120;
    Color bg = success ? (Color){28, 94, 52, 240} : (Color){120, 45, 38, 240};

    DrawRectangle(toast_x, toast_y, toast_w, toast_h, bg);
    DrawRectangleLines(toast_x, toast_y, toast_w, toast_h, (Color){198, 230, 205, 255});
    gui_text(msg ? msg : "Operacion completada", (float)toast_x + 14.0f, (float)toast_y + 12.0f,
             18.0f, (Color){241, 252, 244, 255});
}

static int cancha_gui_tick_toast(float *timer)
{
    if (!timer || *timer <= 0.0f)
    {
        return 0;
    }

    *timer -= GetFrameTime();
    return *timer <= 0.0f;
}

static int cancha_gui_draw_action_button(Rectangle rect, const char *label, int primary)
{
    const GuiTheme *theme = gui_get_active_theme();
    int hovered = CheckCollisionPointRec(GetMousePosition(), rect);
    Color fill = primary ? theme->accent_primary : theme->bg_sidebar;
    Color border = primary ? theme->accent_primary_hv : theme->border;
    Color text = primary ? (Color){255, 255, 255, 255} : theme->text_primary;

    if (hovered)
    {
        fill = primary ? theme->accent_primary_hv : theme->row_hover;
    }

    DrawRectangleRec(rect, fill);
    DrawRectangleLinesEx(rect, 1.0f, border);

    {
        Vector2 m = gui_text_measure(label ? label : "", 18.0f);
        float tx = rect.x + (rect.width - m.x) * 0.5f;
        float ty = rect.y + (rect.height - m.y) * 0.5f;
        gui_text(label ? label : "", tx, ty, 18.0f, text);
    }

    return hovered;
}

static void cancha_gui_button_rects(int panel_x, int panel_y, int panel_w, int panel_h,
                                    Rectangle *btn_primary, Rectangle *btn_secondary)
{
    float btn_w = 168.0f;
    float btn_h = 38.0f;
    float y = (float)(panel_y + panel_h) - btn_h - 16.0f;
    if (btn_primary)
    {
        *btn_primary = (Rectangle){(float)(panel_x + panel_w) - (btn_w * 2.0f) - 28.0f, y, btn_w, btn_h};
    }
    if (btn_secondary)
    {
        *btn_secondary = (Rectangle){(float)(panel_x + panel_w) - btn_w - 14.0f, y, btn_w, btn_h};
    }
}

static void cancha_gui_show_action_feedback(const char *title, const char *msg, int success)
{
    float timer = 1.15f;
    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header(title ? title : "CANCHAS", sw);
        cancha_gui_draw_action_toast(sw, sh, msg, success);
        gui_draw_footer_hint("Continuando...", 40.0f, sh);
        EndDrawing();

        if (cancha_gui_tick_toast(&timer))
        {
            return;
        }
    }
}

static int dibujar_y_detectar_click_canchas_gui(const CanchaRow *rows,
                                                 int count,
                                                 int scroll,
                                                 int row_h,
                                                 const CanchaGuiPanel *panel)
{
    if (count == 0)
    {
        gui_text("No hay canchas cargadas.", (float)(panel->x + 24), (float)(panel->y + 24), 24.0f, (Color){233, 247, 236, 255});
        return 0;
    }

    BeginScissorMode(panel->x, panel->y, panel->w, panel->h);
    for (int i = scroll; i < count; i++)
    {
        int row = i - scroll;
        int y = panel->y + row * row_h;
        if (y + row_h > panel->y + panel->h)
        {
            break;
        }

        Rectangle fila = {(float)(panel->x + 6), (float)y, (float)(panel->w - 12), (float)(row_h - 2)};
        gui_draw_list_row_bg(fila, row, CheckCollisionPointRec(GetMousePosition(), fila));
        if (CheckCollisionPointRec(GetMousePosition(), fila) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            EndScissorMode();
            return rows[i].id;
        }

        gui_text(TextFormat("%3d", rows[i].id), (float)(panel->x + 12), (float)(y + 7), 18.0f, (Color){220, 238, 225, 255});
        gui_text(rows[i].nombre, (float)(panel->x + 78), (float)(y + 7), 18.0f, (Color){233, 247, 236, 255});
    }
    EndScissorMode();

    return 0;
}

static int seleccionar_cancha_gui(const char *titulo, const char *ayuda, int *id_out)
{
    CanchaRow *rows = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 34;
    CanchaGuiPanel panel = {80, 130, 1100, 520, 1};

    if (!id_out)
    {
        return 0;
    }
    *id_out = 0;

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    if (!cargar_canchas_gui_rows(&rows, &count))
    {
        return 0;
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int clicked_id = 0;
        Rectangle area = {0};
        Rectangle content_rect = {0};
        CanchaGuiPanel content_panel = {0};

        calcular_panel_listado_canchas_gui(sw, sh, row_h, &panel);

        content_rect = (Rectangle){(float)panel.x, (float)(panel.y + 32), (float)panel.w, (float)(panel.h - 32)};
        if (content_rect.height < (float)row_h)
        {
            content_rect.height = (float)row_h;
        }
        content_panel.visible_rows = (int)(content_rect.height / (float)row_h);
        if (content_panel.visible_rows < 1)
        {
            content_panel.visible_rows = 1;
        }
        area = content_rect;
        scroll = actualizar_scroll_canchas_gui(scroll, count, content_panel.visible_rows, area);

        content_panel.x = (int)content_rect.x;
        content_panel.y = (int)content_rect.y;
        content_panel.w = (int)content_rect.width;
        content_panel.h = (int)content_rect.height;

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header(titulo, sw);

        gui_draw_list_shell((Rectangle){(float)panel.x, (float)panel.y, (float)panel.w, (float)panel.h},
                            "ID", 12.0f,
                            "NOMBRE", 78.0f);

        clicked_id = dibujar_y_detectar_click_canchas_gui(rows, count, scroll, row_h, &content_panel);
        gui_draw_footer_hint(ayuda, (float)panel.x, sh);
        EndDrawing();

        if (clicked_id > 0)
        {
            *id_out = clicked_id;
            free(rows);
            return 1;
        }

        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ESCAPE);
            input_consume_key(KEY_ENTER);
            break;
        }
    }

    free(rows);
    return 0;
}

static int guardar_nueva_cancha_gui(const char *nombre, int *id_out)
{
    sqlite3_stmt *stmt = NULL;
    long long id = obtener_siguiente_id("cancha");

    if (!preparar_stmt(&stmt, "INSERT INTO cancha(id, nombre) VALUES(?, ?)"))
    {
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, nombre, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Error al crear cancha nombre=%.180s", nombre);
        app_log_event("CANCHA", log_msg);
        return 0;
    }

    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Creada cancha id=%lld nombre=%.180s", id, nombre);
        app_log_event("CANCHA", log_msg);
    }

    if (id_out)
    {
        *id_out = (int)id;
    }

    return 1;
}

static int cancha_gui_confirmar_cargar_imagen(int id)
{
    input_consume_key(KEY_ENTER);
    input_consume_key(KEY_ESCAPE);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 760 : sw - 40;
        int panel_h = 220;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle btn_yes;
        Rectangle btn_no;
        const GuiTheme *theme = gui_get_active_theme();
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        cancha_gui_button_rects(panel_x, panel_y, panel_w, panel_h, &btn_yes, &btn_no);

        BeginDrawing();
        ClearBackground(theme->bg_main);
        gui_draw_module_header("NUEVA CANCHA", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, theme->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, theme->card_border);

        gui_text(TextFormat("Cancha ID %d creada correctamente", id),
                 (float)panel_x + 24.0f, (float)panel_y + 42.0f, 22.0f, theme->text_primary);
        gui_text("Desea agregarle una imagen ahora?", (float)panel_x + 24.0f,
                 (float)panel_y + 86.0f, 18.0f, theme->text_secondary);

        cancha_gui_draw_action_button(btn_yes, "Si, cargar", 1);
        cancha_gui_draw_action_button(btn_no, "No, omitir", 0);
        gui_draw_footer_hint("ENTER/Y: si | ESC/N: no", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_yes)) ||
            IsKeyPressed(KEY_Y) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_Y);
            input_consume_key(KEY_ENTER);
            return 1;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_no)) ||
            IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_N);
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static void cancha_gui_show_creacion_resultado(int id_creada, int intento_cargar_imagen, int imagen_cargada)
{
    const char *detalle = NULL;
    if (intento_cargar_imagen)
    {
        detalle = imagen_cargada
                     ? "Imagen cargada correctamente"
                     : "No se pudo cargar la imagen (puedes cargarla luego)";
    }
    else
    {
        detalle = "Puedes cambiar la imagen desde Modificar";
    }

    cancha_gui_show_action_feedback("NUEVA CANCHA",
                                    detalle,
                                    intento_cargar_imagen ? imagen_cargada : 1);

    {
        char log_msg[256];
        snprintf(log_msg,
                 sizeof(log_msg),
                 "Cancha id=%d creada (intento_imagen=%d, imagen_ok=%d)",
                 id_creada,
                 intento_cargar_imagen,
                 imagen_cargada);
        app_log_event("CANCHA", log_msg);
    }
}

static void cancha_gui_post_crear(int id_creada, int preguntar_imagen)
{
    int intento_cargar_imagen = 0;
    int imagen_cargada = 0;

    if (id_creada <= 0)
    {
        return;
    }

    if (preguntar_imagen && cancha_gui_confirmar_cargar_imagen(id_creada))
    {
        intento_cargar_imagen = 1;
        imagen_cargada = cargar_imagen_para_cancha_id(id_creada);
    }

    cancha_gui_show_creacion_resultado(id_creada, intento_cargar_imagen, imagen_cargada);
}

static int crear_cancha_gui(void)
{
    char nombre[128] = {0};
    int cursor = 0;
    float toast_timer = 0.0f;
    int toast_ok = 0;
    char toast_msg[96] = {0};
    int id_creada = 0;
    int preguntar_imagen = 0;

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = (sw > 900) ? 80 : 20;
        int panel_y = 140;
        int panel_w = sw - panel_x * 2;
        int panel_h = 220;

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header("CREAR CANCHA", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){19, 40, 27, 255});
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){110, 161, 125, 255});

        gui_text("Nombre:", (float)(panel_x + 24), (float)(panel_y + 36), 18.0f, (Color){233, 247, 236, 255});

        Rectangle input_rect = {(float)(panel_x + 24), (float)(panel_y + 70), (float)(panel_w - 48), 36.0f};
        DrawRectangleRec(input_rect, (Color){18, 36, 28, 255});
        DrawRectangleLines((int)input_rect.x,
                           (int)input_rect.y,
                           (int)input_rect.width,
                           (int)input_rect.height,
                           (Color){55, 100, 72, 255});
        gui_text(nombre, input_rect.x + 8.0f, input_rect.y + 8.0f, 18.0f, (Color){233, 247, 236, 255});

        if (toast_timer <= 0.0f)
        {
            cancha_gui_draw_input_caret(nombre, input_rect);
        }

        if (toast_timer > 0.0f)
        {
            cancha_gui_draw_action_toast(sw, sh, toast_msg, toast_ok);
        }

        gui_draw_footer_hint("ENTER: Guardar | ESC: Volver", (float)panel_x, sh);
        EndDrawing();

        if (cancha_gui_tick_toast(&toast_timer) && toast_ok)
        {
            cancha_gui_post_crear(id_creada, preguntar_imagen);
            return 1;
        }

        if (toast_timer > 0.0f)
        {
            continue;
        }

        cancha_gui_append_printable_ascii(nombre, &cursor, sizeof(nombre));
        cancha_gui_handle_backspace(nombre, &cursor);

        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 1;
        }

        if (IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ENTER);
            trim_whitespace(nombre);
            preguntar_imagen = 0;

            if (nombre[0] != '\0' && guardar_nueva_cancha_gui(nombre, &id_creada))
            {
                toast_ok = 1;
                snprintf(toast_msg, sizeof(toast_msg), "Cancha creada correctamente");
                toast_timer = 1.1f;
                preguntar_imagen = 1;
            }
            else if (nombre[0] != '\0')
            {
                toast_ok = 0;
                snprintf(toast_msg, sizeof(toast_msg), "No se pudo crear la cancha");
                toast_timer = 1.3f;
            }
        }
    }

    return 1;
}

enum
{
    CANCHA_EDIT_ACCION_CANCELAR = 0,
    CANCHA_EDIT_ACCION_NOMBRE = 1,
    CANCHA_EDIT_ACCION_IMAGEN = 2
};

static int modal_accion_editar_cancha_gui(int id)
{
    input_consume_key(KEY_ENTER);
    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_N);
    input_consume_key(KEY_I);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 760 : sw - 40;
        int panel_h = 220;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle btn_nombre;
        Rectangle btn_imagen;
        const GuiTheme *theme = gui_get_active_theme();
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        cancha_gui_button_rects(panel_x, panel_y, panel_w, panel_h, &btn_nombre, &btn_imagen);

        BeginDrawing();
        ClearBackground(theme->bg_main);
        gui_draw_module_header("MODIFICAR CANCHA", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, theme->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, theme->card_border);

        gui_text(TextFormat("ID seleccionado: %d", id),
                 (float)panel_x + 24.0f, (float)panel_y + 44.0f, 22.0f, theme->text_primary);
        gui_text("Elige que deseas modificar", (float)panel_x + 24.0f,
                 (float)panel_y + 90.0f, 18.0f, theme->text_secondary);

        cancha_gui_draw_action_button(btn_nombre, "Editar Nombre", 1);
        cancha_gui_draw_action_button(btn_imagen, "Cambiar Imagen", 0);
        gui_draw_footer_hint("N/ENTER: nombre | I: imagen | ESC: cancelar", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_nombre)) ||
            IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_N);
            input_consume_key(KEY_ENTER);
            return CANCHA_EDIT_ACCION_NOMBRE;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_imagen)) || IsKeyPressed(KEY_I))
        {
            input_consume_key(KEY_I);
            return CANCHA_EDIT_ACCION_IMAGEN;
        }

        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return CANCHA_EDIT_ACCION_CANCELAR;
        }
    }

    return CANCHA_EDIT_ACCION_CANCELAR;
}

static int guardar_nombre_editado_cancha_gui(int id, const char *nombre_nuevo)
{
    sqlite3_stmt *stmt = NULL;

    if (!preparar_stmt(&stmt, "UPDATE cancha SET nombre=? WHERE id=?"))
    {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, nombre_nuevo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Editada cancha id=%d nuevo_nombre=%.180s", id, nombre_nuevo);
        app_log_event("CANCHA", log_msg);
    }

    return 1;
}

static void cancha_gui_submit_editar_nombre(int id,
                                            char *nombre_nuevo,
                                            int *toast_ok,
                                            char *toast_msg,
                                            size_t toast_msg_size,
                                            float *toast_timer)
{
    if (!nombre_nuevo || !toast_ok || !toast_msg || toast_msg_size == 0 || !toast_timer)
    {
        return;
    }

    trim_whitespace(nombre_nuevo);
    if (nombre_nuevo[0] == '\0')
    {
        return;
    }

    if (guardar_nombre_editado_cancha_gui(id, nombre_nuevo))
    {
        *toast_ok = 1;
        snprintf(toast_msg, toast_msg_size, "Cancha modificada correctamente");
        *toast_timer = 1.1f;
        return;
    }

    *toast_ok = 0;
    snprintf(toast_msg, toast_msg_size, "No se pudo modificar la cancha");
    *toast_timer = 1.3f;
}

static int modal_editar_nombre_cancha_gui(int id)
{
    char nombre_nuevo[128] = {0};
    int cursor = 0;
    float toast_timer = 0.0f;
    int toast_ok = 0;
    char toast_msg[96] = {0};

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_x = (sw > 900) ? 80 : 20;
        int panel_y = 140;
        int panel_w = sw - panel_x * 2;
        int panel_h = 220;

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header(TextFormat("MODIFICAR CANCHA %d", id), sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){19, 40, 27, 255});
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){110, 161, 125, 255});

        gui_text("Nuevo nombre:", (float)(panel_x + 24), (float)(panel_y + 36), 18.0f, (Color){233, 247, 236, 255});

        Rectangle input_rect = {(float)(panel_x + 24), (float)(panel_y + 70), (float)(panel_w - 48), 36.0f};
        DrawRectangleRec(input_rect, (Color){18, 36, 28, 255});
        DrawRectangleLines((int)input_rect.x,
                           (int)input_rect.y,
                           (int)input_rect.width,
                           (int)input_rect.height,
                           (Color){55, 100, 72, 255});
        gui_text(nombre_nuevo, input_rect.x + 8.0f, input_rect.y + 8.0f, 18.0f, (Color){233, 247, 236, 255});

        if (toast_timer <= 0.0f)
        {
            cancha_gui_draw_input_caret(nombre_nuevo, input_rect);
        }

        if (toast_timer > 0.0f)
        {
            cancha_gui_draw_action_toast(sw, sh, toast_msg, toast_ok);
        }

        gui_draw_footer_hint("ENTER: Guardar | ESC: Volver", (float)panel_x, sh);
        EndDrawing();

        if (cancha_gui_tick_toast(&toast_timer) && toast_ok)
        {
            return 1;
        }

        if (toast_timer > 0.0f)
        {
            continue;
        }

        cancha_gui_append_printable_ascii(nombre_nuevo, &cursor, sizeof(nombre_nuevo));
        cancha_gui_handle_backspace(nombre_nuevo, &cursor);

        if (IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_ESCAPE);
            return 0;
        }

        if (IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_ENTER);
            cancha_gui_submit_editar_nombre(id,
                                            nombre_nuevo,
                                            &toast_ok,
                                            toast_msg,
                                            sizeof(toast_msg),
                                            &toast_timer);
        }
    }

    return 0;
}

static int modificar_cancha_gui(void)
{
    int id = 0;
    int accion;

    if (!seleccionar_cancha_gui("MODIFICAR CANCHA", "Click para seleccionar | ESC/Enter: volver", &id))
    {
        return 1;
    }

    if (id <= 0)
    {
        return 1;
    }

    accion = modal_accion_editar_cancha_gui(id);

    if (accion == CANCHA_EDIT_ACCION_NOMBRE)
    {
        return modal_editar_nombre_cancha_gui(id);
    }

    if (accion == CANCHA_EDIT_ACCION_IMAGEN)
    {
        if (cargar_imagen_para_cancha_id(id))
        {
            cancha_gui_show_action_feedback("MODIFICAR CANCHA", "Imagen actualizada correctamente", 1);
        }
        else
        {
            cancha_gui_show_action_feedback("MODIFICAR CANCHA", "No se pudo actualizar la imagen", 0);
        }
    }

    return 1;
}

static int eliminar_cancha_por_id_gui(int id)
{
    sqlite3_stmt *stmt = NULL;
    if (!preparar_stmt(&stmt, "DELETE FROM cancha WHERE id = ?"))
    {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Eliminada cancha id=%d", id);
        app_log_event("CANCHA", log_msg);
    }

    return 1;
}

static int modal_confirmar_eliminar_cancha_gui(int id, const char *nombre)
{
    input_consume_key(KEY_Y);
    input_consume_key(KEY_N);
    input_consume_key(KEY_ESCAPE);

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 700 : sw - 40;
        int panel_h = 210;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle btn_confirm;
        Rectangle btn_cancel;
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        const GuiTheme *theme = gui_get_active_theme();

        cancha_gui_button_rects(panel_x, panel_y, panel_w, panel_h, &btn_confirm, &btn_cancel);

        BeginDrawing();
        ClearBackground(theme->bg_main);
        gui_draw_module_header("CONFIRMAR ELIMINACION", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, theme->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, theme->card_border);

        gui_text("Confirmar eliminacion de cancha", (float)panel_x + 24.0f,
                 (float)panel_y + 44.0f, 24.0f, theme->text_primary);
        gui_text_truncated(TextFormat("%d(%s)", id, nombre ? nombre : "(sin nombre)"),
                           (float)panel_x + 24.0f,
                           (float)panel_y + 80.0f,
                           18.0f,
                           (float)panel_w - 48.0f,
                           theme->text_secondary);
        gui_text("Usa botones o teclas ENTER/ESC", (float)panel_x + 24.0f,
                 (float)panel_y + 112.0f, 18.0f, theme->text_secondary);

        cancha_gui_draw_action_button(btn_confirm, "Eliminar", 1);
        cancha_gui_draw_action_button(btn_cancel, "Cancelar", 0);

        gui_draw_footer_hint("Esta accion no se puede deshacer", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_confirm)) ||
            IsKeyPressed(KEY_Y) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_Y);
            input_consume_key(KEY_ENTER);
            return eliminar_cancha_por_id_gui(id);
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_cancel)) ||
            IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_N);
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static int dibujar_y_detectar_toggle_eliminar_canchas(const CanchaRow *rows,
                                                       const int *seleccionados,
                                                       int count,
                                                       int scroll,
                                                       int row_h,
                                                       const CanchaGuiPanel *panel)
{
    if (count == 0)
    {
        gui_text("No hay canchas.", (float)panel->x + 24.0f, (float)panel->y + 24.0f,
                 24.0f, gui_get_active_theme()->text_primary);
        return -1;
    }

    BeginScissorMode(panel->x, panel->y, panel->w, panel->h);
    for (int i = scroll; i < count; i++)
    {
        int row = i - scroll;
        int y = panel->y + row * row_h;
        if (y + row_h > panel->y + panel->h)
        {
            break;
        }

        Rectangle r = {(float)(panel->x + 6), (float)y, (float)(panel->w - 12), (float)(row_h - 2)};
        int hovered = CheckCollisionPointRec(GetMousePosition(), r);

        gui_draw_list_row_bg(r, row, hovered);
        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            EndScissorMode();
            return i;
        }

        gui_text((seleccionados && seleccionados[i]) ? "[x]" : "[ ]",
                 (float)(panel->x + 12), (float)(y + 7), 18.0f,
                 gui_get_active_theme()->text_secondary);
        gui_text(TextFormat("%3d", rows[i].id), (float)(panel->x + 64), (float)(y + 7), 18.0f,
                 gui_get_active_theme()->text_secondary);
        gui_text_truncated(rows[i].nombre, (float)(panel->x + 122), (float)(y + 7), 18.0f,
                           (float)panel->w - 140.0f,
                           gui_get_active_theme()->text_primary);
    }
    EndScissorMode();

    return -1;
}

static int contar_canchas_seleccionadas(const int *seleccionados, int count)
{
    int total = 0;

    if (!seleccionados || count <= 0)
    {
        return 0;
    }

    for (int i = 0; i < count; i++)
    {
        if (seleccionados[i])
        {
            total++;
        }
    }

    return total;
}

static int obtener_primer_index_cancha_seleccionada(const int *seleccionados, int count)
{
    if (!seleccionados || count <= 0)
    {
        return -1;
    }

    for (int i = 0; i < count; i++)
    {
        if (seleccionados[i])
        {
            return i;
        }
    }

    return -1;
}

static void set_seleccion_canchas(int *seleccionados, int count, int value)
{
    if (!seleccionados || count <= 0)
    {
        return;
    }

    for (int i = 0; i < count; i++)
    {
        seleccionados[i] = value;
    }
}

static void construir_resumen_canchas_seleccionadas(const CanchaRow *rows,
                                                    const int *seleccionados,
                                                    int count,
                                                    char *buffer,
                                                    size_t buffer_size)
{
    int escritos = 0;
    int mostrados = 0;

    if (!buffer || buffer_size == 0)
    {
        return;
    }

    buffer[0] = '\0';
    if (!rows || !seleccionados || count <= 0)
    {
        return;
    }

    for (int i = 0; i < count; i++)
    {
        if (!seleccionados[i])
        {
            continue;
        }

        if (mostrados >= 12)
        {
            (void)snprintf(buffer + escritos,
                           (escritos < (int)buffer_size) ? buffer_size - (size_t)escritos : 0,
                           "%s...",
                           (mostrados == 0) ? "" : ", ");
            return;
        }

        {
            int n = snprintf(buffer + escritos,
                             (escritos < (int)buffer_size) ? buffer_size - (size_t)escritos : 0,
                             "%s%d(%s)",
                             (mostrados == 0) ? "" : ", ",
                             rows[i].id,
                             rows[i].nombre[0] ? rows[i].nombre : "(sin nombre)");
            if (n < 0)
            {
                return;
            }
            escritos += n;
            mostrados++;
        }

        if (escritos >= (int)buffer_size - 1)
        {
            return;
        }
    }
}

static int modal_confirmar_eliminar_varias_canchas_gui(int cantidad, const char *resumen)
{
    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int panel_w = sw > 980 ? 740 : sw - 40;
        int panel_h = 210;
        int panel_x = (sw - panel_w) / 2;
        int panel_y = (sh - panel_h) / 2;
        Rectangle btn_confirm;
        Rectangle btn_cancel;
        const GuiTheme *theme = gui_get_active_theme();
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        cancha_gui_button_rects(panel_x, panel_y, panel_w, panel_h, &btn_confirm, &btn_cancel);

        BeginDrawing();
        ClearBackground(theme->bg_main);
        gui_draw_module_header("ELIMINAR CANCHAS", sw);

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, theme->card_bg);
        DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, theme->card_border);

        gui_text(TextFormat("Confirmar eliminacion de %d canchas", cantidad),
                 (float)panel_x + 24.0f, (float)panel_y + 44.0f, 22.0f, theme->text_primary);
        gui_text_truncated(resumen ? resumen : "(sin seleccion)",
                           (float)panel_x + 24.0f,
                           (float)panel_y + 82.0f,
                           18.0f,
                           (float)panel_w - 48.0f,
                           theme->text_secondary);
        gui_text("Usa botones o teclas ENTER/ESC", (float)panel_x + 24.0f,
                 (float)panel_y + 112.0f, 18.0f, theme->text_secondary);

        cancha_gui_draw_action_button(btn_confirm, "Eliminar", 1);
        cancha_gui_draw_action_button(btn_cancel, "Cancelar", 0);

        gui_draw_footer_hint("Esta accion no se puede deshacer", (float)panel_x, sh);
        EndDrawing();

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_confirm)) ||
            IsKeyPressed(KEY_Y) || IsKeyPressed(KEY_ENTER))
        {
            input_consume_key(KEY_Y);
            input_consume_key(KEY_ENTER);
            return 1;
        }

        if ((click && CheckCollisionPointRec(GetMousePosition(), btn_cancel)) ||
            IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE))
        {
            input_consume_key(KEY_N);
            input_consume_key(KEY_ESCAPE);
            return 0;
        }
    }

    return 0;
}

static int eliminar_canchas_seleccionadas_gui(const CanchaRow *rows,
                                              const int *seleccionados,
                                              int count,
                                              int *eliminadas_out)
{
    int eliminadas = 0;

    if (!rows || !seleccionados || count <= 0)
    {
        if (eliminadas_out)
        {
            *eliminadas_out = 0;
        }
        return 0;
    }

    for (int i = 0; i < count; i++)
    {
        if (seleccionados[i] && eliminar_cancha_por_id_gui(rows[i].id))
        {
            eliminadas++;
        }
    }

    if (eliminadas_out)
    {
        *eliminadas_out = eliminadas;
    }

    return eliminadas > 0;
}

static int recargar_estado_eliminar_canchas(CanchaRow **rows, int *count, int **seleccionados)
{
    if (!rows || !count || !seleccionados)
    {
        return 0;
    }

    free(*rows);
    *rows = NULL;
    free(*seleccionados);
    *seleccionados = NULL;
    *count = 0;

    if (!cargar_canchas_gui_rows(rows, count))
    {
        return 0;
    }

    if (*count <= 0)
    {
        return 1;
    }

    *seleccionados = (int *)calloc((size_t)(*count), sizeof(int));
    if (!*seleccionados)
    {
        free(*rows);
        *rows = NULL;
        *count = 0;
        return 0;
    }

    return 1;
}

enum
{
    CANCHA_ELIM_INTERACCION_CONTINUAR = 0,
    CANCHA_ELIM_INTERACCION_SALIR = 1,
    CANCHA_ELIM_INTERACCION_ERROR = 2
};

typedef struct CanchaEliminarContext
{
    CanchaRow **rows;
    int *count;
    int **seleccionados;
    int *scroll;
    int *toast_ok;
    char *toast_msg;
    size_t toast_msg_size;
    float *toast_timer;
} CanchaEliminarContext;

static int procesar_enter_eliminar_canchas(CanchaEliminarContext *ctx, int seleccionadas)
{
    int eliminado_count = 0;
    int confirmado = 0;
    char resumen[256] = {0};

    if (!ctx || !ctx->rows || !ctx->count || !ctx->seleccionados || !ctx->toast_ok ||
        !ctx->toast_msg || ctx->toast_msg_size == 0 || !ctx->toast_timer)
    {
        return 0;
    }

    if (seleccionadas <= 0)
    {
        *ctx->toast_ok = 0;
        snprintf(ctx->toast_msg, ctx->toast_msg_size, "Marca al menos una cancha");
        *ctx->toast_timer = 1.2f;
        return 1;
    }

    if (seleccionadas == 1)
    {
        int idx = obtener_primer_index_cancha_seleccionada(*ctx->seleccionados, *ctx->count);
        int id = (idx >= 0) ? (*ctx->rows)[idx].id : 0;
        const char *nombre = (idx >= 0) ? (*ctx->rows)[idx].nombre : NULL;
        confirmado = (id > 0) ? modal_confirmar_eliminar_cancha_gui(id, nombre) : 0;
        eliminado_count = confirmado ? 1 : 0;
    }
    else
    {
        construir_resumen_canchas_seleccionadas(*ctx->rows,
                                                *ctx->seleccionados,
                                                *ctx->count,
                                                resumen,
                                                sizeof(resumen));
        confirmado = modal_confirmar_eliminar_varias_canchas_gui(seleccionadas, resumen);
        if (confirmado)
        {
            (void)eliminar_canchas_seleccionadas_gui(*ctx->rows,
                                                     *ctx->seleccionados,
                                                     *ctx->count,
                                                     &eliminado_count);
        }
    }

    if (!confirmado)
    {
        *ctx->toast_ok = 0;
        snprintf(ctx->toast_msg, ctx->toast_msg_size, "Eliminacion cancelada");
        *ctx->toast_timer = 1.2f;
        return 1;
    }

    if (eliminado_count > 0)
    {
        *ctx->toast_ok = 1;
        snprintf(ctx->toast_msg, ctx->toast_msg_size, "Se eliminaron %d canchas", eliminado_count);
        *ctx->toast_timer = 1.2f;
        if (!recargar_estado_eliminar_canchas(ctx->rows, ctx->count, ctx->seleccionados))
        {
            return 0;
        }
        if (ctx->scroll)
        {
            *ctx->scroll = 0;
        }
        return 1;
    }

    *ctx->toast_ok = 0;
    snprintf(ctx->toast_msg, ctx->toast_msg_size, "No se pudieron eliminar canchas");
    *ctx->toast_timer = 1.2f;
    return 1;
}

static int manejar_interaccion_eliminar_canchas(CanchaEliminarContext *ctx,
                                                 int click,
                                                 Rectangle btn_toggle_all,
                                                 int all_selected,
                                                 int toggled_index,
                                                 int seleccionadas)
{
    if (!ctx || !ctx->count || !ctx->seleccionados)
    {
        return CANCHA_ELIM_INTERACCION_ERROR;
    }

    if ((click && CheckCollisionPointRec(GetMousePosition(), btn_toggle_all)) || IsKeyPressed(KEY_A))
    {
        input_consume_key(KEY_A);
        set_seleccion_canchas(*ctx->seleccionados, *ctx->count, all_selected ? 0 : 1);
        return CANCHA_ELIM_INTERACCION_CONTINUAR;
    }

    if (toggled_index >= 0 && toggled_index < *ctx->count && *ctx->seleccionados)
    {
        (*ctx->seleccionados)[toggled_index] = !(*ctx->seleccionados)[toggled_index];
        return CANCHA_ELIM_INTERACCION_CONTINUAR;
    }

    if (IsKeyPressed(KEY_ENTER))
    {
        input_consume_key(KEY_ENTER);
        if (!procesar_enter_eliminar_canchas(ctx, seleccionadas))
        {
            return CANCHA_ELIM_INTERACCION_ERROR;
        }
        return CANCHA_ELIM_INTERACCION_CONTINUAR;
    }

    if (IsKeyPressed(KEY_ESCAPE))
    {
        input_consume_key(KEY_ESCAPE);
        return CANCHA_ELIM_INTERACCION_SALIR;
    }

    return CANCHA_ELIM_INTERACCION_CONTINUAR;
}

static int eliminar_cancha_gui(void)
{
    CanchaRow *rows = NULL;
    int *seleccionados = NULL;
    int count = 0;
    int scroll = 0;
    const int row_h = 34;
    float toast_timer = 0.0f;
    int toast_ok = 0;
    char toast_msg[96] = {0};

    input_consume_key(KEY_ESCAPE);
    input_consume_key(KEY_ENTER);

    if (!recargar_estado_eliminar_canchas(&rows, &count, &seleccionados))
    {
        return 0;
    }

    while (!WindowShouldClose())
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        int click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        int content_h = 0;
        int visible_rows = 1;
        int toggled_index = -1;
        int seleccionadas = 0;
        int all_selected = 0;
        int accion = CANCHA_ELIM_INTERACCION_CONTINUAR;
        CanchaGuiPanel panel = {0};
        CanchaGuiPanel content = {0};
        Rectangle area = {0};
        Rectangle btn_toggle_all = {0};
        char footer_msg[160] = {0};
        CanchaEliminarContext ctx = {
            &rows,
            &count,
            &seleccionados,
            &scroll,
            &toast_ok,
            toast_msg,
            sizeof(toast_msg),
            &toast_timer};

        calcular_panel_listado_canchas_gui(sw, sh, row_h, &panel);

        content_h = panel.h - 32;
        if (content_h < row_h)
        {
            content_h = row_h;
        }
        visible_rows = content_h / row_h;
        if (visible_rows < 1)
        {
            visible_rows = 1;
        }

        area = (Rectangle){(float)panel.x, (float)(panel.y + 32), (float)panel.w, (float)content_h};
        scroll = actualizar_scroll_canchas_gui(scroll, count, visible_rows, area);

        content.x = (int)area.x;
        content.y = (int)area.y;
        content.w = (int)area.width;
        content.h = (int)area.height;
        content.visible_rows = visible_rows;

        seleccionadas = contar_canchas_seleccionadas(seleccionados, count);
        all_selected = (count > 0 && seleccionadas == count);

        BeginDrawing();
        ClearBackground((Color){14, 27, 20, 255});
        gui_draw_module_header("ELIMINAR CANCHA", sw);
        gui_text("Marca una o varias canchas para eliminar", (float)panel.x, 92.0f, 18.0f,
                 gui_get_active_theme()->text_secondary);
        btn_toggle_all = (Rectangle){(float)panel.x + (float)panel.w - 220.0f, 88.0f, 200.0f, 30.0f};
        cancha_gui_draw_action_button(btn_toggle_all, all_selected ? "Limpiar Todo" : "Seleccionar Todo", 0);
        gui_draw_list_shell((Rectangle){(float)panel.x, (float)panel.y, (float)panel.w, (float)panel.h},
                            "SEL", 12.0f,
                            "ID / NOMBRE", 64.0f);

        toggled_index = dibujar_y_detectar_toggle_eliminar_canchas(rows,
                                                                    seleccionados,
                                                                    count,
                                                                    scroll,
                                                                    row_h,
                                                                    &content);

        if (toast_timer > 0.0f)
        {
            cancha_gui_draw_action_toast(sw, sh, toast_msg, toast_ok);
        }

        snprintf(footer_msg,
                 sizeof(footer_msg),
                 "Click: marcar | A: todo/limpiar | ENTER: eliminar (%d) | Rueda: scroll | ESC: volver",
                 seleccionadas);
        gui_draw_footer_hint(footer_msg, (float)panel.x, sh);
        EndDrawing();

        if (cancha_gui_tick_toast(&toast_timer))
        {
            toast_timer = 0.0f;
        }
        if (toast_timer > 0.0f)
        {
            continue;
        }

        accion = manejar_interaccion_eliminar_canchas(&ctx,
                                                      click,
                                                      btn_toggle_all,
                                                      all_selected,
                                                      toggled_index,
                                                      seleccionadas);
        if (accion == CANCHA_ELIM_INTERACCION_ERROR)
        {
            free(rows);
            free(seleccionados);
            return 0;
        }

        if (accion == CANCHA_ELIM_INTERACCION_SALIR)
        {
            break;
        }

        continue;
    }

    free(rows);
    free(seleccionados);
    return 1;
}

/**
 * @brief Crea una nueva cancha en la base de datos
 *
 * Permite a los usuarios agregar canchas para asignacion en partidos,
 * reutilizando IDs eliminados para mantener la secuencia.
 */
void crear_cancha()
{
    (void)crear_cancha_gui();
}

/**
 * @brief Muestra un listado de todas las canchas registradas
 *
 * Proporciona visibilidad de canchas disponibles para seleccion
 * en partidos y operaciones de gestion.
 */
void listar_canchas()
{
    app_log_event("CANCHA", "Listado de canchas consultado");

    (void)listar_canchas_gui();
}

/**
 * @brief Elimina una cancha de la base de datos
 *
 * Permite remover canchas obsoletas mientras mantiene integridad
 * de datos con validaciones y confirmaciones de usuario.
 */
void eliminar_cancha()
{
    (void)eliminar_cancha_gui();
}

/**
 * @brief Permite modificar el nombre de una cancha existente
 *
 * Permite correcciones en nombres de canchas sin necesidad
 * de eliminar y recrear registros, mejorando usabilidad.
 */
void modificar_cancha()
{
    (void)modificar_cancha_gui();
}

/**
 * @brief Muestra el menu principal de gestion de canchas
 *
 * Proporciona interfaz centralizada para operaciones CRUD de canchas,
 * facilitando la navegacion y delegacion de tareas especificas.
 */
void menu_canchas()
{
    MenuItem items[] =
    {
        {1, "Crear", crear_cancha},
        {2, "Listar", listar_canchas},
        {3, "Modificar", modificar_cancha},
        {4, "Eliminar", eliminar_cancha},
        {0, "Volver", NULL}
    };

    ejecutar_menu_estandar("CANCHAS", items, 5);
}
