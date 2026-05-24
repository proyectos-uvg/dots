/**
 * @file screens.h
 * @brief Dispatcher de renderizado y pantallas de navegación.
 */

#ifndef SCREENS_H
#define SCREENS_H

/**
 * @brief Renderiza la pantalla activa según g_state.current_screen.
 *
 * @pre render_mutex adquirido por el llamador (si se invoca desde un hilo).
 * @note Realiza clear() y refresh() al finalizar.
 */
void render_screen(void);

#endif
