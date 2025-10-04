/**
 * @file radar_sensing.h
 * @author Paulo Santos (prms@ic.ufal.br)
 * @brief Radar sensor declarations.
 * @version 0.1
 * @date 25/11/2025
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef RADAR_SENSING_H
#define RADAR_SENSING_H

#include <zephyr/kernel.h>

/**
 * @brief Tipos de veículo.
 */
enum vehicle_type {
    VEHICLE_TYPE_HEAVY = 0,
    VEHICLE_TYPE_LIGHT,
};

/**
 * @brief Estados da FSM do radar.
 */
enum radar_state {
    RADAR_STATE_IDLE = 0, /**< Aguardando o primeiro eixo (início da medição). */
    RADAR_STATE_SPEED_MEASURING, /**< Medindo a velocidade do veículo. */
    RADAR_STATE_OBSERVING, /**< Observando os eixos seguintes do véiculo. */
};

/**
 * @brief Mensagem de sensores.
 */
struct sensor_data_msg {
    enum vehicle_type type; /**< Tipo do veículo. */
    uint32_t speed_kmph; /**< Velocidade do veículo em quilometros por hora. */
};

/**
 * @brief Fila de mensagens para enviar dados de sensores para a thread principal.
 */
extern struct k_msgq sensor_msgq;

/**
 * @brief Calcula a velocidade do veículo com base no tempo de trânsito entre sensores.
 *
 * @note A distância entre sensores é definida por @kconfig{CONFIG_RADAR_SENSOR_DISTANCE_MM}.
 *
 * @param delta_us Tempo decorrido entre o acionamento do Sensor 1 e Sensor 2 (em µs).
 * @param[out] speed_cm_s Ponteiro para armazenar a velocidade calculada em centímetros por segundo (cm/s).
 * @param[out] speed_kmph Ponteiro para armazenar a velocidade calculada em quilômetros por hora (km/h).
 */
void sensor_calc_speed(uint64_t delta_us, uint32_t* speed_cm_s, uint32_t* speed_kmph);

/**
 * @brief Obtém o estado da FSM do radar.
 *
 * @return Estado da FSM do radar.
 */
enum radar_state sensor_get_state(void);

#endif /* RADAR_SENSING_H */
