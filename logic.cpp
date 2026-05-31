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
#include "game_config.h"
#include "sync.h"
#include "board.h"
#include "screens.h"
#include "game_state.h"
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
            if (g_state.board[r][col].color != -1 &&
                g_state.board[r + 1][col].color == -1 &&
                g_state.board[r + 1][col].type != OBSTACLE)
            {
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
        if (g_state.board[r][col].color == -1 && g_state.board[r][col].type != OBSTACLE) {
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
    bool won = false;

    if (g_state.challenge_mode) {
        switch (g_state.goal_type) {
        case GOAL_SCORE:
            won = (g_state.score >= g_state.score_goal);
            break;
        case GOAL_COLOR_ELIM:
            won = (g_state.color_eliminated >= g_state.color_goal);
            break;
        case GOAL_CYCLES:
            won = (g_state.cycles_formed >= g_state.cycles_target);
            break;
        case GOAL_COMBO:
            won = (g_state.score >= g_state.score_goal &&
                   g_state.cycles_formed >= g_state.cycles_target);
            break;
        }
    } else {
        won = (g_state.score >= g_state.score_goal);
    }

    if (won) {
        g_state.game_status = STATUS_WON;
    } else if (g_state.moves_remaining <= 0) {
        g_state.game_status = STATUS_LOST;
    } else {
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

    if (is_cycle && g_state.challenge_mode)
        g_state.cycles_formed++;

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
                if (t == NORMAL || t == OBSTACLE)
                    continue;

                g_state.board[r][c].type = NORMAL;
                changed = true;

                if (t == BOMB) {
                    for (int dr = -1; dr <= 1; dr++)
                        for (int dc = -1; dc <= 1; dc++) {
                            int nr = r + dr, nc = c + dc;
                            if (nr >= 0 && nr < BOARD_SIZE &&
                                nc >= 0 && nc < BOARD_SIZE &&
                                g_state.board[nr][nc].type != OBSTACLE)
                            {
                                if (g_state.challenge_mode &&
                                    g_state.goal_type == GOAL_COLOR_ELIM &&
                                    g_state.board[nr][nc].color == g_state.color_target)
                                    g_state.color_eliminated++;
                                g_state.board[nr][nc].color = -1;
                            }
                        }
                }
                else if (t == CROSS) {
                    for (int i = 0; i < BOARD_SIZE; i++) {
                        if (g_state.board[r][i].type != OBSTACLE) {
                            if (g_state.challenge_mode &&
                                g_state.goal_type == GOAL_COLOR_ELIM &&
                                g_state.board[r][i].color == g_state.color_target)
                                g_state.color_eliminated++;
                            g_state.board[r][i].color = -1;
                        }
                    }
                    for (int i = 0; i < BOARD_SIZE; i++) {
                        if (g_state.board[i][c].type != OBSTACLE) {
                            if (g_state.challenge_mode &&
                                g_state.goal_type == GOAL_COLOR_ELIM &&
                                g_state.board[i][c].color == g_state.color_target)
                                g_state.color_eliminated++;
                            g_state.board[i][c].color = -1;
                        }
                    }
                }
                else if (t == MULTIPLIER) {
                    has_multiplier = true;
                }
            }
        }
    }

    /* Contar huecos y columnas afectadas (excluir obstáculos) */

    int total_removed = 0;
    bool affected[BOARD_SIZE] = {};

    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (g_state.board[r][c].color == -1 && g_state.board[r][c].type != OBSTACLE) {
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
 * Espera en el semáforo input_ready hasta que input_thread señalice
 * que el jugador confirmó una jugada (sem_post).  Cada post representa
 * exactamente un movimiento pendiente de procesar; no hay wakeups
 * espurios ni mutex auxiliar innecesario.
 *
 * Flujo de señalización:
 *   input_thread  →  sem_post(&input_ready)   (jugada confirmada)
 *   game_loop_thread  →  sem_wait(&input_ready)   (despierta, procesa)
 *   main()        →  sem_post(&input_ready)   (al salir, para desbloquear)
 */
void* game_loop_thread(void* arg) {
    (void)arg;

    while (true) {
        /*
         * sem_wait decrementa el contador del semáforo.
         * Si es 0, bloquea el hilo hasta que input_thread (o main)
         * haga sem_post.  No requiere mutex auxiliar.
         */
        sem_wait(&input_ready);

        if (g_state.current_screen == SCREEN_EXIT)
            break;

        if (g_state.current_screen != SCREEN_PLAYING ||
            g_state.game_status != STATUS_RUNNING)
        {
            continue;
        }
 
        /*
         * Verificar huecos y leer pending_cycle atomicamente bajo board_mutex.
         */
        bool has_holes = false;
        bool is_cycle  = false;
        pthread_mutex_lock(&board_mutex);
        for (int r = 0; r < BOARD_SIZE && !has_holes; r++)
            for (int c = 0; c < BOARD_SIZE && !has_holes; c++)
                if (g_state.board[r][c].color == -1 && g_state.board[r][c].type != OBSTACLE)
                    has_holes = true;
        is_cycle = g_state.pending_cycle;
        g_state.pending_cycle = false;
        pthread_mutex_unlock(&board_mutex);

        if (!has_holes)
            continue;

        std::vector<SelectedPoint> empty_path;
        process_move(empty_path, is_cycle);

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
 
    return NULL;
}