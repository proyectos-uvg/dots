/**
 * @file board.h
 * @brief Declaraciones de inicialización, renderizado y limpieza
 *        del sistema de juego.
 */

#ifndef BOARD_H
#define BOARD_H

/**
 * @brief Configura los pares de colores utilizados por ncurses.
 *
 * Registra los colores empleados para renderizar
 * los puntos y encabezados del tablero.
 *
 * @pre initscr() ya fue ejecutado.
 * @pre has_colors() debe retornar TRUE.
 *
 * @note Debe llamarse una sola vez al iniciar la aplicación.
 */
void init_display(void);

/**
 * @brief Inicializa el estado lógico del tablero.
 *
 * Genera colores aleatorios para cada celda y reinicia:
 * - Puntaje.
 * - Movimientos restantes.
 * - Modo de juego.
 * - Estado de la partida.
 *
 * @pre init_sync() ya fue ejecutado.
 *
 * @note El acceso al tablero se protege mediante board_mutex.
 * @post Se notifica a los hilos esperando actualizaciones.
 */
void init_board(void);

/**
 * @brief Renderiza la pantalla de partida (tablero y HUD).
 *
 * No realiza clear() ni refresh(); pensado para el hilo de lógica
 * y overlays de input_thread sobre SCREEN_PLAYING.
 *
 * @note El acceso al tablero se protege mediante board_mutex.
 */
void render_playing(void);

/**
 * @brief Renderiza la partida y refresca la pantalla.
 *
 * @note Usado por game_loop_thread durante animaciones de gravedad.
 */
void render_board(void);

/**
 * @brief Libera todos los recursos utilizados por el juego.
 *
 * Destruye primitivas de sincronización y finaliza ncurses,
 * restaurando el estado original de la terminal.
 *
 * @note Debe llamarse antes de finalizar el programa.
 */
void cleanup(void);

#endif