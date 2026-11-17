#pragma once
#include <stdbool.h>

void  mpu_init(void);
float mpu_calibrate_gyro(int n);              // sensor quieto
bool  mpu_update(float dt, float *angle_deg, float *rate_dps);
