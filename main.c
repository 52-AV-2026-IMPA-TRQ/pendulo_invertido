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