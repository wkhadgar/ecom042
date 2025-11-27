/**
 * @file main.c
 * @author Paulo Santos (prms@ic.ufal.br)
 * @brief Simulated Classifier Radar.
 * @version 0.1
 * @date 25/11/2025
 *
 * Projeto Final - Radar Eletrônico com Classificação
 *
 * @copyright Copyright (c) 2025
 *
 */


#include "radar_sensing.h"
#include "camera_service.h"
#include "occurrences.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

ZBUS_MSG_SUBSCRIBER_DEFINE(msub_camera_evt);
ZBUS_CHAN_ADD_OBS(chan_camera_evt, msub_camera_evt, 3);


static bool is_speeding(const struct occurrence_msg* occurrence) {
    if (occurrence->type == VEHICLE_TYPE_HEAVY && occurrence->speed_kmph > CONFIG_RADAR_SPEED_LIMIT_HEAVY_KMH) {
        return true;
    }

    if (occurrence->type == VEHICLE_TYPE_LIGHT && occurrence->speed_kmph > CONFIG_RADAR_SPEED_LIMIT_LIGHT_KMH) {
        return true;
    }

    return false;
}

_Noreturn int main(void) {
    const struct zbus_channel* chan;
    struct sensor_data_msg sensor_data_msg;
    struct occurrence_msg occurrence;


    while (true) {
        if (k_msgq_get(&sensor_msgq, &sensor_data_msg,K_FOREVER) != 0) {
            continue;
        }

        occurrence.plate = "";
        occurrence.type = sensor_data_msg.type;
        occurrence.speed_kmph = sensor_data_msg.speed_kmph;

        if (!is_speeding(&occurrence)) {
            if (zbus_chan_pub(&non_infraction_chan, &occurrence, K_FOREVER) != 0) {
                LOG_WRN("Não foi possível registrar o veiculo (permissivo).");
            }
            continue;
        }

        if (camera_api_capture(K_FOREVER) != 0) {
            continue;
        }

        struct msg_camera_evt rsp;

        const int err = zbus_sub_wait_msg(&msub_camera_evt, &chan, &rsp, K_FOREVER);

        if (err) {
            LOG_ERR("Não foi possível obter a placa: %d\n", err);
            continue;
        }

        if (chan != &chan_camera_evt) {
            continue;
        }

        if (rsp.type != MSG_CAMERA_EVT_TYPE_DATA) {
            LOG_ERR("Camera indisponível: %d\n", rsp.error_code);
            continue;
        }

        occurrence.plate = rsp.captured_data->plate;

        if (zbus_chan_pub(&infraction_chan, &occurrence, K_FOREVER) == -ENOMSG) {
            LOG_WRN("Não foi possível registrar o veiculo infrator.");
        }
    }
}
