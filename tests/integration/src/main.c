/**
 * @file main.c
 * @author Paulo Santos (pauloroberto.santos@edge.ufal.br)
 * @brief Testes de integração.
 * @version 0.1
 * @date 26-11-2025
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "radar_sensing.h"
#include "occurrences.h"

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "camera_service.h"
#include "../../../../zephyr/subsys/testsuite/ztest/include/zephyr/ztest_assert.h"
#include "../../../app/include/radar_sensing.h"
#include "zephyr/logging/log.h"
#include "zephyr/zbus/zbus.h"

ZTEST_SUITE(radar_integration, NULL, NULL
,
NULL
,
NULL
,
NULL
);

extern void sensor1_isr_callback(const struct device* dev, struct gpio_callback* cb, uint32_t pins);

extern void sensor2_isr_callback(const struct device* dev, struct gpio_callback* cb, uint32_t pins);

extern struct k_msgq sensor_msgq;

/* Simula a passagem do tempo e dispara o Sensor 1 */
static void hit_sensor_1(const uint32_t wait_us) {
    if (wait_us > 0) {
        k_busy_wait(wait_us);
    }

    sensor1_isr_callback(NULL, NULL, 0);
}

/* Simula a passagem do tempo e dispara o Sensor 2 */
static void hit_sensor_2(const uint32_t wait_us) {
    if (wait_us > 0) {
        k_busy_wait(wait_us);
    }

    sensor2_isr_callback(NULL, NULL, 0);
}

ZTEST(radar_integration, test_sensor_fsm) {
    k_msgq_purge(&sensor_msgq);

    zassert_equal(sensor_get_state(), RADAR_STATE_IDLE, "Estado inicial errado");
    hit_sensor_1(0); /* Eixo 1 */
    zassert_equal(sensor_get_state(), RADAR_STATE_SPEED_MEASURING, "Estado errado apos primeiro toque do sensor 1");
    hit_sensor_1(90000); /* Eixo 2 (aprox 2.6m depois) */
    zassert_equal(sensor_get_state(), RADAR_STATE_SPEED_MEASURING, "Estado errado apos toque em sensor 1");
    hit_sensor_2(10000); /* Eixo 1 no S2 (100ms = 30m/s = 108 km/h) */
    zassert_equal(sensor_get_state(), RADAR_STATE_OBSERVING, "Estado errado apos primeiro toque do sensor 2");

    k_sleep(K_MSEC(1000)); /* A 54km/h, o timeout de 12m seria ~800ms. Esperamos 1s. */

    zassert_equal(sensor_get_state(), RADAR_STATE_IDLE, "Estado errado apos fim do veiculo 1");
    hit_sensor_1(0); /* Eixo 1 */
    zassert_equal(sensor_get_state(), RADAR_STATE_SPEED_MEASURING, "Estado errado apos 'primeiro' toque do sensor 1");
    hit_sensor_2(200000); /* Fecha Velocidade no S2 (após 200ms = 54 km/h para 3m) */
    zassert_equal(sensor_get_state(), RADAR_STATE_OBSERVING, "Estado errado apos primeiro toque do sensor 2");
    hit_sensor_1(80000); /* Eixo 2 (Tandem - Gap curto de 80ms) */
    zassert_equal(sensor_get_state(), RADAR_STATE_OBSERVING, "Estado errado apos toque subsequente do sensor 1");
    hit_sensor_1(80000); /* Eixo 3 (Tandem - Gap curto de 80ms) */
    zassert_equal(sensor_get_state(), RADAR_STATE_OBSERVING, "Estado errado apos toque subsequente do sensor 1");

    k_sleep(K_MSEC(1000)); /* A 54km/h, o timeout de 12m seria ~800ms. Esperamos 1s. */
    zassert_equal(sensor_get_state(), RADAR_STATE_IDLE, "Estado errado apos fim do veiculo 2");
}

ZTEST(radar_integration, test_light_vehicle) {
    struct sensor_data_msg msg;
    k_msgq_purge(&sensor_msgq);

    hit_sensor_1(0); /* Eixo 1 */
    hit_sensor_1(90000); /* Eixo 2 (aprox 2.6m depois) */
    hit_sensor_2(10000); /* Eixo 1 no S2 (100ms = 30m/s = 108 km/h) */


    k_sleep(K_MSEC(1000)); /* A 54km/h, o timeout de 12m seria ~800ms. Esperamos 1s. */

    zassert_equal(k_msgq_get(&sensor_msgq, &msg, K_NO_WAIT), 0, "Fila vazia!");
    zassert_equal(msg.type, VEHICLE_TYPE_LIGHT, "Nao classificou como PESADO");
    zassert_within(msg.speed_kmph, 108, 2, "Nao calculou a velocidade correta");
}

ZTEST(radar_integration, test_heavy_vehicle) {
    struct sensor_data_msg msg;
    k_msgq_purge(&sensor_msgq);

    hit_sensor_1(0); /* Eixo 1 */
    hit_sensor_2(200000); /* Fecha Velocidade no S2 (após 200ms = 54 km/h para 3m) */
    hit_sensor_1(80000); /* Eixo 2 (Tandem - Gap curto de 80ms) */
    hit_sensor_1(80000); /* Eixo 3 (Tandem - Gap curto de 80ms) */


    k_sleep(K_MSEC(1000)); /* A 54km/h, o timeout de 12m seria ~800ms. Esperamos 1s. */

    zassert_equal(k_msgq_get(&sensor_msgq, &msg, K_NO_WAIT), 0, "Fila vazia!");
    zassert_equal(msg.type, VEHICLE_TYPE_HEAVY, "Nao classificou como PESADO");
    zassert_within(msg.speed_kmph, 54, 2, "Nao calculou a velocidade correta");
}

ZTEST(radar_integration, test_tailgating_cars) {
    struct sensor_data_msg msg;
    k_msgq_purge(&sensor_msgq);

    /* Veiculo 1 */
    hit_sensor_1(0); /* Eixo 1 */
    hit_sensor_1(90000); /* Eixo 2 (aprox 2.6m depois) */
    hit_sensor_2(10000); /* Eixo 1 no S2 (100ms = 30m/s = 108 km/h) */

    /* Veiculo 2 */
    /* A 30m/s (108km/h), 15m leva 500ms (500000us), se o limite é 12m, isso DEVE disparar o corte imediato. */
    hit_sensor_1(500000);
    hit_sensor_1(90000);
    hit_sensor_2(10000);

    k_busy_wait(500000);

    zassert_equal(k_msgq_get(&sensor_msgq, &msg, K_MSEC(1)), 0, "Veiculo 1 nao foi detectado");
    zassert_equal(msg.type, VEHICLE_TYPE_LIGHT, "Veiculo 1 deveria ser LEVE");
    zassert_within(msg.speed_kmph, 108, 1, "Nao calculou a velocidade correta");

    zassert_equal(k_msgq_get(&sensor_msgq, &msg, K_MSEC(1)), 0, "Veiculo 2 nao foi detectado");
    zassert_equal(msg.type, VEHICLE_TYPE_LIGHT, "Veiculo 2 deveria ser LEVE");
    zassert_within(msg.speed_kmph, 108, 1, "Nao calculou a velocidade correta");
}

ZTEST(radar_integration, test_tailgating_trucks) {
    struct sensor_data_msg msg;
    k_msgq_purge(&sensor_msgq);

    hit_sensor_1(1000000); /* Eixo 1 */
    hit_sensor_2(200000); /* Fecha Velocidade no S2 (após 200ms = 54 km/h para 3m) */
    hit_sensor_1(80000); /* Eixo 2 (Tandem - Gap curto de 80ms) */
    hit_sensor_1(80000); /* Eixo 3 (Tandem - Gap curto de 80ms) */
    hit_sensor_1(120000); /* Eixo 4 (Tandem - Gap maior de 120ms) */
    hit_sensor_1(80000); /* Eixo 5 (Tandem - Gap curto de 80ms) */

    /* A 54km/h, o timeout de 12m seria ~800ms. Esperamos 1s para o próximo veículo. */
    hit_sensor_1(1000000); /* Eixo 1 */
    hit_sensor_2(200000); /* Fecha Velocidade no S2 (após 200ms = 54 km/h para 3m) */
    hit_sensor_1(80000); /* Eixo 2 (Tandem - Gap curto de 80ms) */
    hit_sensor_1(80000); /* Eixo 3 (Tandem - Gap curto de 80ms) */

    k_sleep(K_MSEC(1000));

    zassert_equal(k_msgq_get(&sensor_msgq, &msg, K_MSEC(1)), 0, "Veiculo 1 nao foi detectado");
    zassert_equal(msg.type, VEHICLE_TYPE_HEAVY, "Veiculo 1 deveria ser pesado");
    zassert_within(msg.speed_kmph, 54, 1, "Nao calculou a velocidade correta");

    zassert_equal(k_msgq_get(&sensor_msgq, &msg, K_MSEC(1)), 0, "Veiculo 2 nao foi detectado");
    zassert_equal(msg.type, VEHICLE_TYPE_HEAVY, "Veiculo 2 deveria ser pesado");
    zassert_within(msg.speed_kmph, 54, 1, "Nao calculou a velocidade correta");
}

ZTEST(radar_integration, test_tailgating_car_truck) {
    struct sensor_data_msg msg;
    k_msgq_purge(&sensor_msgq);

    hit_sensor_1(0); /* Eixo 1 */
    hit_sensor_1(90000); /* Eixo 2 (aprox 2.6m depois) */
    hit_sensor_2(110000); /* Eixo 1 no S2 (200ms = 15m/s = 54 km/h) */

    /* A 54km/h, o timeout de 12m seria ~800ms. Esperamos 1s para o próximo veículo. */
    hit_sensor_1(1000000); /* Eixo 1 */
    hit_sensor_2(200000); /* Fecha Velocidade no S2 (após 200ms = 54 km/h para 3m) */
    hit_sensor_1(80000); /* Eixo 2 (Tandem - Gap curto de 80ms) */
    hit_sensor_1(80000); /* Eixo 3 (Tandem - Gap curto de 80ms) */

    k_sleep(K_MSEC(1000));

    zassert_equal(k_msgq_get(&sensor_msgq, &msg, K_MSEC(1)), 0, "Veiculo 1 nao foi detectado");
    zassert_equal(msg.type, VEHICLE_TYPE_LIGHT, "Veiculo 1 deveria ser leve");
    zassert_within(msg.speed_kmph, 54, 1, "Nao calculou a velocidade correta");

    zassert_equal(k_msgq_get(&sensor_msgq, &msg, K_MSEC(1)), 0, "Veiculo 2 nao foi detectado");
    zassert_equal(msg.type, VEHICLE_TYPE_HEAVY, "Veiculo 2 deveria ser pesado");
    zassert_within(msg.speed_kmph, 54, 1, "Nao calculou a velocidade correta");
}

ZTEST(radar_integration, test_tailgating_truck_car) {
    struct sensor_data_msg msg;
    k_msgq_purge(&sensor_msgq);

    hit_sensor_1(0); /* Eixo 1 */
    hit_sensor_2(200000); /* Fecha Velocidade no S2 (após 200ms = 54 km/h para 3m) */
    hit_sensor_1(80000); /* Eixo 2 (Tandem - Gap curto de 80ms) */
    hit_sensor_1(80000); /* Eixo 3 (Tandem - Gap curto de 80ms) */

    /* A 54km/h, o timeout de 12m seria ~800ms. Esperamos 1s para o próximo veículo. */
    hit_sensor_1(1000000); /* Eixo 1 */
    hit_sensor_1(90000); /* Eixo 2 (aprox 2.6m depois) */
    hit_sensor_2(110000); /* Eixo 1 no S2 (200ms = 15m/s = 54 km/h) */

    k_sleep(K_MSEC(1000));

    zassert_equal(k_msgq_get(&sensor_msgq, &msg, K_MSEC(1)), 0, "Veiculo 1 nao foi detectado");
    zassert_equal(msg.type, VEHICLE_TYPE_HEAVY, "Veiculo 2 deveria ser pesado");
    zassert_within(msg.speed_kmph, 54, 1, "Nao calculou a velocidade correta");

    zassert_equal(k_msgq_get(&sensor_msgq, &msg, K_MSEC(1)), 0, "Veiculo 2 nao foi detectado");
    zassert_equal(msg.type, VEHICLE_TYPE_LIGHT, "Veiculo 1 deveria ser leve");
    zassert_within(msg.speed_kmph, 54, 1, "Nao calculou a velocidade correta");
}

ZBUS_MSG_SUBSCRIBER_DEFINE(msub_camera_evt);
ZBUS_CHAN_ADD_OBS(chan_camera_evt, msub_camera_evt, 3);

ZTEST(radar_integration, test_zbus_flow) {
    struct sensor_data_msg sensor_data_msg;
    const struct zbus_channel* chan;
    struct occurrence_msg occurrence;

    k_msgq_purge(&sensor_msgq);

    hit_sensor_1(0); /* Eixo 1 */
    hit_sensor_2(100000); /* Fecha Velocidade no S2 (após 100ms = 108 km/h para 3m) */
    hit_sensor_1(40000); /* Eixo 2 (Tandem - Gap curto de 40ms) */
    hit_sensor_1(40000); /* Eixo 3 (Tandem - Gap curto de 40ms) */

    k_sleep(K_MSEC(500));

    zassert_equal(k_msgq_get(&sensor_msgq, &sensor_data_msg, K_MSEC(1)), 0, "Veiculo 1 nao foi detectado");
    zassert_equal(sensor_data_msg.type, VEHICLE_TYPE_HEAVY, "Veiculo 2 deveria ser pesado");
    zassert_within(sensor_data_msg.speed_kmph, 108, 1, "Nao calculou a velocidade correta");

    zassert_equal(camera_api_capture(K_FOREVER), 0, "API da camera indisponivel");

    struct msg_camera_evt rsp;

    zassert_equal(zbus_sub_wait_msg(&msub_camera_evt, &chan, &rsp, K_FOREVER), 0, "Nao foi possivel obter uma placa");

    zassert_equal(chan, &chan_camera_evt, "Canal espurio");

    if (rsp.type != MSG_CAMERA_EVT_TYPE_DATA) {
        printk("I: Camera falhou de forma esperada.\n");
        zassert_not_equal(rsp.type, MSG_CAMERA_EVT_TYPE_UNDEFINED, "Camera nao funcionou corretamente");
        ztest_test_skip();
    }

    occurrence.plate = rsp.captured_data->plate;
    occurrence.type = sensor_data_msg.type;
    occurrence.speed_kmph = sensor_data_msg.speed_kmph;

    if (occurrences_is_plate_valid(occurrence.plate)) {
        zassert_not_equal(zbus_chan_pub(&infraction_chan, &occurrence, K_FOREVER), -ENOMSG,
                          "Nao foi possivel registrar o veiculo.");
    } else {
        zassert_equal(zbus_chan_pub(&infraction_chan, &occurrence, K_FOREVER), -ENOMSG,
                      "Veiculo invalido registrado.");
    }
}

ZTEST(radar_integration, test_display) {
    struct occurrence_msg occurrence;

    occurrence.plate = "";
    occurrence.type = VEHICLE_TYPE_LIGHT;
    occurrence.speed_kmph = 42;
    zassert_equal(zbus_chan_pub(&non_infraction_chan, &occurrence, K_FOREVER), 0,
                  "Nao foi possivel registrar o veiculo 1.");

    occurrence.plate = "";
    occurrence.type = VEHICLE_TYPE_LIGHT;
    occurrence.speed_kmph = 78;
    zassert_equal(zbus_chan_pub(&non_infraction_chan, &occurrence, K_FOREVER), 0,
                  "Nao foi possivel registrar o veiculo 2.");

    occurrence.plate = "ABC1D23";
    occurrence.type = VEHICLE_TYPE_LIGHT;
    occurrence.speed_kmph = 123;
    zassert_not_equal(zbus_chan_pub(&infraction_chan, &occurrence, K_FOREVER), -ENOMSG,
                      "Nao foi possivel registrar o veiculo 3.");

    occurrence.plate = "ABC123D";
    occurrence.type = VEHICLE_TYPE_LIGHT;
    occurrence.speed_kmph = 123;
    zassert_equal(zbus_chan_pub(&infraction_chan, &occurrence, K_FOREVER), -ENOMSG, "Veiculo 4 foi registrado.");

    occurrence.plate = "";
    occurrence.type = VEHICLE_TYPE_HEAVY;
    occurrence.speed_kmph = 42;
    zassert_equal(zbus_chan_pub(&non_infraction_chan, &occurrence, K_FOREVER), 0,
                  "Nao foi possivel registrar o veiculo 5.");

    occurrence.plate = "";
    occurrence.type = VEHICLE_TYPE_HEAVY;
    occurrence.speed_kmph = 56;
    zassert_equal(zbus_chan_pub(&non_infraction_chan, &occurrence, K_FOREVER), 0,
                  "Nao foi possivel registrar o veiculo 6.");

    occurrence.plate = "ABC1D23";
    occurrence.type = VEHICLE_TYPE_HEAVY;
    occurrence.speed_kmph = 69;
    zassert_equal(zbus_chan_pub(&infraction_chan, &occurrence, K_FOREVER), 0,
                  "Nao foi possivel registrar o veiculo 7.");

    k_sleep(K_MSEC(1000));
}
