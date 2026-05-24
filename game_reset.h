/**
 * @file game_reset.h
 * @brief Reinicio completo de una partida en curso.
 */

#ifndef GAME_RESET_H
#define GAME_RESET_H

#include "game_state.h"

/**
 * @brief Reinicia tablero, puntaje, movimientos y flags de gameplay.
 *
 * Conserva game_mode, high_score y datos de sesión del menú.
 * Deja current_screen en SCREEN_PLAYING.
 *
 * @param state Estado global a reiniciar (típicamente g_state).
 * @note Limpia la cadena de selección del hilo de entrada.
 * @note Seguro para llamarse desde input_thread (usa board_mutex).
 */
void reset_game(GameState& state);

#endif
