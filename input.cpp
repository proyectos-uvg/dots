/**
 * @file input.cpp
 * @brief Lógica de interacción del jugador.
 */

#include "input.h"
#include "sync.h"
#include "board.h"

#include <ncurses.h>
#include <vector>
#include <cstdlib>

static int cursor_row = 0;
static int cursor_col = 0;

/**
 * Lista de puntos seleccionados.
 */
static std::vector<SelectedPoint> selection;

/**
 * Color actual de la cadena.
 */
static int current_color = -1;

/**
 * Verifica adyacencia ortogonal.
 */
bool is_adjacent(int r1, int c1, int r2, int c2) {

    int dr = abs(r1 - r2);
    int dc = abs(c1 - c2);

    return (dr + dc) == 1;
}

/**
 * Verifica si ya existe un punto en la cadena.
 */
bool already_selected(const std::vector<SelectedPoint>& path,
                      int row,
                      int col) {

    for (const auto& p : path) {

        if (p.row == row && p.col == col) {
            return true;
        }
    }

    return false;
}

/**
 * Detecta ciclo cerrado.
 */
bool detect_cycle(const std::vector<SelectedPoint>& path) {

    if (path.size() < 4) {
        return false;
    }

    SelectedPoint last = path.back();

    int count = 0;

    for (const auto& p : path) {

        if (p.row == last.row &&
            p.col == last.col) {

            count++;
        }
    }

    return count >= 2;
}

/**
 * Elimina puntos seleccionados.
 */
static void remove_selection(void) {

    pthread_mutex_lock(&board_mutex);

    bool cycle = detect_cycle(selection);

    int target_color = current_color;

    if (cycle) {

        for (int r = 0; r < BOARD_SIZE; r++) {

            for (int c = 0; c < BOARD_SIZE; c++) {

                if (g_state.board[r][c] == target_color) {
                    g_state.board[r][c] = -1;
                }
            }
        }

    } else {

        for (const auto& p : selection) {
            g_state.board[p.row][p.col] = -1;
        }
    }

    g_state.moves_remaining--;

    pthread_mutex_unlock(&board_mutex);

    selection.clear();
    current_color = -1;
}

/**
 * Hilo principal de entrada.
 */
void* input_thread(void* arg) {

    (void)arg;

    while (g_state.game_status == STATUS_RUNNING) {

        int ch = getch();

        switch (ch) {

            case KEY_UP:

                if (cursor_row > 0)
                    cursor_row--;

                break;

            case KEY_DOWN:

                if (cursor_row < BOARD_SIZE - 1)
                    cursor_row++;

                break;

            case KEY_LEFT:

                if (cursor_col > 0)
                    cursor_col--;

                break;

            case KEY_RIGHT:

                if (cursor_col < BOARD_SIZE - 1)
                    cursor_col++;

                break;

            case ' ':
            {
                pthread_mutex_lock(&board_mutex);

                int color =
                    g_state.board[cursor_row][cursor_col];

                pthread_mutex_unlock(&board_mutex);

                if (color == -1)
                    break;

                if (selection.empty()) {

                    selection.push_back(
                        {cursor_row, cursor_col});

                    current_color = color;

                } else {

                    SelectedPoint last = selection.back();

                    if (!is_adjacent(last.row,
                                     last.col,
                                     cursor_row,
                                     cursor_col)) {

                        break;
                    }

                    if (color != current_color) {
                        break;
                    }

                    selection.push_back(
                        {cursor_row, cursor_col});
                }

                break;
            }

            case '\n':

                if (selection.size() >= 2) {

                    remove_selection();

                    pthread_cond_broadcast(&board_updated);
                }

                break;

            case 'q':

                g_state.game_status = STATUS_LOST;
                return NULL;
        }

        render_board();

        /*
         * Dibujar cursor
         */

        int draw_row = 6 + cursor_row;
        int draw_col = 6 + cursor_col * 4;

        attron(A_REVERSE | A_BOLD);
        mvaddch(draw_row, draw_col, ACS_BLOCK);
        attroff(A_REVERSE | A_BOLD);

        refresh();
    }

    return NULL;
}