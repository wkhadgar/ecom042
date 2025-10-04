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

ZTEST(radar_unit, test_mercosul_validation)
{
    zassert_true(occurrences_is_plate_valid("ABC1D23"), "Brasil Falhou");
    zassert_true(occurrences_is_plate_valid("AB123CD"), "Argentina Falhou");

    zassert_false(occurrences_is_plate_valid("ABC1234"), "Placa Antiga deve falhar (se estrito)");
    zassert_false(occurrences_is_plate_valid("1234567"), "Apenas números");
    zassert_false(occurrences_is_plate_valid("ABC"), "Tamanho incorreto");
    zassert_false(occurrences_is_plate_valid("ABC1D2@"), "Caracter inválido");
}

ZTEST(radar_unit, test_speed_calculation)
{
    /* Garantindo a distância do sensor para o teste (3m = 3000mm). */
    /* Tempo simulado: 108ms (0.108s) -> Esperado ~100 km/h */
    const uint64_t time_delta_us = 108000;
    uint32_t speed_kmph;
    uint32_t speed_cmps;

    /* (3000 * 1000) / 108000 = 277.7 cm/s -> 99.9 km/h */
    sensor_calc_speed(time_delta_us, &speed_kmph, &speed_cmps);

    zassert_within(speed_kmph, 100, 1, "Erro de arredondamento na velocidade");
    zassert_within(speed_cmps, 277, 1, "Erro de arredondamento na velocidade");

    sensor_calc_speed(0, &speed_kmph, &speed_cmps);
    zassert_equal(speed_kmph, 0, "Deveria tratar tempo zero como erro");
    zassert_equal(speed_cmps, 0, "Deveria tratar tempo zero como erro");
}
