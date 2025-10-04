/**
 * @file main.c
 * @author Paulo Santos (prms@ic.ufal.br)
 * @brief Timer simples.
 * @version 0.1
 * @date 03/10/2025
 *
 * @copyright Copyright (c) 2025 Paulo Santos
 *
 */

#include <zephyr/kernel.h>

#include "zephyr/logging/log.h"

LOG_MODULE_REGISTER(main);

void hello_timer_expiry_cb(struct k_timer* timer) {
    ARG_UNUSED(timer);

    LOG_DBG("Hello World (from debug!!!!)");
    LOG_INF("Hello World (from info!!!)");
    LOG_WRN("Hello World (from warning!!)");
    LOG_ERR("Hello World (from error!)");

    LOG_WRN_ONCE("Hello World (from very shy warning?!)");
}


K_TIMER_DEFINE(hello_timer, hello_timer_expiry_cb, NULL);

int main() {
    k_timer_start(&hello_timer, K_NO_WAIT, K_MSEC(CONFIG_HELLO_TIMER_PERIOD_MS));
}
