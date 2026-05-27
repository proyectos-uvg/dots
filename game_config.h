/**
 * @file game_config.h
 * @brief Parámetros de partida, animación y niveles Challenge.
 */

#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#include "game_state.h"

/** Tipos de objetivo para niveles Challenge. */
#define GOAL_SCORE       0
#define GOAL_COLOR_ELIM  1
#define GOAL_CYCLES      2
#define GOAL_COMBO       3

/**
 * @brief Configuración de un nivel Challenge.
 */
typedef struct {
    int goal_type;
    int score_goal;
    int color_goal;
    int cycles_target;
    int color_target;
    int moves_limit;
    int num_colors;
    int obstacle_count;
} LevelConfig;

/** Array con los 10 niveles Challenge. */
extern const LevelConfig LEVELS[NUM_LEVELS];

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
