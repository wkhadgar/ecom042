/**
 * @file occurrences.h
 * @author Paulo Santos (prms@ic.ufal.br)
 * @brief Declara as ocorrências.
 * @version 0.1
 * @date 26-11-2025
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef OCCURRENCES_H
#define OCCURRENCES_H

#include "radar_sensing.h"

#include <zephyr/zbus/zbus.h>

struct occurrence_msg {
    enum vehicle_type type;
    const char* plate;
    uint32_t speed_kmph;
};

/**
 * @brief Valida uma placa por meio de uma mensagem de ocorrência.
 *
 * @param plate Ponteiro para string da placa.
 *
 * @retval true Placa válida.
 * @retval false Placa inválida.
 */
bool occurrences_is_plate_valid(const char* plate);

ZBUS_CHAN_DECLARE(infraction_chan);
ZBUS_CHAN_DECLARE(non_infraction_chan);

#endif /* OCCURRENCES_H */
