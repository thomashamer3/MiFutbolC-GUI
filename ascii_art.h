/**
 * @file ascii_art.h
 * @brief Colección de artes ASCII y funciones de visualización gráfica para MiFutbolC
 *
 * Proporciona un conjunto de constantes y funciones para mejorar la interfaz visual
 * de la aplicación mediante el uso de arte ASCII y animaciones sencillas en consola.
 */

#ifndef ASCII_ART_H
#define ASCII_ART_H

#include <stdio.h>

/**
 * @brief Arte ASCII para la pantalla de bienvenida con el título del sistema
 */
#define ASCII_BIENVENIDA \
     ".___  ___.  __   _______  __    __  .___________..______     ______    __        ______ \n" \
    "|   \\/   | |  | |   ____||  |  |  | |           ||   _  \\   /  __  \\  |  |      /      |\n" \
    "|  \\  /  | |  | |  |__   |  |  |  | `---|  |----`|  |_)  | |  |  |  | |  |     |  ,----'\n" \
    "|  |\\/|  | |  | |   __|  |  |  |  |     |  |     |   _  <  |  |  |  | |  |     |  |     \n" \
    "|  |  |  | |  | |  |     |  `--'  |     |  |     |  |_)  | |  `--'  | |  `----.|  `----.\n" \
    "|__|  |__| |__| |__|      \\______/      |__|     |______/   \\______/  |_______| \\______|\n" \
    "Sistema de Gestion Futbolistica\n" \
    "MiFutbolC v4.0\n\n"

/**
 * @brief Título artístico para el módulo de Camisetas
 */
#define ASCII_CAMISETA \
    "   ____                _          _            \n" \
    "  / ___|__ _ _ __ ___ (_)___  ___| |_ __ _ ___ \n" \
    " | |   / _` | '_ ` _ \\| / __|/ _ \\ __/ _` / __|\n" \
    " | |__| (_| | | | | | | \\__ \\  __/ || (_| \\__ \\\n" \
    "  \\____\\__,_|_| |_| |_|_|___/\\___|\\__\\__,_|___/\n" \
    "                                                \n"

/**
 * @brief Título artístico para el módulo de Estadísticas
 */
#define ASCII_ESTADISTICAS \
    "  _____     _            _ _     _   _               \n" \
    " | ____|___| |_ __ _  __| (_)___| |_(_) ___ __ _ ___ \n" \
    " |  _| / __| __/ _` |/ _` | / __| __| |/ __/ _` / __|\n" \
    " | |___\\__ \\ || (_| | (_| | \\__ \\ |_| | (_| (_| \\__ \\\n" \
    " |_____|___/\\__\\__,_|\\__,_|_|___/\\__|_|\\___\\__,_|___/\n"

/**
 * @brief Título artístico general de Fútbol
 */
#define ASCII_FUTBOL \
    "  ____            _   _     _           \n" \
    " |  _ \\ __ _ _ __| |_(_) __| | ___  ___ \n" \
    " | |_) / _` | '__| __| |/ _` |/ _ \\/ __|\n" \
    " |  __/ (_| | |  | |_| | (_| | (_) \\__ \\\n" \
    " |_|   \\__,_|_|   \\__|_|\\__,_|\\___/|___/\n" \
    "                                        \n"

/**
 * @brief Título artístico para el módulo de Canchas
 */
#define ASCII_CANCHA \
    "   ____                 _               \n" \
    "  / ___|__ _ _ __   ___| |__   __ _ ___ \n" \
    " | |   / _` | '_ \\ / __| '_ \\ / _` / __|\n" \
    " | |__| (_| | | | | (__| | | | (_| \\__ \\\n" \
    "  \\____\\__,_|_| |_|\\___|_| |_|\\__,_|___/\n" \
    "                                        \n"

/**
 * @brief Título artístico para el módulo de Equipos
 */
#define ASCII_EQUIPO \
    "  _____            _                 \n" \
    " | ____|__ _ _   _(_)_ __   ___  ___ \n" \
    " |  _| / _` | | | | | '_ \\ / _ \\/ __|\n" \
    " | |__| (_| | |_| | | |_) | (_) \\__ \\\n" \
    " |_____\\__, |\\__,_|_| .__/ \\___/|___/\n" \
    "         |_|       |_|              \n"

/**
 * @brief Título artístico para el módulo de Logros y Badges
 */
#define ASCII_LOGROS \
    "  _                              \n" \
    " | |    ___   __ _ _ __ ___  ___ \n" \
    " | |   / _ \\ / _` | '__/ _ \\/ __|\n" \
    " | |__| (_) | (_| | | | (_) \\__ \\\n" \
    " |_____\\___/ \\__, |_|  \\___/|___/\n" \
    "             |___/               \n"

/**
 * @brief Título artístico para el módulo de Análisis Técnico
 */
#define ASCII_ANALISIS \
    "     _                _ _     _     \n" \
    "    / \\   _ __   __ _| (_)___(_)___ \n" \
    "   / _ \\ | '_ \\ / _` | | / __| / __|\n" \
    "  / ___ \\| | | | (_| | | \\__ \\ \\__ \\\n" \
    " /_/   \\_\\_| |_|\\__,_|_|_|___/_|___/\n" \
    "                                    \n"

/**
 * @brief Título artístico para el módulo de Lesiones
 */
#define ASCII_LESIONES \
    "  _              _                       \n" \
    " | |    ___  ___(_) ___  _ __   ___  ___ \n" \
    " | |   / _ \\/ __| |/ _ \\| '_ \\ / _ \\/ __|\n" \
    " | |__|  __/\\__ \\ | (_) | | | |  __/\\__ \\\n" \
    " |_____\\___||___/_|\\___/|_| |_|\\___||___/\n"

/**
 * @brief Título artístico para el módulo de Financiamiento
 */
#define ASCII_FINANCIAMIENTO \
    "  _____ _                        _                 _            _        \n" \
    " |  ___(_)_ __   __ _ _ __   ___(_) __ _ _ __ ___ (_) ___ _ __ | |_ ___  \n" \
    " | |_  | | '_ \\ / _` | '_ \\ / __| |/ _` | '_ ` _ \\| |/ _ \\ '_ \\| __/ _ \\ \n" \
    " |  _| | | | | | (_| | | | | (__| | (_| | | | | | | |  __/ | | | || (_) |\n" \
    " |_|   |_|_| |_|\\__,_|_| |_|\\___|_|\\__,_|_| |_| |_|_|\\___|_| |_|\\__|\\___/ \n"

/**
 * @brief Título artístico para el módulo de Exportación de Datos
 */
#define ASCII_EXPORTAR \
    "  _____                       _             \n" \
    " | ____|_  ___ __   ___  _ __| |_ __ _ _ __ \n" \
    " |  _| \\ \\/ / '_ \\ / _ \\| '__| __/ _` | '__|\n" \
    " | |___ >  <| |_) | (_) | |  | || (_| | |   \n" \
    " |_____/_/\\_\\ .__/ \\___/|_|   \\__\\__,_|_|   \n" \
    "            |_|                              \n"

/**
 * @brief Título artístico para el módulo de Importación de Datos
 */
#define ASCII_IMPORTAR \
    "  ___                            _             \n" \
    " |_ _|_ __ ___  _ __   ___  _ __| |_ __ _ _ __ \n" \
    "  | || '_ ` _ \\| '_ \\ / _ \\| '__| __/ _` | '__|\n" \
    "  | || | | | | | |_) | (_) | |  | || (_| | |   \n" \
    " |___|_| |_| |_| .__/ \\___/|_|   \\__\\__,_|_|   \n" \
               "               |_|                              \n"

/**
 * @brief Título artístico para el módulo de Torneos
 */
#define ASCII_TORNEOS \
    "  _____                               \n" \
    " |_   _|__  _ __ _ __   ___  ___  ___ \n" \
    "   | |/ _ \\| '__| '_ \\ / _ \\/ _ \\/ __|\n" \
    "   | | (_) | |  | | | |  __/ (_) \\__ \\\n" \
    "   |_|\\___/|_|  |_| |_|\\___|\\___/|___/\n"

/**
 * @brief Título artístico para el panel de Ajustes y Configuración
 */
#define ASCII_AJUSTES \
    "     _     _           _            \n" \
    "    / \\   (_)_   _ ___| |_ ___  ___ \n" \
    "   / _ \\  | | | | / __| __/ _ \\/ __|\n" \
    "  / ___ \\ | | |_| \\__ \\ ||  __/\\__ \\\n" \
    " /_/   \\_\\/ |\\__,_|___/\\__\\___||___/\n" \
    "        |__/                        \n"

/**
 * @brief Título artístico para la gestión de Temporadas
 */
#define ASCII_TEMPORADA \
    "  _____                                        _       \n" \
    " |_   _|__ _ __ ___  _ __   ___  _ __ __ _  __| | __ _ \n" \
    "   | |/ _ \\ '_ ` _ \\| '_ \\ / _ \\| '__/ _` |/ _` |/ _` |\n" \
    "   | |  __/ | | | | | |_) | (_) | | | (_| | (_| | (_| |\n" \
    "   |_|\\___|_| |_| |_| .__/ \\___/|_|  \\__,_|\\__,_|\\__,_|\n" \
    "                    |_|                                \n"

/**
 * @brief Título artístico para el módulo de Bienestar
 */
#define ASCII_BIENESTAR \
    "  ____  _                      _              \n" \
    " |  _ \\(_)                    | |             \n" \
    " | |_) |_  ___ _ __   ___  ___| |_ __ _ _ __  \n" \
    " |  _ <| |/ _ \\ '_ \\ / _ \\/ __| __/ _` | '__| \n" \
    " | |_) | |  __/ | | |  __/\\__ \\ || (_| | |    \n" \
    " |____/|_|\\___|_| |_|\\___||___/\\__\\__,_|_|    \n" \
    "                                              \n"

/**
 * @brief Título artístico para el Entrenador IA
 */
#define ASCII_ENTRENADOR_IA \
    "  _____       _                            _              ___    _    \n" \
    " | ____|_ __ | |_ _ __ ___ _ __   __ _  __| | ___  _ __  |_ _|  / \\   \n" \
    " |  _| | '_ \\| __| '__/ _ \\ '_ \\ / _` |/ _` |/ _ \\| '__|  | |  / _ \\  \n" \
    " | |___| | | | |_| | |  __/ | | | (_| | (_| | (_) | |     | | / ___ \\ \n" \
    " |_____|_| |_|\\__|_|  \\___|_| |_|\\__,_|\\__,_|\\___/|_|    |___/_/   \\_\\\n"

/**
 * @brief Genera una representación visual de una cancha de fútbol animada en consola
 *
 * Muestra el progreso de un partido simulado mediante un campo visual dinámico
 * donde un balón se desplaza según el minuto y los eventos que ocurren.
 *
 * @param minuto Minuto actual del partido (0-90+)
 * @param evento_tipo Código de evento (0=normal, 1=gol, 2=oportunidad, 3=falta)
 */
void mostrar_cancha_animada(int minuto, int evento_tipo);

/**
 * @brief Logotipo artístico de Simulación de Partido en tiempo real
 */
#define ASCII_SIMULACION \
    ".___  ___.  __   _______  __    __  .___________..______     ______    __        ______ \n" \
    "|   \\/   | |  | |   ____||  |  |  | |           ||   _  \\   /  __  \\  |  |      /      |\n" \
    "|  \\  /  | |  | |  |__   |  |  |  | `---|  |----`|  |_)  | |  |  |  | |  |     |  ,----'\n" \
    "|  |\\/|  | |  | |   __|  |  |  |  |     |  |     |   _  <  |  |  |  | |  |     |  |     \n" \
    "|  |  |  | |  | |  |     |  `--'  |     |  |     |  |_)  | |  `--'  | |  `----.|  `----.\n" \
    "|__|  |__| |__| |__|      \\______/      |__|     |______/   \\______/  |_______| \\______|\n"

#endif /* ASCII_ART_H */

