/**
 * @file scores.h
 * @brief Persistencia de puntajes en scores.txt.
 */

#ifndef SCORES_H
#define SCORES_H

#include "game_state.h"

/** Cantidad de puntajes mostrados en la pantalla SCORES. */
#define SCORES_DISPLAY_COUNT 10

/**
 * @brief Entrada del ranking (nombre + puntaje).
 */
typedef struct {
    char name[PLAYER_NAME_MAX];
    int  score;
} ScoreEntry;

/**
 * @brief Carga puntajes desde scores.txt (o deja la lista vacía si no existe).
 */
void load_scores(void);

/**
 * @brief Añade nombre y puntaje, reescribe scores.txt.
 *
 * @param name  Nombre del jugador (no NULL).
 * @param score Puntaje (>= 0).
 * @return true si se guardó correctamente.
 */
bool save_score(const char* name, int score);

/**
 * @brief Cantidad de entradas para mostrar (0 … SCORES_DISPLAY_COUNT).
 */
int scores_display_count(void);

/**
 * @brief Obtiene una entrada del ranking en pantalla.
 *
 * @param index Posición 0-based (0 = mayor puntaje).
 * @param out   Estructura de salida.
 * @return true si el índice es válido.
 */
bool scores_display_entry(int index, ScoreEntry* out);

/**
 * @brief Prepara el fin de partida (sin escribir archivo aún).
 */
void scores_prepare_match_end(int score);

/**
 * @brief Guarda puntaje con nombre; solo una vez por partida.
 */
bool scores_commit_with_name(const char* name, int score);

/**
 * @brief Permite guardar de nuevo en la siguiente partida.
 */
void scores_reset_match_flag(void);

#endif
