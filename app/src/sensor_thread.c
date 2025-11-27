/**
 * @file sensor_thread.c
 * @brief Implementa a thread responsável pela leitura dos sensores magnéticos.
 * @version 0.1
 * @date 25/11/2025
 *
 * Utiliza interrupções de GPIO para detectar a passagem e o tempo de trânsito.
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "radar_sensing.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/timing/timing.h>
#include <zephyr/logging/log.h>

/**
 * @brief Converte milissegundos para microssegundos.
 *
 * @param ms Milissegundos para converter em microssegundos.
 */
#define MS_TO_US(ms) (ms * 1000)

/**
 * @brief GPIO para o primeiro sensor (GPIO 5).
 */
static const struct gpio_dt_spec sensor1_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(sensor_gpio5), gpios);
/**
 * @brief GPIO para o segundo sensor (GPIO 6s).
 */
static const struct gpio_dt_spec sensor2_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(sensor_gpio6), gpios);

/**
 * @brief Estrutura de callback para o primeiro sensor.
 */
static struct gpio_callback sensor1_cb_data;

/**
 * @brief Estrutura de callback para o segundo sensor.
 */
static struct gpio_callback sensor2_cb_data;

/**
 * @brief Função auxiliar para despachar dados e resetar estado.
 * Deve ser chamada em contexto de interrupção ou atômico.
 */
static void finalize_vehicle(void);

static struct {
    volatile uint8_t axle_count; /**< Contador de eixos detectados (pulsos no sensor 1). */
    enum radar_state current_state; /**< Estado atual. */
    uint32_t start_time_us; /**< Tempo de início da medição de trânsito (primeiro sensor) em microssegundos. */
    uint32_t last_pulse_us;
    uint32_t max_inter_axle_us;
    struct sensor_data_msg msg; /**< Mensagem de sensoreamento. */
} self = {
    .current_state = RADAR_STATE_IDLE,
};

K_MSGQ_DEFINE(sensor_msgq, sizeof(struct sensor_data_msg), 10, 4);
K_SEM_DEFINE(vehicle_detected_sem, 0, 1);

LOG_MODULE_REGISTER(sensors, LOG_LEVEL_INF);

/**
 * @brief Timer de inatividade.
 * Se nenhum eixo for detectado dentro da janela calculada, o veículo é finalizado.
 */
static void vehicle_timeout_expiry(struct k_timer* timer) {
    ARG_UNUSED(timer);

    finalize_vehicle();
}

K_TIMER_DEFINE(vehicle_finish_timer, vehicle_timeout_expiry, NULL);

static void finalize_vehicle(void) {
    k_timer_stop(&vehicle_finish_timer);

    if (self.axle_count > 2) {
        self.msg.type = VEHICLE_TYPE_HEAVY;
    } else {
        self.msg.type = VEHICLE_TYPE_LIGHT;
    }

    k_sem_give(&vehicle_detected_sem);

    self.current_state = RADAR_STATE_IDLE;
    self.axle_count = 0;
}

void sensor_calc_speed(const uint32_t delta_us, uint32_t* speed_cm_s, uint32_t* speed_kmph) {
    __ASSERT((speed_kmph != NULL && speed_cm_s != NULL), "NULL pointers were given.");

    if (delta_us == 0) {
        *speed_kmph = 0;
        *speed_cm_s = 0;
        return;
    }

    *speed_cm_s = (CONFIG_RADAR_SENSOR_DISTANCE_MM * 100) / (delta_us / 1000);
    *speed_kmph = (*speed_cm_s * 9) / 250;
}

enum radar_state sensor_get_state(void) {
    return self.current_state;
}

/**
 * @brief Callback da ISR do sensor 1
 *
 * Responsável por iniciar eventos, contar eixos e detectar tailgating.
 */
void sensor1_isr_callback(const struct device* dev, struct gpio_callback* cb, uint32_t pins) {
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    const uint32_t now_cycles = k_cycle_get_32();
    const uint32_t now_us = k_cyc_to_us_ceil32(now_cycles);

    switch (self.current_state) {
        case RADAR_STATE_IDLE:
            /* Novo veículo detectado */
            self.start_time_us = now_us;
            self.last_pulse_us = now_us;
            self.axle_count = 1;

            /* Estado muda para esperando medição de velocidade no S2 */
            self.current_state = RADAR_STATE_SPEED_MEASURING;
            break;

        case RADAR_STATE_SPEED_MEASURING:
            self.axle_count += 1;
            self.last_pulse_us = now_us;
            break;

        case RADAR_STATE_OBSERVING: {
            const uint32_t delta_us = now_us - self.last_pulse_us;

            /* Caso em que outro veículo (pela distância) está sendo registrado. */
            if (delta_us > self.max_inter_axle_us) {
                finalize_vehicle();

                self.start_time_us = now_us;
                self.last_pulse_us = now_us;
                self.axle_count = 1;
                self.msg.speed_kmph = 0;
                self.current_state = RADAR_STATE_SPEED_MEASURING;
            } else {
                self.axle_count += 1;
                self.last_pulse_us = now_us;

                /* Reinicia o timer de timeout para estender a janela. */
                k_timer_start(&vehicle_finish_timer, K_USEC(self.max_inter_axle_us), K_NO_WAIT);
            }
            break;
        }
    }
}

/**
 * @brief Callback do Sensor 2
 *
 * Responsável exclusivamente pelo fechamento do cálculo de velocidade inicial.
 */
void sensor2_isr_callback(const struct device* dev, struct gpio_callback* cb, uint32_t pins) {
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    uint32_t speed_cm_s = 0;

    if (self.current_state == RADAR_STATE_SPEED_MEASURING) {
        const uint32_t now_cycles = k_cycle_get_32();
        uint32_t cycles_delta_us = k_cyc_to_us_ceil32(now_cycles) - self.start_time_us;

        /* Proteção contra divisão por zero ou ruído extremo */
        if (cycles_delta_us < 1000) {
            cycles_delta_us = 1000;
        }

        sensor_calc_speed(cycles_delta_us, &speed_cm_s, &self.msg.speed_kmph);

        if (speed_cm_s > 0) {
            self.max_inter_axle_us = (CONFIG_RADAR_MAX_AXLE_SPACING_MM * 100000) / speed_cm_s;
            self.max_inter_axle_us += self.max_inter_axle_us / 10; /* Tolerância adicional. */
        } else {
            self.max_inter_axle_us = 2000000; /* Fallback 2s */
        }

        self.current_state = RADAR_STATE_OBSERVING;

        k_timer_start(&vehicle_finish_timer, K_USEC(self.max_inter_axle_us), K_NO_WAIT);
    }
}

/**
 * @brief Inicializa o driver GPIO e configura a interrupção para o primeiro sensor.
 *
 * @return 0 se sucesso, negativo em caso de erro.
 */
int sensor_init(void) {
    if (!device_is_ready(sensor1_gpio.port) || !device_is_ready(sensor2_gpio.port)) {
        LOG_ERR("Dispositivos GPIO dos sensores nao estao prontos!\n");
        return -ENODEV;
    }

    int err = gpio_pin_configure_dt(&sensor1_gpio, GPIO_INPUT | GPIO_OUTPUT | GPIO_PULL_DOWN) || gpio_pin_configure_dt(
                  &sensor2_gpio, GPIO_INPUT | GPIO_OUTPUT | GPIO_PULL_DOWN);
    if (err < 0) {
        LOG_ERR("Erro ao configurar GPIOs: %d\n", err);
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(&sensor1_gpio, GPIO_INT_EDGE_RISING) || gpio_pin_interrupt_configure_dt(
              &sensor2_gpio, GPIO_INT_EDGE_RISING);
    if (err < 0) {
        LOG_ERR("Erro ao configurar IRQ para GPIOs: %d\n", err);
        return err;
    }

    gpio_init_callback(&sensor1_cb_data, sensor1_isr_callback, BIT(sensor1_gpio.pin));
    gpio_init_callback(&sensor2_cb_data, sensor2_isr_callback, BIT(sensor2_gpio.pin));
    err = gpio_add_callback(sensor1_gpio.port, &sensor1_cb_data) || gpio_add_callback(
              sensor2_gpio.port, &sensor2_cb_data);
    if (err < 0) {
        LOG_ERR("Erro ao adicionar callback para GPIOs: %d\n", err);
        return err;
    }

    LOG_INF("Inicializacao concluida. Aguardando passagem.\n");

    return 0;
}

/**
 * @brief Função de entrada da Thread de Sensores.
 */
void sensor_thread(void* p1, void* p2, void* p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    if (sensor_init() != 0) {
        return;
    }

    while (true) {
        if (k_sem_take(&vehicle_detected_sem, K_FOREVER) != 0) {
            continue;
        }

        if (k_msgq_put(&sensor_msgq, &self.msg, K_USEC(self.max_inter_axle_us)) != 0) {
            LOG_WRN("Fila de sensores cheia, veiculo perdido.");
        }
    }
}

K_THREAD_DEFINE(sensor_thread_id, 1024, sensor_thread, NULL, NULL, NULL, 5, K_ESSENTIAL, 0);
