# NAM Pedal — fundação STM32H743/XIP

Este repositório está sendo portado para a WeAct Studio STM32H743VIT6
(Cortex-M7, LQFP100, HSE de 25 MHz). A etapa atual contém somente a fundação
bare-metal/HAL: startup CMSIS, clocks, UART, DWT, MPU/cache, bootloader, QSPI e
uma aplicação de diagnóstico XIP.

Ainda **não** há áudio, processamento NAM, carregamento `.namb`, display,
encoder ou IR funcionais. O engine `NeuralAmpModelerCore` e o
`nam-binary-loader` permanecem submódulos protegidos e não participam dos
alvos desta etapa.

## Dependências e fronteiras

- STM32CubeH7 `v1.12.1`, fixado como submódulo em
  `third_party/STM32CubeH7`.
- Arm GNU Toolchain com `arm-none-eabi-gcc`, CMake 3.21+ e Ninja.
- Alvo `STM32H743xx`, Cortex-M7, FPv5-D16, hard-float e
  `NAM_SAMPLE_FLOAT`.
- Sem libDaisy, RTOS, JUCE, fixed-point ou headers HAL simulados.
- O build nunca atualiza submódulos automaticamente.

Após clonar, inicialize exatamente as dependências versionadas:

```powershell
git submodule update --init --recursive third_party/STM32CubeH7
```

## Build

Os presets criam diretórios independentes em `build/`:

```powershell
cmake --preset bootloader-debug
cmake --build --preset bootloader-debug

cmake --preset bootloader-release
cmake --build --preset bootloader-release

cmake --preset bringup-400-debug
cmake --build --preset bringup-400-debug

cmake --preset bringup-480-debug
cmake --build --preset bringup-480-debug

cmake --preset xip-debug
cmake --build --preset xip-debug

cmake --preset xip-release
cmake --build --preset xip-release
```

O `Makefile` é somente um wrapper para CMake. Por exemplo:

```powershell
make bringup CLOCK_PROFILE=SAFE_400MHZ
```

Cada alvo produz `.elf`, `.bin`, `.hex` e `.map`. O alvo XIP também produz:

- `*.manifest.bin`: header de 64 bytes;
- `*.qspi.bin`: manifest, preenchimento até `0x10000` e aplicação XIP.

## Imagens e arquitetura de boot

| Imagem | Endereço | Papel |
|---|---:|---|
| bootloader | `0x08000000` | Clock, QSPI, validação e salto |
| bring-up interno | `0x08020000` | Diagnóstico 400/480 MHz sem ocupar a reserva do bootloader |
| aplicação XIP | `0x90010000` | Verifica o contrato, configura MPU/cache e executa diagnóstico |

O startup CMSIS da revisão fixada do CubeH7 chama a implementação local de
`SystemInit()`. Essa implementação habilita a FPU e ajusta VTOR, mas
intencionalmente não reseta RCC: uma aplicação já executando da QSPI perderia
o mapeamento durante o fetch.

Fluxo do bootloader:

1. configura LDO e clock;
2. inicializa USART3 sem esperar terminal;
3. reseta a QSPI para estado SPI conhecido;
4. lê o JEDEC ID em uma linha;
5. aceita apenas um item explícito da tabela suportada;
6. configura QE e dummy cycles segundo esse item;
7. lê 256 bytes em Quad-I/O SDR 1-4-4 e calcula CRC32;
8. entra em memory-mapped e compara o CRC da mesma região;
9. valida manifest, stack pointer, reset vector e CRC32 da imagem;
10. desliga SysTick, limpa enable/pending do NVIC, desliga caches/MPU,
    define VTOR/MSP e salta para `0x90010000`.

Estado entregue à aplicação: CPU a 480 MHz, HCLK a 240 MHz, QSPI
memory-mapped, caches e MPU desligados, SysTick parado, IRQs mascaradas e VTOR
em `0x90010000`. A aplicação valida esse estado antes de chamar `HAL_Init()`;
ela não reconfigura RCC/QSPI, prepara MPU/cache e só então habilita IRQs.

## Perfis de clock e revisão de silício

Os perfis de compilação são:

| Define | PLL1 a partir de HSE 25 MHz | CPU | HCLK/AXI | APB1–4 | Escala |
|---|---|---:|---:|---:|---|
| `CLOCK_PROFILE_SAFE_400MHZ` | M=5, N=160, P=2 | 400 MHz | 200 MHz | 100 MHz | VOS1 |
| `CLOCK_PROFILE_PERFORMANCE_480MHZ` | M=5, N=192, P=2 | 480 MHz | 240 MHz | 120 MHz | VOS0 |

Ambos usam D1CPRE `/1`, AHB `/2`, APB `/2` e quatro wait states de flash. O
perfil final padrão é 480 MHz. A espera por VOS é limitada e falha com um
código próprio.

O firmware lê `DBGMCU->IDCODE` antes de decidir o perfil. O device esperado é
`DEV_ID=0x450`. O mapeamento documentado pela errata do STM32H743 é:

| REV_ID | Revisão | Política |
|---:|---|---|
| `0x1003` | Y | fallback explícito para 400 MHz; boot XIP é recusado |
| `0x2001` | X | 480 MHz permitido |
| `0x2003` | V | 480 MHz permitido |
| outro | desconhecida | fallback conservador para 400 MHz |

O bring-up imprime clocks calculados, IDCODE, DEV_ID/REV_ID, VTOR, MSP,
cache/MPU, seções de memória, teste DWT e resultado QSPI por USART3
115200 8N1. TX usa polling com timeout e nunca aguarda conexão de terminal.

## QSPI e manifest

Pinagem QUADSPI Bank 1:

| Sinal | Pino | AF |
|---|---|---:|
| CLK | PB2 | AF9 |
| NCS | PB6 | AF10 |
| IO0 | PD11 | AF9 |
| IO1 | PD12 | AF9 |
| IO2 | PE2 | AF9 |
| IO3 | PD13 | AF9 |

PLL2R gera 120 MHz (HSE/5 × 96 / 4) e o prescaler do periférico divide por
dois: o clock inicial da flash é exatamente 60 MHz. O driver usa SDR, endereço
de 24 bits, comando `0xEB` em 1-4-4 e mantém DTR desabilitado. Ele não entra
permanentemente em QPI.

Dispositivos explicitamente suportados:

| JEDEC ID | Variante | QE | Quad-I/O |
|---|---|---|---|
| `EF 40 17` | Winbond W25Q64JV | SR2 bit 1 | `0xEB`, mode `0xF0`, 4 dummy clocks |
| `EF 70 17` | Winbond W25Q64JV-IQ/JQ | SR2 bit 1 | `0xEB`, mode `0xF0`, 4 dummy clocks |

O sufixo montado na placa ainda deve ser lido fisicamente e confirmado pelo
JEDEC ID do bring-up. Qualquer ID diferente é rejeitado antes de habilitar
Quad-I/O.

Para adicionar uma flash:

1. obtenha o datasheet exato do fabricante;
2. confirme capacidade, comando de reset, localização/semântica de QE,
   opcode, mode bits e dummy cycles em SDR a 60 MHz;
3. adicione uma entrada independente em `supported_devices`;
4. valide no hardware leitura de uma linha, leitura indireta 1-4-4, CRC
   memory-mapped, reset e recuperação;
5. somente depois considere elevar o clock. 80 MHz não está habilitado nesta
   etapa.

Layout:

| Região | Intervalo |
|---|---|
| metadados/manifest | `0x90000000–0x9000FFFF` |
| aplicação | `0x90010000–0x907FFFFF` |

O manifest little-endian tem 64 bytes:

| Offset | Campo | Tamanho |
|---:|---|---:|
| `0x00` | magic `NAMP` (`0x504D414E`) | 4 |
| `0x04` | versão (`1`) | 2 |
| `0x06` | tamanho do header (`64`) | 2 |
| `0x08` | endereço da imagem (`0x90010000`) | 4 |
| `0x0C` | tamanho da imagem | 4 |
| `0x10` | CRC32 da imagem | 4 |
| `0x14` | versão de firmware | 4 |
| `0x18` | flags; bit 0 = válida | 4 |
| `0x1C` | endereço do vetor (`0x90010000`) | 4 |
| `0x20` | reservado | 28 |
| `0x3C` | CRC32 dos primeiros 60 bytes | 4 |

CRC32 usa o polinômio refletido `0xEDB88320`, compatível com `zlib.crc32`.

## Mapa de RAM, MPU e seções

| Região | Endereço | Tamanho | Uso |
|---|---:|---:|---|
| DTCM | `0x20000000` | 128 KiB | `.model_weights_dtcm`, XN, nunca DMA |
| AXI SRAM | `0x24000000` | 512 KiB | `.nam_ring_buffers`, `.audio_work_buffers`, `.ir_buffers`, data/stack |
| D2 DMA | `0x30000000` | 32 KiB | `.audio_dma_buffers`, `.display_dma_buffers`, não cacheável, XN |
| D2 restante | `0x30008000` | 256 KiB | reserva futura |
| D3 SRAM | `0x38000000` | 64 KiB | reserva futura |

Os linkers exportam símbolos de início/fim, fazem asserts de tamanho e usam
pequenos sentinelas externos ao engine. A região D2 DMA é alinhada a 32 KiB e
tem uma região MPU shareable, não cacheável e execute-never. A QSPI é
read-only/cacheable/executável; DTCM e AXI de dados são execute-never.

## Pinagem consolidada

Pinagem implementada nesta etapa:

| Função | Pinos | Estado |
|---|---|---|
| QSPI firmware | PB2, PB6, PD11, PD12, PE2, PD13 | implementada |
| USART3 | PB10 TX, PB11 RX (AF7) | implementada, 115200 8N1 |
| LED de erro | PE3 | implementado, padrão por código |
| SWD | PA13 SWDIO, PA14 SWCLK | preservado |

Pinagem reservada, ainda sem driver:

| Subsistema | Sinais |
|---|---|
| flash de dados SPI1 | PD6 CS, PB3 SCK, PB4 MISO, PD7 MOSI |
| display SPI4 | PE12 SCK/AF5, PE14 MOSI/AF5, PE11 CS, PE10 DC, PE9 RESET, PE8 backlight |
| encoder | PC6 A, PC7 B, PC8 switch; futuros pull-ups, contatos a GND |
| USB FS | PA11 DM, PA12 DP |
| botão K1 | PC13 |
| ROM serial recovery | PA9/PA10 livres |
| áudio SAI3 futuro | PD0 SCK_A, PD4 FS_A, PD1 SD_A, PD15 MCLK_A, PD9 SD_B; AF6 |

SPI1 fica exclusivo para modelos/IRs e não será compartilhado com o display.
MCLK nunca será ligado ao PCM5102.

### Pinagem de áudio aprovada para a próxima etapa

A proposta original para SAI1 era impossível no STM32H743VI **LQFP100**:
`PF6=SAI1_SD_B` e `PG7=SAI1_MCLK_A` não existem nesse encapsulamento. A solução
aprovada usa os dois blocos do SAI3:

| Rede de áudio | Função SAI3 | GPIO | Header WeAct |
|---|---|---|---|
| BCLK compartilhado | SCK_A | PD0/AF6 | P1-17 |
| LRCK compartilhado | FS_A | PD4/AF6 | P1-13 |
| PCM5102 DIN | SD_A | PD1/AF6 | P1-16 |
| PCM1808 DOUT | SD_B | PD9/AF6 | P1-37 |
| PCM1808 SCKI/MCLK | MCLK_A | PD15/AF6 | P1-31 |

O Block A será master transmitter. O Block B será synchronous receiver e usará
internamente SCK/FS do Block A, portanto `SCK_B`, `FS_B` e `MCLK_B` não serão
roteados. BCLK e LRCK serão ligados aos dois codecs; MCLK será ligado somente
ao PCM1808. O pino SCK/MCLK da breakout PCM5102 permanece aterrado.

Antes de conectar `PD4` ao LRCK, é obrigatório **abrir o solder bridge SB2** da
WeAct V1.2, que liga PD4 ao contato `MicroSD_SW`, e confirmar com multímetro que
não existe continuidade. A USART3 foi movida de PD8/PD9 para PB10/PB11 para
liberar PD9 ao SAI3 Block B. PLL3 ficará reservada ao clock de áudio; PLL2
continua exclusiva para QSPI.

SAI3, DMA de áudio e passthrough continuam fora do escopo desta fundação e não
estão implementados.

## Erros e recuperação

Erros críticos tentam registrar `FATAL=<código>` pela UART e piscam PE3 em
grupos de pulsos. Há códigos distintos para clock, VOS, UART, JEDEC
desconhecido, leitura indireta, memory-mapped, manifest, stack, reset vector,
CRC, contrato de boot e configuração QSPI. O firmware não salta após erro e
permanece recuperável por SWD.

## Programação e bring-up

Use ST-LINK/SWD e STM32CubeProgrammer. Primeiro grave o bring-up interno para
identificar `DEV_ID/REV_ID` e JEDEC sem depender da aplicação XIP:

```powershell
STM32_Programmer_CLI -c port=SWD -w build/bringup-400-debug/nam_bringup_SAFE_400MHZ.hex -v -rst
STM32_Programmer_CLI -c port=SWD -w build/bringup-480-debug/nam_bringup_PERFORMANCE_480MHZ.hex -v -rst
```

Grave o bootloader interno:

```powershell
STM32_Programmer_CLI -c port=SWD -w build/bootloader-release/nam_bootloader_PERFORMANCE_480MHZ.hex -v
```

Depois de confirmar o modelo da QSPI e instalar no CubeProgrammer um external
loader compatível com essa flash/placa, grave o binário combinado no início da
QSPI:

```powershell
STM32_Programmer_CLI -c port=SWD -el <external-loader.stldr> -w build/xip-release/nam_xip_PERFORMANCE_480MHZ.qspi.bin 0x90000000 -v -rst
```

Não use um loader escolhido apenas pela capacidade. O loader precisa coincidir
com a pinagem e o dispositivo identificado. O ROM bootloader não é assumido
como programador da QSPI.

## Testes de hardware ainda obrigatórios

- bring-up interno a 400 e 480 MHz, registrando clocks e DEV_ID/REV_ID;
- identificação física e JEDEC da QSPI;
- leitura indireta e memory-mapped a 60 MHz;
- gravação/validação do manifest e salto para `0x90010000`;
- soak test XIP, reset normal e reset/watchdog durante inicialização QSPI;
- recuperação por SWD.

Resultados de compilação e inspeção de ELF não substituem esses testes.
