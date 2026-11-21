#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "config.h"

// Conexión ánodo común: PUL+ y DIR+ a 3V3, PUL- a GP15, DIR- a GP14.
// La corriente circula (optoacoplador encendido) cuando el GPIO está en BAJO.

// ---------- Parámetros (editá estos valores) ----------
#define STEPS_PER_MOVE  2000   // pasos por tramo (2000 = 10 vueltas en paso completo)
#define START_SPEED_SPS 200    // velocidad de arranque y frenado final (pasos/s)
#define MAX_SPEED_SPS   2000 // velocidad máxima (pasos/s): subila de a poco
#define ACCEL_SPS2      2000   // aceleración (pasos/s por segundo): bajala si pierde pasos
// -------------------------------------------------------

static int32_t position = 0;  // pasos acumulados (+ adelante, - atrás)

static void move(bool dir, int steps) {
    printf("Inicio: sentido=%s, %d pasos, vmax=%d pasos/s\n",
           dir ? "adelante" : "atras", steps, MAX_SPEED_SPS);

    gpio_put(DIR_PIN, dir);
    sleep_ms(10);                       // DIR estable antes de pulsar

    const float v0sq = (float)START_SPEED_SPS * START_SPEED_SPS;
    uint32_t t_start = time_us_32();

    for (int i = 0; i < steps; i++) {
        // Perfil trapezoidal: acelera al inicio, mantiene, frena al final
        float v_acc = sqrtf(v0sq + 2.0f * ACCEL_SPS2 * (float)i);
        float v_dec = sqrtf(v0sq + 2.0f * ACCEL_SPS2 * (float)(steps - 1 - i));
        float v = (float)MAX_SPEED_SPS;
        if (v_acc < v) v = v_acc;
        if (v_dec < v) v = v_dec;

        uint32_t half_us = (uint32_t)(500000.0f / v);

        gpio_put(PUL_PIN, 0);           // pulso (optoacoplador encendido)
        sleep_us(half_us);
        gpio_put(PUL_PIN, 1);           // reposo
        sleep_us(half_us);

        position += dir ? 1 : -1;
    }

    uint32_t ms = (time_us_32() - t_start) / 1000;
    printf("Fin: posicion=%ld, tiempo=%lu ms\n", (long)position, (unsigned long)ms);
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

    printf("Test de motor TB6600 con rampa iniciado\n");
    sleep_ms(1000);

    while (true) {
        move(true, STEPS_PER_MOVE);
        sleep_ms(800);
        move(false, STEPS_PER_MOVE);
        sleep_ms(800);
    }
}
















#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "config.h"
#include "mpu6050.h"

#define PRINT_EVERY 10   // el lazo corre a 200 Hz: imprime cada 10 ciclos (20 Hz)

int main(void) {
    stdio_init_all();

    // I2C para la MPU6050
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // Espera hasta 5 s a que se abra el monitor serie
    absolute_time_t t0 = get_absolute_time();
    while (!stdio_usb_connected() &&
           absolute_time_diff_us(t0, get_absolute_time()) < 5000000) {
        sleep_ms(100);
    }
    sleep_ms(500);

    printf("Test de MPU6050\n");
    mpu_init();

    printf("Calibrando giroscopio: no muevas el sensor...\n");
    sleep_ms(3000);   // calentamiento del sensor
    float offset = mpu_calibrate_gyro(1000);
    printf("Offset del giroscopio: %.3f deg/s\n", offset);
    printf("Columnas: theta[deg], omega[deg/s]\n");

    float theta = 0.0f;   // ángulo del péndulo (grados)
    float omega = 0.0f;   // velocidad angular (grados/s)

    uint64_t t_prev = time_us_64();
    absolute_time_t next = make_timeout_time_us(LOOP_US);
    int count = 0;

    while (true) {
        uint64_t t_now = time_us_64();
        float dt = (t_now - t_prev) / 1000000.0f;   // dt real de cada ciclo
        t_prev = t_now;

        bool ok = mpu_update(dt, &theta, &omega);

        if (++count >= PRINT_EVERY) {
            count = 0;
            if (ok) {
                printf("%.2f, %.2f\n", theta, omega);
            } else {
                printf("Error MPU6050: revisa SDA/SCL, 3V3 y GND\n");
            }
        }

        sleep_until(next);
        next = delayed_by_us(next, LOOP_US);
    }
}