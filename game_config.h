/**
 * @file game_config.h
 * @brief Parámetros de partida y animación según el modo de juego.
 */

#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#include "game_state.h"

/**
 * @brief Retardo entre pasos de gravedad (microsegundos).
 */
unsigned int get_tick_delay(GameMode mode);

/**
 * @brief Puntaje necesario para ganar en el modo dado.
 */
int get_score_goal(GameMode mode);

/**
 * @brief Cantidad inicial de movimientos en el modo dado.
 */
int get_moves_limit(GameMode mode);

/**
 * @brief Etiqueta legible del modo para la interfaz.
 */
const char* game_mode_label(GameMode mode);

#endif
