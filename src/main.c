/**
 * @file main.c
 * @author Paulo Santos (prms@ic.ufal.br)
 * @brief Fade e blink com botão.
 * @version 0.1
 * @date 09/10/2025
 *
 * @copyright Copyright (c) 2025 Paulo Santos
 *
 */

#include <zephyr/kernel.h>

#include "zephyr/drivers/pwm.h"
#include "zephyr/drivers/gpio.h"
#include "zephyr/logging/log.h"

/**
 * @brief Especificação do GPIO.
 */
#define BUTTON_GPIO_SPEC GPIO_DT_SPEC_GET(DT_NODELABEL(button0), gpios)

/**
 * @brief Especificação do PWM.
 */
#define PWM_SPEC PWM_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), indicator_led)

/**
 * @brief Resolução do fade.
 */
#define PWM_FADE_STEPS 50

LOG_MODULE_REGISTER(main);

enum modes {
    MODO_1 = 0,
    MODO_2,
};

static struct {
    bool toggle_state;
    uint32_t pwm_pulse;
    enum modes mode;
} self = {
    .mode = MODO_1,
};

const struct pwm_dt_spec pwm_spec = PWM_SPEC;

int blink_thread() {
    while (true) {
        pwm_set_pulse_dt(&pwm_spec, self.toggle_state ? pwm_spec.period : 0);
        self.toggle_state = !self.toggle_state;

        k_msleep(CONFIG_LED_PERIOD_MS);
    }
}

int fade_thread() {
    int8_t direction = 1;
    const uint32_t PWM_STEP_NS = (pwm_spec.period / PWM_FADE_STEPS);

    while (true) {
        for (uint8_t steps = 0; steps < CONFIG_LED_PERIOD_MS / PWM_FADE_STEPS; steps++) {
            self.pwm_pulse += direction * PWM_STEP_NS;
            pwm_set_pulse_dt(&pwm_spec, self.pwm_pulse);
            k_msleep(CONFIG_LED_PERIOD_MS / PWM_FADE_STEPS);
        }
        direction *= -1;
    }
}

K_THREAD_DEFINE(blinking_thread_id, 1048, blink_thread, NULL, NULL, NULL, 5, 0, -1);
K_THREAD_DEFINE(fading_thread_id, 1048, fade_thread, NULL, NULL, NULL, 5, 0, 0);

void button_pressed_cb() {
    switch (self.mode) {
        case MODO_1:
            self.mode = MODO_2;
            self.pwm_pulse = 0;
            k_thread_resume(fading_thread_id);
            k_thread_suspend(blinking_thread_id);
            break;
        case MODO_2:
            self.mode = MODO_1;
            k_thread_resume(blinking_thread_id);
            k_thread_suspend(fading_thread_id);
            break;
    }
}

static struct gpio_callback button_cb;

int main() {
    const struct gpio_dt_spec button_spec = BUTTON_GPIO_SPEC;

    if (!gpio_is_ready_dt(&button_spec)) {
        LOG_ERR("Button gpio configure failed");
        return -ENODEV;
    }

    if (!pwm_is_ready_dt(&pwm_spec)) {
        LOG_ERR("PWM gpio configure failed");
        return -ENODEV;
    }

    if (gpio_pin_configure_dt(&button_spec, GPIO_INPUT | GPIO_PULL_UP) || gpio_pin_interrupt_configure_dt(
            &button_spec, GPIO_INT_EDGE_FALLING)) {
        LOG_ERR("Could not setup button as interrupt.");
        return -ENOTSUP;
    }
    gpio_init_callback(&button_cb, button_pressed_cb, BIT(button_spec.pin));
    gpio_add_callback_dt(&button_spec, &button_cb);

    self.mode = MODO_1;
    k_thread_suspend(fading_thread_id);
    k_thread_start(blinking_thread_id);
}
