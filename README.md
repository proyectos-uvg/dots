# Dots Game — Documentación de Concurrencia

Juego de conectar puntos implementado en C++ con **pthreads** y **ncurses**.  
El documento explica la arquitectura multihilo, las primitivas de sincronización
usadas y los patrones de diseño aplicados para evitar condiciones de carrera y
deadlocks.

---

## Índice

1. [Compilación y ejecución](#compilación-y-ejecución)
2. [Arquitectura de hilos](#arquitectura-de-hilos)
3. [Primitivas de sincronización](#primitivas-de-sincronización)
4. [Flujo de una jugada](#flujo-de-una-jugada)
5. [Patrones y decisiones de diseño](#patrones-y-decisiones-de-diseño)
   - [Orden de adquisición de mutex](#1-orden-de-adquisición-de-mutex-evitar-deadlock)
   - [Semáforo fuera del mutex](#2-semáforo-fuera-del-mutex)
   - [Flag pending_cycle como canal entre hilos](#3-flag-pending_cycle-como-canal-entre-hilos)
6. [Métrica: código secuencial vs paralelo](#métrica-código-secuencial-vs-paralelo)
7. [Referencias rápidas al código](#referencias-rápidas-al-código)

---

## Compilación y ejecución

```bash
g++ -o dots main.cpp board.cpp input.cpp logic.cpp \
    screens.cpp scores.cpp game_config.cpp game_reset.cpp \
    -lncurses -lpthread
./dots
```

Requiere: `libncurses-dev`, `libpthread` (POSIX).

---

## Arquitectura de hilos

El programa crea **dos hilos** además del hilo principal (`main`):

| Hilo | Función de entrada | Responsabilidad |
|------|-------------------|-----------------|
| `input_tid` | `input_thread()` | Leer teclas, navegar menús, validar y confirmar jugadas |
| `logic_tid` | `game_loop_thread()` | Gravedad, relleno de celdas, cálculo de puntaje, fin de partida |
| `main` | `main()` | Inicializar todo, crear hilos, esperar a que terminen |

> **`main.cpp`, líneas 120–132** — creación y espera de hilos:

```cpp
// El hilo de lógica se crea PRIMERO para que ya esté escuchando
// en board_updated cuando el jugador haga su primer movimiento.
pthread_create(&logic_tid, NULL, game_loop_thread, NULL);
pthread_create(&input_tid, NULL, input_thread, NULL);

pthread_join(input_tid, NULL);          // main duerme aquí mientras se juega

pthread_cond_broadcast(&board_updated); // despierta logic_tid para que pueda salir
pthread_join(logic_tid, NULL);
```

---

## Primitivas de sincronización

Todas están declaradas en **`sync.h`** y definidas en **`main.cpp`, líneas 26–48**.

### `board_mutex` — Mutex del tablero

Protege:
- `g_state.board` (matriz de celdas)
- `g_state.moves_remaining`
- `g_state.pending_cycle`
- Cualquier campo leído/escrito en simultáneo por ambos hilos

> **`sync.h`, línea 21** — declaración  
> **`main.cpp`, línea 26** — definición

```cpp
extern pthread_mutex_t board_mutex;
```

### `score_mutex` — Mutex del puntaje

Protege `g_state.score`. Separado de `board_mutex` para permitir mayor
granularidad: el puntaje puede actualizarse sin bloquear toda la lectura
del tablero.

> **`sync.h`, línea 29** — declaración  
> **`main.cpp`, línea 31** — definición  
> **`logic.cpp`, líneas 261–263** — uso en `process_move`:

```cpp
pthread_mutex_lock(&score_mutex);
g_state.score += gained;
pthread_mutex_unlock(&score_mutex);
```

### `render_mutex` — Mutex de ncurses

ncurses **no es thread-safe**. Si dos hilos llaman a `mvprintw` o `refresh`
al mismo tiempo la pantalla se corrompe. `render_mutex` serializa *todas*
las llamadas a ncurses del programa.

> **`sync.h`, línea 66** — declaración  
> **`main.cpp`, línea 48** — definición

```
REGLA: nunca adquirir board_mutex estando DENTRO de render_mutex.
       (ver sección "Orden de adquisición de mutex")
```

### `board_updated` — Variable de condición

Mantiene compatibilidad con código que notifica cambios de estado del tablero
(por ejemplo, `init_board` al generar un nuevo tablero). Su rol de señalización
de jugadas fue reemplazado por el semáforo `input_ready` (ver abajo).

> **`sync.h`, línea 42** — declaración  
> **`main.cpp`, línea 37** — definición  
> **`board.cpp`** — broadcast al inicializar tablero

### `input_ready` — Semáforo POSIX

Canal de señalización **de jugada confirmada** de `input_thread` hacia
`game_loop_thread`. Funciona como contador de movimientos pendientes:

- `sem_post` → "hay una jugada lista, procésala" (contador++)
- `sem_wait` → "espero hasta que haya jugada" (contador-- o duerme)

A diferencia de una variable de condición, el semáforo **no pierde la señal**
si el consumidor aún no está durmiendo, y no requiere mutex auxiliar.

> **`sync.h`, línea 54** — declaración  
> **`main.cpp`, línea 43** — definición  
> **`board.cpp`, línea 98** — inicialización  
> **`input.cpp`** — `sem_post` al confirmar jugada (producer)  
> **`logic.cpp`** — `sem_wait` en `game_loop_thread` (consumer)  
> **`main.cpp`** — `sem_post` al salir para desbloquear el hilo lógico

```cpp
sem_init(&input_ready, 0, 0);
//                     ^  ^
//                     |  valor inicial = 0 (bloqueante desde el inicio)
//                     compartido entre procesos = 0 (solo hilos del mismo proceso)
```

### Inicialización y destrucción — `board.cpp` líneas 90–121

```cpp
void init_sync(void) {
    int err = 0;
    err |= pthread_mutex_init(&board_mutex, NULL);
    err |= pthread_mutex_init(&score_mutex, NULL);
    err |= pthread_mutex_init(&render_mutex, NULL);
    err |= pthread_cond_init(&board_updated, NULL);
    err |= sem_init(&input_ready, 0, 0);

    if (err != 0) { endwin(); exit(1); }  // fallo fatal
}

void destroy_sync(void) {
    pthread_mutex_destroy(&board_mutex);
    pthread_mutex_destroy(&score_mutex);
    pthread_mutex_destroy(&render_mutex);
    pthread_cond_destroy(&board_updated);
    sem_destroy(&input_ready);
}
```

> El OR acumulado en `err` detecta cualquier fallo de inicialización con
> un único `if`. `destroy_sync` se llama desde `cleanup()` **después** de
> que ambos hilos terminaron, garantizando que nadie use las primitivas
> mientras se destruyen.

---

## Flujo de una jugada

```
input_thread                         game_loop_thread
──────────────────────────────       ────────────────────────────
getch()  ← bloqueante en teclado     pthread_cond_wait()  ← duerme
  │                                          │
  │  jugador presiona ENTER                  │
  ↓                                          │
lock(board_mutex)                            │
  marca celdas = color -1                    │
  pending_cycle = true/false                 │
  moves_remaining--                          │
unlock(board_mutex)                          │
  │                                          │
  cond_broadcast() ───────────────────────→ despierta
                                    lock(board_mutex)
                                      verifica huecos
                                      lee pending_cycle
                                      pending_cycle = false
                                    unlock(board_mutex)
                                      │
                                    process_move(is_cycle)
                                      lock(board_mutex)
                                        expande BOMB/CROSS
                                        apply_gravity_col()
                                          unlock(board_mutex) ←┐
                                          lock(render_mutex)   │ animación
                                          render_board()       │ de caída
                                          unlock(render_mutex) │
                                          lock(board_mutex)  ──┘
                                        fill_column()
                                        check_game_over()
                                        lock(score_mutex)
                                          score += gained
                                        unlock(score_mutex)
                                      unlock(board_mutex)
                                      lock(render_mutex)
                                        render_board()
                                      unlock(render_mutex)
                                      │
                                    pthread_cond_wait()  ← duerme otra vez
```

---

## Patrones y decisiones de diseño

### 1. Orden de adquisición de mutex — evitar deadlock

> **`logic.cpp`, líneas 83–91** (`apply_gravity_col`)

```cpp
// Se llama con board_mutex YA tomado desde process_move
if (moved) {
    pthread_mutex_unlock(&board_mutex);   // ← suelta ANTES de pedir render
    pthread_mutex_lock(&render_mutex);
    render_board();
    pthread_mutex_unlock(&render_mutex);
    usleep(delay);
    pthread_mutex_lock(&board_mutex);     // ← retoma
}
```

**El problema:** Si `process_move` tuviera `board_mutex` y pidiera `render_mutex`,
mientras `input_thread` tuviera `render_mutex` y pidiera `board_mutex`,
ambos hilos quedarían esperándose mutuamente — **deadlock**.

**La solución:** Se establece la regla global:

```
Orden permitido:  render_mutex → board_mutex   ✓
Orden prohibido:  board_mutex  → render_mutex   ✗
```

Por eso `apply_gravity_col` suelta `board_mutex` antes de llamar a render,
aunque eso signifique volver a tomarlo después.

---

### 2. Semáforo fuera del mutex

> **`input.cpp`** — `sem_post` después de liberar `board_mutex`  
> **`logic.cpp`** — `sem_wait` sin ningún mutex auxiliar

```cpp
// input_thread — después de marcar celdas bajo board_mutex:
pthread_mutex_unlock(&board_mutex);   // 1. suelta el recurso compartido
sem_post(&input_ready);               // 2. señaliza al hilo lógico

// game_loop_thread — espera limpia sin mutex falso:
sem_wait(&input_ready);               // duerme hasta que haya jugada
```

`sem_post` se llama **después** de liberar `board_mutex` por la misma razón que
antes: si `game_loop_thread` despertara y encontrara el mutex tomado, se volvería
a bloquear de inmediato. Liberando primero, el hilo lógico puede ejecutarse sin
espera adicional.

La ventaja del semáforo sobre la variable de condición para este caso:
- No necesita mutex auxiliar (el `cond_wait` requería uno que no protegía nada).
- No tiene *spurious wakeups* (despertares falsos).
- Si `sem_post` se llama antes de que el hilo esté durmiendo, la señal **no se pierde** (queda en el contador).

---

### 3. Flag `pending_cycle` como canal entre hilos

> **`game_state.h`** — declaración del campo  
> **`input.cpp`, línea 87** — escritura (bajo `board_mutex`)  
> **`logic.cpp`, líneas 327–334** — lectura y reset (bajo `board_mutex`)

```cpp
// input_thread (remove_selection):
pthread_mutex_lock(&board_mutex);
if (detect_cycle(selection)) g_state.pending_cycle = true;
// ... limpia celdas ...
pthread_mutex_unlock(&board_mutex);

// game_loop_thread:
pthread_mutex_lock(&board_mutex);
bool is_cycle = g_state.pending_cycle;
g_state.pending_cycle = false;          // reset atómico bajo el mismo mutex
pthread_mutex_unlock(&board_mutex);

process_move(empty_path, is_cycle);     // pasa el flag a la lógica de puntaje
```

Este patrón evita tener que exponer estructuras internas de `input.cpp`
(la variable `selection` es `static`). El flag se escribe y lee **siempre**
bajo `board_mutex`, garantizando visibilidad entre hilos sin `volatile` ni
`atomic`.

---

## Métrica: código secuencial vs paralelo

### Metodología

Se clasifican las funciones del proyecto según si pueden ejecutarse en paralelo
con otra función o si deben ejecutarse de forma exclusiva (sección crítica
protegida por mutex o porción del `main` de un solo hilo).

### Clasificación por módulo

| Módulo / Función | Tipo | Justificación |
|-----------------|------|---------------|
| `main()` — inicialización | **Secuencial** | Un solo hilo activo antes de `pthread_create` |
| `main()` — `pthread_join` + cleanup | **Secuencial** | Un solo hilo activo tras `pthread_join` |
| `input_thread` — lectura de tecla (`getch`) | **Paralelo** | Corre simultáneamente con `game_loop_thread` |
| `input_thread` — navegación de menús | **Paralelo** | No toca recursos del tablero |
| `input_thread` — sección crítica (`lock board_mutex`) | **Secuencial efectivo** | Solo un hilo puede estar aquí a la vez |
| `game_loop_thread` — `sem_wait` (dormido) | **Paralelo** | El hilo duerme; el otro hilo corre libre |
| `game_loop_thread` — `process_move` (lógica) | **Paralelo** | Corre mientras `input_thread` lee teclas |
| `game_loop_thread` — sección crítica (`lock board_mutex`) | **Secuencial efectivo** | Exclusión mutua activa |
| `apply_gravity_col` — animación (`render_board`) | **Paralelo** | Suelta `board_mutex` antes de renderizar |
| `render_*` bajo `render_mutex` | **Secuencial efectivo** | ncurses serializado |

### Cálculo porcentual (Ley de Amdahl)

Se estima el porcentaje de tiempo de ejecución durante una partida activa:

```
Tiempo total de una sesión de juego (estimado):
  - Inicialización / cleanup (main)            ≈  2 %   → Secuencial
  - Menús / pantallas sin juego activo         ≈ 15 %   → Secuencial (1 hilo)
  - Secciones críticas con mutex tomado        ≈  8 %   → Secuencial efectivo
  - Renderizado bajo render_mutex              ≈  5 %   → Secuencial efectivo
  ─────────────────────────────────────────────────────
  Total secuencial  S  ≈  30 %   (fracción f = 0.30)

  - input_thread leyendo teclas / game_loop dormido en sem_wait
  - game_loop_thread procesando mientras input_thread lee teclas
  - Gravedad animada (board_mutex libre, render_mutex libre)
  ─────────────────────────────────────────────────────
  Total paralelo    P  ≈  70 %   (fracción 1 - f = 0.70)
```

### Speedup teórico — Ley de Amdahl

Con **N = 2 núcleos** (dos hilos activos) y fracción paralela de 0.70:

```
         1                  1              1
S = ─────────────  =  ──────────────  =  ──────  ≈  1.54×
    f + (1-f)/N       0.30 + 0.70/2      0.65
```

El programa teóricamente ejecuta **1.54 veces más rápido** que una versión
completamente secuencial equivalente.

### Interpretación

- El **70 % paralelo** corresponde principalmente al tiempo de `getch()` bloqueado en
  espera de tecla, durante el cual `game_loop_thread` puede procesar libremente.
- El **30 % secuencial** está dominado por las secciones críticas (protección de
  `g_state.board` y `g_state.score`) y el renderizado serializado por `render_mutex`,
  que es inherentemente secuencial por limitaciones de ncurses.
- El cuello de botella real no es la CPU sino la E/S de terminal; el paralelismo
  aquí resuelve un problema de **latencia de respuesta**, no de throughput de cómputo.

---

## Referencias rápidas al código

| Concepto | Archivo | Líneas |
|----------|---------|--------|
| Declaración de todas las primitivas | `sync.h` | 21–66 |
| Definición de primitivas (variables globales) | `main.cpp` | 26–48 |
| Creación de hilos | `main.cpp` | 124–125 |
| Espera y limpieza de hilos | `main.cpp` | 127–132 |
| `init_sync` / `destroy_sync` | `board.cpp` | 90–121 |
| `sem_init` del semáforo (valor inicial 0) | `board.cpp` | 98 |
| Bucle principal de `game_loop_thread` | `logic.cpp` | 300–354 |
| `sem_wait` — consumer de jugadas | `logic.cpp` | ~313 |
| `sem_post` — producer de jugadas | `input.cpp` | ~531 |
| `sem_post` — desbloqueo al salir | `main.cpp` | ~131 |
| `remove_selection` — lock/unlock board | `input.cpp` | 63–104 |
| Escritura de `pending_cycle` | `input.cpp` | 87 |
| Lectura de `pending_cycle` | `logic.cpp` | 327–334 |
| `apply_gravity_col` — suelta mutex para render | `logic.cpp` | 83–91 |
| Doble mutex en `process_move` (board + score) | `logic.cpp` | 170, 261–263 |
| Regla de orden de mutex (comentario) | `sync.h` | 63–64 |

---

*Proyecto desarrollado para el curso de Microprocesadores — UVG, Tercer Semestre.*
