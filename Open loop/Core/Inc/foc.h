
#ifndef TWO_PI
#define TWO_PI 6.28318530717958647692f
#endif
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void FOC_Init(void);

// Current offset calibration: motor must be stopped, PWM outputs disabled
void FOC_CalibrateCurrentOffsets(uint32_t samples, uint32_t delay_ms);

// One FOC update step (call at fixed rate, e.g. from TIM6 interrupt)
void FOC_ControlStep(void);

// Hard trip status (overcurrent or driver fault)
bool FOC_IsTripped(void);

// Debug variables (read-only)
float FOC_GetMechRpm(void);
float FOC_GetIabcRms(void);
float FOC_GetIqRef(void);

#ifdef __cplusplus
}
#endif
