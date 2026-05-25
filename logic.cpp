/**
 * @file logic.cpp
 * @brief Implementación del motor de lógica: puntuación, gravedad,
 *        relleno y hilo de actualización dinámica del tablero.
 *
 * Se engancha al flujo existente sin modificar ningún archivo original:
 *
 *   input_thread llama remove_selection() → marca celdas como -1
 *                → pthread_cond_broadcast(&board_updated)
 *
 *   game_loop_thread despierta → process_move() → render_board()
 */
 
#include "logic.h"
#include "sync.h"
#include "board.h"
#include "screens.h"
#include "game_state.h"
#include "game_config.h"
#include "scores.h"

#include <ncurses.h>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
 
/* ------------------------------------------------------------------ */
/* Variables internas del hilo de juego                                */
/* ------------------------------------------------------------------ */
 
/**
 * Último snapshot de la cadena confirmada por input_thread.
 * Se lee desde game_loop_thread bajo board_mutex.
 *
 * input_thread ya expone `selection`, `current_color` y `detect_cycle`
 * pero son estáticos. Para no modificar input.cpp, game_loop_thread
 * deduce la información directamente del tablero (celdas == -1) y del
 * broadcast recibido. Ver process_move() para el detalle.
 */
 
/* ------------------------------------------------------------------ */
/* calculate_score                                                      */
/* ------------------------------------------------------------------ */
 
int calculate_score(int chain_len, bool is_cycle, int extras)
{
    int base = chain_len * BASE_POINTS_PER_DOT;
 
    if (!is_cycle)
        return base;
 
    return (base + extras * CYCLE_BONUS_PER_EXTRA) * CYCLE_MULTIPLIER;
}
 
/* ------------------------------------------------------------------ */
/* apply_gravity_col                                                    */
/* ------------------------------------------------------------------ */
 
void apply_gravity_col(int col) {
    unsigned int delay = get_tick_delay(g_state.game_mode);

    /*
     * Animación de caída paso a paso.
     * Cada iteración baja todos los puntos de la columna una fila
     * si la celda inferior está vacía (-1).
     * Paramos cuando ningún punto se movió en el último paso.
     */
    bool moved = true;

    while (moved) {
        moved = false;

        for (int r = BOARD_SIZE - 2; r >= 0; r--) {
            if (g_state.board[r][col].color != -1 && g_state.board[r + 1][col].color == -1) {
                g_state.board[r + 1][col] = g_state.board[r][col];
                g_state.board[r][col].color = -1;
                g_state.board[r][col].type  = NORMAL;
                moved = true;
            }
        }

        if (moved) {
            pthread_mutex_unlock(&board_mutex);
            pthread_mutex_lock(&render_mutex);
            render_board();
            pthread_mutex_unlock(&render_mutex);
            usleep(delay);
            pthread_mutex_lock(&board_mutex);
        }
    }
}
 
/* ------------------------------------------------------------------ */
/* fill_column                                                          */
/* ------------------------------------------------------------------ */
 
void fill_column(int col) {
    for (int r = 0; r < BOARD_SIZE; r++) {
        if (g_state.board[r][col].color == -1) {
            g_state.board[r][col].color = rand() % g_state.num_colors;
            if (g_state.special_enabled && rand() % 10 == 0)
                g_state.board[r][col].type = (CellType)(1 + rand() % 3);
            else
                g_state.board[r][col].type = NORMAL;
        }
    }
}
 
/* ------------------------------------------------------------------ */
/* check_game_over                                                      */
/* ------------------------------------------------------------------ */
 
void check_game_over(void)
{
    if (g_state.score >= g_state.score_goal)
    {
        g_state.game_status = STATUS_WON;
    }
    else if (g_state.moves_remaining <= 0)
    {
        g_state.game_status = STATUS_LOST;
    }
    else
    {
        return;
    }

    scores_prepare_match_end(g_state.score);
    g_state.current_screen = SCREEN_GAME_OVER;
}
 
/* ------------------------------------------------------------------ */
/* process_move                                                         */
/* ------------------------------------------------------------------ */
 
/**
 * Recibe la cadena que input_thread ya validó y cuyas celdas ya fueron
 * marcadas como -1 por remove_selection().
 *
 * Trabajo que queda:
 *   1. Contar extras (solo en ciclo, todos los -1 del tablero que
 *      estén fuera del path original ya fueron borrados por
 *      remove_selection en modo ciclo, así que contamos los huecos).
 *   2. Calcular y acumular puntaje.
 *   3. Gravedad + relleno por columna afectada.
 *   4. Evaluar fin de partida.
 */
void process_move(const std::vector<SelectedPoint>& path,
                  bool is_cycle) {
    pthread_mutex_lock(&board_mutex);

    /* Expandir efectos de celdas especiales */

    bool has_multiplier = false;
    bool changed = true;

    while (changed) {
        changed = false;

        for (int r = 0; r < BOARD_SIZE; r++) {
            for (int c = 0; c < BOARD_SIZE; c++) {
                if (g_state.board[r][c].color != -1)
                    continue;

                CellType t = g_state.board[r][c].type;
                if (t == NORMAL)
                    continue;

                g_state.board[r][c].type = NORMAL;
                changed = true;

                if (t == BOMB) {
                    for (int dr = -1; dr <= 1; dr++)
                        for (int dc = -1; dc <= 1; dc++) {
                            int nr = r + dr, nc = c + dc;
                            if (nr >= 0 && nr < BOARD_SIZE &&
                                nc >= 0 && nc < BOARD_SIZE)
                                g_state.board[nr][nc].color = -1;
                        }
                }
                else if (t == CROSS) {
                    for (int i = 0; i < BOARD_SIZE; i++) {
                        g_state.board[r][i].color = -1;
                        g_state.board[i][c].color = -1;
                    }
                }
                else if (t == MULTIPLIER) {
                    has_multiplier = true;
                }
            }
        }
    }

    /* Contar huecos y columnas afectadas */

    int total_removed = 0;
    bool affected[BOARD_SIZE] = {};

    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (g_state.board[r][c].color == -1) {
                total_removed++;
                affected[c] = true;
            }
        }
    }

    int chain_len = path.empty() ? total_removed : (int)path.size();
    int extras = (is_cycle) ? (total_removed - chain_len) : 0;
    if (extras < 0) extras = 0;

    /* Calcular puntaje */

    int gained = calculate_score(chain_len, is_cycle, extras);
    if (has_multiplier) gained *= 2;

    pthread_mutex_lock(&score_mutex);
    g_state.score += gained;
    pthread_mutex_unlock(&score_mutex);

    /* Gravedad y relleno */

    for (int c = 0; c < BOARD_SIZE; c++) {
        if (affected[c]) {
            apply_gravity_col(c);
            fill_column(c);
        }
    }

    /* Fin de partida */

    check_game_over();

    pthread_mutex_unlock(&board_mutex);
}
 
/* ------------------------------------------------------------------ */
/* game_loop_thread                                                     */
/* ------------------------------------------------------------------ */
 
/**
 * Se engancha a board_updated, la misma variable de condición que
 * input_thread ya señaliza con pthread_cond_broadcast tras cada
 * jugada confirmada (línea existente en input.cpp, case '\n').
 *
 * Para saber si hay una jugada real que procesar (y no un broadcast
 * espurio), verifica si existen celdas -1 en el tablero.
 *
 * Limitación conocida: necesita que input_thread exponga la cadena
 * y el color de la última jugada. Como no podemos modificar input.cpp,
 * reconstruimos lo mínimo: contamos los huecos y usamos is_cycle=false
 * como default conservador (remove_selection ya borró todo lo correcto).
 * Si en el futuro se expone una estructura compartida desde input.cpp,
 * process_move() puede recibir los datos exactos.
 */
void* game_loop_thread(void* arg)
{
    (void)arg;
 
    /*
     * Mutex auxiliar requerido por pthread_cond_wait.
     * No protege datos propios; solo satisface la API de pthreads.
     */
    pthread_mutex_t wait_mutex = PTHREAD_MUTEX_INITIALIZER;
 
    while (g_state.current_screen != SCREEN_EXIT)
    {
        pthread_mutex_lock(&wait_mutex);
        pthread_cond_wait(&board_updated, &wait_mutex);
        pthread_mutex_unlock(&wait_mutex);

        if (g_state.current_screen != SCREEN_PLAYING ||
            g_state.game_status != STATUS_RUNNING)
        {
            continue;
        }
 
        /*
         * Verificar si hay huecos que procesar.
         * (Evita actuar sobre broadcasts que no corresponden a jugadas.)
         */
        bool has_holes = false;
        pthread_mutex_lock(&board_mutex);
        for (int r = 0; r < BOARD_SIZE && !has_holes; r++)
            for (int c = 0; c < BOARD_SIZE && !has_holes; c++)
                if (g_state.board[r][c].color == -1)
                    has_holes = true;
        pthread_mutex_unlock(&board_mutex);
 
        if (!has_holes)
            continue;
 
        /*
         * path vacío y is_cycle=false: process_move detecta los huecos
         * directamente del tablero (ya marcados por remove_selection).
         * chain_len se recalcula desde total_removed en ese caso.
         */
        std::vector<SelectedPoint> empty_path;
        process_move(empty_path, false);

        pthread_mutex_lock(&render_mutex);
        if (g_state.current_screen == SCREEN_GAME_OVER) {
            render_screen();
        } else if (g_state.current_screen == SCREEN_PLAYING) {
            clear();
            render_playing();
            refresh();
        }
        pthread_mutex_unlock(&render_mutex);
    }
 
    pthread_mutex_destroy(&wait_mutex);
    return NULL;
}