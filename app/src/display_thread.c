/**
 * @file display_thread.c
 * @author Paulo Santos (prms@ic.ufal.br)
 * @brief Thread responsável pela exibição dos dados no "Display Virtual" (Console).
 * @version 0.1
 * @date 26-11-2025
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "occurrences.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(display, LOG_LEVEL_INF);

/* Códigos ANSI para cores do terminal. */
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"

ZBUS_MSG_SUBSCRIBER_DEFINE(msub_display);

ZBUS_CHAN_ADD_OBS(infraction_chan, msub_display, 4);
ZBUS_CHAN_ADD_OBS(non_infraction_chan, msub_display, 4);

/**
 * @brief Determina a cor e o limite aplicável baseando-se no veículo e velocidade.
 *
 * @param[in] msg Ponteiro para a mensagem de ocorrência.
 * @param[out] limit_out Ponteiro para retornar o limite de velocidade aplicado.
 * @return String contendo o código de cor ANSI.
 */
static const char* resolve_status_color(const struct occurrence_msg* msg, uint32_t* limit_out) {
    uint32_t limit = 0;

    if (msg->type == VEHICLE_TYPE_HEAVY) {
        limit = CONFIG_RADAR_SPEED_LIMIT_HEAVY_KMH;
    } else {
        limit = CONFIG_RADAR_SPEED_LIMIT_LIGHT_KMH;
    }

    *limit_out = limit;

    if (msg->speed_kmph > limit) {
        return ANSI_COLOR_RED;
    }

    const uint32_t warning_speed = (limit * CONFIG_RADAR_WARNING_THRESHOLD_PERCENT) / 100;

    if (msg->speed_kmph >= warning_speed) {
        return ANSI_COLOR_YELLOW;
    }

    return ANSI_COLOR_GREEN;
}

__NO_RETURN void display_thread(void) {
    const struct zbus_channel* chan;
    struct occurrence_msg msg;
    uint32_t current_limit;

    while (true) {
        if (zbus_sub_wait_msg(&msub_display, &chan, &msg, K_FOREVER) != 0) {
            continue;
        }

        /* Define string legível para o tipo */
        const char* vehicle_str = (msg.type == VEHICLE_TYPE_HEAVY) ? "PESADO" : "LEVE  ";

        /* Calcula cor e recupera limite */
        const char* color_code = resolve_status_color(&msg, &current_limit);

        /* * Renderiza no "Display" (Console).
         * Formato: [ VELOCIDADE ] [ TIPO ] [ STATUS ] [ PLACA? ]
         */
        printk("\r\n%s========================================%s\n", color_code, ANSI_COLOR_RESET);

        printk("%s[RADAR] Velocidade: %3d km/h (Lim: %d)%s\n",
               color_code, msg.speed_kmph, current_limit, ANSI_COLOR_RESET);

        printk("%s[CLASS] Veiculo: %s%s\n",
               color_code, vehicle_str, ANSI_COLOR_RESET);

        if (chan == &infraction_chan) {
            printk("%s[ALERT] INFRAÇÃO REGISTRADA! PLACA: %s%s\n",
                   color_code, msg.plate, ANSI_COLOR_RESET);
        } else {
            printk("%s[INFO ] Passagem Segura%s\n", color_code, ANSI_COLOR_RESET);
        }

        printk("%s========================================%s\n", color_code, ANSI_COLOR_RESET);
    }
}

K_THREAD_DEFINE(display_thread_id, 1024, display_thread, NULL, NULL, NULL, 7, 0, 0);
