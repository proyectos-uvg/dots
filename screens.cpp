/**
 * @file screens.cpp
 * @brief Pantallas de menú, instrucciones, puntuaciones y dispatcher.
 */

#include "screens.h"
#include "board.h"
#include "game_state.h"

#include <ncurses.h>

#define PAIR_TITLE 6

static void draw_title(const char* text, int row)
{
    int len = 0;
    for (const char* p = text; *p; p++)
        len++;

    attron(A_BOLD | COLOR_PAIR(PAIR_TITLE));
    mvaddstr(row, (COLS - len) / 2, text);
    attroff(A_BOLD | COLOR_PAIR(PAIR_TITLE));
}

static void draw_menu_item(int row, int col, const char* label, bool selected)
{
    if (selected) {
        attron(A_REVERSE | COLOR_PAIR(PAIR_TITLE));
        mvprintw(row, col, "> %s <", label);
        attroff(A_REVERSE | COLOR_PAIR(PAIR_TITLE));
    } else {
        mvprintw(row, col, "  %s", label);
    }
}

static void render_menu_screen(void)
{
    draw_title("=== DOTS GAME ===", 2);

    static const char* items[] = {
        "Start Game",
        "Instructions",
        "Scores",
        "Exit"
    };

    int start_row = 6;
    int start_col = (COLS - 20) / 2;
    if (start_col < 2)
        start_col = 2;

    for (int i = 0; i < MENU_ITEM_COUNT; i++)
        draw_menu_item(start_row + i * 2, start_col, items[i],
                       i == g_state.menu_index);

    mvprintw(start_row + MENU_ITEM_COUNT * 2 + 2, start_col,
             "Flechas = navegar | ENTER = seleccionar");
}

static void render_instructions_screen(void)
{
    draw_title("=== INSTRUCCIONES ===", 2);

    mvprintw(5, 4, "Forma cadenas de puntos adyacentes del mismo color.");
    mvprintw(6, 4, "ESPACIO: agregar punto | ENTER: confirmar jugada");
    mvprintw(7, 4, "Ciclo cerrado: elimina todos los puntos de ese color.");
    mvprintw(8, 4, "BACKSPACE: deshacer | ESC: cancelar seleccion");
    mvprintw(9, 4, "Q: abandonar partida");
    mvprintw(11, 4, "Modo lento: 500 pts en 30 movimientos.");
    mvprintw(12, 4, "Modo rapido: 800 pts en 20 movimientos.");

    mvprintw(15, 4, "ENTER = volver al menu");
}

static void render_mode_select_screen(void)
{
    draw_title("=== SELECCIONAR MODO ===", 2);

    static const char* modes[] = {
        "Modo Lento  (500 pts / 30 movs)",
        "Modo Rapido (800 pts / 20 movs)"
    };

    int start_row = 7;
    int start_col = (COLS - 34) / 2;
    if (start_col < 2)
        start_col = 2;

    for (int i = 0; i < MODE_ITEM_COUNT; i++)
        draw_menu_item(start_row + i * 2, start_col, modes[i],
                       i == g_state.mode_index);

    mvprintw(start_row + MODE_ITEM_COUNT * 2 + 2, start_col,
             "Flechas = navegar | ENTER = iniciar");
    mvprintw(start_row + MODE_ITEM_COUNT * 2 + 4, start_col,
             "ENTER en menu anterior = volver");
}

static void render_scores_screen(void)
{
    draw_title("=== PUNTUACIONES ===", 2);

    mvprintw(7, 4, "Mejor puntaje : %d", g_state.high_score);
    mvprintw(8, 4, "Ultima partida : %d", g_state.last_score);

    mvprintw(12, 4, "ENTER = volver al menu");
}

static void render_game_over_screen(void)
{
    draw_title("=== FIN DE PARTIDA ===", 2);

    mvprintw(6, 4, "Puntaje final : %d / %d",
             g_state.last_score, g_state.score_goal);

    if (g_state.game_status == STATUS_WON) {
        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(8, 4, "*** GANASTE! ***");
        attroff(COLOR_PAIR(3) | A_BOLD);
    } else {
        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(8, 4, "*** PERDISTE! ***");
        attroff(COLOR_PAIR(1) | A_BOLD);
    }

    mvprintw(11, 4, "ENTER = menu principal");
}

void render_screen(void)
{
    clear();

    switch (g_state.current_screen) {

    case SCREEN_MENU:
        render_menu_screen();
        break;

    case SCREEN_INSTRUCTIONS:
        render_instructions_screen();
        break;

    case SCREEN_MODE_SELECT:
        render_mode_select_screen();
        break;

    case SCREEN_PLAYING:
        render_playing();
        break;

    case SCREEN_SCORES:
        render_scores_screen();
        break;

    case SCREEN_GAME_OVER:
        render_game_over_screen();
        break;

    default:
        break;
    }

    refresh();
}
