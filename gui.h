#ifndef GUI_H
#define GUI_H

#include "menu.h"

typedef enum
{
    GUI_ACTION_EXIT = 0,
    GUI_ACTION_OPEN_CLASSIC_MENU = 1,
    GUI_ACTION_RUN_SELECTED_OPTION = 2
} GuiAction;

/*
 * Ejecuta una interfaz grafica basica con Raylib.
 *
 * Retorna:
 * GUI_ACTION_EXIT -> salir de la aplicacion
 * GUI_ACTION_OPEN_CLASSIC_MENU -> continuar en menu de consola tradicional
 * GUI_ACTION_RUN_SELECTED_OPTION -> ejecutar opcion seleccionada y volver a GUI
 */
int run_raylib_gui(const MenuItem *items, int count);
int gui_get_last_selected_index(void);
int gui_is_compiled(void);
void gui_set_context_title(const char *title);
void gui_request_escape_cooldown(void);

#endif
