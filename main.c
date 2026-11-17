#include <stdio.h>
#include "pico/stdlib.h"
#include "config.h"

// Conexión ánodo común: PUL+ y DIR+ a 3V3, PUL- a GP15, DIR- a GP14.
// La corriente circula (optoacoplador encendido) cuando el GPIO está en BAJO.

#define HALF_PERIOD_MS 5      // 5 ms alto + 5 ms bajo = 100 pasos/s
#define STEPS_PER_MOVE 400    // 2 vueltas en paso completo
#define PRINT_EVERY    100    // informa cada 100 pasos (1 s a esta velocidad)

static int32_t position = 0;  // pasos acumulados (+ adelante, - atrás)

static void move(bool dir, int steps) {
    printf("Inicio movimiento: sentido=%s, %d pasos\n", dir ? "adelante" : "atras", steps);

    gpio_put(DIR_PIN, dir);
    sleep_ms(10);

    for (int i = 0; i < steps; i++) {
        gpio_put(PUL_PIN, 0);
        sleep_ms(HALF_PERIOD_MS);
        gpio_put(PUL_PIN, 1);
        sleep_ms(HALF_PERIOD_MS);

        position += dir ? 1 : -1;

        if ((i + 1) % PRINT_EVERY == 0) {
            printf("  paso %d/%d | posicion=%ld\n", i + 1, steps, (long)position);
        }
    }

    printf("Fin movimiento: posicion=%ld\n", (long)position);
}

int main(void) {
    stdio_init_all();

    gpio_init(PUL_PIN); gpio_set_dir(PUL_PIN, GPIO_OUT); gpio_put(PUL_PIN, 1);
    gpio_init(DIR_PIN); gpio_set_dir(DIR_PIN, GPIO_OUT); gpio_put(DIR_PIN, 1);

    // Espera hasta 5 s a que se abra el monitor serie
    absolute_time_t t0 = get_absolute_time();
    while (!stdio_usb_connected() &&
           absolute_time_diff_us(t0, get_absolute_time()) < 5000000) {
        sleep_ms(100);
    }
    sleep_ms(500);

    printf("Test de motor TB6600 iniciado\n");
    sleep_ms(1000);

    while (true) {
        move(true, STEPS_PER_MOVE);
        sleep_ms(1000);
        move(false, STEPS_PER_MOVE);
        sleep_ms(1000);
    }
}