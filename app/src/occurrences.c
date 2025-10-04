/**
 * @file occurrences.c
 * @author Paulo Santos (prms@ic.ufal.br)
 * @brief Define as ocorrencias.
 * @version 0.1
 * @date 26-11-2025
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "occurrences.h"

#include <ctype.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(occurrences, LOG_LEVEL_INF);

bool occurrences_is_plate_valid(const char* plate) {
    if (strlen(plate) != 7) {
        return false;
    }

    /* Brasil: ABC1D23 */
    if (isalpha(plate[0]) && isalpha(plate[1]) && isalpha(plate[2]) &&
        isdigit(plate[3]) && isalpha(plate[4]) && isdigit(plate[5]) &&
        isdigit(plate[6])) {
        return true;
        }

    /* Argentina: AB 123 CD */
    if (isalpha(plate[0]) && isalpha(plate[1]) && isdigit(plate[2]) &&
        isdigit(plate[3]) && isdigit(plate[4]) && isalpha(plate[5]) &&
        isalpha(plate[6])) {
        return true;
        }

    /* Paraguai: ABCD 123 */
    if (isalpha(plate[0]) && isalpha(plate[1]) && isalpha(plate[2]) &&
        isalpha(plate[3]) && isdigit(plate[4]) && isdigit(plate[5]) &&
        isdigit(plate[6])) {
        return true;
        }

    /* Uruguai: ABC 1234 */
    if (isalpha(plate[0]) && isalpha(plate[1]) && isalpha(plate[2]) &&
        isdigit(plate[3]) && isdigit(plate[4]) && isdigit(plate[5]) &&
        isdigit(plate[6])) {
        return true;
        }


    /* Bolívia: AB 12345 */
    if (isalpha(plate[0]) && isalpha(plate[1]) && isdigit(plate[2]) &&
        isdigit(plate[3]) && isdigit(plate[4]) && isdigit(plate[5]) &&
        isdigit(plate[6])) {
        return true;
        }

    LOG_WRN("Placa inválida: %s", plate);

    return false;
}


static bool plate_validator(const void* msg, size_t msg_size) {
    ARG_UNUSED(msg_size);

    const struct occurrence_msg* occurrence = msg;

    return occurrences_is_plate_valid(occurrence->plate);
}

ZBUS_CHAN_DEFINE(infraction_chan, struct occurrence_msg, plate_validator, NULL, ZBUS_OBSERVERS_EMPTY, {});
ZBUS_CHAN_DEFINE(non_infraction_chan, struct occurrence_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY, {});
