# AGENTS.md — NAM Embedded Pedal (STM32H743, WeAct board)

## Contexto do projeto

Fork do motor de inferência C dependency-free da TONE3000 (`tone-3000/nam-pedal`),
portado do Daisy Seed (STM32H750) para uma placa WeAct baseada em STM32H743.
Sem RAM externa.

### Hardware confirmado

- **MCU:** WeAct STM32H743VIT6, Cortex-M7 @ 480MHz, FPU, DSP instructions, MPU
- **Memória interna:** 2048KB flash ROM interna, 1MB SRAM interna
- **Flash externa:**
  - QSPI 8MB — programa executável (XIP, arquitetura `BOOT_QSPI` igual Daisy,
    exige bootloader mínimo antes do firmware principal)
  - SPI 8MB — dedicada a dados: modelos `.namb` e IRs de cabinet
- **ADC de áudio:** breakout PCM1808 (I2S, 24-bit, modo slave)
- **DAC de áudio:** breakout PCM5102 (I2S, até 32-bit, provavelmente sem MCLK
  explícito — SCK/MCLK amarrado em GND na maioria dessas breakouts, PLL
  interno derivado do BCK; **confirmar fisicamente na placa antes de codar**)
- **Interface de áudio:** periférico **SAI** do H743 (não SPI genérico) —
  SAI1 Block A (playback → PCM5102) + Block B (record ← PCM1808) sincronizados
  no mesmo BCLK/LRCK
- **Display:** LCD colorido 2.25" 284x76, driver ST7789, interface SPI

Objetivo do MVP: pedal NAM funcional com um encoder rotativo + display LCD
colorido 284x76 (provável ST7789 via SPI), rodando modelos A2-Lite em tempo
real com folga de CPU.

## Regras estritas (NÃO NEGOCIÁVEIS)

1. **Nunca altere o núcleo de inferência do NAM** (arquivos do engine C
   extraídos de `nam-pedal`: matmuls, ativações, lógica de camadas dilatadas).
   Se algo parecer precisar de mudança ali, pare e reporte — não "conserte"
   sozinho.
2. **Nunca misture aritmética fixed-point neste projeto.** O engine NAM usa
   `float` (single precision) por design — não introduza Q31/Q15 aqui,
   mesmo que outros projetos do autor usem fixed-point.
3. **Camada de áudio (DSP) e camada de UI são módulos separados**, comunicando
   apenas via uma struct de parâmetros lida no início de cada bloco de áudio.
   A UI (encoder, display, debounce) NUNCA escreve parâmetros direto dentro
   do callback de áudio/interrupção de amostra.
4. **Nunca aloque memória dinamicamente (malloc/new) dentro do callback de
   áudio.** Toda alocação de buffers de modelo, ring buffers, buffers de IR
   acontece no boot/carregamento de preset, fora do caminho de tempo real.
5. **Placement de memória é intencional, não deixado ao compilador:**
   - Pesos do modelo A2-Lite ativo → DTCM (128KB, zero wait-state)
   - Ring buffers das camadas dilatadas → AXI SRAM / SRAM1-3
   - Buffer do IR de cabinet (curto, ~500-2400 taps) → SRAM comum
   - Arquivos `.namb` e IRs em repouso → flash externa QSPI/SPI
   Ao adicionar/mover buffers, declare explicitamente a seção de memória
   (linker script / atributos `__attribute__((section(...)))`), não confie
   em default.
6. **Build deve permanecer bare-metal / HAL da ST (CubeH7).** Não introduza
   libDaisy, JUCE, ou outros frameworks de abstração sem discutir antes —
   o objetivo é controle direto de registradores e timing.

## Estrutura de diretórios (alvo)

```
/core/           <- engine NAM extraído do nam-pedal, intocado pelas regras acima
/ir/             <- convolução FIR do cabinet (curto, tempo direto, CMSIS-DSP)
/ui/             <- encoder (rotação + clique curto/longo), state machine de página
/display/        <- driver ST7789 (ou equivalente confirmado), double buffer via DMA
/audio/          <- SAI/I2S init, callback de bloco, glue entre core/ir/ui
/storage/        <- loader .namb da flash QSPI/SPI, gerenciamento de presets
/linker/         <- seções de memória customizadas (DTCM, AXI SRAM, etc)
AGENTS.md
```

## Fase 1 (MVP) — escopo desta primeira rodada

- [ ] Extrair engine de inferência do `nam-pedal`, remover dependências de libDaisy
- [ ] HAL mínima do H743: clock tree (480MHz), SAI/I2S para codec de áudio, DMA
- [ ] Carregar UM modelo `.namb` fixo (hardcoded, sem seleção ainda) da flash QSPI
- [ ] Pesos em DTCM, ring buffers em SRAM — confirmar via linker script
- [ ] Callback de áudio rodando o modelo em tempo real, validar com osciloscópio/
      analisador que não há xruns em buffer de 48 amostras @ 48kHz
- [ ] Encoder básico: girar = ajusta input gain trim (sem display ainda,
      só validar leitura via UART debug)
- [ ] Display: inicializar driver, mostrar texto estático (nome do preset fixo)

**Fora de escopo nesta fase:** seleção de múltiplos modelos, IR de cabinet,
EQ pós-modelo, gate, crossfade entre modelos, páginas de UI múltiplas.
Isso vem depois que o pipeline básico estiver validado como estável em
tempo real.

## Perguntas para parar e confirmar com o humano (não assumir)

- Pinagem exata usada entre WeAct H743, breakout PCM1808, breakout PCM5102
  e display ST7789 (quais GPIOs/periféricos físicos estão conectados a quê)
- Confirmar se a breakout PCM5102 tem SCK/MCLK amarrado em GND (modo sem
  MCLK) ou se espera MCLK do host — muda a config do SAI
- Confirmar pino de strapping de formato/modo do PCM1808 (I2S vs modos
  alternativos que o chip suporta)
- Qual bootloader mínimo usar para o esquema `BOOT_QSPI` nesta placa WeAct
  (o `nam-pedal` original assume o bootloader específico do Daisy — precisa
  de um equivalente para esta placa, não copiar o do Daisy sem adaptar)

**Resolvido:** PCM5102 com SCK/MCLK aterrado (modo sem MCLK, clock derivado
via PLL interno do BCK). Configurar o SAI sem gerar/rotear saída de MCLK
para este bloco.
