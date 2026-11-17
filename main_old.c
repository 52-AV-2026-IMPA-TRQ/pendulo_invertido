#include "pico/stdlib.h"
#include "config.h"

// Conexión ánodo común: PUL+ y DIR+ a 3V3, PUL- a GP15, DIR- a GP14.
// La corriente circula (optoacoplador encendido) cuando el GPIO está en BAJO.

#define HALF_PERIOD_MS 5      // 5 ms alto + 5 ms bajo = 100 pasos/s
#define STEPS_PER_MOVE 400    // 2 vueltas en paso completo

static void move(bool dir, int steps) {
    gpio_put(DIR_PIN, dir);
    sleep_ms(10);
    for (int i = 0; i < steps; i++) {
        gpio_put(PUL_PIN, 0);
        sleep_ms(HALF_PERIOD_MS);
        gpio_put(PUL_PIN, 1);
        sleep_ms(HALF_PERIOD_MS);
    }
}

int main(void) {
    gpio_init(PUL_PIN); gpio_set_dir(PUL_PIN, GPIO_OUT); gpio_put(PUL_PIN, 1);
    gpio_init(DIR_PIN); gpio_set_dir(DIR_PIN, GPIO_OUT); gpio_put(DIR_PIN, 1);
    sleep_ms(1000);

    while (true) {
        move(true, STEPS_PER_MOVE);
        sleep_ms(1000);
        move(false, STEPS_PER_MOVE);
        sleep_ms(1000);
    }
}
