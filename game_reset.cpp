/**
 * @file game_reset.cpp
 * @brief Reinicio de partida sin afectar ncurses ni hilos.
 */

#include "game_reset.h"
#include "board.h"
#include "input.h"
#include "scores.h"
#include "sync.h"

void reset_game(GameState& state)
{
    input_reset_playing_state();
    scores_reset_match_flag();

    state.player_name_len      = 0;
    state.player_name_input[0] = '\0';
    state.score_saved          = false;

    if (state.challenge_mode) {
        state.cycles_formed    = 0;
        state.color_eliminated = 0;
        state.pending_cycle    = false;
    }

    init_board();

    pthread_mutex_lock(&board_mutex);
    state.game_status     = STATUS_RUNNING;
    state.current_screen  = SCREEN_PLAYING;
    pthread_mutex_unlock(&board_mutex);

    pthread_cond_broadcast(&board_updated);
}
