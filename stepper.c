#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "stepper.h"

// Salida: la corriente del optoacoplador circula con el pin en BAJO.
// Reposo = pin en ALTO (nivel del PWM por encima del wrap = siempre alto).
// Cada periodo del PWM = 1 pulso (alto la primera mitad, bajo la segunda).

#define IDLE_LEVEL    0xFFFF
#define IDLE_FREQ_HZ  1000.0f   // en reposo el PWM sigue corriendo a 1 kHz para poder arrancar rápido

typedef struct {
    uint32_t div;      // divisor en unidades de 1/16 (formato del registro DIV)
    uint16_t wrap;
    uint16_t level;
    bool     pulsing;
} pwm_cfg_t;

static unsigned int dir_gpio, slice, channel;
static float clk_hz;

// Estado compartido con la interrupción
static volatile int32_t  position = 0;
static volatile int8_t   cur_dir = 1;       // dirección aplicada en el pin DIR
static volatile int8_t   want_dir = 1;      // dirección pedida
static volatile bool     pulsing = false;
static volatile int32_t  limit_min = -2000000000;
static volatile int32_t  limit_max = 2000000000;
static volatile uint32_t last_update_us = 0;

// Configuración pendiente: se aplica justo al final de un periodo (sin glitches)
static volatile bool     pending_valid = false;
static volatile uint32_t p_div = 0;
static volatile uint16_t p_wrap = 0;
static volatile uint16_t p_level = IDLE_LEVEL;
static volatile bool     p_pulsing = false;

// Estado del lado del lazo principal
static float target_speed = 0.0f;
static float cur_speed = 0.0f;

static pwm_cfg_t make_cfg(float hz, bool pulse) {
    pwm_cfg_t c;
    c.pulsing = pulse;
    if (!pulse) hz = IDLE_FREQ_HZ;

    // Divisor más chico que permita contar un periodo completo en 16 bits
    float div = clk_hz / (hz * 65535.0f);
    if (div < 1.0f) div = 1.0f;
    uint32_t div16 = (uint32_t)(div * 16.0f + 0.999f);
    if (div16 < 16)   div16 = 16;
    if (div16 > 4095) div16 = 4095;
    c.div = div16;

    uint32_t counts = (uint32_t)(clk_hz / (((float)div16 / 16.0f) * hz) + 0.5f);
    if (counts > 65535) counts = 65535;
    if (counts < 4)     counts = 4;
    c.wrap  = (uint16_t)(counts - 1);                    // wrap <= 65534
    c.level = pulse ? (uint16_t)(c.wrap / 2) : IDLE_LEVEL;
    return c;
}

static void apply_cfg_now(const pwm_cfg_t *c) {
    pwm_hw->slice[slice].div = c->div;
    pwm_set_wrap(slice, c->wrap);
    pwm_set_chan_level(slice, channel, c->level);
}

static inline void force_idle(void) {
    pwm_set_chan_level(slice, channel, IDLE_LEVEL);
    pulsing = false;
    pending_valid = false;
}

// Se ejecuta al final de cada periodo del PWM (= un paso si estaba pulsando)
static void pwm_isr(void) {
    pwm_clear_irq(slice);

    bool was_pulsing = pulsing;

    if (pulsing) {
        position += cur_dir;

        // Límite de software (seguro de último recurso: el lazo principal frena antes)
        if ((cur_dir > 0 && position >= limit_max) ||
            (cur_dir < 0 && position <= limit_min)) {
            force_idle();
            return;
        }

        // Watchdog: si el lazo principal dejó de llamar a stepper_update(), corta
        if ((uint32_t)(time_us_32() - last_update_us) > STEPPER_WATCHDOG_US) {
            force_idle();
            return;
        }
    }

    if (pending_valid) {
        pwm_hw->slice[slice].div = p_div;
        pwm_set_wrap(slice, p_wrap);
        pwm_set_chan_level(slice, channel, p_level);
        pulsing = p_pulsing;
        pending_valid = false;
    }

    // DIR solo se cambia con el motor en reposo, un periodo después del último pulso
    if (!pulsing && !was_pulsing && want_dir != cur_dir) {
        gpio_put(dir_gpio, want_dir > 0 ? 1 : 0);
        cur_dir = want_dir;
    }
}

void stepper_init(unsigned int pul_pin, unsigned int dir_pin) {
    dir_gpio = dir_pin;
    gpio_init(dir_pin);
    gpio_set_dir(dir_pin, GPIO_OUT);
    gpio_put(dir_pin, 1);
    cur_dir = 1;
    want_dir = 1;

    clk_hz = (float)clock_get_hz(clk_sys);

    gpio_set_function(pul_pin, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(pul_pin);
    channel = pwm_gpio_to_channel(pul_pin);

    pwm_set_enabled(slice, false);
    pwm_cfg_t idle = make_cfg(0.0f, false);
    apply_cfg_now(&idle);
    pulsing = false;
    pending_valid = false;
    last_update_us = time_us_32();

    pwm_clear_irq(slice);
    pwm_set_irq_enabled(slice, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, pwm_isr);
    irq_set_enabled(PWM_IRQ_WRAP, true);

    pwm_set_enabled(slice, true);
}

void stepper_set_limits(int32_t min_steps, int32_t max_steps) {
    uint32_t s = save_and_disable_interrupts();
    limit_min = min_steps;
    limit_max = max_steps;
    restore_interrupts(s);
}

void stepper_set_position(int32_t steps) {
    uint32_t s = save_and_disable_interrupts();
    position = steps;
    restore_interrupts(s);
}

void stepper_set_target_speed(float steps_per_s) {
    if (steps_per_s >  STEPPER_MAX_SPEED_SPS) steps_per_s =  STEPPER_MAX_SPEED_SPS;
    if (steps_per_s < -STEPPER_MAX_SPEED_SPS) steps_per_s = -STEPPER_MAX_SPEED_SPS;
    target_speed = steps_per_s;
}

void stepper_update(float dt) {
    last_update_us = time_us_32();

    // 1) Rampa de aceleración hacia la velocidad deseada
    float dv = STEPPER_ACCEL_SPS2 * dt;
    if (target_speed > cur_speed + dv)      cur_speed += dv;
    else if (target_speed < cur_speed - dv) cur_speed -= dv;
    else                                    cur_speed = target_speed;

    // 2) Frenado anticipado antes de los límites: v <= sqrt(2 * a * pasos_restantes)
    int32_t pos = position;
    int32_t rem = (cur_speed >= 0.0f) ? (limit_max - pos) : (pos - limit_min);
    if (rem < 0) rem = 0;
    float v_cap = sqrtf(2.0f * STEPPER_ACCEL_SPS2 * (float)rem);
    if (cur_speed >  v_cap) cur_speed =  v_cap;
    if (cur_speed < -v_cap) cur_speed = -v_cap;

    // 3) Convertir la velocidad en configuración del PWM
    int8_t d = (cur_speed >= 0.0f) ? 1 : -1;
    bool pulse = fabsf(cur_speed) >= STEPPER_MIN_SPEED_SPS;

    if (pulse) {
        want_dir = d;
        if (d != cur_dir) pulse = false;   // primero se detiene y se cambia DIR, después arranca
    }

    pwm_cfg_t c = make_cfg(fabsf(cur_speed), pulse);

    uint32_t s = save_and_disable_interrupts();
    p_div = c.div;
    p_wrap = c.wrap;
    p_level = c.level;
    p_pulsing = c.pulsing;
    pending_valid = true;
    restore_interrupts(s);
}

int32_t stepper_get_position(void) {
    return position;
}

float stepper_get_speed(void) {
    return cur_speed;
}

bool stepper_at_limit(void) {
    int32_t pos = position;
    return (pos >= limit_max) || (pos <= limit_min);
}
