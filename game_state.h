/**
 * @file game_state.h
 * @brief Definiciones del estado global y estructuras compartidas del juego.
 */

#ifndef GAME_STATE_H
#define GAME_STATE_H

/**
 * @brief Dimensión del tablero del juego.
 */
#define BOARD_SIZE 10

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
 * @brief Modo de juego (velocidad y reglas de partida).
 */
typedef enum {
    SLOW,
    FAST
} GameMode;

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
 * @brief Cantidad de opciones en el menú principal.
 */
#define MENU_ITEM_COUNT 4

/**
 * @brief Cantidad de opciones en la pantalla de selección de modo.
 */
#define MODE_ITEM_COUNT 2

/**
 * @brief Longitud máxima del nombre del jugador (incluye '\0').
 */
#define PLAYER_NAME_MAX 24

/**
 * @brief Pantallas de la aplicación.
 */
typedef enum {
    SCREEN_MENU,
    SCREEN_INSTRUCTIONS,
    SCREEN_MODE_SELECT,
    SCREEN_PLAYING,
    SCREEN_SCORES,
    SCREEN_GAME_OVER,
    SCREEN_EXIT
} ScreenState;

/**
 * @brief Tipo de celda especial del tablero.
 */
typedef enum { NORMAL, BOMB, CROSS, MULTIPLIER } CellType;

/**
 * @brief Celda del tablero con color y tipo.
 */
typedef struct {
    int color;
    CellType type;
} Cell;

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
    Cell board[BOARD_SIZE][BOARD_SIZE];

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
     * @brief Modo de juego seleccionado (SLOW o FAST).
     *
     * Se asigna en MODE_SELECT antes de iniciar PLAYING.
     */
    GameMode game_mode;

    /**
     * @brief Estado actual de la partida.
     *
     * Valores válidos:
     * - STATUS_RUNNING
     * - STATUS_WON
     * - STATUS_LOST
     */
    int game_status;

    /**
     * @brief Puntaje objetivo para ganar la partida.
     *
     * Se asigna en init_board() con get_score_goal(game_mode).
     */
    int score_goal;

    /**
     * @brief Pantalla activa de la aplicación.
     *
     * @note Modificada principalmente por input_thread.
     */
    ScreenState current_screen;

    /**
     * @brief Índice resaltado en el menú principal (0 … MENU_ITEM_COUNT-1).
     */
    int menu_index;

    /**
     * @brief Índice resaltado en la selección de modo (0=lento, 1=rápido).
     */
    int mode_index;

    /**
     * @brief Mejor puntaje alcanzado en la sesión.
     */
    int high_score;

    /**
     * @brief Puntaje de la última partida finalizada.
     */
    int last_score;

    /**
     * @brief Nombre del jugador en la última partida guardada.
     */
    char last_player_name[PLAYER_NAME_MAX];

    /**
     * @brief Buffer de nombre al finalizar partida (pantalla GAME_OVER).
     */
    char player_name_input[PLAYER_NAME_MAX];

    /**
     * @brief Longitud actual de player_name_input.
     */
    int player_name_len;

    /**
     * @brief true si el puntaje ya se guardó con nombre en scores.txt.
     */
    bool score_saved;

    /**
     * @brief Si es true, el tablero incluye celdas especiales.
     */
    bool special_enabled;

} GameState;

/**
 * @brief Estado global compartido de la aplicación.
 *
 * Definido en main.cpp.
 */
extern GameState g_state;

#endif