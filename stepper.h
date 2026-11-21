#pragma once
#include <stdint.h>
#include <stdbool.h>

// Generador de pasos NO bloqueante para TB6600 (ánodo común: PUL+/DIR+ a 3V3).
// Los pulsos los genera el PWM por hardware y los pasos se cuentan en una interrupción,
// así el lazo principal queda libre para leer la MPU y calcular el PID.
//
// Las velocidades están en pulsos por segundo (pasos/s). Cuántas vueltas son depende
// del DIP del TB6600 (con 200 pulsos/vuelta, 200 pasos/s = 1 vuelta/s).

// ---------- Parámetros (ajustalos con el motor ya probado) ----------
#define STEPPER_MAX_SPEED_SPS   800.0f    // velocidad máxima permitida
#define STEPPER_ACCEL_SPS2      3000.0f   // aceleración máxima (pasos/s por segundo)
#define STEPPER_MIN_SPEED_SPS   20.0f     // por debajo de esto el motor se detiene
#define STEPPER_WATCHDOG_US     100000    // si no se llama a stepper_update() por 100 ms, corta los pulsos

// Inicializa el PWM, la interrupción y el pin de dirección.
void    stepper_init(unsigned int pul_pin, unsigned int dir_pin);

// Límites de posición en pasos. El motor frena antes de llegar y no los supera.
void    stepper_set_limits(int32_t min_steps, int32_t max_steps);

// Define la posición actual (por ejemplo 0 al tocar el final de carrera).
void    stepper_set_position(int32_t steps);

// Velocidad deseada con signo (+ adelante, - atrás). Se aplica con rampa de aceleración.
void    stepper_set_target_speed(float steps_per_s);

// Llamar en cada ciclo del lazo de control (con el dt real en segundos).
void    stepper_update(float dt);

int32_t stepper_get_position(void);   // pasos acumulados
float   stepper_get_speed(void);      // velocidad actual (con signo, pasos/s)
bool    stepper_at_limit(void);       // true si está en alguno de los límites
