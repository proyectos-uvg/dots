/**
 * @file sync.h
 * @brief Declaraciones de primitivas globales de sincronización.
 */

#ifndef SYNC_H
#define SYNC_H

#include <pthread.h>
#include <semaphore.h>

/**
 * @brief Mutex que protege el acceso al tablero y movimientos restantes.
 *
 * Recursos protegidos:
 * - g_state.board
 * - g_state.moves_remaining
 *
 * @note Debe adquirirse antes de leer o modificar dichos campos.
 */
extern pthread_mutex_t board_mutex;

/**
 * @brief Mutex que protege el acceso al puntaje del jugador.
 *
 * Recurso protegido:
 * - g_state.score
 */
extern pthread_mutex_t score_mutex;

/**
 * @brief Variable de condición utilizada para notificar
 *        actualizaciones del tablero.
 *
 * Se señaliza cuando ocurre un cambio relevante en:
 * - El tablero.
 * - El estado de la partida.
 * - Una nueva jugada.
 *
 * @note Debe utilizarse junto con board_mutex.
 */
extern pthread_cond_t board_updated;

/**
 * @brief Semáforo utilizado para sincronizar entrada de usuario
 *        y lógica del juego.
 *
 * Flujo típico:
 * - El hilo de entrada ejecuta sem_post().
 * - El hilo lógico ejecuta sem_wait().
 *
 * @note Su valor inicial es 0.
 */
extern sem_t input_ready;

/**
 * @brief Mutex que serializa todas las llamadas a ncurses.
 *
 * Debe adquirirse antes de cualquier función de ncurses
 * (render_board, attron, mvaddch, refresh, etc.) para evitar
 * que dos hilos escriban en la pantalla al mismo tiempo.
 *
 * @note Nunca adquirir board_mutex estando dentro de render_mutex.
 *       render_board() libera board_mutex antes de retornar.
 */
extern pthread_mutex_t render_mutex;

/**
 * @brief Inicializa todas las primitivas de sincronización.
 *
 * Inicializa:
 * - board_mutex
 * - score_mutex
 * - board_updated
 * - input_ready
 *
 * @note Debe ejecutarse antes de crear cualquier hilo.
 * @note El programa termina si ocurre un error de inicialización.
 */
void init_sync(void);

/**
 * @brief Libera todas las primitivas de sincronización.
 *
 * Destruye mutexes, variables de condición y semáforos
 * utilizados por la aplicación.
 *
 * @note Debe ejecutarse después de finalizar todos los hilos.
 */
void destroy_sync(void);

#endif