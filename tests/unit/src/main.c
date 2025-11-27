/**
 * @file main.c
 * @author Paulo Santos (pauloroberto.santos@edge.ufal.br)
 * @brief Testes unitários.
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

ZTEST_SUITE(radar_unit, NULL, NULL, NULL, NULL, NULL);

ZTEST(radar_unit, test_mercosul_happy_way) {
    zassert_true(occurrences_is_plate_valid("ABC1D23"), "Brasil falhou");
    zassert_true(occurrences_is_plate_valid("AB123CD"), "Argentina falhou");
    zassert_true(occurrences_is_plate_valid("ABCD123"), "Paraguai falhou");
    zassert_true(occurrences_is_plate_valid("ABC1234"), "Uruguai falhou");
    zassert_true(occurrences_is_plate_valid("AB12345"), "Bolivia falhou");
}

ZTEST(radar_unit, test_mercosul_only_numbers) {
    zassert_false(occurrences_is_plate_valid("1234567"), "Apenas numeros passou");
}

ZTEST(radar_unit, test_mercosul_invalid_size) {
    zassert_false(occurrences_is_plate_valid("ABC"), "Tamanho menor passou");
    zassert_false(occurrences_is_plate_valid("ABCD12345"), "Tamanho maior passou");
}

ZTEST(radar_unit, test_mercosul_invalid_caracters) {
    zassert_false(occurrences_is_plate_valid("ABC1D2@"), "Caracter invalido passou");
}

ZTEST(radar_unit, test_mercosul_invalid_plate) {
    zassert_false(occurrences_is_plate_valid("ABC1D2E"), "Placa invalida passou");
    zassert_false(occurrences_is_plate_valid("ABC123E"), "Placa invalida passou");
}

ZTEST(radar_unit, test_speed_calculation) {
    /* Garantindo a distância do sensor para o teste (3m = 3000mm). */
    /* Tempo simulado: 108ms (0.108s) -> Esperado ~100 km/h */
    const uint64_t time_delta_us = 108000;
    uint32_t speed_kmph;
    uint32_t speed_cmps;

    /* (3000 * 1000) / 108000 = 2777 cm/s -> 99.9 km/h */
    sensor_calc_speed(time_delta_us, &speed_cmps, &speed_kmph);

    zassert_within(speed_kmph, 100, 1, "Erro de arredondamento na velocidade: %d", speed_kmph);
    zassert_within(speed_cmps, 2777, 1, "Erro de arredondamento na velocidade: %d", speed_cmps);

}

ZTEST(radar_unit, test_speed_invalid) {
    uint32_t speed_kmph;
    uint32_t speed_cmps;

    sensor_calc_speed(0, &speed_kmph, &speed_cmps);
    zassert_equal(speed_kmph, 0, "Deveria tratar tempo zero como erro");
    zassert_equal(speed_cmps, 0, "Deveria tratar tempo zero como erro");
}
