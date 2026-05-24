/**
 * @file input.cpp
 * @brief Lógica de interacción del jugador y navegación entre pantallas.
 */

#include "input.h"
#include "sync.h"
#include "board.h"
#include "screens.h"
#include "scores.h"

#include <ncurses.h>
#include <vector>
#include <cstdlib>
#include <cstring>

static int cursor_row = 0;
static int cursor_col = 0;

static std::vector<SelectedPoint> selection;
static int current_color = -1;

/* ------------------------------------------------------------------ */
/* Utilidades de gameplay (sin cambios de lógica)                      */
/* ------------------------------------------------------------------ */

bool is_adjacent(int r1, int c1, int r2, int c2)
{
    int dr = abs(r1 - r2);
    int dc = abs(c1 - c2);
    return (dr + dc) == 1;
}

bool already_selected(const std::vector<SelectedPoint>& path,
                      int row,
                      int col)
{
    for (const auto& p : path) {
        if (p.row == row && p.col == col)
            return true;
    }
    return false;
}

bool detect_cycle(const std::vector<SelectedPoint>& path)
{
    if (path.size() < 4)
        return false;

    SelectedPoint last = path.back();
    int count = 0;

    for (const auto& p : path) {
        if (p.row == last.row && p.col == last.col)
            count++;
    }

    return count >= 2;
}

static void remove_selection(void)
{
    pthread_mutex_lock(&board_mutex);

    bool cycle = detect_cycle(selection);
    int target_color = current_color;

    if (cycle) {
        for (int r = 0; r < BOARD_SIZE; r++) {
            for (int c = 0; c < BOARD_SIZE; c++) {
                if (g_state.board[r][c] == target_color)
                    g_state.board[r][c] = -1;
            }
        }
    } else {
        for (const auto& p : selection)
            g_state.board[p.row][p.col] = -1;
    }

    g_state.moves_remaining--;

    pthread_mutex_unlock(&board_mutex);

    selection.clear();
    current_color = -1;
}

static void reset_playing_cursor(void)
{
    cursor_row = 0;
    cursor_col = 0;
    selection.clear();
    current_color = -1;
}

static GameMode mode_from_index(int index)
{
    return (index == 0) ? SLOW : FAST;
}

static void start_game_from_mode_select(void)
{
    g_state.game_mode = mode_from_index(g_state.mode_index);
    init_board();
    reset_playing_cursor();
    g_state.current_screen = SCREEN_PLAYING;
}

static void go_to_game_over(void)
{
    pthread_mutex_lock(&board_mutex);

    if (g_state.current_screen == SCREEN_PLAYING) {
        scores_prepare_match_end(g_state.score);
        g_state.current_screen = SCREEN_GAME_OVER;
    }

    pthread_mutex_unlock(&board_mutex);
}

static void draw_playing_overlays(void)
{
    for (const auto& sp : selection) {
        int color = g_state.board[sp.row][sp.col];
        if (color >= 0 && color < NUM_COLORS) {
            attron(COLOR_PAIR(color + 1) | A_BOLD | A_REVERSE);
            mvaddch(6 + sp.row, 6 + sp.col * 4, ACS_BLOCK);
            attroff(COLOR_PAIR(color + 1) | A_BOLD | A_REVERSE);
        }
    }

    attron(A_REVERSE | A_BOLD);
    mvaddch(6 + cursor_row, 6 + cursor_col * 4, ACS_BLOCK);
    attroff(A_REVERSE | A_BOLD);
}

static void redraw(void)
{
    pthread_mutex_lock(&render_mutex);

    if (g_state.current_screen == SCREEN_PLAYING) {
        clear();
        render_playing();
        if (g_state.game_status == STATUS_RUNNING)
            draw_playing_overlays();
        refresh();
    } else {
        render_screen();
    }

    pthread_mutex_unlock(&render_mutex);
}

/* ------------------------------------------------------------------ */
/* Handlers por pantalla                                               */
/* ------------------------------------------------------------------ */

static bool handle_menu_input(int ch)
{
    switch (ch) {

    case KEY_UP:
        if (g_state.menu_index > 0)
            g_state.menu_index--;
        return true;

    case KEY_DOWN:
        if (g_state.menu_index < MENU_ITEM_COUNT - 1)
            g_state.menu_index++;
        return true;

    case '\n':
        switch (g_state.menu_index) {

        case 0:
            g_state.mode_index = 0;
            g_state.current_screen = SCREEN_MODE_SELECT;
            break;

        case 1:
            g_state.current_screen = SCREEN_INSTRUCTIONS;
            break;

        case 2:
            load_scores();
            g_state.current_screen = SCREEN_SCORES;
            break;

        case 3:
            g_state.current_screen = SCREEN_EXIT;
            return false;

        default:
            break;
        }
        return true;

    default:
        return true;
    }
}

static bool handle_instructions_input(int ch)
{
    if (ch == 27 || ch == 'b' || ch == 'B') {
        g_state.current_screen = SCREEN_MENU;
        return true;
    }
    return true;
}

static bool handle_mode_select_input(int ch)
{
    switch (ch) {

    case KEY_UP:
        if (g_state.mode_index > 0)
            g_state.mode_index--;
        return true;

    case KEY_DOWN:
        if (g_state.mode_index < MODE_ITEM_COUNT - 1)
            g_state.mode_index++;
        return true;

    case '\n':
        start_game_from_mode_select();
        return true;

    case 27:
    case 'b':
    case 'B':
        g_state.current_screen = SCREEN_MENU;
        return true;

    default:
        return true;
    }
}

static bool handle_scores_input(int ch)
{
    if (ch == '\n' || ch == 27) {
        g_state.current_screen = SCREEN_MENU;
        return true;
    }
    return true;
}

static void commit_player_name(void)
{
    char name[PLAYER_NAME_MAX];

    if (g_state.player_name_len == 0) {
        strncpy(name, "Jugador", sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    } else {
        memcpy(name, g_state.player_name_input, (size_t)g_state.player_name_len);
        name[g_state.player_name_len] = '\0';
    }

    scores_commit_with_name(name, g_state.last_score);
}

static bool handle_game_over_input(int ch)
{
    if (g_state.score_saved) {
        if (ch == '\n' || ch == 27) {
            g_state.game_status = STATUS_RUNNING;
            g_state.current_screen = SCREEN_MENU;
            g_state.menu_index = 0;
        }
        return true;
    }

    if (ch == '\n') {
        commit_player_name();
        return true;
    }

    if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
        if (g_state.player_name_len > 0)
            g_state.player_name_input[--g_state.player_name_len] = '\0';
        return true;
    }

    if (ch >= 32 && ch < 127 &&
        g_state.player_name_len < PLAYER_NAME_MAX - 1)
    {
        g_state.player_name_input[g_state.player_name_len++] = (char)ch;
        g_state.player_name_input[g_state.player_name_len]   = '\0';
    }

    return true;
}

static bool handle_playing_input(int ch)
{
    bool needs_broadcast = false;

    if (g_state.game_status != STATUS_RUNNING) {
        go_to_game_over();
        return true;
    }

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

    case KEY_BACKSPACE:
        if (!selection.empty()) {
            selection.pop_back();
            if (selection.empty())
                current_color = -1;
        }
        break;

    case 27:
        selection.clear();
        current_color = -1;
        break;

    case ' ':
    {
        pthread_mutex_lock(&board_mutex);
        int color = g_state.board[cursor_row][cursor_col];
        pthread_mutex_unlock(&board_mutex);

        if (color == -1)
            break;

        if (selection.empty()) {
            selection.push_back({cursor_row, cursor_col});
            current_color = color;
        } else {
            SelectedPoint last  = selection.back();
            SelectedPoint first = selection.front();

            if (!is_adjacent(last.row, last.col, cursor_row, cursor_col))
                break;

            if (color != current_color)
                break;

            bool is_first = (cursor_row == first.row &&
                             cursor_col == first.col);

            if (already_selected(selection, cursor_row, cursor_col)) {
                if (is_first && (int)selection.size() >= 3)
                    selection.push_back({cursor_row, cursor_col});
                break;
            }

            selection.push_back({cursor_row, cursor_col});
        }
        break;
    }

    case '\n':
        if (selection.size() >= 2) {
            remove_selection();
            needs_broadcast = true;
        }
        break;

    case 'q':
        g_state.game_status = STATUS_LOST;
        go_to_game_over();
        break;

    default:
        break;
    }

    if (g_state.game_status != STATUS_RUNNING &&
        g_state.current_screen == SCREEN_PLAYING)
    {
        go_to_game_over();
    }

    if (needs_broadcast)
        pthread_cond_broadcast(&board_updated);

    return true;
}

/* ------------------------------------------------------------------ */
/* Hilo de entrada                                                     */
/* ------------------------------------------------------------------ */

void* input_thread(void* arg)
{
    (void)arg;

    while (g_state.current_screen != SCREEN_EXIT) {

        int ch = getch();
        bool keep_running = true;

        switch (g_state.current_screen) {

        case SCREEN_MENU:
            keep_running = handle_menu_input(ch);
            break;

        case SCREEN_INSTRUCTIONS:
            keep_running = handle_instructions_input(ch);
            break;

        case SCREEN_MODE_SELECT:
            keep_running = handle_mode_select_input(ch);
            break;

        case SCREEN_PLAYING:
            keep_running = handle_playing_input(ch);
            break;

        case SCREEN_SCORES:
            keep_running = handle_scores_input(ch);
            break;

        case SCREEN_GAME_OVER:
            keep_running = handle_game_over_input(ch);
            break;

        default:
            break;
        }

        if (!keep_running)
            break;

        redraw();
    }

    return NULL;
}
