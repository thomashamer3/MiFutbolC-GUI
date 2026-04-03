/**
 * @file gui_components.c
 * @brief Implementación de componentes reutilizables, temas, layout, filtros y animaciones
 */
#include "gui_components.h"
#include "gui_config.h"
#include "utils.h"

#include "cJSON.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct GuiIconEntry {
    int option;
    const char *file_name;
    Texture2D tex;
    int loaded;
} GuiIconEntry;

static GuiIconEntry s_menu_icons[] = {
    {0,  "Salir-0.png",        {0}, 0},
    {1,  "Camisetas-1.png",    {0}, 0},
    {2,  "Canchas-2.png",      {0}, 0},
    {3,  "Equipos-3.png",      {0}, 0},
    {4,  "Partidos-4.png",     {0}, 0},
    {5,  "Lesiones-5.png",     {0}, 0},
    {6,  "Estadisticas-6.png", {0}, 0},
    {7,  "Logros-7.png",       {0}, 0},
    {8,  "Financiamiento-8.png", {0}, 0},
    {9,  "Torneos-9.png",      {0}, 0},
    {10, "Temporada-10.png",   {0}, 0},
    {11, "Analisis-11.png",    {0}, 0},
    {12, "Bienestar-12.png",   {0}, 0},
    {13, "CarreraFutbol-13.png", {0}, 0},
    {14, "Ajustes-14.png",     {0}, 0},
    {15,  "MiFutbolC.png",     {0}, 0},
};

static const int s_menu_icons_count =
    (int)(sizeof(s_menu_icons) / sizeof(s_menu_icons[0]));

static Texture2D *gui_icon_get_by_option(int option)
{
    for (int i = 0; i < s_menu_icons_count; i++) {
        if (s_menu_icons[i].option == option && s_menu_icons[i].loaded)
            return &s_menu_icons[i].tex;
    }
    return NULL;
}

/* Resuelve la ruta base de Icons/ probando rutas relativas al CWD y al exe */
static const char *s_icons_base = NULL;

static const char *gui_resolve_icons_base(void)
{
    if (s_icons_base) return s_icons_base;
    /* Probar rutas comunes */
    static const char *candidates[] = {
        "./Icons/",
        "../../Icons/",   /* bin/Debug/ -> raiz */
        "../Icons/",      /* bin/ -> raiz */
        "Icons/"
    };
    int n = (int)(sizeof(candidates) / sizeof(candidates[0]));
    for (int i = 0; i < n; i++) {
        char test[256];
          int suffix_len = (int)(sizeof("Salir-0.png") - 1);
        /* Limitar la porción de prefijo para dejar sitio al sufijo y evitar
           advertencias de truncation del compilador */
        snprintf(test, sizeof(test), "%.*sSalir-0.png",
                      (int)(sizeof(test) - suffix_len - 1),
                 candidates[i]);
        if (FileExists(test)) {
            s_icons_base = candidates[i];
            return s_icons_base;
        }
    }
    /* Ultimo intento: ruta absoluta basada en directorio del exe */
    {
        static char abs_path[512];
        const char *app_dir = GetApplicationDirectory();
        if (app_dir && app_dir[0]) {
            snprintf(abs_path, sizeof(abs_path), "%s../../Icons/", app_dir);
            int suffix_len = (int)(sizeof("Salir-0.png") - 1);
            char test[512];            snprintf(test, sizeof(test), "%.*sSalir-0.png",
                     (int)(sizeof(test) - suffix_len - 1),
                     abs_path);
            if (FileExists(test)) {
                s_icons_base = abs_path;
                return s_icons_base;
            }
        }
    }
    s_icons_base = "./Icons/";  /* fallback */
    return s_icons_base;
}

static void gui_icons_load_all(void)
{
    const char *base = gui_resolve_icons_base();
    int loaded_count = 0;
    for (int i = 0; i < s_menu_icons_count; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s%s", base, s_menu_icons[i].file_name);
        if (!FileExists(path))
            continue;
        Texture2D t = LoadTexture(path);
        if (t.id == 0)
            continue;
        SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
        s_menu_icons[i].tex = t;
        s_menu_icons[i].loaded = 1;
        loaded_count++;
    }
    TraceLog(LOG_INFO, "ICONS: Cargados %d/%d iconos desde [%s]",
             loaded_count, s_menu_icons_count, base);
}

static void gui_icons_unload_all(void)
{
    for (int i = 0; i < s_menu_icons_count; i++) {
        if (!s_menu_icons[i].loaded)
            continue;
        if (s_menu_icons[i].tex.id != 0)
            UnloadTexture(s_menu_icons[i].tex);
        s_menu_icons[i].tex = (Texture2D){0};
        s_menu_icons[i].loaded = 0;
    }
}

void gui_draw_option_icon(int option, Rectangle dst)
{
    Texture2D *tex = gui_icon_get_by_option(option);
    if (!tex)
        return;

    Rectangle src = {0.0f, 0.0f, (float)tex->width, (float)tex->height};
    DrawTexturePro(*tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
}

/* ═══════════════════════════════════════════════════════════
   Datos de tema
   ═══════════════════════════════════════════════════════════ */

static const GuiTheme s_theme_dark = {
    {16, 24, 20, 255},        /* bg_main        — lifted      */
    {14, 42, 26, 255},        /* bg_header      — dark accent */
    {18, 36, 26, 255},        /* bg_sidebar     — mid-dark    */
    {12, 20, 16, 255},        /* bg_list        — darker than main */
    {235, 248, 240, 255},     /* text_primary   */
    {180, 215, 192, 255},     /* text_secondary */
    {120, 158, 138, 255},     /* text_muted     */
    {255, 230, 120, 255},     /* text_highlight — gold        */
    {55, 100, 72, 255},       /* border                       */
    {30, 78, 48, 255},        /* accent         — buttons     */
    {42, 105, 64, 255},       /* accent_hover                 */
    {40, 200, 85, 255},       /* accent_primary — CTA vivid   */
    {55, 235, 110, 255},      /* accent_primary_hv            */
    {45, 130, 72, 255},       /* row_selected   — bright      */
    {30, 72, 48, 255},        /* row_hover      — visible     */
    {18, 40, 28, 255},        /* card_bg                      */
    {60, 120, 80, 255},       /* card_border                  */
    {22, 70, 42, 220},        /* toast_bg                     */
    {140, 220, 170, 220},     /* toast_border                 */
    {40, 200, 100, 100},      /* search_glow                  */
    {80, 160, 110, 120}       /* list_focus     — subtle      */
};

static const GuiTheme s_theme_fm = {
    {12, 18, 32, 255},        /* bg_main        — deep blue    */
    {16, 28, 52, 255},        /* bg_header      — navy         */
    {14, 22, 42, 255},        /* bg_sidebar     — dark navy    */
    {10, 16, 28, 255},        /* bg_list        — darkest blue */
    {220, 235, 250, 255},     /* text_primary   */
    {160, 190, 220, 255},     /* text_secondary */
    {100, 130, 165, 255},     /* text_muted     */
    {255, 200, 60, 255},      /* text_highlight — gold         */
    {40, 65, 100, 255},       /* border                        */
    {25, 55, 95, 255},        /* accent                        */
    {35, 75, 125, 255},       /* accent_hover                  */
    {30, 140, 220, 255},      /* accent_primary — CTA blue     */
    {50, 170, 250, 255},      /* accent_primary_hv             */
    {35, 80, 140, 255},       /* row_selected                  */
    {25, 50, 85, 255},        /* row_hover                     */
    {16, 26, 46, 255},        /* card_bg                       */
    {50, 85, 130, 255},       /* card_border                   */
    {20, 50, 90, 220},        /* toast_bg                      */
    {100, 160, 220, 220},     /* toast_border                  */
    {30, 140, 220, 100},      /* search_glow                   */
    {50, 100, 160, 120}       /* list_focus                    */
};

static const GuiTheme s_theme_light = {
    {238, 246, 240, 255},     /* bg_main        — lightest    */
    {210, 232, 218, 255},     /* bg_header                    */
    {224, 240, 228, 255},     /* bg_sidebar                   */
    {248, 252, 249, 255},     /* bg_list        — white tint  */
    {18, 48, 30, 255},        /* text_primary                 */
    {40, 80, 56, 255},        /* text_secondary               */
    {80, 115, 92, 255},       /* text_muted                   */
    {160, 105, 0, 255},       /* text_highlight — amber       */
    {160, 192, 172, 255},     /* border                       */
    {110, 185, 140, 255},     /* accent                       */
    {90, 165, 120, 255},      /* accent_hover                 */
    {30, 155, 65, 255},       /* accent_primary — CTA vivid   */
    {22, 135, 52, 255},       /* accent_primary_hv            */
    {155, 212, 178, 255},     /* row_selected   — bright      */
    {200, 225, 210, 255},     /* row_hover      — visible     */
    {222, 238, 228, 255},     /* card_bg                      */
    {150, 195, 165, 255},     /* card_border                  */
    {130, 190, 150, 220},     /* toast_bg                     */
    {95, 155, 115, 220},      /* toast_border                 */
    {35, 148, 70, 80},        /* search_glow                  */
    {110, 175, 140, 100}      /* list_focus     — subtle      */
};

/* Nuevos palettes para los temas agregados en la lista de nombres */
static const GuiTheme s_theme_solarized = {
    {253, 246, 227, 255}, /* bg_main */
    {238, 232, 213, 255}, /* bg_header */
    {245, 241, 229, 255}, /* bg_sidebar */
    {250, 247, 235, 255}, /* bg_list */
    {88, 110, 117, 255},  /* text_primary */
    {101, 123, 131, 255}, /* text_secondary */
    {133, 153, 0, 255},   /* text_muted */
    {38, 139, 210, 255},  /* text_highlight */
    {147, 161, 161, 255}, /* border */
    {133, 153, 0, 255},   /* accent */
    {150, 170, 20, 255},  /* accent_hover */
    {42, 161, 152, 255},  /* accent_primary */
    {60, 200, 180, 255},  /* accent_primary_hv */
    {220, 230, 200, 255}, /* row_selected */
    {240, 245, 235, 255}, /* row_hover */
    {250, 247, 235, 255}, /* card_bg */
    {200, 210, 190, 255}, /* card_border */
    {245, 241, 229, 220}, /* toast_bg */
    {200, 220, 210, 220}, /* toast_border */
    {38, 139, 210, 60},   /* search_glow */
    {200, 220, 210, 100}  /* list_focus */
};

static const GuiTheme s_theme_monokai = {
    {39, 40, 34, 255},    /* bg_main */
    {49, 50, 44, 255},    /* bg_header */
    {45, 46, 40, 255},    /* bg_sidebar */
    {32, 34, 30, 255},    /* bg_list */
    {248, 248, 242, 255}, /* text_primary */
    {200, 200, 190, 255}, /* text_secondary */
    {120, 120, 110, 255}, /* text_muted */
    {249, 38, 114, 255},  /* text_highlight */
    {80, 80, 72, 255},    /* border */
    {166, 226, 46, 255},  /* accent */
    {180, 230, 80, 255},  /* accent_hover */
    {102, 217, 239, 255}, /* accent_primary */
    {140, 240, 255, 255}, /* accent_primary_hv */
    {72, 209, 135, 255},  /* row_selected */
    {63, 60, 55, 255},    /* row_hover */
    {40, 40, 36, 255},    /* card_bg */
    {60, 60, 56, 255},    /* card_border */
    {50, 50, 46, 220},    /* toast_bg */
    {90, 90, 86, 220},    /* toast_border */
    {102, 217, 239, 60},  /* search_glow */
    {70, 70, 66, 100}     /* list_focus */
};

static const GuiTheme s_theme_contrast = {
    {0, 0, 0, 255},       /* bg_main */
    {20, 20, 20, 255},    /* bg_header */
    {10, 10, 10, 255},    /* bg_sidebar */
    {5, 5, 5, 255},       /* bg_list */
    {255, 255, 255, 255}, /* text_primary */
    {200, 200, 200, 255}, /* text_secondary */
    {160, 160, 160, 255}, /* text_muted */
    {255, 255, 0, 255},   /* text_highlight */
    {255, 255, 255, 255}, /* border */
    {255, 0, 0, 255},     /* accent */
    {255, 80, 80, 255},   /* accent_hover */
    {0, 200, 255, 255},   /* accent_primary */
    {80, 240, 255, 255},  /* accent_primary_hv */
    {255, 128, 0, 255},   /* row_selected */
    {60, 60, 60, 255},    /* row_hover */
    {20, 20, 20, 255},    /* card_bg */
    {80, 80, 80, 255},    /* card_border */
    {40, 40, 40, 220},    /* toast_bg */
    {120, 120, 120, 220}, /* toast_border */
    {255, 255, 0, 80},    /* search_glow */
    {200, 200, 200, 120}  /* list_focus */
};

static const GuiTheme s_theme_grayscale = {
    {240, 240, 240, 255}, /* bg_main */
    {220, 220, 220, 255}, /* bg_header */
    {230, 230, 230, 255}, /* bg_sidebar */
    {250, 250, 250, 255}, /* bg_list */
    {30, 30, 30, 255},    /* text_primary */
    {80, 80, 80, 255},    /* text_secondary */
    {120, 120, 120, 255}, /* text_muted */
    {60, 60, 60, 255},    /* text_highlight */
    {200, 200, 200, 255}, /* border */
    {120, 120, 120, 255}, /* accent */
    {140, 140, 140, 255}, /* accent_hover */
    {80, 80, 80, 255},    /* accent_primary */
    {100, 100, 100, 255}, /* accent_primary_hv */
    {200, 200, 200, 255}, /* row_selected */
    {230, 230, 230, 255}, /* row_hover */
    {245, 245, 245, 255}, /* card_bg */
    {210, 210, 210, 255}, /* card_border */
    {240, 240, 240, 220}, /* toast_bg */
    {200, 200, 200, 220}, /* toast_border */
    {120, 120, 120, 60},  /* search_glow */
    {180, 180, 180, 100}  /* list_focus */
};

static const GuiTheme s_theme_pastel = {
    {249, 243, 255, 255}, /* bg_main */
    {245, 238, 252, 255}, /* bg_header */
    {247, 241, 253, 255}, /* bg_sidebar */
    {255, 250, 255, 255}, /* bg_list */
    {60, 50, 70, 255},    /* text_primary */
    {100, 90, 110, 255},  /* text_secondary */
    {140, 130, 150, 255}, /* text_muted */
    {255, 105, 180, 255}, /* text_highlight */
    {220, 210, 230, 255}, /* border */
    {255, 179, 186, 255}, /* accent */
    {255, 200, 210, 255}, /* accent_hover */
    {179, 255, 205, 255}, /* accent_primary */
    {200, 255, 225, 255}, /* accent_primary_hv */
    {230, 240, 235, 255}, /* row_selected */
    {245, 245, 245, 255}, /* row_hover */
    {255, 250, 250, 255}, /* card_bg */
    {220, 215, 225, 255}, /* card_border */
    {250, 240, 250, 220}, /* toast_bg */
    {210, 200, 210, 220}, /* toast_border */
    {255, 105, 180, 50},  /* search_glow */
    {230, 220, 230, 100}  /* list_focus */
};

static const GuiTheme s_theme_midnight = {
    {10, 12, 25, 255},    /* bg_main */
    {8, 10, 20, 255},     /* bg_header */
    {12, 14, 28, 255},    /* bg_sidebar */
    {6, 8, 18, 255},      /* bg_list */
    {200, 220, 255, 255}, /* text_primary */
    {160, 180, 210, 255}, /* text_secondary */
    {120, 140, 160, 255}, /* text_muted */
    {180, 200, 255, 255}, /* text_highlight */
    {30, 40, 60, 255},    /* border */
    {80, 120, 200, 255},  /* accent */
    {100, 150, 230, 255}, /* accent_hover */
    {60, 130, 220, 255},  /* accent_primary */
    {100, 170, 255, 255}, /* accent_primary_hv */
    {50, 90, 140, 255},   /* row_selected */
    {30, 60, 100, 255},   /* row_hover */
    {12, 14, 26, 255},    /* card_bg */
    {40, 60, 90, 255},    /* card_border */
    {10, 14, 28, 220},    /* toast_bg */
    {50, 80, 120, 220},   /* toast_border */
    {60, 130, 220, 60},   /* search_glow */
    {40, 70, 110, 100}    /* list_focus */
};

static const GuiTheme s_theme_sepia = {
    {244, 236, 220, 255}, /* bg_main */
    {232, 218, 196, 255}, /* bg_header */
    {238, 228, 210, 255}, /* bg_sidebar */
    {250, 242, 230, 255}, /* bg_list */
    {60, 40, 30, 255},    /* text_primary */
    {90, 65, 50, 255},    /* text_secondary */
    {120, 90, 70, 255},   /* text_muted */
    {150, 110, 70, 255},  /* text_highlight */
    {200, 180, 150, 255}, /* border */
    {160, 120, 80, 255},  /* accent */
    {190, 140, 90, 255},  /* accent_hover */
    {120, 90, 60, 255},   /* accent_primary */
    {150, 110, 70, 255},  /* accent_primary_hv */
    {210, 190, 160, 255}, /* row_selected */
    {230, 215, 190, 255}, /* row_hover */
    {245, 235, 220, 255}, /* card_bg */
    {200, 185, 160, 255}, /* card_border */
    {240, 230, 210, 220}, /* toast_bg */
    {210, 195, 170, 220}, /* toast_border */
    {150, 110, 70, 60},   /* search_glow */
    {200, 180, 150, 100}  /* list_focus */
};

static const GuiTheme s_theme_neon = {
    {8, 6, 20, 255},      /* bg_main */
    {10, 8, 28, 255},     /* bg_header */
    {6, 4, 16, 255},      /* bg_sidebar */
    {12, 10, 30, 255},    /* bg_list */
    {200, 255, 230, 255}, /* text_primary */
    {160, 220, 200, 255}, /* text_secondary */
    {100, 150, 130, 255}, /* text_muted */
    {255, 0, 170, 255},   /* text_highlight */
    {40, 30, 60, 255},    /* border */
    {0, 255, 170, 255},   /* accent */
    {80, 255, 200, 255},  /* accent_hover */
    {255, 0, 110, 255},   /* accent_primary */
    {255, 120, 200, 255}, /* accent_primary_hv */
    {180, 0, 200, 255},   /* row_selected */
    {60, 10, 60, 255},    /* row_hover */
    {10, 8, 22, 255},     /* card_bg */
    {60, 30, 80, 255},    /* card_border */
    {20, 6, 40, 220},     /* toast_bg */
    {120, 30, 160, 220},  /* toast_border */
    {255, 0, 170, 80},    /* search_glow */
    {100, 40, 120, 100}   /* list_focus */
};

static const GuiTheme s_theme_vintage = {
    {245, 238, 224, 255}, /* bg_main */
    {235, 225, 210, 255}, /* bg_header */
    {240, 232, 218, 255}, /* bg_sidebar */
    {250, 244, 232, 255}, /* bg_list */
    {70, 45, 30, 255},    /* text_primary */
    {100, 80, 60, 255},   /* text_secondary */
    {140, 120, 95, 255},  /* text_muted */
    {150, 80, 40, 255},   /* text_highlight */
    {200, 185, 160, 255}, /* border */
    {180, 140, 110, 255}, /* accent */
    {200, 160, 130, 255}, /* accent_hover */
    {120, 90, 70, 255},   /* accent_primary */
    {150, 120, 95, 255},  /* accent_primary_hv */
    {220, 200, 180, 255}, /* row_selected */
    {235, 220, 200, 255}, /* row_hover */
    {248, 240, 228, 255}, /* card_bg */
    {200, 185, 165, 255}, /* card_border */
    {240, 230, 215, 220}, /* toast_bg */
    {200, 190, 170, 220}, /* toast_border */
    {150, 120, 90, 60},   /* search_glow */
    {200, 180, 150, 100}  /* list_focus */
};

static const GuiModuleInfo s_module_info[GUI_FILTER_COUNT] = {
    {"Todos",       "Vista global de todas las opciones"},
    {"Gestion",     "Recursos y operaciones diarias"},
    {"Competencia", "Torneos, temporadas y carrera"},
    {"Analisis",    "Estadisticas, logros y bienestar"},
    {"Admin",       "Finanzas, ajustes y salida"}
};

/* Icon mapping por modulo (paralelo a s_module_info) */
static const int s_module_icons_map[GUI_FILTER_COUNT] = {
    15, /* Todos (use app icon) */
    3,  /* Gestion -> Equipos-3.png */
    9,  /* Competencia -> Torneos-9.png */
    11, /* Analisis -> Analisis-11.png */
    14  /* Admin -> Ajustes-14.png */
};

int gui_get_module_icon(int index)
{
    if (index < 0 || index >= GUI_FILTER_COUNT) return -1;
    return s_module_icons_map[index];
}

static const GuiModuleHelp s_module_help[GUI_FILTER_COUNT] = {
    {"Revisa el estado general antes de ejecutar.",
     "Usa busqueda para ubicar acciones rapido.",
    "Atajos: F2 inicio, F3 modulos, F4 tema."},
    {"Gestiona equipos, canchas, partidos y lesiones.",
     "Verifica datos antes de guardar cambios.",
     "Prioriza orden: equipo -> cancha -> partido."},
    {"Configura torneo, temporada y carrera.",
     "Simula desde datos consistentes.",
     "Analiza rendimiento al cerrar cada fecha."},
    {"Consulta estadisticas por mes y anio.",
     "Cruza logros y bienestar para decisiones.",
     "Detecta tendencias antes de exportar."},
    {"Controla financiamiento y ajustes generales.",
     "Exporta reportes periodicos de respaldo.",
     "Usa salir solo al finalizar operaciones."}
};

/* Storage para tema activo con variante aplicada. Un único slot global
    es suficiente porque la app sólo mantiene una GUI a la vez. */
static GuiTheme s_active_theme_storage;

const GuiTheme *gui_get_theme_dark(void)  { return &s_theme_dark;  }
const GuiTheme *gui_get_theme_light(void) { return &s_theme_light; }
const GuiTheme *gui_get_theme_fm(void)    { return &s_theme_fm;    }

const GuiTheme *gui_get_active_theme(void)
{
    if (s_active_theme_storage.bg_main.a != 0 ||
        s_active_theme_storage.text_primary.a != 0)
    {
        return &s_active_theme_storage;
    }
    return &s_theme_dark;
}

/* Lista de nombres visibles para el usuario. Podemos exponer muchos nombres
   aunque internamente mapemos los nombres a los temas implementados.
   Esto permite añadir muchas opciones al UI sin requerir inmediatamente
   nuevos palettes si no están definidos. */
static const char *s_theme_names[] = {
    "Tema Verde Oscuro",
    "Tema Claro",
    "Tema Azul",
    "Tema Solarizado",
    "Tema Monokai",
    "Tema Alto Contraste",
    "Tema Escala Grises",
    "Tema Pastel",
    "Tema Medianoche",
    "Tema Sepia",
    "Tema Neon",
    "Tema Vintage"
};

static const int s_theme_names_count =
    (int)(sizeof(s_theme_names) / sizeof(s_theme_names[0]));

/* Punteros reales a palettes implementados. El orden coincide con
   `s_theme_names` para mapeo directo (wrap-around si hay menos palettes
    implementados que nombres). */
static const GuiTheme *s_theme_ptrs[] = {
    &s_theme_dark,
    &s_theme_light,
    &s_theme_fm,
    &s_theme_solarized,
    &s_theme_monokai,
    &s_theme_contrast,
    &s_theme_grayscale,
    &s_theme_pastel,
    &s_theme_midnight,
    &s_theme_sepia,
    &s_theme_neon,
    &s_theme_vintage
};
static const int s_theme_ptrs_count =
    (int)(sizeof(s_theme_ptrs) / sizeof(s_theme_ptrs[0]));

int gui_get_theme_count(void) { return s_theme_names_count; }
const char *gui_get_theme_name(int index)
{
    if (index < 0) index = 0;
    return s_theme_names[index % s_theme_names_count];
}

const GuiTheme *gui_get_theme_by_index(int index)
{
    if (index < 0) index = 0;
    /* Mapear de forma segura a los palettes disponibles. */
    return s_theme_ptrs[index % s_theme_ptrs_count];
}

/* Variantes automáticas (derivadas) */
static const char *s_variant_names[] = {
    "Default",
    "Brillante",
    "Oscuro",
    "Saturado",
    "Desaturado"
};

static const int s_variant_count =
    (int)(sizeof(s_variant_names) / sizeof(s_variant_names[0]));

int gui_get_variant_count(void) { return s_variant_count; }
const char *gui_get_variant_name(int index)
{
    if (index < 0) index = 0;
    return s_variant_names[index % s_variant_count];
}

/* Helpers sencillos para manipular colores (0..255) */
static Color col_mul(Color c, float f)
{
    Color r;
    r.a = c.a;
    r.r = (unsigned char)fminf(255.0f, fmaxf(0.0f, c.r * f));
    r.g = (unsigned char)fminf(255.0f, fmaxf(0.0f, c.g * f));
    r.b = (unsigned char)fminf(255.0f, fmaxf(0.0f, c.b * f));
    return r;
}

static Color col_lerp(Color a, Color b, float t)
{
    Color r;
    r.a = (unsigned char)((1.0f - t) * a.a + t * b.a);
    r.r = (unsigned char)((1.0f - t) * a.r + t * b.r);
    r.g = (unsigned char)((1.0f - t) * a.g + t * b.g);
    r.b = (unsigned char)((1.0f - t) * a.b + t * b.b);
    return r;
}

/* Aplica una variante simple al tema base y escribe el resultado en out. */
void gui_theme_apply_variant(const GuiTheme *base, int variant_index, GuiTheme *out)
{
    if (!base || !out) return;
    *out = *base;

    int v = (variant_index < 0) ? 0 : (variant_index % s_variant_count);
    switch (v) {
        case 1: /* Brillante: aumentar brillo general */
            out->bg_main = col_mul(out->bg_main, 1.08f);
            out->bg_header = col_mul(out->bg_header, 1.06f);
            out->bg_sidebar = col_mul(out->bg_sidebar, 1.06f);
            out->bg_list = col_mul(out->bg_list, 1.06f);
            out->text_primary = col_mul(out->text_primary, 1.05f);
            break;
        case 2: /* Oscuro: disminuir brillo */
            out->bg_main = col_mul(out->bg_main, 0.88f);
            out->bg_header = col_mul(out->bg_header, 0.9f);
            out->bg_sidebar = col_mul(out->bg_sidebar, 0.9f);
            out->bg_list = col_mul(out->bg_list, 0.9f);
            out->text_primary = col_mul(out->text_primary, 0.95f);
            break;
        case 3: /* Saturado: mezclar con accent_primary ligeramente */
            out->text_highlight = col_lerp(out->text_highlight, out->accent_primary, 0.18f);
            out->accent = col_lerp(out->accent, out->accent_primary, 0.25f);
            out->accent_primary = col_lerp(out->accent_primary, out->accent_primary_hv, 0.12f);
            break;
        case 4: /* Desaturado: mezcla hacia gris */
            {
                Color gray = {128, 128, 128, 255};
                out->text_primary = col_lerp(out->text_primary, gray, 0.28f);
                out->text_secondary = col_lerp(out->text_secondary, gray, 0.3f);
                out->accent = col_lerp(out->accent, gray, 0.4f);
            }
            break;
        default:
            /* 0 = Default, no cambios */
            break;
    }
}

const GuiModuleInfo *gui_get_module_info(int index)
{
    if (index < 0 || index >= GUI_FILTER_COUNT) index = 0;
    return &s_module_info[index];
}

const GuiModuleHelp *gui_get_module_help(int index)
{
    if (index < 0 || index >= GUI_FILTER_COUNT) index = 0;
    return &s_module_help[index];
}

void gui_update_active_theme(GuiState *st)
{
    if (!st) return;
    const GuiTheme *base = gui_get_theme_by_index(st->theme_index);
    gui_theme_apply_variant(base, st->variant_index, &s_active_theme_storage);
    st->theme = &s_active_theme_storage;
}

/* ═══════════════════════════════════════════════════════════
   Escala DPI
   ═══════════════════════════════════════════════════════════ */

float gui_compute_scale(int screen_width, int screen_height)
{
    float sw = (float)screen_width / 1280.0f;
    float sh = (float)screen_height / 720.0f;
    float s = fminf(sw, sh);
    if (s < 0.5f) s = 0.5f;
    if (s > 3.0f) s = 3.0f;
    return s;
}

/* ═══════════════════════════════════════════════════════════
   Animaciones
   ═══════════════════════════════════════════════════════════ */

void gui_anim_tick(GuiAnim *a, float dt)
{
    if (!a) return;
    float t = 1.0f - expf(-a->speed * dt);
    a->value += (a->target - a->value) * t;
    if (fabsf(a->value - a->target) < 0.02f)
        a->value = a->target;
}

void gui_anim_set_target(GuiAnim *a, float target)
{
    if (a) a->target = target;
}

void gui_anim_snap(GuiAnim *a, float value)
{
    if (!a) return;
    a->value = value;
    a->target = value;
}

/* ═══════════════════════════════════════════════════════════
   Fuente (estado global — persiste entre llamadas)
   ═══════════════════════════════════════════════════════════ */

static Font  g_ui_font;
static int   g_ui_font_loaded = 0;

static unsigned char gui_luma(Color c)
{
    return (unsigned char)((77 * c.r + 150 * c.g + 29 * c.b) / 256);
}

void gui_load_font(void)
{
    const char *candidates[] = {
        "./images/fonts/NotoSans-Bold.ttf",
        "./images/fonts/NotoSans-Regular.ttf",
        "./Imagenes/fonts/NotoSans-Bold.ttf",
        "./Imagenes/fonts/NotoSans-Regular.ttf",
        "C:/Windows/Fonts/segoeuib.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
        "C:/Windows/Fonts/verdana.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    };
    int n = (int)(sizeof(candidates) / sizeof(candidates[0]));

    if (g_ui_font_loaded) {
        UnloadFont(g_ui_font);
        g_ui_font_loaded = 0;
    }

    for (int i = 0; i < n; i++) {
        if (!FileExists(candidates[i])) continue;
        g_ui_font = LoadFontEx(candidates[i], 96, NULL, 0);
        if (g_ui_font.texture.id != 0) {
            SetTextureFilter(g_ui_font.texture, TEXTURE_FILTER_BILINEAR);
            g_ui_font_loaded = 1;
            gui_icons_load_all();
            return;
        }
    }

    gui_icons_load_all();
}

void gui_unload_font(void)
{
    gui_icons_unload_all();
    if (g_ui_font_loaded) {
        UnloadFont(g_ui_font);
        g_ui_font_loaded = 0;
    }
}

Font gui_get_font(void)
{
    return g_ui_font_loaded ? g_ui_font : GetFontDefault();
}

void gui_text(const char *text, float x, float y, float size, Color color)
{
    Font font = gui_get_font();
    const char *safe = text ? text : "";
    Color shadow = {5, 12, 8, color.a};
    if (gui_luma(color) < 96)
    {
        shadow = (Color){255, 255, 255, (unsigned char)(color.a * 0.45f)};
    }
    else
    {
        shadow = (Color){4, 10, 8, (unsigned char)(color.a * 0.7f)};
    }
    DrawTextEx(font, safe, (Vector2){x + 1.0f, y + 1.0f}, size, 1.0f, shadow);
    DrawTextEx(font, safe, (Vector2){x, y}, size, 1.0f, color);
}

void gui_draw_module_header(const char *title, int screen_width)
{
    const GuiTheme *theme = gui_get_active_theme();
    DrawRectangle(0, 0, screen_width, 84, theme->bg_header);
    gui_text("MiFutbolC", 26.0f, 20.0f, 36.0f, theme->text_primary);
    gui_text(title ? title : "", 230.0f, 34.0f, 20.0f, theme->text_secondary);
}

Rectangle gui_draw_list_shell(Rectangle panel,
                              const char *col1,
                              float col1_x,
                              const char *col2,
                              float col2_x)
{
    const GuiTheme *theme = gui_get_active_theme();
    float header_h = 32.0f;

    DrawRectangleRec(panel, theme->card_bg);
    DrawRectangleLinesEx(panel, 1.0f, theme->border);

    if (col1 && col1[0] != '\0')
    {
        gui_text(col1, panel.x + col1_x, panel.y + 8.0f, 16.0f, theme->text_secondary);
    }

    if (col2 && col2[0] != '\0')
    {
        gui_text(col2, panel.x + col2_x, panel.y + 8.0f, 16.0f, theme->text_secondary);
    }

    DrawLine((int)panel.x + 4,
             (int)(panel.y + header_h - 2.0f),
             (int)(panel.x + panel.width) - 4,
             (int)(panel.y + header_h - 2.0f),
             theme->border);

    {
        Rectangle content = {panel.x, panel.y + header_h, panel.width, panel.height - header_h};
        if (content.height < 1.0f)
        {
            content.height = 1.0f;
        }
        return content;
    }
}

void gui_draw_list_row_bg(Rectangle row_rect, int row_index, int hovered)
{
    const GuiTheme *theme = gui_get_active_theme();
    Color even_bg = col_lerp(theme->bg_list, theme->card_bg, 0.22f);
    Color odd_bg = col_lerp(theme->bg_list, theme->card_bg, 0.36f);
    Color bg = (row_index % 2 == 0) ? even_bg : odd_bg;
    if (hovered)
    {
        bg = theme->row_hover;
    }
    DrawRectangleRec(row_rect, bg);
}

void gui_draw_footer_hint(const char *text, float x, int screen_height)
{
    const GuiTheme *theme = gui_get_active_theme();
    gui_text(text ? text : "", x, (float)screen_height - 62.0f, 18.0f, theme->text_secondary);
}

Vector2 gui_text_measure(const char *text, float size)
{
    return MeasureTextEx(gui_get_font(), text ? text : "", size, 1.0f);
}

/* ═══════════════════════════════════════════════════════════
   Helpers de texto responsive
   ═══════════════════════════════════════════════════════════ */

void gui_text_truncated(const char *text, float x, float y,
                        float size, float max_width, Color color)
{
    const char *safe = text ? text : "";
    if (gui_text_measure(safe, size).x <= max_width) {
        gui_text(safe, x, y, size, color);
        return;
    }
    char buf[256];
    int slen = (int)strlen_s(safe, 252);
    for (int n = slen; n > 0; n--) {
        snprintf(buf, sizeof(buf), "%.*s...", n, safe);
        if (gui_text_measure(buf, size).x <= max_width) {
            gui_text(buf, x, y, size, color);
            return;
        }
    }
    gui_text("...", x, y, size, color);
}

void gui_text_wrapped(const char *text, Rectangle bounds,
                      float font_size, Color color)
{
    const char *safe = text ? text : "";
    char buffer[2048];
    snprintf(buffer, sizeof(buffer), "%s", safe);

    char line[256] = "";
    float y = bounds.y;
    float line_h = font_size + 4.0f;
    Font font = gui_get_font();

    char *ctx = NULL;
#if defined(_MSC_VER) || defined(_WIN32)
    char *word = strtok_s(buffer, " ", &ctx);
#else
    char *word = strtok_r(buffer, " ", &ctx);
#endif
    while (word) {
        char test[256];
        if (line[0] == '\0')
            snprintf(test, sizeof(test), "%s", word);
        else
            snprintf(test, sizeof(test), "%s %s", line, word);

        if (MeasureTextEx(font, test, font_size, 1.0f).x > bounds.width
            && line[0] != '\0')
        {
            gui_text(line, bounds.x, y, font_size, color);
            y += line_h;
            if (y + font_size > bounds.y + bounds.height) return;
            snprintf(line, sizeof(line), "%s", word);
        } else {
            snprintf(line, sizeof(line), "%s", test);
        }
        
    #if defined(_MSC_VER) || defined(_WIN32)
        word = strtok_s(NULL, " ", &ctx);
    #else
        word = strtok_r(NULL, " ", &ctx);
    #endif
    }
    if (line[0] != '\0')
        gui_text(line, bounds.x, y, font_size, color);
}

float gui_text_fit(const char *text, float font_size,
                   float max_width, float min_size)
{
    const char *safe = text ? text : "";
    while (font_size > min_size &&
           gui_text_measure(safe, font_size).x > max_width)
        font_size -= 1.0f;
    if (font_size < min_size) font_size = min_size;
    return font_size;
}

/* ═══════════════════════════════════════════════════════════
   Normalización y búsqueda
   ═══════════════════════════════════════════════════════════ */

static int normalize_advance_char(const unsigned char *src, int *consumed)
{
    unsigned char c0 = src[0];
    if (c0 == '\0') { *consumed = 0; return -1; }

    if (c0 < 128) {
        *consumed = 1;
        if (isalnum((int)c0)) return tolower((int)c0);
        return ' ';
    }

    if (c0 == 0xC3 && src[1] != '\0') {
        unsigned char c1 = src[1];
        *consumed = 2;
        switch (c1) {
            case 0xA1: case 0x81: return 'a';
            case 0xA9: case 0x89: return 'e';
            case 0xAD: case 0x8D: return 'i';
            case 0xB3: case 0x93: return 'o';
            case 0xBA: case 0x9A: return 'u';
            case 0xB1: case 0x91: return 'n';
            case 0xBC: case 0x9C: return 'u';
            default: return ' ';
        }
    }

    /* Fallback: treat unknown single-byte (non UTF-8) as separator to avoid
       platform-dependent Latin-1 assumptions. */
    *consumed = 1;
    return ' ';
}

void gui_normalize(const char *src, char *dst, size_t dst_size)
{
    size_t out = 0;
    int prev_space = 1;
    static const unsigned char safe_empty[2] = { 0, 0 };
    const unsigned char *p = (src && src[0] ? (const unsigned char *)src : safe_empty);

    if (!dst || dst_size == 0) return;

    while (*p != '\0' && out < dst_size - 1) {
        int consumed = 0;
        int n = normalize_advance_char(p, &consumed);
        if (consumed <= 0) break;
        p += consumed;

        if (n == ' ') {
            if (!prev_space) { dst[out++] = ' '; prev_space = 1; }
        } else {
            dst[out++] = (char)n;
            prev_space = 0;
        }
    }

    if (out > 0 && dst[out - 1] == ' ') out--;
    dst[out] = '\0';
}

QueryTokens gui_tokenize_query(const char *query)
{
    QueryTokens q = {{{0}}, 0};
    char buf[128];
    char const *tok = NULL;
    char *ctx = NULL;

    gui_normalize(query ? query : "", buf, sizeof(buf));
    tok = strtok_s(buf, " ", &ctx);
    while (tok && q.count < 8) {
        if (strlen_s(tok, sizeof(buf)) > 0) {
            strncpy_s(q.tokens[q.count], sizeof(q.tokens[q.count]), tok, _TRUNCATE);
            q.count++;
        }
        tok = strtok_s(NULL, " ", &ctx);
    }
    return q;
}

int gui_matches_query(const char *label, const QueryTokens *tokens)
{
    char buf[256];
    if (!tokens || tokens->count <= 0) return 1;
    gui_normalize(label ? label : "", buf, sizeof(buf));
    for (int i = 0; i < tokens->count; i++) {
        if (!strstr(buf, tokens->tokens[i])) return 0;
    }
    return 1;
}

int gui_matches_filter(const MenuItem *item, GuiFilter filter)
{
    if (!item) return 0;
    if (filter == GUI_FILTER_ALL) return 1;
    return item->categoria == (MenuCategory)filter;
}

int gui_rebuild_visible(const MenuItem *items, int count,
                        GuiFilter filter, const QueryTokens *tokens,
                        int *indices, int *out_count)
{
    int vis = 0;
    if (!items || count <= 0 || !indices || !out_count) {
        if (out_count) *out_count = 0;
        return 0;
    }

    for (int i = 0; i < count; i++) {
        if (gui_matches_filter(&items[i], filter) &&
            gui_matches_query(items[i].texto ? items[i].texto : "", tokens))
        {
            indices[vis++] = i;
        }
    }
    *out_count = vis;
    return vis;
}

/* ═══════════════════════════════════════════════════════════
   Componentes de dibujo
   ═══════════════════════════════════════════════════════════ */

/* Helper: draw button rectangle, border and centered text, return hovered */
static int gui_button_draw_and_text(const GuiState *st, Rectangle rect, const char *label, float font_size, Color fill, Color border_col, Color text_col)
{
    float scale = st->scale;
    Vector2 mouse = GetMousePosition();
    int hovered = CheckCollisionPointRec(mouse, rect);
    float padding = GS(GUI_PAD_SMALL);
    Vector2 measure = gui_text_measure(label, font_size);
    float text_x = rect.x + (rect.width - measure.x) * 0.5f;
    float text_y = rect.y + (rect.height - measure.y) * 0.5f;

    DrawRectangleRec(rect, fill);
    DrawRectangleLinesEx(rect, 1.0f, border_col);
    gui_text_truncated(label, text_x, text_y, font_size, rect.width - padding * 2, text_col);
    return hovered;
}

int gui_btn(const GuiState *st, Rectangle rect, const char *label)
{
    float scale = st->scale;
    float font_sz = gui_text_fit(label, FONT_BODY, rect.width - GS(GUI_PAD_SMALL) * 2, FONT_TINY);
    Color default_fill = st->theme->accent;
    Color border_col = st->theme->border;
    Color text_col = st->theme->text_primary;
    Vector2 mouse = GetMousePosition();
    int hovered = CheckCollisionPointRec(mouse, rect);
    Color used_fill = hovered ? st->theme->accent_hover : default_fill;
    gui_button_draw_and_text(st, rect, label, font_sz, used_fill, border_col, text_col);
    return hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

int gui_btn_primary(const GuiState *st, Rectangle rect, const char *label)
{
    float scale = st->scale;
    float pulse = 0.7f + 0.3f * sinf((float)GetTime() * 5.0f);
    float font_sz = gui_text_fit(label, FONT_SUB, rect.width - GS(GUI_PAD_MEDIUM) * 2, FONT_TINY);
    Vector2 mouse = GetMousePosition();
    int hovered = CheckCollisionPointRec(mouse, rect);

    /* Drop shadow */
    Rectangle shadow = {rect.x + 2.0f, rect.y + 2.0f, rect.width, rect.height};
    DrawRectangleRec(shadow, (Color){0, 0, 0, 50});

    Color fill = hovered
        ? ColorLerp(st->theme->accent_primary, st->theme->accent_primary_hv, pulse)
        : st->theme->accent_primary;
    Color border_col = hovered ? st->theme->accent_primary_hv : st->theme->accent_primary;
    Color text_col = (Color){255,255,255,255};
    gui_button_draw_and_text(st, rect, label, font_sz, fill, border_col, text_col);
    return hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

int gui_module_btn(const GuiState *st, Rectangle rect, const char *label, int selected, int icon_option)
{
    float scale = st->scale;
    float pad = GS(6);
    int hovered = CheckCollisionPointRec(GetMousePosition(), rect);
    float icon_sz = 0.0f;
    float icon_gap = 0.0f;
    if (icon_option >= 0) {
        icon_sz = rect.height - GS(10);
        icon_gap = GS(6);
    }
    float avail = rect.width - pad * 2 - icon_sz - icon_gap;
    float sz = gui_text_fit(label, FONT_SUB, avail, FONT_TINY);
    Vector2 m = gui_text_measure(label, sz);
    float content_w = icon_sz + icon_gap + m.x;
    float cx = rect.x + (rect.width - content_w) * 0.5f;
    float ty = rect.y + (rect.height - m.y) * 0.5f;

    Color fill = st->theme->accent;
    if (selected)
        fill = st->theme->row_selected;
    else if (hovered)
        fill = st->theme->accent_hover;
    Color border = selected ? st->theme->text_secondary : st->theme->border;
    DrawRectangleRounded(rect, 0.25f, 4, fill);
    DrawRectangleLinesEx(rect, 1.0f, border);

    if (icon_option >= 0) {
        /* Hover/selected: icono ligeramente mas grande */
        float boost = (hovered || selected) ? GS(2) : 0.0f;
        Rectangle icon_r = {
            cx - boost * 0.5f,
            rect.y + GS(5) - boost * 0.5f,
            icon_sz + boost,
            icon_sz + boost
        };
        gui_draw_option_icon(icon_option, icon_r);
    }
    gui_text_truncated(label, cx + icon_sz + icon_gap, ty, sz, avail, st->theme->text_primary);

    return hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

void gui_card(const GuiState *st, Rectangle rect, const char *title, int value)
{
    float scale = st->scale;
    DrawRectangleRec(rect, st->theme->card_bg);
    DrawRectangleLinesEx(rect, 1.0f, st->theme->card_border);
    gui_text_truncated(title, rect.x + GS(8), rect.y + GS(4), FONT_SMALL,
                       rect.width - GS(16), st->theme->text_secondary);
    gui_text(TextFormat("%d", value),
             rect.x + GS(8), rect.y + GS(20), FONT_SUB, st->theme->text_primary);
}

int gui_tab_bar(const GuiState *st, float x, float y,
                const char *labels[], const int counts[], int n, int active)
{
    float scale = st->scale;
    int clicked = -1;
    float cx = x;
    float tab_h = GS(28);
    float gap = GS(4);

    for (int i = 0; i < n; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s (%d)", labels[i], counts[i]);
        float sz = FONT_SMALL;
        Vector2 m = gui_text_measure(buf, sz);
        float tw = m.x + GS(20);
        if (tw > GS(160)) tw = GS(160);
        Rectangle r = {cx, y, tw, tab_h};

        int is_active = (i == active);
        int hovered = CheckCollisionPointRec(GetMousePosition(), r);

        if (is_active) {
            DrawRectangleRec(r, st->theme->accent_primary);
            /* Underline indicator */
            DrawRectangle((int)r.x, (int)(r.y + tab_h - GS(3)),
                          (int)r.width, GSI(3),
                          st->theme->accent_primary_hv);
            Color white = {255, 255, 255, 255};
            gui_text_truncated(buf, r.x + GS(10), r.y + (tab_h - m.y) * 0.5f,
                               sz, tw - GS(20), white);
        } else {
            Color bg = hovered ? st->theme->row_hover : st->theme->bg_sidebar;
            DrawRectangleRec(r, bg);
            DrawRectangleLinesEx(r, 1.0f, st->theme->border);
            gui_text_truncated(buf, r.x + GS(10), r.y + (tab_h - m.y) * 0.5f, sz,
                     tw - GS(20),
                     hovered ? st->theme->text_primary : st->theme->text_muted);
        }

        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            clicked = i;
        cx += tw + gap;
    }
    return clicked;
}

void gui_toast_draw(const GuiState *st, Rectangle rect, const char *text, float alpha)
{
    float scale = st->scale;
    unsigned char a = (unsigned char)(220.0f * (alpha > 1.0f ? 1.0f : alpha));
    Color bg     = {27, 86, 52, a};
    Color border = {181, 230, 196, a};
    Color fg     = {235, 248, 239, a};

    DrawRectangleRec(rect, bg);
    DrawRectangleLinesEx(rect, 1.0f, border);
    gui_text(text, rect.x + GS(14), rect.y + GS(12), FONT_SUB, fg);
}

void gui_scrollbar_draw(const GuiState *st, Rectangle area,
                        int total, int visible_rows, float scroll_pos)
{
    float scale = st->scale;
    if (total <= visible_rows) return;

    float track_w = GS(8);
    float track_x = area.x + area.width - track_w - GS(2);
    float track_y = area.y + GS(4);
    float track_h = area.height - GS(8);
    float thumb_h = track_h * ((float)visible_rows / (float)total);
    if (thumb_h < GS(28)) thumb_h = GS(28);

    float max_s  = (float)(total - visible_rows);
    float ratio  = max_s > 0.0f ? (scroll_pos / max_s) : 0.0f;
    float thumb_y = track_y + (track_h - thumb_h) * ratio;

    /* Track */
    DrawRectangleRounded(
        (Rectangle){track_x, track_y, track_w, track_h},
        0.5f, 4, st->theme->bg_sidebar);
    /* Thumb — brighter and wider */
    int thumb_hov = CheckCollisionPointRec(GetMousePosition(),
        (Rectangle){track_x - GS(4), thumb_y, track_w + GS(8), thumb_h});
    Color thumb_c = thumb_hov
        ? st->theme->accent_primary_hv
        : st->theme->accent_primary;
    DrawRectangleRounded(
        (Rectangle){track_x, thumb_y, track_w, thumb_h},
        0.5f, 4, thumb_c);
}

void gui_searchbox_draw(const GuiState *st, Rectangle rect,
                        const char *query, int focused, float blink)
{
    float scale = st->scale;

    Color bg = focused ? st->theme->row_hover : st->theme->card_bg;
    DrawRectangleRec(rect, bg);

    if (focused) {
        /* Outer glow — double layer for stronger effect */
        Rectangle glow2 = {rect.x - 4, rect.y - 4,
                           rect.width + 8, rect.height + 8};
        Rectangle glow = {rect.x - 2, rect.y - 2,
                          rect.width + 4, rect.height + 4};
        DrawRectangleLinesEx(glow2, 1.0f, st->theme->search_glow);
        DrawRectangleLinesEx(glow, 2.0f, st->theme->search_glow);
        DrawRectangleLinesEx(rect, 2.0f, st->theme->accent_primary);
    } else {
        DrawRectangleLinesEx(rect, 1.0f, st->theme->border);
    }

    /* Magnifying glass icon (drawn as circle + line) */
    float icon_r = GS(6);
    float icon_cx = rect.x + GS(14);
    float icon_cy = rect.y + rect.height * 0.5f;
    DrawCircleLines((int)icon_cx, (int)icon_cy, icon_r,
                    focused ? st->theme->accent_primary : st->theme->text_muted);
    DrawLine((int)(icon_cx + icon_r * 0.7f), (int)(icon_cy + icon_r * 0.7f),
             (int)(icon_cx + icon_r * 1.4f), (int)(icon_cy + icon_r * 1.4f),
             focused ? st->theme->accent_primary : st->theme->text_muted);

    char display[96];
    if (query[0] == '\0' && !focused)
        snprintf(display, sizeof(display), "Buscar... (Ctrl+F)");
    else
        snprintf(display, sizeof(display), "%s", query);

    if (focused && blink < 0.5f) {
        size_t len = strlen_s(display, sizeof(display));
        if (len < sizeof(display) - 1) {
            display[len]     = '|';
            display[len + 1] = '\0';
        }
    }

    gui_text(display,
             rect.x + GS(28), rect.y + GS(8), FONT_SMALL,
             focused ? st->theme->text_primary : st->theme->text_muted);
}

void gui_help_panel_draw(const GuiState *st, Rectangle rect, const GuiModuleHelp *help)
{
    float scale = st->scale;
    DrawRectangleRec(rect, st->theme->bg_list);
    DrawRectangleLinesEx(rect, 1.0f, st->theme->border);
    gui_text("Ayuda del modulo", rect.x + GS(12), rect.y + GS(16), FONT_SUB, st->theme->text_primary);
    float tip_w = rect.width - GS(24);
    float tip_h = GS(32);
    gui_text_wrapped(help->tip_1,
        (Rectangle){rect.x + GS(12), rect.y + GS(46), tip_w, tip_h},
        FONT_BODY, st->theme->text_muted);
    gui_text_wrapped(help->tip_2,
        (Rectangle){rect.x + GS(12), rect.y + GS(80), tip_w, tip_h},
        FONT_BODY, st->theme->text_muted);
    gui_text_wrapped(help->tip_3,
        (Rectangle){rect.x + GS(12), rect.y + GS(114), tip_w, tip_h},
        FONT_BODY, st->theme->text_muted);
}

void gui_breadcrumb_draw(const GuiState *st, float x, float y,
                         const char *crumbs[], int count)
{
    float scale = st->scale;
    float cx = x;
    float sz = FONT_SMALL;
    float max_x = (float)GetScreenWidth() - GS(30);

    for (int i = 0; i < count; i++) {
        float avail = max_x - cx;
        if (avail < GS(40)) break;

        /* Color gradation: first=muted, middle=secondary, last=accent */
        Color c;
        if (i == count - 1)
            c = st->theme->accent_primary;
        else if (i == 0)
            c = st->theme->text_muted;
        else
            c = st->theme->text_secondary;

        if (i == count - 1)
            gui_text_truncated(crumbs[i], cx, y, sz, avail, c);
        else
            gui_text(crumbs[i], cx, y, sz, c);

        Vector2 m = gui_text_measure(crumbs[i], sz);
        float advance = (m.x < avail) ? m.x : avail;
        cx += advance;

        if (i < count - 1) {
            Color sep_c = {st->theme->text_muted.r, st->theme->text_muted.g,
                           st->theme->text_muted.b, 150};
            gui_text(" > ", cx, y, sz, sep_c);
            Vector2 sep = gui_text_measure(" > ", sz);
            cx += sep.x;
        }
    }
}

void gui_history_panel_draw(const GuiState *st, Rectangle rect,
                            const char history[][96], int count)
{
    float scale = st->scale;
    DrawRectangleRec(rect, st->theme->card_bg);
    DrawRectangleLinesEx(rect, 1.0f, st->theme->card_border);
    gui_text("Historial", rect.x + GS(12), rect.y + GS(8), FONT_SMALL, st->theme->text_secondary);
    float hist_line_h = FONT_TINY + 4.0f;
    for (int i = 0; i < count; i++) {
        gui_text_truncated(history[i],
                 rect.x + GS(12), rect.y + GS(30) + (float)i * hist_line_h,
                 FONT_TINY, rect.width - GS(24), st->theme->text_muted);
    }
}

void gui_detail_panel_draw(const GuiState *st, Rectangle rect,
                           const MenuItem *item,
                           const char history[][96], int hist_count)
{
    float scale = st->scale;
    DrawRectangleRec(rect, st->theme->card_bg);
    DrawRectangleLinesEx(rect, 1.0f, st->theme->card_border);

    if (!item) {
        gui_text("Selecciona un item", rect.x + GS(10), rect.y + GS(10),
                 FONT_SMALL, st->theme->text_muted);
        return;
    }

    /* Title */
    gui_text("Detalle", rect.x + GS(10), rect.y + GS(6),
             FONT_TINY, st->theme->text_muted);

    Rectangle icon_rect = {rect.x + GS(10), rect.y + GS(22), GS(30), GS(30)};
    gui_draw_option_icon(item->opcion, icon_rect);

    /* Item name */
    gui_text_truncated(item->texto ? item->texto : "(sin texto)",
             rect.x + GS(46), rect.y + GS(24), FONT_BODY,
             rect.width - GS(56), st->theme->text_primary);

    /* Category badge */
    const char *cat_names[] = {"Todos", "Gestion", "Competencia", "Analisis", "Admin"};
    int cat_idx = (int)item->categoria;
    if (cat_idx < 0 || cat_idx >= 5) cat_idx = 0;
    {
        const char *badge = cat_names[cat_idx];
        float bsz = FONT_TINY;
        Vector2 bm = gui_text_measure(badge, bsz);
        float bx = rect.x + GS(46);
        float by = rect.y + GS(42);
        Rectangle br = {bx, by, bm.x + GS(12), bm.y + GS(4)};
        DrawRectangleRec(br, st->theme->accent);
        gui_text(badge, bx + GS(6), by + GS(2), bsz, st->theme->text_primary);

        gui_text(TextFormat("#%d", item->opcion),
                 bx + br.width + GS(8), by + GS(2), bsz, st->theme->text_muted);
    }

    /* Separator */
    float sep_y = rect.y + GS(62);
    DrawRectangle((int)(rect.x + GS(10)), (int)sep_y,
                  (int)(rect.width - GS(20)), 1, st->theme->border);

    /* Description from module info */
    gui_text("Categoria:", rect.x + GS(10), sep_y + GS(6),
             FONT_TINY, st->theme->text_secondary);
    gui_text_truncated(
             gui_get_module_info(cat_idx)->subtitle,
             rect.x + GS(10), sep_y + GS(20), FONT_TINY,
             rect.width - GS(20), st->theme->text_muted);

    /* Recent actions */
    if (hist_count > 0) {
        float ay = sep_y + GS(38);
        DrawRectangle((int)(rect.x + GS(10)), (int)ay,
                      (int)(rect.width - GS(20)), 1, st->theme->border);
        gui_text("Ultimas acciones:", rect.x + GS(10), ay + GS(4),
                 FONT_TINY, st->theme->text_secondary);
        int show = hist_count > 3 ? 3 : hist_count;
        for (int i = 0; i < show; i++) {
            gui_text_truncated(history[i],
                     rect.x + GS(10), ay + GS(18) + (float)i * (FONT_TINY + GS(2)),
                     FONT_TINY, rect.width - GS(20), st->theme->text_muted);
        }
    }
}

/* ═══════════════════════════════════════════════════════════
   Lista virtualizada
   Solo itera y dibuja los ítems realmente visibles en pantalla.
   ═══════════════════════════════════════════════════════════ */

static void gui_list_draw_empty(const GuiState *st, Rectangle area)
{
    float scale = st->scale;
    gui_text("No hay resultados con el filtro actual.",
             area.x + GS(22), area.y + GS(26), FONT_SUB,
             st->theme->text_primary);
    gui_text("Escribe otra busqueda o presiona Esc para limpiar.",
             area.x + GS(22), area.y + GS(60), FONT_BODY,
             st->theme->text_secondary);
}

static void gui_list_visible_range(const GuiListDrawContext *ctx,
                                   Rectangle area,
                                   int *first,
                                   int *last,
                                   int *y_shift)
{
    int local_first = (int)floorf(ctx->scroll_smooth);
    float sub = ctx->scroll_smooth - (float)local_first;
    int max_rows = (int)(area.height / (float)ctx->row_h) + 2;
    int local_last = local_first + max_rows;

    if (local_first < 0)
        local_first = 0;
    if (local_last > ctx->visible_count)
        local_last = ctx->visible_count;

    *first = local_first;
    *last = local_last;
    *y_shift = (int)(sub * (float)ctx->row_h);
}

static int gui_list_has_match(const GuiListDrawContext *ctx, int gidx)
{
    const char *item_text;

    if (!ctx->tokens || ctx->tokens->count <= 0)
        return 0;

    item_text = ctx->items[gidx].texto;
    if (!item_text)
        item_text = "";
    return gui_matches_query(item_text, ctx->tokens);
}

static void gui_list_draw_row_background(const GuiState *st,
                                         Rectangle row_rect,
                                         int gidx,
                                         int is_sel,
                                         int hovered)
{
    float scale = st->scale;
    int is_flash = (st->click_flash_timer > 0.0f && st->click_flash_row == gidx);

    if (is_flash) {
        unsigned char fa = (unsigned char)(st->click_flash_timer * 600.0f);
        if (fa > 255)
            fa = 255;
        {
            Color flash = {st->theme->accent_primary.r,
                           st->theme->accent_primary.g,
                           st->theme->accent_primary.b, fa};
            DrawRectangleRec(row_rect, flash);
        }
        return;
    }

    if (is_sel) {
        DrawRectangleRec(row_rect, st->theme->row_selected);
        {
            float hl = st->row_highlight.value;
            if (hl < 0.1f)
                hl = 0.1f;
            if (hl > 1.0f)
                hl = 1.0f;
            {
                float bar_w = GS(4) * hl;
                if (bar_w < 1.0f)
                    bar_w = 1.0f;
                DrawRectangleRounded(
                    (Rectangle){row_rect.x, row_rect.y + 2,
                                bar_w, row_rect.height - 4},
                    0.5f, 2, st->theme->accent_primary);
            }
        }
        return;
    }

    if (hovered) {
        DrawRectangleRec(row_rect, st->theme->row_hover);
        DrawRectangleRounded(
            (Rectangle){row_rect.x, row_rect.y + 2,
                        GS(3), row_rect.height - 4},
            0.5f, 2, st->theme->accent_hover);
    }
}

static void gui_list_draw_row_content(const GuiState *st,
                                      const GuiListDrawContext *ctx,
                                      Rectangle row_rect,
                                      int gidx,
                                      int is_sel,
                                      int hovered,
                                      int has_match)
{
    float scale = st->scale;
    Color num_c = has_match ? st->theme->text_highlight : st->theme->text_secondary;
    Color txt_c = has_match ? st->theme->text_highlight : st->theme->text_primary;
    const char *item_text = ctx->items[gidx].texto;

    if (!item_text)
        item_text = "(sin texto)";

    {
        Rectangle icon_rect = {
            row_rect.x + GS(6),
            row_rect.y + GS(3),
            (float)(ctx->row_h - 10),
            (float)(ctx->row_h - 10)
        };
        if (hovered || is_sel) {
            float boost = GS(2);
            icon_rect.x -= boost * 0.5f;
            icon_rect.y -= boost * 0.5f;
            icon_rect.width += boost;
            icon_rect.height += boost;
        }
        gui_draw_option_icon(ctx->items[gidx].opcion, icon_rect);
    }

    gui_text(TextFormat("%2d", ctx->items[gidx].opcion),
             row_rect.x + GS(10) + (float)(ctx->row_h - 10), row_rect.y + GS(6),
             FONT_SUB, num_c);
    gui_text_truncated(item_text,
                       row_rect.x + GS(62) + (float)(ctx->row_h - 10), row_rect.y + GS(6),
                       FONT_SUB,
                       row_rect.width - GS(72) - (float)(ctx->row_h - 10), txt_c);
}

GuiListResult gui_list_draw(const GuiState *st, Rectangle area,
                            const GuiListDrawContext *ctx)
{
    GuiListResult result = {-1};
    float scale = st->scale;
    int first = 0;
    int last = 0;
    int y_shift = 0;

    if (!ctx || !ctx->items || !ctx->visible_indices || ctx->row_h <= 0)
        return result;

    if (ctx->visible_count == 0) {
        gui_list_draw_empty(st, area);
        return result;
    }

    gui_list_visible_range(ctx, area, &first, &last, &y_shift);

    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);

    for (int i = first; i < last; i++) {
        int row;
        int y;
        int gidx;
        Rectangle row_rect;
        int is_sel;
        int hovered;
        int has_match;

        row = i - first;
        y = (int)area.y + row * ctx->row_h - y_shift;

        if (y + ctx->row_h < (int)area.y)
            continue;
        if (y > (int)area.y + (int)area.height)
            break;

        gidx = ctx->visible_indices[i];
        row_rect = (Rectangle){
            area.x + GS(10), (float)y + 2.0f,
            area.width - GS(22), (float)(ctx->row_h - 4)
        };

        is_sel = (gidx == ctx->selected_global);
        hovered = CheckCollisionPointRec(GetMousePosition(), row_rect);
        has_match = gui_list_has_match(ctx, gidx);

        gui_list_draw_row_background(st, row_rect, gidx, is_sel, hovered);
        gui_list_draw_row_content(st, ctx, row_rect, gidx, is_sel, hovered, has_match);

        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            result.clicked_global = gidx;
    }

    EndScissorMode();
    return result;
}

/* ═══════════════════════════════════════════════════════════
   Layout escalado
   Todas las posiciones/tamaños se multiplican por `scale`.
   Referencia: 1280 × 720 (scale = 1.0).
   ═══════════════════════════════════════════════════════════ */

Rectangle gui_lay_header(float scale, int sw)
{
    return (Rectangle){0.0f, 0.0f, (float)sw, GS(84)};
}

Rectangle gui_lay_sidebar(float scale, int sw, int sh)
{
    float w = (float)sw * 0.2f;
    if (w < GS(180)) w = GS(180);
    if (w > GS(320)) w = GS(320);
    return (Rectangle){0.0f, GS(84), w, (float)sh - GS(84)};
}

Rectangle gui_lay_search(float scale, int sw, Rectangle sidebar)
{
    float x = sidebar.width + GS(30);
    float w = (float)sw - x - GS(150);
    if (w < GS(120)) w = GS(120);
    return (Rectangle){x, GS(142), w, GS(30)};
}

Rectangle gui_lay_list(float scale, int sw, int sh, Rectangle sidebar)
{
    float top = GS(230);
    float bot = GS(100);
    float h = (float)sh - top - bot;
    if (h < GS(120)) h = GS(120);
    float pad = GS(30);
    return (Rectangle){
        sidebar.width + pad, top,
        (float)sw - sidebar.width - pad * 2, h
    };
}

Rectangle gui_lay_action_btn(float scale, int sh, Rectangle sidebar, int index)
{
    float start_x = sidebar.width + GS(30);
    float y       = (float)sh - GS(58);
    float gap     = GS(12);
    float widths[3] = {GS(220), GS(180), GS(120)};
    float x = start_x;

    for (int i = 0; i < index; i++)
        x += widths[i] + gap;

    return (Rectangle){x, y, widths[index], GS(34)};
}

Rectangle gui_lay_toast(float scale, int sw)
{
    return (Rectangle){(float)sw - GS(420), GS(92), GS(390), GS(44)};
}

Rectangle gui_lay_history(float scale, int sh, Rectangle sidebar)
{
    float y = sidebar.y + GS(486);
    float h = (float)sh - y - GS(30);
    if (h < GS(80)) h = GS(80);
    return (Rectangle){
        GS(10), y,
        sidebar.width - GS(20), h
    };
}

/* ═══════════════════════════════════════════════════════════
   Layout helpers (composable)
   ═══════════════════════════════════════════════════════════ */

Rectangle gui_lay_row(Rectangle parent, float y_off, float h)
{
    return (Rectangle){parent.x, parent.y + y_off, parent.width, h};
}

Rectangle gui_lay_col(Rectangle parent, float x_off, float w)
{
    return (Rectangle){parent.x + x_off, parent.y, w, parent.height};
}

Rectangle gui_lay_pad(Rectangle rect, float pad)
{
    return (Rectangle){
        rect.x + pad, rect.y + pad,
        rect.width - 2.0f * pad, rect.height - 2.0f * pad
    };
}

Rectangle gui_lay_split_h(Rectangle parent, float ratio, Rectangle *right)
{
    float lw = parent.width * ratio;
    if (right)
        *right = (Rectangle){parent.x + lw, parent.y,
                              parent.width - lw, parent.height};
    return (Rectangle){parent.x, parent.y, lw, parent.height};
}

Rectangle gui_lay_split_v(Rectangle parent, float ratio, Rectangle *bottom)
{
    float th = parent.height * ratio;
    if (bottom)
        *bottom = (Rectangle){parent.x, parent.y + th,
                               parent.width, parent.height - th};
    return (Rectangle){parent.x, parent.y, parent.width, th};
}

/* ═══════════════════════════════════════════════════════════
   Persistencia de preferencias (cJSON)
   ═══════════════════════════════════════════════════════════ */

static FILE *gui_fopen_safe(const char *path, const char *mode)
{
    if (!path || !mode)
        return NULL;
#if defined(_MSC_VER)
    {
        FILE *f = NULL;
        if (fopen_s(&f, path, mode) != 0)
            return NULL;
        return f;
    }
#else
    return fopen(path, mode);
#endif
}

void gui_prefs_save(const GuiState *st)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return;
    cJSON_AddNumberToObject(root, "theme_index", st->theme_index);
    cJSON_AddNumberToObject(root, "variant_index", st->variant_index);
    cJSON_AddNumberToObject(root, "compact_mode", st->compact_mode);
    cJSON_AddNumberToObject(root, "active_filter", (int)st->active_filter);
    cJSON_AddNumberToObject(root, "last_selected", st->selected_global);
    char *json = cJSON_Print(root);
    if (json) {
        FILE *f = gui_fopen_safe("gui_prefs.json", "w");
        if (f) { fprintf(f, "%s", json); fclose(f); }
        free(json);
    }
    cJSON_Delete(root);
}

static int gui_json_read_int(const cJSON *root, const char *key, int *out)
{
    cJSON const *j;
    if (!root || !key || !out)
        return 0;

    j = cJSON_GetObjectItem(root, key);
    if (!j || !cJSON_IsNumber(j))
        return 0;

    *out = j->valueint;
    return 1;
}

static int gui_clamp_int(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

void gui_prefs_load(GuiState *st)
{
    int v = 0;
    FILE *f = gui_fopen_safe("gui_prefs.json", "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 4096) { fclose(f); return; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return;

    if (gui_json_read_int(root, "theme_index", &v))
        st->theme_index = gui_clamp_int(v, 0, gui_get_theme_count() - 1);

    if (gui_json_read_int(root, "variant_index", &v))
        st->variant_index = gui_clamp_int(v, 0, gui_get_variant_count() - 1);

    /* Aplicar tema+variante al storage activo */
    gui_update_active_theme(st);

    if (gui_json_read_int(root, "compact_mode", &v))
        st->compact_mode = v ? 1 : 0;

    if (gui_json_read_int(root, "active_filter", &v) && v >= 0 && v < GUI_FILTER_COUNT) {
        st->active_filter = (GuiFilter)v;
        st->current_screen = (v == 0) ? GUI_SCREEN_HOME : GUI_SCREEN_MODULE;
    }

    if (gui_json_read_int(root, "last_selected", &v) && v >= 0 && v < st->item_count)
        st->selected_global = v;

    st->needs_rebuild = 1;
    cJSON_Delete(root);
}

/* ═══════════════════════════════════════════════════════════
   Audio feedback (sonidos procedurales)
   ═══════════════════════════════════════════════════════════ */

static Sound s_snd_click   = {0};
static Sound s_snd_hover   = {0};
static Sound s_snd_execute = {0};
static int   s_sounds_loaded = 0;

static short *gen_tone(int sample_rate, int samples, float freq, float decay)
{
    short *data = (short *)malloc((size_t)samples * sizeof(short));
    if (!data) return NULL;
    for (int i = 0; i < samples; i++) {
        float t = (float)i / (float)sample_rate;
        float env = 1.0f - (float)i / (float)samples;
        env = env * env * decay;
        data[i] = (short)(sinf(2.0f * 3.14159265f * freq * t) * 12000.0f * env);
    }
    return data;
}

void gui_sounds_init(void)
{
    if (s_sounds_loaded) return;
    InitAudioDevice();
    int sr = 44100;

    /* Click: 800Hz, 50ms */
    {
        int n = sr / 20;
        short *d = gen_tone(sr, n, 800.0f, 1.0f);
        if (d) {
            Wave w = {0};
            w.frameCount = (unsigned int)n;
            w.sampleRate = (unsigned int)sr;
            w.sampleSize = 16;
            w.channels = 1;
            w.data = d;
            s_snd_click = LoadSoundFromWave(w);
            SetSoundVolume(s_snd_click, 0.4f);
            free(d);
        }
    }
    /* Hover: 1200Hz, 15ms, gentle */
    {
        int n = sr / 66;
        short *d = gen_tone(sr, n, 1200.0f, 0.5f);
        if (d) {
            Wave w = {0};
            w.frameCount = (unsigned int)n;
            w.sampleRate = (unsigned int)sr;
            w.sampleSize = 16;
            w.channels = 1;
            w.data = d;
            s_snd_hover = LoadSoundFromWave(w);
            SetSoundVolume(s_snd_hover, 0.25f);
            free(d);
        }
    }
    /* Execute: 500Hz, 120ms */
    {
        int n = sr / 8;
        short *d = gen_tone(sr, n, 500.0f, 1.2f);
        if (d) {
            Wave w = {0};
            w.frameCount = (unsigned int)n;
            w.sampleRate = (unsigned int)sr;
            w.sampleSize = 16;
            w.channels = 1;
            w.data = d;
            s_snd_execute = LoadSoundFromWave(w);
            SetSoundVolume(s_snd_execute, 0.5f);
            free(d);
        }
    }
    s_sounds_loaded = 1;
}

void gui_sounds_cleanup(void)
{
    if (!s_sounds_loaded) return;
    UnloadSound(s_snd_click);
    UnloadSound(s_snd_hover);
    UnloadSound(s_snd_execute);
    CloseAudioDevice();
    s_sounds_loaded = 0;
}

void gui_sound_click(void)   { if (s_sounds_loaded) PlaySound(s_snd_click);   }
void gui_sound_hover(void)   { if (s_sounds_loaded) PlaySound(s_snd_hover);   }
void gui_sound_execute(void) { if (s_sounds_loaded) PlaySound(s_snd_execute); }

/* ═══════════════════════════════════════════════════════════
   Debug overlay (F9)
   ═══════════════════════════════════════════════════════════ */

void gui_debug_overlay(const GuiState *st, int sw, int sh)
{
    float scale = st->scale;
    (void)sh;
    /* Posicionar a la derecha para no tapar sidebar */
    float ox = (float)sw - GS(280);
    float oy = GS(90);
    DrawRectangle((int)ox, (int)oy, GSI(270), GSI(210),
                  (Color){0, 0, 0, 180});
    DrawRectangleLinesEx(
        (Rectangle){ox, oy, GS(270), GS(210)},
        1.0f, (Color){0, 255, 0, 200});

    float y = oy + GS(6);
    float x = ox + GS(6);
    float lh = FONT_TINY + 4.0f;
    Color g = {0, 255, 0, 255};

    gui_text(TextFormat("FPS: %d", GetFPS()), x, y, FONT_SMALL, g); y += lh + 2;
    gui_text(TextFormat("Scale: %.2f", st->scale), x, y, FONT_TINY, g); y += lh;
    const char *scr_names[] = {"HOME", "MODULE", "DETAIL"};
    int scr_i = (int)st->current_screen;
    if (scr_i < 0 || scr_i > 2) scr_i = 0;
    gui_text(TextFormat("Screen: %s", scr_names[scr_i]), x, y, FONT_TINY, g); y += lh;
    const char *foc_names[] = {"NONE", "SEARCH", "LIST", "BUTTON"};
    int foc_i = (int)st->focus;
    if (foc_i < 0 || foc_i > 3) foc_i = 0;
    gui_text(TextFormat("Focus: %s", foc_names[foc_i]), x, y, FONT_TINY, g); y += lh;
    gui_text(TextFormat("Filter: %d", (int)st->active_filter), x, y, FONT_TINY, g); y += lh;
    gui_text(TextFormat("Items: %d/%d vis", st->visible_count, st->item_count), x, y, FONT_TINY, g); y += lh;
    gui_text(TextFormat("Selected: %d", st->selected_global), x, y, FONT_TINY, g); y += lh;
    gui_text(TextFormat("Scroll: %d (%.1f)", st->scroll_offset, st->scroll_anim.value), x, y, FONT_TINY, g); y += lh;
    const char *tn = gui_get_theme_name(st->theme_index);
    const char *vn = gui_get_variant_name(st->variant_index);
    gui_text(TextFormat("Theme: %s (%s)", tn, vn), x, y, FONT_TINY, g); y += lh;
    gui_text(TextFormat("Compact: %s", st->compact_mode ? "ON" : "OFF"), x, y, FONT_TINY, g); y += lh;
    gui_text(TextFormat("Transition: %.2f", st->transition.value), x, y, FONT_TINY, g); y += lh;
    gui_text(TextFormat("Window: %dx%d", GetScreenWidth(), GetScreenHeight()), x, y, FONT_TINY, g);
}

/* ═══════════════════════════════════════════════════════════
   Pantalla de detalle (full-screen)
   ═══════════════════════════════════════════════════════════ */

void gui_detail_screen_draw(const GuiState *st, Rectangle area,
                            const MenuItem *item,
                            const char history[][96], int hist_count)
{
    float scale = st->scale;
    if (!item) {
        gui_text("No hay item seleccionado.",
                 area.x + GS(20), area.y + GS(20), FONT_SUB,
                 st->theme->text_muted);
        return;
    }

    DrawRectangleRec(area, st->theme->card_bg);
    DrawRectangleLinesEx(area, 1.0f, st->theme->card_border);
    Rectangle inner = gui_lay_pad(area, GS(20));

    Rectangle hero_icon = {inner.x, inner.y, GS(56), GS(56)};
    gui_draw_option_icon(item->opcion, hero_icon);

    /* Title */
    gui_text_truncated(item->texto ? item->texto : "(sin texto)",
             inner.x + GS(68), inner.y + GS(8), FONT_TITLE,
             inner.width - GS(68), st->theme->text_primary);

    /* Category badge + option number */
    const char *cat_names[] = {"Todos", "Gestion", "Competencia", "Analisis", "Admin"};
    int cat_idx = (int)item->categoria;
    if (cat_idx < 0 || cat_idx >= 5) cat_idx = 0;
    float badge_y = inner.y + GS(64);
    {
        const char *badge = cat_names[cat_idx];
        float bsz = FONT_BODY;
        Vector2 bm = gui_text_measure(badge, bsz);
        Rectangle br = {inner.x, badge_y, bm.x + GS(16), bm.y + GS(6)};
        DrawRectangleRounded(br, 0.3f, 4, st->theme->accent);
        gui_text(badge, inner.x + GS(8), badge_y + GS(3), bsz, st->theme->text_primary);
        gui_text(TextFormat("Opcion #%d", item->opcion),
                 inner.x + br.width + GS(14), badge_y + GS(3), FONT_BODY,
                 st->theme->text_muted);
    }

    /* Separator */
    float sep_y = badge_y + GS(36);
    DrawRectangle((int)inner.x, (int)sep_y, (int)inner.width, 1, st->theme->border);

    /* Command card */
    gui_text("Comando:", inner.x, sep_y + GS(10), FONT_SMALL, st->theme->text_secondary);
    float cmd_y = sep_y + GS(28);
    Rectangle cmd_rect = {inner.x, cmd_y, inner.width, GS(44)};
    DrawRectangleRounded(cmd_rect, 0.2f, 4, st->theme->bg_list);
    DrawRectangleLinesEx(cmd_rect, 1.0f, st->theme->accent_primary);
    gui_text_truncated(item->texto ? item->texto : "(sin nombre)",
             cmd_rect.x + GS(12), cmd_rect.y + GS(6), FONT_SUB,
             cmd_rect.width - GS(24), st->theme->accent_primary);
    gui_text("ENTER \u2192 Ejecutar | ESC \u2192 Volver",
             cmd_rect.x + GS(12), cmd_rect.y + GS(28), FONT_TINY,
             st->theme->text_muted);

    /* Module description */
    float desc_y = cmd_y + GS(54);
    DrawRectangle((int)inner.x, (int)desc_y, (int)inner.width, 1, st->theme->border);
    gui_text("Modulo:", inner.x, desc_y + GS(8), FONT_SMALL, st->theme->text_secondary);
    gui_text_wrapped(gui_get_module_info(cat_idx)->subtitle,
        (Rectangle){inner.x, desc_y + GS(26), inner.width, GS(36)},
        FONT_BODY, st->theme->text_muted);

    /* Module help tips */
    float tips_y = desc_y + GS(68);
    DrawRectangle((int)inner.x, (int)tips_y, (int)inner.width, 1, st->theme->border);
    gui_text("Ayuda:", inner.x, tips_y + GS(8), FONT_SMALL, st->theme->text_secondary);
    const GuiModuleHelp *help = gui_get_module_help(cat_idx);
    float tip_y = tips_y + GS(24);
    gui_text_wrapped(help->tip_1,
        (Rectangle){inner.x + GS(8), tip_y, inner.width - GS(16), GS(24)},
        FONT_SMALL, st->theme->text_muted);
    gui_text_wrapped(help->tip_2,
        (Rectangle){inner.x + GS(8), tip_y + GS(26), inner.width - GS(16), GS(24)},
        FONT_SMALL, st->theme->text_muted);
    gui_text_wrapped(help->tip_3,
        (Rectangle){inner.x + GS(8), tip_y + GS(52), inner.width - GS(16), GS(24)},
        FONT_SMALL, st->theme->text_muted);

    /* Recent actions */
    if (hist_count > 0) {
        float hy = tip_y + GS(82);
        DrawRectangle((int)inner.x, (int)hy, (int)inner.width, 1, st->theme->border);
        gui_text("Ultimas acciones:", inner.x, hy + GS(6),
                 FONT_SMALL, st->theme->text_secondary);
        int show = hist_count > 5 ? 5 : hist_count;
        for (int i = 0; i < show; i++) {
            gui_text_truncated(history[i],
                     inner.x + GS(8), hy + GS(22) + (float)i * (FONT_SMALL + GS(4)),
                     FONT_SMALL, inner.width - GS(16), st->theme->text_muted);
        }
    }
}

/* ═══════════════════════════════════════════════════════════
   Cola de eventos
   ═══════════════════════════════════════════════════════════ */

void gui_evt_push(GuiEventQueue *q, GuiEventType type, int param)
{
    if (!q || q->count >= GUI_EVENT_CAP) return;
    q->buf[q->count].type  = type;
    q->buf[q->count].param = param;
    q->count++;
}

int gui_evt_has(const GuiEventQueue *q, GuiEventType type)
{
    if (!q) return 0;
    for (int i = 0; i < q->count; i++) {
        if (q->buf[i].type == type) return 1;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════
   Ciclo de vida del estado
   ═══════════════════════════════════════════════════════════ */

void gui_state_init(GuiState *st, const MenuItem *items, int count, int initial_selected)
{
    memset(st, 0, sizeof(*st));
    st->items      = items;
    st->item_count = count;
    st->selected_global = (initial_selected >= 0 && initial_selected < count)
                              ? initial_selected : 0;
    st->focus          = FOCUS_LIST;
    st->active_filter  = GUI_FILTER_ALL;
    st->current_screen = GUI_SCREEN_HOME;
    st->theme          = NULL;
    st->scale          = 1.0f;

     st->scroll_anim.speed = 10.0f;
    st->scroll_velocity = 0.0f;
     /* Hacer la animacion de seleccion y transicion más suaves
         reduciendo ligeramente la velocidad (más easing perceptible) */
     st->row_highlight.speed = 9.0f;
     st->transition.speed = 6.5f;
    gui_anim_snap(&st->transition, 1.0f);
    st->prev_screen = GUI_SCREEN_HOME;
    st->debug_mode = 0;
    st->theme_index = 0;
    st->variant_index = 0;
    st->click_flash_row = -1;

    gui_update_active_theme(st);

    st->visible_capacity = count;
    st->visible_indices  = (int *)malloc((size_t)count * sizeof(int));

    snprintf(st->status_line,  sizeof(st->status_line),  "Listo");
    snprintf(st->last_action,  sizeof(st->last_action),  "Ninguna");
}

void gui_state_cleanup(GuiState *st)
{
    if (!st) return;
    free(st->visible_indices);
    st->visible_indices = NULL;
}

void gui_state_push_history(GuiState *st, const char *text)
{
    if (!st) return;
    for (int i = 4; i > 0; i--)
        memmove(st->history[i], st->history[i - 1], 96);
    snprintf(st->history[0], 96, "%s", text ? text : "(sin accion)");
    if (st->history_count < 5) st->history_count++;
}

void gui_state_set_toast(GuiState *st, const char *text, float duration)
{
    if (!st) return;
    snprintf(st->toast_text, sizeof(st->toast_text), "%s", text ? text : "");
    st->toast_timer = duration;
}

void gui_state_change_screen(GuiState *st, GuiScreen new_screen)
{
    if (!st || st->current_screen == new_screen) return;
    st->prev_screen = st->current_screen;
    st->current_screen = new_screen;
    gui_anim_snap(&st->transition, 0.0f);
    gui_anim_set_target(&st->transition, 1.0f);
}

/* ═══════════════════════════════════════════════════════════
   Input de búsqueda
   ═══════════════════════════════════════════════════════════ */

int gui_input_search(char *query, int max_len, int focused)
{
    int changed = 0;
    if (!focused) return 0;

    int key = GetCharPressed();
    while (key > 0) {
        int len = (int)strlen_s(query, (size_t)max_len);
        if (len < max_len - 1 && key >= 32 && key <= 126) {
            query[len]     = (char)key;
            query[len + 1] = '\0';
            changed = 1;
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        int len = (int)strlen_s(query, (size_t)max_len);
        if (len > 0) {
            query[len - 1] = '\0';
            changed = 1;
        }
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (query[0] != '\0') changed = 1;
        query[0] = '\0';
    }

    return changed;
}
