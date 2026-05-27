/**
 * @file screens.cpp
 * @brief Pantallas de menú, instrucciones, puntuaciones y dispatcher.
 */

#include "screens.h"
#include "board.h"
#include "game_state.h"
#include "game_config.h"
#include "logic.h"
#include "scores.h"
#include <cstring>

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
        "  Ganas al llegar a la meta: Lento 5000/30 movs | Rapido 8000/20.");
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
    char slow_line[64];
    char fast_line[64];

    snprintf(slow_line, sizeof(slow_line),
             "Slow Mode      (%d pts / %d movs)",
             get_score_goal(SLOW), get_moves_limit(SLOW));
    snprintf(fast_line, sizeof(fast_line),
             "Fast Mode      (%d pts / %d movs)",
             get_score_goal(FAST), get_moves_limit(FAST));

    const char* modes[] = { slow_line, fast_line, "Challenge Mode (10 niveles)" };

    draw_title("=== SELECT MODE ===", 2);

    int start_row = 6;
    int start_col = (COLS - 40) / 2;
    if (start_col < 2)
        start_col = 2;

    for (int i = 0; i < MODE_ITEM_COUNT; i++)
        draw_menu_item(start_row + i * 2, start_col, modes[i],
                       i == g_state.mode_index);

    int check_row = start_row + MODE_ITEM_COUNT * 2 + 1;
    const char* mark = g_state.special_enabled ? "X" : " ";

    if (g_state.mode_index == 3) {
        attron(A_REVERSE | COLOR_PAIR(PAIR_TITLE));
        mvprintw(check_row, start_col, "> [%s] Special elements <", mark);
        attroff(A_REVERSE | COLOR_PAIR(PAIR_TITLE));
    } else {
        mvprintw(check_row, start_col, "[%s] Special elements", mark);
    }

    int color_row = check_row + 1;

    if (g_state.mode_index == 4) {
        attron(A_REVERSE | COLOR_PAIR(PAIR_TITLE));
        mvprintw(color_row, start_col, "Colores: < %d >", g_state.num_colors);
        attroff(A_REVERSE | COLOR_PAIR(PAIR_TITLE));
    } else {
        mvprintw(color_row, start_col, "Colores:   %d  ", g_state.num_colors);
    }

    mvprintw(color_row + 2, start_col, "Flechas = navegar | < > = ajustar colores | ESPACIO/ENTER = marcar");
    mvprintw(color_row + 4, start_col, "ESC o B = volver al menu");
}

static const char* goal_type_label(int goal_type, int level_idx)
{
    static char buf[64];
    const LevelConfig& lv = LEVELS[level_idx];
    switch (goal_type) {
    case GOAL_SCORE:
        snprintf(buf, sizeof(buf), "Meta: %d pts", lv.score_goal);
        break;
    case GOAL_COLOR_ELIM: {
        const char* col_names[] = { "rojo","azul","verde","amarillo","magenta","cyan","blanco" };
        const char* cn = (lv.color_target >= 0 && lv.color_target < 7)
                         ? col_names[lv.color_target] : "?";
        snprintf(buf, sizeof(buf), "Eliminar %d %s", lv.color_goal, cn);
        break;
    }
    case GOAL_CYCLES:
        snprintf(buf, sizeof(buf), "%d ciclos", lv.cycles_target);
        break;
    case GOAL_COMBO:
        snprintf(buf, sizeof(buf), "%d pts + %d ciclos", lv.score_goal, lv.cycles_target);
        break;
    default:
        snprintf(buf, sizeof(buf), "?");
        break;
    }
    return buf;
}

static void render_challenge_select_screen(void)
{
    const int col = content_left();
    int row = 1;

    draw_title("=== CHALLENGE MODE ===", row);
    row += 2;

    row = draw_section_header(row, col, "Selecciona un nivel:");
    row++;

    for (int i = 0; i < NUM_LEVELS; i++) {
        const LevelConfig& lv = LEVELS[i];
        char line[80];
        snprintf(line, sizeof(line),
                 "Niv %2d  %-28s  %2d movs  %d colores%s",
                 i + 1,
                 goal_type_label(lv.goal_type, i),
                 lv.moves_limit,
                 lv.num_colors,
                 lv.obstacle_count > 0 ? "  [#]" : "");
        bool sel = (g_state.current_level == i + 1);
        draw_menu_item(row, col, line, sel);
        row++;
    }

    const char* footer = "Flechas = navegar | ENTER = iniciar | ESC/B = volver";
    int flen = 0;
    for (const char* p = footer; *p; p++) flen++;
    int foot_row = (LINES > 2) ? LINES - 2 : row + 1;
    if (foot_row <= row) foot_row = row + 1;
    attron(A_DIM);
    mvaddstr(foot_row, (COLS - flen) / 2, footer);
    attroff(A_DIM);
}

static void render_scores_screen(void)
{
    const int col = content_left();
    int row     = 1;

    draw_title("=== PUNTUACIONES ===", row);
    row += 2;

    row = draw_section_header(row, col, "TOP 10 (mayor a menor)");

    int count = scores_display_count();

    if (count == 0) {
        row = draw_body_line(row, col, "  (sin puntajes guardados aun)");
    } else {
        for (int i = 0; i < count; i++) {
            ScoreEntry entry;

            if (scores_display_entry(i, &entry))
                row = draw_body_fmt(row, col, "  %2d.  %-14s  %d pts",
                                    i + 1, entry.name, entry.score);
        }
    }

    row++;
    if (g_state.last_player_name[0] != '\0') {
        row = draw_body_fmt(row, col, "  Ultima: %s (%d pts)",
                            g_state.last_player_name, g_state.last_score);
    } else {
        row = draw_body_fmt(row, col, "  Ultima partida : %d pts",
                            g_state.last_score);
    }
    row = draw_body_fmt(row, col, "  Mejor en sesion : %d", g_state.high_score);

    const char* footer = "ENTER o ESC = volver al menu";
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

static void render_game_over_screen(void)
{
    const int col = content_left();
    int row     = 1;

    draw_title("=== GAME OVER ===", row);
    row += 2;

    if (g_state.game_status == STATUS_WON) {
        attron(COLOR_PAIR(3) | A_BOLD);
        row = draw_body_line(row, col, "  Victory");
        attroff(COLOR_PAIR(3) | A_BOLD);
    } else {
        attron(COLOR_PAIR(1) | A_BOLD);
        row = draw_body_line(row, col, "  Defeat");
        attroff(COLOR_PAIR(1) | A_BOLD);
    }

    row++;
    row = draw_body_fmt(row, col, "  Final score      : %d / %d",
                        g_state.last_score, g_state.score_goal);
    row = draw_body_fmt(row, col, "  Remaining moves  : %d",
                        g_state.moves_remaining);

    row++;

    if (!g_state.score_saved) {
        row = draw_section_header(row, col, "SAVE SCORE");
        row = draw_body_line(row, col, "  Enter your name (ENTER to save):");
        row = draw_body_fmt(row, col, "  > %s_", g_state.player_name_input);
        row++;
    } else {
        row = draw_body_fmt(row, col, "  Saved as: %s", g_state.last_player_name);
        row++;
    }

    if (g_state.challenge_mode) {
        row = draw_body_fmt(row, col, "  Nivel completado: %d / %d",
                            g_state.current_level, NUM_LEVELS);
        row++;
    }

    row = draw_section_header(row, col, "OPTIONS");
    if (g_state.challenge_mode && g_state.game_status == STATUS_WON &&
        g_state.current_level < NUM_LEVELS)
        row = draw_body_line(row, col, "  N = Next Level");
    row = draw_body_line(row, col, "  R = Restart");
    row = draw_body_line(row, col, "  M = Return to main menu");
    row = draw_body_line(row, col, "  Q = Quit");
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

    case SCREEN_CHALLENGE_SELECT:
        render_challenge_select_screen();
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
