/**
 * @file main.cpp
 * @brief Punto de entrada principal del juego.
 */

#include <ncurses.h>
#include <clocale>
#include <cstdio>

#include "game_state.h"
#include "sync.h"
#include "board.h"

/**
 * @brief Estado global compartido de la partida.
 */
GameState g_state;

/**
 * @brief Mutex que protege el tablero y movimientos restantes.
 */
pthread_mutex_t board_mutex;

/**
 * @brief Mutex que protege el puntaje del jugador.
 */
pthread_mutex_t score_mutex;

/**
 * @brief Variable de condición utilizada para notificar
 *        cambios en el tablero.
 */
pthread_cond_t board_updated;

/**
 * @brief Semáforo utilizado para sincronizar
 *        entrada de usuario y lógica del juego.
 */
sem_t input_ready;

/**
 * @brief Función principal de la aplicación.
 *
 * Inicializa ncurses, configura colores y primitivas
 * de sincronización, crea el estado inicial del tablero
 * y renderiza la interfaz del juego.
 *
 * La aplicación permanece esperando entrada del usuario
 * hasta que se presione una tecla.
 *
 * @return 0 si la ejecución finaliza correctamente.
 * @return 1 si la terminal no soporta colores.
 */
int main(void) {

    /* Habilitar soporte UTF-8 */
    setlocale(LC_ALL, "");

    initscr();

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (!has_colors()) {
        endwin();

        fprintf(stderr,
                "Error: la terminal no soporta colores ANSI.\n");

        return 1;
    }

    init_display();

    init_sync();

    init_board();

    render_board();

    getch();

    cleanup();

    return 0;
}