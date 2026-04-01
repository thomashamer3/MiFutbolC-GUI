#ifndef INPUT_H
#define INPUT_H

#include "gui_defs.h"

/* Inicializa el modulo de input (llamar después de iniciar ventana/raylib) */
void input_init(void);

/* Actualiza el snapshot interno una vez por frame (debe llamarse al inicio del loop) */
void input_update(void);

/* Obtiene snapshot inmutable del frame actual */
const InputState *input_get_state(void);

/* Consulta genérica para teclas (usada para evitar IsKeyPressed disperso) */
int input_key_pressed(int key);
int input_key_down(int key);
int input_key_released(int key);

/* Consume un evento de tecla previamente detectado por frame.
   Usar para evitar que un modal interno provoque acciones en el loop
   de GUI principal cuando se devuelve al caller. Ej: ESC/ENTER. */
void input_consume_key(int key);

/* Double-click manager por target: llamar cuando hubo hit en target_id.
   Devuelve 1 si se detectó double-click para ese target. */
int input_register_click(int target_id, int hit);

/* Limpia todos los contadores de click (opcional) */
void input_clear_clicks(void);

#endif /* INPUT_H */
