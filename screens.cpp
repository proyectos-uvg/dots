/**
 * @file screens.cpp
 * @brief Pantallas de menú, instrucciones, puntuaciones y dispatcher.
 */

#include "screens.h"
#include "board.h"
#include "game_state.h"
#include "logic.h"

#include <ncurses.h>
#include <cstdio>
#include <cstdarg>

#define PAIR_TITLE 6

/** Margen izquierdo del bloque de contenido en pantallas de texto. */
static int content_left(void)
{
    int margin = (COLS - 68) / 2;
    return (margin < 2) ? 2 : margin;
}

/** Avanza una fila tras imprimir una línea de cuerpo. */
static int draw_body_line(int row, int col, const char* text)
{
    mvprintw(row, col, "%s", text);
    return row + 1;
}

/** Igual que draw_body_line, con formato printf. */
static int draw_body_fmt(int row, int col, const char* fmt, ...)
{
    char buf[128];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    return draw_body_line(row, col, buf);
}

/** Imprime un encabezado de sección y devuelve la siguiente fila libre. */
static int draw_section_header(int row, int col, const char* title)
{
    attron(A_BOLD | COLOR_PAIR(PAIR_TITLE));
    mvprintw(row, col, "%s", title);
    attroff(A_BOLD | COLOR_PAIR(PAIR_TITLE));
    return row + 1;
}

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
    const int col = content_left();
    int row     = 1;

    draw_title("=== INSTRUCCIONES ===", row);
    row += 2;

    row = draw_section_header(row, col, "OBJETIVO");
    row = draw_body_line(row, col,
        "Conecta puntos del mismo color, forma cadenas y alcanza la meta");
    row = draw_body_line(row, col,
        "de puntos antes de quedarte sin movimientos.");

    row++;
    row = draw_section_header(row, col, "CONTROLES");
    row = draw_body_line(row, col,
        "  Flechas   Mover cursor    ESPACIO   Agregar a la cadena");
    row = draw_body_line(row, col,
        "  ENTER     Confirmar       BACKSPACE Deshacer ultimo punto");
    row = draw_body_line(row, col,
        "  ESC       Cancelar sel.   Q         Abandonar partida");

    row++;
    row = draw_section_header(row, col, "COMO FORMAR CADENAS");
    row = draw_body_line(row, col,
        "  Elige un punto (ESPACIO). Une vecinos ortogonales del mismo");
    row = draw_body_line(row, col,
        "  color. Minimo 2 puntos. ENTER elimina la cadena; caen y");
    row = draw_body_line(row, col,
        "  aparecen puntos nuevos en los huecos.");

    row++;
    row = draw_section_header(row, col, "BONUS POR CICLOS");
    row = draw_body_line(row, col,
        "  Cierra un ciclo (vuelve al inicio, 4+ puntos): borra todo");
    row = draw_body_fmt(row, col,
        "  el color. Puntos: (cadena x %d + extras x %d) x %d.",
        BASE_POINTS_PER_DOT, CYCLE_BONUS_PER_EXTRA, CYCLE_MULTIPLIER);
    row = draw_body_fmt(row, col,
        "  Cadena normal: longitud x %d.", BASE_POINTS_PER_DOT);

    row++;
    row = draw_section_header(row, col, "VICTORIA Y DERROTA");
    row = draw_body_line(row, col,
        "  Ganas al llegar a la meta: Lento 500/30 movs | Rapido 800/20.");
    row = draw_body_line(row, col,
        "  Pierdes si se acaban los movimientos o abandonas con Q.");

    const char* footer = "B o ESC = volver al menu";
    int flen = 0;
    for (const char* p = footer; *p; p++)
        flen++;

    int foot_row = (LINES > 2) ? LINES - 2 : row + 1;
    if (foot_row <= row)
        foot_row = row + 1;

    attron(A_DIM);
    mvaddstr(foot_row, (COLS - flen) / 2, footer);
    attroff(A_DIM);
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
