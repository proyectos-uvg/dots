/**
 * @file game_config.cpp
 * @brief Implementación de parámetros por modo de juego y niveles Challenge.
 */

#include "game_config.h"

/* goal_type | score_goal | color_goal | cycles_target | color_target |
   moves_limit | num_colors | obstacle_count                           */
const LevelConfig LEVELS[NUM_LEVELS] = {
    { GOAL_SCORE,      150,  0,  0, -1, 30, 3, 0 }, /* Nivel  1 */
    { GOAL_SCORE,      300,  0,  0, -1, 25, 4, 2 }, /* Nivel  2 */
    { GOAL_COLOR_ELIM,   0, 20,  0,  0, 20, 4, 0 }, /* Nivel  3 */
    { GOAL_SCORE,      500,  0,  0, -1, 25, 5, 4 }, /* Nivel  4 */
    { GOAL_CYCLES,       0,  0,  3, -1, 20, 4, 2 }, /* Nivel  5 */
    { GOAL_SCORE,      800,  0,  0, -1, 20, 5, 6 }, /* Nivel  6 */
    { GOAL_COLOR_ELIM,   0, 25,  0,  1, 20, 6, 3 }, /* Nivel  7 */
    { GOAL_COMBO,      600,  0,  2, -1, 20, 5, 5 }, /* Nivel  8 */
    { GOAL_CYCLES,       0,  0,  5, -1, 25, 6, 4 }, /* Nivel  9 */
    { GOAL_COMBO,     1000,  0,  3, -1, 20, 7, 8 }, /* Nivel 10 */
};

unsigned int get_tick_delay(GameMode mode)
{
    switch (mode) {

    case FAST:
        return 120000u;

    case SLOW:
    default:
        return 240000u;
    }
}

int get_score_goal(GameMode mode)
{
    switch (mode) {

    case FAST:
        return 8000;

    case SLOW:
    default:
        return 5000;
    }
}

int get_moves_limit(GameMode mode)
{
    switch (mode) {

    case FAST:
        return 20;

    case SLOW:
    default:
        return 30;
    }
}

const char* game_mode_label(GameMode mode)
{
    switch (mode) {

    case FAST:
        return "Fast";

    case SLOW:
    default:
        return "Slow";
    }
}
