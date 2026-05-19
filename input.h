/**
 * @file input.h
 * @brief Sistema de interacción del jugador.
 */

#ifndef INPUT_H
#define INPUT_H

#include <vector>
#include "game_state.h"

/**
 * @brief Punto actualmente seleccionado por el jugador.
 */
typedef struct {
    int row;
    int col;
} SelectedPoint;

/**
 * @brief Hilo principal de captura de entrada.
 */
void* input_thread(void* arg);

/**
 * @brief Verifica si dos puntos son adyacentes.
 */
bool is_adjacent(int r1, int c1, int r2, int c2);

/**
 * @brief Detecta si existe un ciclo cerrado.
 */
bool detect_cycle(const std::vector<SelectedPoint>& path);

/**
 * @brief Verifica si un punto ya fue seleccionado.
 */
bool already_selected(const std::vector<SelectedPoint>& path,
                      int row,
                      int col);

#endif