/**
 * @file game_config.cpp
 * @brief Implementación de parámetros por modo de juego.
 */

#include "game_config.h"

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
        return 800;

    case SLOW:
    default:
        return 500;
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
