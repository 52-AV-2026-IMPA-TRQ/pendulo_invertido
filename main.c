#include <stdio.h>
#include "pico/stdlib.h"
#include "config.h"
#include "stepper.h"

// Test del generador de pasos no bloqueante.
// El lazo corre a 200 Hz mientras el motor se mueve; si el lazo se bloqueara,
// la columna "dt max" se iría muy por encima de 5 ms.

// Límites de posición en pasos (poné valores con margen antes del tope físico del riel)
#define LIMIT_MIN_STEPS  (-2000)
#define LIMIT_MAX_STEPS  ( 2000)

#define PRINT_MS 200

typedef struct { float sps; uint32_t ms; } phase_t;

// Guion de prueba: velocidad deseada (pasos/s) y duración (ms).
// Las fases de +-800 durante 6 s intentan pasarse de los límites, y el motor tiene que frenar solo.
static const phase_t script[] = {
    {  400, 2000 },
    {    0, 1000 },
    { -400, 4000 },
    {    0, 1000 },
    {  800, 6000 },
    {    0, 1000 },
    { -800, 6000 },
    {    0, 1000 },
};
#define N_PHASES (sizeof(script) / sizeof(script[0]))

int main(void) {
    stdio_init_all();

    // Espera hasta 5 s a que se abra el monitor serie
    absolute_time_t t0 = get_absolute_time();
    while (!stdio_usb_connected() &&
           absolute_time_diff_us(t0, get_absolute_time()) < 5000000) {
        sleep_ms(100);
    }
    sleep_ms(500);

    printf("Test del generador de pasos no bloqueante\n");
    printf("Limites: %d a %d pasos | vmax=%.0f pasos/s | aceleracion=%.0f pasos/s2\n",
           LIMIT_MIN_STEPS, LIMIT_MAX_STEPS, STEPPER_MAX_SPEED_SPS, STEPPER_ACCEL_SPS2);

    stepper_init(PUL_PIN, DIR_PIN);
    stepper_set_limits(LIMIT_MIN_STEPS, LIMIT_MAX_STEPS);
    stepper_set_position(0);

    uint64_t t_start = time_us_64();
    uint64_t t_prev = t_start;
    absolute_time_t next = make_timeout_time_us(LOOP_US);

    size_t idx = 0;
    uint32_t phase_t0_ms = 0;
    uint32_t last_print_ms = 0;
    float max_dt = 0.0f;

    while (true) {
        uint64_t now = time_us_64();
        float dt = (now - t_prev) / 1000000.0f;
        t_prev = now;
        if (dt > max_dt) max_dt = dt;
        uint32_t t_ms = (uint32_t)((now - t_start) / 1000);

        // Avanza por el guion de prueba
        if (t_ms - phase_t0_ms >= script[idx].ms) {
            idx = (idx + 1) % N_PHASES;
            phase_t0_ms = t_ms;
        }

        stepper_set_target_speed(script[idx].sps);
        stepper_update(dt);

        if (t_ms - last_print_ms >= PRINT_MS) {
            last_print_ms = t_ms;
            printf("t=%6.1f s | objetivo=%5.0f | vel=%6.0f | pos=%6ld | limite=%s | dt max=%.1f ms\n",
                   t_ms / 1000.0f, script[idx].sps, stepper_get_speed(),
                   (long)stepper_get_position(), stepper_at_limit() ? "SI" : "no",
                   max_dt * 1000.0f);
            max_dt = 0.0f;
        }

        sleep_until(next);
        next = delayed_by_us(next, LOOP_US);
    }
}
