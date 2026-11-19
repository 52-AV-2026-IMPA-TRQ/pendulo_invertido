#pragma once

// ---------- Pines ----------
// Motor (TB6600, ánodo común: PUL+/DIR+ a 3V3, PUL-/DIR- a estos pines)
#define PUL_PIN       15
#define DIR_PIN       14

// MPU6050 (I2C0)
#define I2C_PORT      i2c0
#define I2C_SDA_PIN   4
#define I2C_SCL_PIN   5
#define MPU_ADDR      0x68

// Final de carrera (con pull-up interno, activo en bajo)
#define LIMIT_PIN     16

// ---------- Control ----------
#define LOOP_HZ       200
#define LOOP_US       (1000000 / LOOP_HZ)

#define ANGLE_LIMIT_DEG  35.0f   // si se pasa de este ángulo, se corta el motor
#define MICROSTEP        1       // según el DIP del TB6600

// Ganancias del PID (se sintonizan después)
#define KP  0.0f
#define KI  0.0f
#define KD  0.0f

// ---------- MPU6050 ----------
#define MPU_ALPHA        0.998f
