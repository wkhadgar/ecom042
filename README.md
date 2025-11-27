# Sistema de Radar Eletrônico com Classificação de Veículos

## Descrição do Projeto

Este projeto implementa um sistema de radar eletrônico simulado utilizando o Zephyr RTOS. O
sistema foi projetado para a placa `mps2/an385` e executado em ambiente de emulação QEMU.

O objetivo principal é monitorar o tráfego, calcular a velocidade de veículos por sensores magnéticos simulados (
GPIOs), classificar os veículos em categorias (leves ou pesados) com base na contagem de eixos e verificar infrações de
velocidade conforme limites configuráveis.

O sistema também simula a captura de placas via uma "câmera" virtual e exibe os resultados em um console formatado com
cores ANSI, indicando status de passagem segura, alerta ou infração.

## Arquitetura do Sistema

O sistema é modular e utiliza o barramento de mensagens ZBus do Zephyr para comunicação assíncrona entre threads. A
arquitetura é composta pelos seguintes módulos principais:

### 1. Módulo de Sensores (`sensor_thread.c`)

Responsável pela aquisição de dados. Utiliza interrupções de GPIO para detectar pulsos nos sensores físicos simulados.

- **Máquina de Estados:** Gerencia os estados `IDLE`, `SPEED_MEASURING` e `OBSERVING`.
- **Cálculo de Velocidade:** Mede o tempo de trânsito entre o primeiro e o segundo sensor (distância configurável).
- **Classificação de Veículos:** Conta o número de eixos detectados para diferenciar:
    - **Leves:** Até 2 eixos.
    - **Pesados:** Mais de 2 eixos.
    - **Timeout:** Utiliza um timer dinâmico baseado na velocidade calculada para determinar o fim da passagem de um
      veículo.

### 2. Lógica Principal (`main.c`)

Processa os dados brutos recebidos da fila de sensores (`sensor_msgq`).

- Verifica se a velocidade excede os limites configurados via Kconfig para o tipo de veículo específico.
- Se houver infração, solicita a captura da placa ao serviço de câmera.
- Publica os eventos resultantes nos canais do ZBus (`infraction_chan` ou `non_infraction_chan`).

### 3. Display Virtual (`display_thread.c`)

Atua como consumidor (subscriber) das mensagens do ZBus. Exibe as informações no console UART utilizando códigos ANSI:

- **Verde:** Velocidade segura.
- **Amarelo:** Velocidade de alerta (próxima ao limite).
- **Vermelho:** Infração de velocidade registrada.

### 4. Validação de Ocorrências (`occurrences.c`)

Implementa filtros de validação para garantir que as placas capturadas estejam conforme padrões Mercosul, e define os
canais de ocorrências.

## Configuração e Opções Kconfig

O comportamento do radar pode ser ajustado através do sistema de configuração do Zephyr (Kconfig). As principais opções
disponíveis são:

| Opção                                      | Padrão | Descrição                                                              |
|:-------------------------------------------|:-------|:-----------------------------------------------------------------------|
| `CONFIG_RADAR_SENSOR_DISTANCE_MM`          | 3000   | Distância física entre os dois sensores magnéticos (em milímetros).    |
| `CONFIG_RADAR_SPEED_LIMIT_LIGHT_KMH`       | 80     | Limite de velocidade máximo para veículos leves (2 eixos).             |
| `CONFIG_RADAR_SPEED_LIMIT_HEAVY_KMH`       | 60     | Limite de velocidade máximo para veículos pesados (3+ eixos).          |
| `CONFIG_RADAR_WARNING_THRESHOLD_PERCENT`   | 90     | Porcentagem do limite de velocidade que aciona o alerta amarelo.       |
| `CONFIG_RADAR_MAX_AXLE_SPACING_MM`         | 12000  | Espaçamento máximo entre eixos para considerar parte do mesmo veículo. |
| `CONFIG_RADAR_CAMERA_FAILURE_RATE_PERCENT` | 10     | Chance da placa capturada ser inválida.                                |

## Instruções de Configuração e Execução (QEMU)

### Pré-requisitos

* Ambiente de desenvolvimento Zephyr SDK configurado.
* Ferramenta `west` instalada.
* Topologia T1 do west. (este repositório deve estar dentro do ambiente west padrão e.g.: `zephyrproject`)

### Compilação

Para compilar o projeto para a plataforma `mps2/an385`:

```bash
west build -p -b mps2/an385
```

### Execução

Para simular o radar:

```bash
west build -t run
```

A shell estará aberta, por onde será possível interagir com o sistema.
Uma série de comandos simulando um veículo está disponível para testar o sistema em `simulate_vehicle_shell.txt`

A saída esperada é algo como:

```bash
uart:~$ gpio set gpio_emul 5 1
uart:~$ gpio set gpio_emul 5 0
uart:~$ kernel sleep 25
uart:~$ gpio set gpio_emul 6 1
uart:~$ gpio set gpio_emul 6 0
uart:~$ 
uart:~$ 
========================================                                                                                                                                                                 
[RADAR] Velocidade:  99 km/h (Lim: 80)
[CLASS] Veiculo: LEVE  
[ALERT] INFRACAO REGISTRADA! PLACA: ONO3D09
========================================
```

## Testes

O sistema conta com uma suíte de testes desenvolvida utilizando o framework **ZTest** do Zephyr. Os testes são divididos
em duas suítes principais: unitários e de integração, cobrindo desde a lógica matemática até o fluxo completo de
mensagens entre threads.

### Testes Unitários (`radar_unit`)

Esta suíte valida funções isoladas e regras de negócio fundamentais.

* **Validação de Placas (Mercosul):**
    * **Cenários Positivos:** Valida formatos corretos para Brasil, Argentina, Paraguai, Uruguai e Bolívia.
    * **Cenários Negativos:** Rejeita placas com tamanho incorreto, caracteres inválidos ou formatos compostos apenas
      por números.
* **Cálculo de Velocidade:**
    * Verifica a precisão matemática da conversão de microssegundos (tempo entre sensores) para km/h e cm/s.
    * Testa comportamento em casos de borda (ex: delta de tempo zero).

### Testes de Integração (`radar_integration`)

Esta suíte simula o comportamento do hardware e a interação entre módulos. As interrupções de GPIO são simuladas via
software através das funções auxiliares `hit_sensor_1` e `hit_sensor_2`, permitindo a injeção de estímulos temporais
precisos.

* **Máquina de Estados do Sensor (FSM):**
    * Verifica as transições entre os estados `IDLE`, `SPEED_MEASURING` e `OBSERVING` conforme os eixos são detectados.
* **Classificação de Veículos:**
    * **Veículos Leves:** Simula a passagem de 2 eixos e valida a classificação como `VEHICLE_TYPE_LIGHT`.
    * **Veículos Pesados:** Simula 3 ou mais eixos e valida a classificação como `VEHICLE_TYPE_HEAVY`.
* **Cenários de Tráfego Intenso (Tailgating):**
    * Valida a capacidade do sistema de distinguir dois veículos trafegando muito próximos.
    * Cobre combinações: Carro-Carro, Caminhão-Caminhão, Carro-Caminhão e Caminhão-Carro.
* **Fluxo ZBus e Câmera:**
    * Simula o ciclo completo: Detecção -> Disparo da Câmera -> Recebimento da Placa -> Publicação no canal de
      infração (`infraction_chan`).
    * Valida se veículos não infratores são corretamente roteados para o canal `non_infraction_chan`.

### Execução dos Testes

Para executar as suítes de teste:

```bash
cd tests/
west twister -T unit/ -vv
west twister -T integration/ -vv
```
