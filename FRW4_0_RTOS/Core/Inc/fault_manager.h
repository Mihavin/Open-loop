#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    FAULT_NONE            = 0,
    FAULT_BLDC_DRIVER     = 1u << 0,
    FAULT_STEPPER_DRIVER  = 1u << 1,
    FAULT_FAN             = 1u << 2,
    FAULT_LIMIT           = 1u << 3,
    FAULT_SPI             = 1u << 4,
    FAULT_MOTION_TIMEOUT  = 1u << 5,
} FaultBits_t;

#define FAULT_CRITICAL_MASK (FAULT_BLDC_DRIVER | \
                             FAULT_STEPPER_DRIVER | \
                             FAULT_LIMIT | \
                             FAULT_SPI | \
                             FAULT_MOTION_TIMEOUT)

void Fault_Report(uint32_t fault_bits);
void Fault_Clear(uint32_t fault_bits);
uint32_t Fault_GetActive(void);
bool Fault_IsActive(void);
bool Fault_IsCriticalActive(void);

#ifdef __cplusplus
}
#endif

#endif /* FAULT_MANAGER_H */
