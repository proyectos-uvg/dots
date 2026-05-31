/**
 * @file main.cpp
 * @brief Punto de entrada principal del juego.
 */

#include <ncurses.h>
#include <clocale>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include "input.h"
#include "game_state.h"
#include "sync.h"
#include "board.h"
#include "logic.h"
#include "screens.h"
#include "scores.h"
#include <unistd.h>   
#include <cstdlib>    

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
 * @brief Copia los archivos de sonido desde la carpeta sounds/
 *        del proyecto a la carpeta Downloads del usuario de Windows.
 *
 * Permite que Media.SoundPlayer de PowerShell pueda acceder
 * a los archivos, ya que no puede leer rutas WSL directamente.
 */
void setup_sounds(void)
{
    char win_user[64];
    char src_path[512];
    char cmd[1024];

    // Obtiene el usuario de Windows
    FILE* fp = popen("powershell.exe -c '$env:USERNAME'", "r");
    if (!fp) return;
    fgets(win_user, sizeof(win_user), fp);
    pclose(fp);
    win_user[strcspn(win_user, "\n\r")] = '\0';

    // Obtiene la ruta absoluta de la carpeta sounds/ del proyecto
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) == NULL) return;
    snprintf(src_path, sizeof(src_path), "%s/sounds/connect.wav", cwd);

    // Copia el archivo a Downloads de Windows
    snprintf(cmd, sizeof(cmd),
        "cp '%s' /mnt/c/Users/%s/Downloads/connect.wav",
        src_path, win_user);
    system(cmd);
}

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
    /* Copiar sonidos a Windows antes de iniciar */
    setup_sounds();

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

    g_state.current_screen      = SCREEN_MENU;
    g_state.menu_index          = 0;
    g_state.mode_index          = 0;
    g_state.game_mode           = SLOW;
    g_state.high_score          = 0;
    g_state.last_score          = 0;
    g_state.last_player_name[0] = '\0';
    g_state.player_name_input[0]= '\0';
    g_state.player_name_len     = 0;
    g_state.score_saved         = false;
    g_state.special_enabled     = false;
    g_state.num_colors          = 4;
    g_state.game_status         = STATUS_RUNNING;
    g_state.challenge_mode      = false;
    g_state.current_level       = 1;
    g_state.goal_type           = 0;
    g_state.cycles_formed       = 0;
    g_state.cycles_target       = 0;
    g_state.color_goal          = 0;
    g_state.color_eliminated    = 0;
    g_state.color_target        = -1;
    g_state.pending_cycle       = false;

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

    /* Despertar game_loop_thread para que detecte SCREEN_EXIT
     * y pueda salir de sem_wait. */
    sem_post(&input_ready);
    pthread_join(logic_tid, NULL);

    cleanup();

    return 0;
}