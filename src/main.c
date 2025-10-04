/**
 * @file main.c
 * @author João Anon (email@edge.ufal.br)
 * @brief
 * @version 0.1
 * @date dd-mm-aaaa
 *
 * @copyright Copyright (c) aaaa
 *
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/**
 * @brief Nó de exemplo contendo o led de testes.
 */
#define LED_GPIO DT_NODELABEL(node_a_label)

/**
 * @brief Especificação do GPIO de led de testes.
 */
static const struct gpio_dt_spec led_exemplo = GPIO_DT_SPEC_GET(LED_GPIO, gpios);

int main() {
    /* Verifica se o dispositivo está pronto. */
    if (gpio_is_ready_dt(&led_exemplo) != 0) {
        return -EBUSY;
    }

    /* Configura o GPIO como saída, e inicia seu valor para 0. */
    if (gpio_pin_configure_dt(&led_exemplo, GPIO_OUTPUT_INACTIVE) != 0) {
        return -ENODEV;
    }

    /* Blink. */
    while (true) {
        gpio_pin_toggle_dt(&led_exemplo);

        k_msleep(1000);
    }
}
