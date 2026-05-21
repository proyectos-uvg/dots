
/**
 * @file logic.h
 * @brief Motor de lógica del juego: puntuación, gravedad y hilo
 *        de actualización dinámica del tablero.
 *
 * Este módulo complementa el código existente (input.cpp, board.cpp)
 * sin modificarlos. Se engancha al flujo ya establecido:
 *
 *   input_thread → pthread_cond_broadcast(&board_updated)
 *                → game_loop_thread despierta y procesa
 */
 
#ifndef LOGIC_H
#define LOGIC_H
 
#include "game_state.h"
#include "input.h"
#include <vector>
 
/* ------------------------------------------------------------------ */
/* Constantes de puntuación                                            */
/* ------------------------------------------------------------------ */
 
/** Puntos base por cada punto eliminado en cadena normal. */
#define BASE_POINTS_PER_DOT   10
 
/** Multiplicador total cuando la jugada es un ciclo. */
#define CYCLE_MULTIPLIER       2
 
/** Puntos extra por cada punto adicional del color eliminado en ciclo. */
#define CYCLE_BONUS_PER_EXTRA  5
 
/** Puntaje necesario para ganar. */
#define WIN_SCORE             300
 
/* ------------------------------------------------------------------ */
/* API pública                                                          */
/* ------------------------------------------------------------------ */
 
/**
 * @brief Calcula el puntaje de una jugada.
 *
 * - Cadena normal : chain_len * BASE_POINTS_PER_DOT
 * - Ciclo         : (chain_len * BASE_POINTS_PER_DOT
 *                   + extras * CYCLE_BONUS_PER_EXTRA)
 *                   * CYCLE_MULTIPLIER
 *
 * @param chain_len  Cantidad de puntos en la cadena.
 * @param is_cycle   true si la jugada forma un ciclo.
 * @param extras     Puntos adicionales del mismo color (solo en ciclo).
 * @return Puntuación de la jugada.
 */
int calculate_score(int chain_len, bool is_cycle, int extras);
 
/**
 * @brief Aplica gravedad a una columna: los puntos caen hacia abajo.
 *
 * Desplaza valores válidos (>=0) al fondo; deja -1 arriba.
 *
 * @param col Columna a procesar [0, BOARD_SIZE).
 * @note Debe llamarse con board_mutex adquirido.
 */
void apply_gravity_col(int col);
 
/**
 * @brief Rellena con colores aleatorios las celdas vacías (-1)
 *        de una columna.
 *
 * @param col Columna a rellenar [0, BOARD_SIZE).
 * @note Debe llamarse con board_mutex adquirido.
 */
void fill_column(int col);
 
/**
 * @brief Evalúa si la partida terminó y actualiza game_status.
 *
 * Victoria : score >= WIN_SCORE
 * Derrota  : moves_remaining <= 0
 *
 * @note Debe llamarse con board_mutex adquirido.
 */
void check_game_over(void);
 
/**
 * @brief Procesa el estado actual del tablero tras una jugada.
 *
 * Recorre el tablero buscando celdas marcadas como -1 por
 * remove_selection() (ya ejecutada por input_thread), luego:
 *   1. Calcula y acumula puntaje.
 *   2. Aplica gravedad por columna.
 *   3. Rellena columnas afectadas con nuevos colores.
 *   4. Evalúa condición de fin de partida.
 *
 * @param path      Cadena de puntos de la jugada confirmada.
 * @param color     Color de la cadena.
 * @param is_cycle  true si la jugada formó un ciclo.
 *
 * @note Adquiere y libera board_mutex internamente.
 */
void process_move(const std::vector<SelectedPoint>& path,
                  int color,
                  bool is_cycle);
 
/**
 * @brief Hilo de actualización dinámica del tablero.
 *
 * Espera en board_updated (la misma señal que ya emite input_thread
 * con pthread_cond_broadcast). Al despertar llama a process_move()
 * y re-renderiza.
 *
 * @param arg No utilizado.
 * @return NULL siempre.
 */
void* game_loop_thread(void* arg);
 
#endif /* LOGIC_H */