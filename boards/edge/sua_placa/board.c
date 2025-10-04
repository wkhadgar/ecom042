/**
 * @file board.c
 * @author João Anon (email@edge.ufal.br)
 * @brief
 * @version 0.1
 * @date dd-mm-aaaa
 *
 * @copyright Copyright (c) aaaa
 *
 */


#include <zephyr/init.h>

/**
 * @brief Executa código de inicialização da placa (apenas para o hardware da MCU).
 *
 * @return Não utilizado.
 */
static int sua_placa_init(void)
{
    printf("Boilerplate para a 'sua_placa'\n");

	return 0;
}

SYS_INIT(sua_placa_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
