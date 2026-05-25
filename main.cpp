/**
 * @file main.cpp
 * @brief Punto de entrada principal del juego.
 */

#include <ncurses.h>
#include <clocale>
#include <cstdio>
#include <pthread.h>
#include "input.h"
#include "game_state.h"
#include "sync.h"
#include "board.h"
#include "logic.h"
#include "screens.h"
#include "scores.h"

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
 * @brief Mutex que serializa el acceso a ncurses entre hilos.
 */
pthread_mutex_t render_mutex;

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
int main(void)
{

    /* Habilitar soporte UTF-8 */
    setlocale(LC_ALL, "");

    initscr();

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (!has_colors())
    {
        endwin();

        fprintf(stderr,
                "Error: la terminal no soporta colores ANSI.\n");

        return 1;
    }

    init_display();

    init_sync();

    load_scores();

    g_state.current_screen = SCREEN_MENU;
    g_state.menu_index     = 0;
    g_state.mode_index     = 0;
    g_state.game_mode      = SLOW;
    g_state.high_score          = 0;
    g_state.last_score          = 0;
    g_state.last_player_name[0] = '\0';
    g_state.player_name_input[0]= '\0';
    g_state.player_name_len     = 0;
    g_state.score_saved         = false;
    g_state.special_enabled     = false;
    g_state.num_colors          = 4;
    g_state.game_status         = STATUS_RUNNING;

    pthread_mutex_lock(&render_mutex);
    render_screen();
    pthread_mutex_unlock(&render_mutex);

    pthread_t input_tid;
    pthread_t logic_tid;

    /* Lanzar hilo de lógica primero para que ya esté esperando
     * en board_updated cuando input_thread empiece a jugar. */
    pthread_create(&logic_tid, NULL, game_loop_thread, NULL);
    pthread_create(&input_tid, NULL, input_thread, NULL);

    pthread_join(input_tid, NULL);

    /* Despertar game_loop_thread para que detecte el fin de partida
     * y pueda salir de pthread_cond_wait. */
    pthread_cond_broadcast(&board_updated);
    pthread_join(logic_tid, NULL);

    cleanup();

    return 0;
}