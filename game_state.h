/**
 * @file game_state.h
 * @brief Definiciones del estado global y estructuras compartidas del juego.
 */

#ifndef GAME_STATE_H
#define GAME_STATE_H

/**
 * @brief Dimensión del tablero del juego.
 */
#define BOARD_SIZE 6

/**
 * @brief Cantidad total de colores disponibles.
 */
#define NUM_COLORS 5

/**
 * @brief Cantidad inicial de movimientos por partida.
 */
#define MAX_MOVES 30

/**
 * @brief Estado que indica que la partida sigue en ejecución.
 */
#define STATUS_RUNNING 0

/**
 * @brief Estado que indica que el jugador ganó la partida.
 */
#define STATUS_WON 1

/**
 * @brief Estado que indica que el jugador perdió la partida.
 */
#define STATUS_LOST 2

/**
 * @brief Modo de juego lento.
 */
#define MODE_SLOW 1

/**
 * @brief Modo de juego rápido.
 */
#define MODE_FAST 2

/**
 * @brief Índice lógico del color rojo.
 */
#define COLOR_IDX_RED 0

/**
 * @brief Índice lógico del color azul.
 */
#define COLOR_IDX_BLUE 1

/**
 * @brief Índice lógico del color verde.
 */
#define COLOR_IDX_GREEN 2

/**
 * @brief Índice lógico del color amarillo.
 */
#define COLOR_IDX_YELLOW 3

/**
 * @brief Índice lógico del color magenta.
 */
#define COLOR_IDX_MAGENTA 4

/**
 * @brief Representa un punto individual dentro del tablero.
 *
 * Cada punto posee:
 * - Posición por fila.
 * - Posición por columna.
 * - Índice lógico de color.
 */
typedef struct {

    /**
     * @brief Fila del punto dentro del tablero.
     */
    int row;

    /**
     * @brief Columna del punto dentro del tablero.
     */
    int col;

    /**
     * @brief Índice lógico de color del punto.
     */
    int color;

} Point;

/**
 * @brief Representa el estado completo de la partida.
 *
 * Esta estructura es compartida entre múltiples hilos,
 * por lo que el acceso concurrente debe protegerse
 * mediante los mutex definidos en sync.h.
 */
typedef struct {

    /**
     * @brief Matriz principal del tablero.
     *
     * Cada celda almacena un índice lógico de color.
     *
     * @note Debe protegerse mediante board_mutex.
     */
    int board[BOARD_SIZE][BOARD_SIZE];

    /**
     * @brief Puntaje acumulado del jugador.
     *
     * @note Debe protegerse mediante score_mutex.
     */
    int score;

    /**
     * @brief Cantidad de movimientos restantes.
     *
     * @note Debe protegerse mediante board_mutex.
     */
    int moves_remaining;

    /**
     * @brief Velocidad actual del juego.
     *
     * Valores válidos:
     * - MODE_SLOW
     * - MODE_FAST
     */
    int game_mode;

    /**
     * @brief Estado actual de la partida.
     *
     * Valores válidos:
     * - STATUS_RUNNING
     * - STATUS_WON
     * - STATUS_LOST
     */
    int game_status;

} GameState;

/**
 * @brief Estado global compartido de la aplicación.
 *
 * Definido en main.cpp.
 */
extern GameState g_state;

#endif