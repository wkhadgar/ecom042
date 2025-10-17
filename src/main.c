/**
 * @file main.c
 * @author Paulo Santos (prms@ic.ufal.br)
 * @brief Comunicação entre threads.
 * @version 0.1
 * @date 16/10/2025
 *
 * @copyright Copyright (c) 2025 Paulo Santos
 *
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

/**
 * @brief Umidade mínima considerada válida.
 */
#define MIN_HUMIDITY 40

/**
 * @brief Umidade máxima considerada válida.
 */
#define MAX_HUMIDITY 70

/**
 * @brief Temperatura mínima considerada válida.
 */
#define MIN_TEMP 18

/**
 * @brief Temperatura máxima considerada válida.
 */
#define MAX_TEMP 30


/**
 * @brief Identificadores dos tipos de sensores.
 */
enum sensor {
    SENSOR_HUMIDITY = 0,
    SENSOR_TEMPERATURE,
};

/**
 * @brief Mensagem de sensoreamento.
 */
struct sensing_msg {
    enum sensor sensor_type;

    union {
        uint8_t humidity;
        int8_t temperature;
    };
};

/**
 * @brief Sensor de umidade simulado.
 *
 * @return Umidade, entre 0% e 100%
 */
uint8_t get_humidity(void) {
    return k_cycle_get_32() % 100;
}

/**
 * @brief Sensor de temperatura simulado.
 *
 * @return Temperatura, em graus Celsius.
 */
int8_t get_temperature(void) {
    return (int8_t) (k_cycle_get_32() % 100);
}

K_MSGQ_DEFINE(filter_q, sizeof(struct sensing_msg), 10, 4);
K_MSGQ_DEFINE(out_q, sizeof(struct sensing_msg), 10, 4);

/**
 * @brief Produtor, simulando os sensores.
 */
int main() {
    struct sensing_msg send_msg;

    while (true) {
        send_msg.sensor_type = SENSOR_HUMIDITY;
        send_msg.humidity = get_humidity();

        k_msgq_put(&filter_q, &send_msg, K_FOREVER);
        k_msleep(k_cycle_get_32() % 500);

        send_msg.sensor_type = SENSOR_TEMPERATURE;
        send_msg.temperature = get_temperature();

        k_msgq_put(&filter_q, &send_msg, K_FOREVER);
        k_msleep(k_cycle_get_32() % 500);
    }

    return 0;
}

int processing_thread(void* arg1, void* arg2, void* arg3) {
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    struct sensing_msg recv_msg;

    while (true) {
        if (k_msgq_get(&out_q, &recv_msg, K_FOREVER) != 0) {
            continue;
        }

        switch (recv_msg.sensor_type) {
            case SENSOR_HUMIDITY:
                LOG_INF("Received humidity reading: %d%%", recv_msg.humidity);
                break;
            case SENSOR_TEMPERATURE:
                LOG_INF("Received temperature reading: %d°C", recv_msg.temperature);
                break;
        }
    }

    return 0;
}

int filtering_thread(void* arg1, void* arg2, void* arg3) {
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    struct sensing_msg recv_msg;

    while (true) {
        if (k_msgq_get(&filter_q, &recv_msg, K_FOREVER) != 0) {
            continue;
        }

        switch (recv_msg.sensor_type) {
            case SENSOR_HUMIDITY:
                if ((recv_msg.humidity > MAX_HUMIDITY) || (recv_msg.humidity < MIN_HUMIDITY)) {
                    LOG_ERR("Invalid data received from humidity sensor: %d%%", recv_msg.humidity);
                    continue;
                }
                break;
            case SENSOR_TEMPERATURE:
                if ((recv_msg.temperature > MAX_TEMP) || (recv_msg.temperature < MIN_TEMP)) {
                    LOG_ERR("Invalid data received from temperature sensor: %d°C", recv_msg.temperature);
                    continue;
                }
                break;
        }

        k_msgq_put(&out_q, &recv_msg, K_FOREVER);
    }

    return 0;
}

K_THREAD_DEFINE(filtering_thread_id, 1048, filtering_thread, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(processing_thread_id, 1048, processing_thread, NULL, NULL, NULL, 5, 0, 0);
