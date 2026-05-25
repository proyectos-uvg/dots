/**
 * @file board.cpp
 * @brief Implementación de renderizado, sincronización
 *        e inicialización del tablero.
 */

#include "board.h"
#include "game_state.h"
#include "game_config.h"
#include "scores.h"
#include "sync.h"

#include <ncurses.h>
#include <cstdlib>
#include <cstdio>
#include <ctime>

/**
 * @brief Identificador del par de color rojo.
 */
#define PAIR_RED 1

/**
 * @brief Identificador del par de color azul.
 */
#define PAIR_BLUE 2

/**
 * @brief Identificador del par de color verde.
 */
#define PAIR_GREEN 3

/**
 * @brief Identificador del par de color amarillo.
 */
#define PAIR_YELLOW 4

/**
 * @brief Identificador del par de color magenta.
 */
#define PAIR_MAGENTA 5

/**
 * @brief Identificador del par de color cian.
 */
#define PAIR_CYAN 7

/**
 * @brief Identificador del par de color blanco.
 */
#define PAIR_WHITE 8

/**
 * @brief Par de color utilizado para títulos y encabezados.
 */
#define PAIR_TITLE 6

/*
 * ACS_BLOCK es parte del conjunto alternativo de ncurses (█).
 * Funciona con -lncurses estándar sin wide-char support.
 * Usar mvaddch, no mvaddstr, para caracteres ACS.
 */

/**
 * @brief Fila inicial donde comienza a renderizarse el tablero.
 */
static const int BOARD_ROW = 6;

/**
 * @brief Columna inicial donde comienza a renderizarse el tablero.
 */
static const int BOARD_COL = 6;

/**
 * @brief Espaciado horizontal entre celdas consecutivas.
 */
static const int CELL_W = 4;

/**
 * @brief Inicializa todas las primitivas de sincronización.
 *
 * Inicializa:
 * - board_mutex
 * - score_mutex
 * - board_updated
 * - input_ready
 *
 * @note El programa finaliza si ocurre un error.
 */
void init_sync(void)
{
    int err = 0;

    err |= pthread_mutex_init(&board_mutex, NULL);
    err |= pthread_mutex_init(&score_mutex, NULL);
    err |= pthread_mutex_init(&render_mutex, NULL);
    err |= pthread_cond_init(&board_updated, NULL);
    err |= sem_init(&input_ready, 0, 0);

    if (err != 0)
    {
        endwin();

        fprintf(stderr,
                "Error: no se pudo inicializar los primitivos de sincronizacion\n");

        exit(1);
    }
}

/**
 * @brief Libera todas las primitivas de sincronización.
 */
void destroy_sync(void)
{
    pthread_mutex_destroy(&board_mutex);
    pthread_mutex_destroy(&score_mutex);
    pthread_mutex_destroy(&render_mutex);
    pthread_cond_destroy(&board_updated);
    sem_destroy(&input_ready);
}

/**
 * @brief Configura los pares de colores utilizados por ncurses.
 *
 * @pre initscr() ya fue ejecutado.
 * @pre has_colors() debe retornar TRUE.
 */
void init_display(void)
{
    start_color();

    init_pair(PAIR_RED, COLOR_RED, COLOR_BLACK);
    init_pair(PAIR_BLUE, COLOR_BLUE, COLOR_BLACK);
    init_pair(PAIR_GREEN, COLOR_GREEN, COLOR_BLACK);
    init_pair(PAIR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
    init_pair(PAIR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(PAIR_CYAN, COLOR_CYAN, COLOR_BLACK);
    init_pair(PAIR_WHITE, COLOR_WHITE, COLOR_BLACK);
    init_pair(PAIR_TITLE, COLOR_WHITE, COLOR_BLACK);
}

/**
 * @brief Obtiene el par de color ncurses asociado
 *        a un color lógico del tablero.
 *
 * @param color_idx Índice lógico de color.
 *
 * @return ID del par de color correspondiente.
 */
static int color_pair_for(int color_idx)
{
    switch (color_idx)
    {

    case COLOR_IDX_RED:
        return PAIR_RED;

    case COLOR_IDX_BLUE:
        return PAIR_BLUE;

    case COLOR_IDX_GREEN:
        return PAIR_GREEN;

    case COLOR_IDX_YELLOW:
        return PAIR_YELLOW;

    case COLOR_IDX_MAGENTA:
        return PAIR_MAGENTA;

    case COLOR_IDX_CYAN:
        return PAIR_CYAN;

    case COLOR_IDX_WHITE:
        return PAIR_WHITE;

    default:
        return PAIR_TITLE;
    }
}

/**
 * @brief Inicializa el estado lógico del tablero.
 *
 * Reinicia:
 * - Puntaje.
 * - Movimientos restantes.
 * - Estado del juego.
 * - Modo de juego.
 *
 * Además, genera colores aleatorios para cada celda.
 *
 * @note El acceso concurrente se protege mediante board_mutex.
 * @post Se notifica a los hilos esperando cambios del tablero.
 */
void init_board(void)
{
    srand((unsigned int)time(NULL));

    scores_reset_match_flag();

    pthread_mutex_lock(&board_mutex);

    g_state.score           = 0;
    g_state.game_status     = STATUS_RUNNING;
    g_state.score_goal      = get_score_goal(g_state.game_mode);
    g_state.moves_remaining = get_moves_limit(g_state.game_mode);

    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            g_state.board[r][c].color = rand() % g_state.num_colors;
            if (g_state.special_enabled && rand() % 10 == 0)
                g_state.board[r][c].type = (CellType)(1 + rand() % 3);
            else
                g_state.board[r][c].type = NORMAL;
        }
    }

    pthread_mutex_unlock(&board_mutex);

    pthread_cond_broadcast(&board_updated);
}

/**
 * @brief Renderiza la pantalla de partida (sin clear ni refresh).
 */
void render_playing(void)
{
    attron(A_BOLD | COLOR_PAIR(PAIR_TITLE));

    mvaddstr(1,
             (COLS - 17) / 2,
             "=== DOTS GAME ===");

    attroff(A_BOLD | COLOR_PAIR(PAIR_TITLE));

    mvprintw(3, 2,
             "Puntaje : %d / %d",
             g_state.score,
             g_state.score_goal);

    mvprintw(4, 2,
             "Movs.   : %d / %d",
             g_state.moves_remaining,
             get_moves_limit(g_state.game_mode));

    mvprintw(3, 24,
             "Modo : %s",
             game_mode_label(g_state.game_mode));

    for (int c = 0; c < BOARD_SIZE; c++)
    {
        mvprintw(BOARD_ROW - 1,
                 BOARD_COL + c * CELL_W,
                 "%c",
                 'A' + c);
    }

    pthread_mutex_lock(&board_mutex);

    for (int r = 0; r < BOARD_SIZE; r++)
    {

        mvprintw(BOARD_ROW + r,
                 BOARD_COL - 3,
                 "%d |",
                 r + 1);

        for (int c = 0; c < BOARD_SIZE; c++) {
            if (g_state.board[r][c].color == -1) {
                mvaddch(BOARD_ROW + r, BOARD_COL + c * CELL_W, ' ');
            } else {
                int pair = color_pair_for(g_state.board[r][c].color);
                attron(COLOR_PAIR(pair) | A_BOLD);
                chtype ch;
                switch (g_state.board[r][c].type) {
                    case BOMB:
                        ch = '*';
                        break;
                    case CROSS:
                        ch = '+';
                        break;
                    case MULTIPLIER:
                        ch = '%';
                        break;
                    default:
                        ch = ACS_DIAMOND;
                        break;
                }
                mvaddch(BOARD_ROW + r, BOARD_COL + c * CELL_W, ch);
                attroff(COLOR_PAIR(pair) | A_BOLD);
            }
        }
    }

    pthread_mutex_unlock(&board_mutex);

    int status_row = BOARD_ROW + BOARD_SIZE + 2;

    switch (g_state.game_status)
    {

    case STATUS_RUNNING:

        mvprintw(status_row,
                 2,
                 "Estado: Jugando...              ");

        break;

    case STATUS_WON:

        attron(COLOR_PAIR(PAIR_GREEN) | A_BOLD);

        mvprintw(status_row,
                 2,
                 "Estado: *** GANASTE! ***        ");

        attroff(COLOR_PAIR(PAIR_GREEN) | A_BOLD);

        break;

    case STATUS_LOST:

        attron(COLOR_PAIR(PAIR_RED) | A_BOLD);

        mvprintw(status_row,
                 2,
                 "Estado: *** PERDISTE! ***       ");

        attroff(COLOR_PAIR(PAIR_RED) | A_BOLD);

        break;
    }

    mvprintw(status_row + 2,
             2,
             "Q = salir");

    mvprintw(status_row + 4,
             2,
             "Flechas = mover | ESPACIO = seleccionar | ENTER = confirmar | BACKSPACE = deshacer ultimo | ESC = cancelar todo | Q = salir");
}

void render_board(void)
{
    clear();
    render_playing();
    refresh();
}

/**
 * @brief Libera todos los recursos utilizados por el sistema.
 *
 * Destruye primitivas de sincronización y finaliza ncurses.
 */
void cleanup(void)
{
    destroy_sync();
    endwin();
}