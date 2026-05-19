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
 * @brief Renderiza completamente la interfaz del juego.
 *
 * Dibuja:
 * - Encabezado principal.
 * - Puntaje y estadísticas.
 * - Etiquetas de filas y columnas.
 * - Tablero de puntos coloreados.
 * - Estado actual de la partida.
 *
 * @pre initscr() ya fue ejecutado.
 * @pre init_display() ya fue ejecutado.
 *
 * @note El acceso al tablero se protege mediante board_mutex.
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