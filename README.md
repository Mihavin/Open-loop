# STM32H723 Open-Loop SVPWM Skeleton

This repository provides a minimal `main.c` skeleton that demonstrates open-loop
space-vector PWM (SVPWM) for an STM32H723ZGT6 (LQFP144) paired with a DRV8323S
gate driver. The implementation keeps the motor spinning without encoder
feedback while sampling phase currents on SOA/SOB/SOC using ADC1 with DMA.

## Highlights
- 25 kHz PWM on TIM1 channels 1-3 driving the DRV8323S inputs.
- ADC1 (BMDA) triggered from TIM1 to capture current feedback on SOA/SOB/SOC.
- Fixed-frequency open-loop electrical rotation; no encoder required.
- Debug heartbeats on LED0 (PC15), LED1 (PG14) and LED3 (PC8).

## Adapting to Hardware
- Replace the placeholder `SystemClock_Config` with your board-specific clock
  tree setup (CubeMX generated or hand-written).
- Ensure TIM1 outputs are routed to the DRV8323S INHx/INLx pins and that PWM
  polarity matches the gate driver expectations.
- Map the SOA/SOB/SOC pins to ADC1 channels 1-3 as shown, or adjust the channel
  numbers to match your PCB layout.
- Configure dead-time and break inputs in `MX_TIM1_Init` if your power stage
  requires them.

## Building
Integrate `src/main.c` into your STM32CubeIDE or CMake-based firmware project
that pulls in the STM32H7 HAL. No encoder or FOC observer is required for this
basic open-loop demonstration. The example expects:

- TIM1 CH1/CH2/CH3 on PA8/PA9/PA10 wired to the DRV8323S PWM inputs.
- ADC1_INP1/INP2/INP3 on PA0/PA1/PA2 wired to SOA/SOB/SOC.
- LED0 on PC15, LED1 on PG14, LED3 on PC8 for heartbeats.

Replace the placeholder `SystemClock_Config` and adjust GPIOs if your board
routes the peripherals differently.
