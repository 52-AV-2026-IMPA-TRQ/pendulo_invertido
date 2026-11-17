#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "config.h"
#include "mpu6050.h"

#define ACC_SCALE 16384.0f   // LSB/g   (±2 g)
#define GYR_SCALE 131.0f     // LSB/°/s (±250 °/s)
#define RAD2DEG   57.2957795f

static float gyro_offset = 0.0f;
static float angle = 0.0f;
static bool  first = true;

static void mpu_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(I2C_PORT, MPU_ADDR, buf, 2, false);
}

static bool mpu_read(float *ax, float *ay, float *az, float *gx) {
    uint8_t reg = 0x3B;
    uint8_t d[14];
    if (i2c_write_blocking(I2C_PORT, MPU_ADDR, &reg, 1, true) < 0) return false;
    if (i2c_read_blocking(I2C_PORT, MPU_ADDR, d, 14, false) < 0) return false;

    *ax = (int16_t)((d[0] << 8) | d[1])  / ACC_SCALE;
    *ay = (int16_t)((d[2] << 8) | d[3])  / ACC_SCALE;
    *az = (int16_t)((d[4] << 8) | d[5])  / ACC_SCALE;
    *gx = (int16_t)((d[8] << 8) | d[9])  / GYR_SCALE;   // eje del giroscopio: cambia según el montaje
    return true;
}

void mpu_init(void) {
    mpu_write(0x6B, 0x00);   // sale de sleep
    mpu_write(0x1A, 0x03);   // filtro pasa bajos ~44 Hz
    mpu_write(0x1B, 0x00);   // giroscopio ±250 °/s
    mpu_write(0x1C, 0x00);   // acelerómetro ±2 g
    sleep_ms(100);
}

float mpu_calibrate_gyro(int n) {
    float ax, ay, az, gx, total = 0.0f;
    int ok = 0;
    for (int i = 0; i < n; i++) {
        if (mpu_read(&ax, &ay, &az, &gx)) { total += gx; ok++; }
        sleep_ms(2);
    }
    gyro_offset = ok ? total / ok : 0.0f;
    return gyro_offset;
}

bool mpu_update(float dt, float *angle_deg, float *rate_dps) {
    float ax, ay, az, gx;
    if (!mpu_read(&ax, &ay, &az, &gx)) return false;

    float acc_angle = atan2f(ay, az) * RAD2DEG;   // cambiá ejes/signo según el montaje
    float rate = gx - gyro_offset;

    if (first) { angle = acc_angle; first = false; }
    angle = MPU_ALPHA * (angle + rate * dt) + (1.0f - MPU_ALPHA) * acc_angle;

    *angle_deg = angle;
    *rate_dps = rate;
    return true;
}
