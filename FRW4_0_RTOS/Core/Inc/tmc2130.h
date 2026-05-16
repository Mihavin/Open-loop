#ifndef TMC2130_H
#define TMC2130_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "stm32h7xx_hal.h"

/*
 * TMC2130 driver for FRW4.0 schematic U10 = TMC2130-TAE.
 *
 * Schematic / FRW4_0_RTOS mapping:
 *   SPI3:  CSN  = EXT_NSS  = PA4 / GPIO software CS
 *          SCK  = EXT_SCK  = PC10 / SPI3_SCK
 *          SDO  = EXT_MISO = PC11 / SPI3_MISO
 *          SDI  = EXT_MOSI = PC12 / SPI3_MOSI
 *   STEP        = PH9 / TIM12_CH2, through R59
 *   DIR         = PH7, through R58
 *   ENN         = PG3 / DRV_ENN, through R57, active low
 *   SPI_MODE    = PI3, should be driven high for SPI mode
 *   DC_EN       = PD11, optional dcStep enable, default low
 *   DC_IN       = PG2, optional dcStep input/status
 *   DIAG0/DIAG1 = PC7 / PC8
 *   CLK         = PC9 / MCO2
 *
 * IMPORTANT:
 *   TMC2130 SPI transfers are 40-bit frames. Configure SPI3 as 8-bit data.
 *   SPI3 must remain configured as 8-bit data with software NSS; EXT_NSS is
 *   driven explicitly as an active-low GPIO chip select.
 */

#ifndef TMC2130_SPI_TIMEOUT_MS
#define TMC2130_SPI_TIMEOUT_MS  10u
#endif

#ifndef TMC2130_MAX_TIMER_ARR
#define TMC2130_MAX_TIMER_ARR   0xFFFFu
#endif

typedef enum
{
    TMC2130_DIR_FORWARD = 0,
    TMC2130_DIR_REVERSE = 1,
} TMC2130_Direction_t;

typedef enum
{
    TMC2130_REG_GCONF       = 0x00,
    TMC2130_REG_GSTAT       = 0x01,
    TMC2130_REG_IOIN        = 0x04,
    TMC2130_REG_IHOLD_IRUN  = 0x10,
    TMC2130_REG_TPOWERDOWN  = 0x11,
    TMC2130_REG_TSTEP       = 0x12,
    TMC2130_REG_TPWMTHRS    = 0x13,
    TMC2130_REG_TCOOLTHRS   = 0x14,
    TMC2130_REG_THIGH       = 0x15,
    TMC2130_REG_XDIRECT     = 0x2D,
    TMC2130_REG_VDCMIN      = 0x33,
    TMC2130_REG_CHOPCONF    = 0x6C,
    TMC2130_REG_COOLCONF    = 0x6D,
    TMC2130_REG_DCCTRL      = 0x6E,
    TMC2130_REG_DRV_STATUS  = 0x6F,
    TMC2130_REG_PWMCONF     = 0x70,
    TMC2130_REG_PWM_SCALE   = 0x71,
    TMC2130_REG_ENCM_CTRL   = 0x72,
    TMC2130_REG_LOST_STEPS  = 0x73,
} TMC2130_Register_t;

typedef struct
{
    SPI_HandleTypeDef *hspi;

    /* Optional software chip select. TMC2130_Init() defaults this to
     * EXT_NSS (PA4) as an active-low GPIO software CS for SPI3.
     */
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;

    TIM_HandleTypeDef *step_tim;
    uint32_t step_channel;

    uint32_t spi_timeout_ms;
    uint32_t step_timer_clock_hz; /* 0 = auto-detect from RCC */

    bool step_clock_running;
    uint32_t step_frequency_hz;
} TMC2130_Handle_t;

typedef struct
{
    uint8_t ihold;          /* 0..31 */
    uint8_t irun;           /* 0..31 */
    uint8_t ihold_delay;    /* 0..15 */

    uint16_t microsteps;    /* 1,2,4,8,16,32,64,128,256 */
    bool interpolate_256;   /* CHOPCONF.INTPOL */
    bool stealthchop_enable;/* GCONF.EN_PWM_MODE */

    uint8_t toff;           /* 1..15; 0 disables driver chopper */
    uint8_t hstrt;          /* 0..7 */
    uint8_t hend;           /* 0..15 raw field */
    uint8_t tbl;            /* 0..3 */

    uint32_t tpwmthrs;
    uint32_t tcoolthrs;
    uint32_t thigh;
    uint32_t pwmconf;

    bool dcstep_enable;
} TMC2130_Config_t;

void TMC2130_ConfigDefaults(TMC2130_Config_t *cfg);

HAL_StatusTypeDef TMC2130_Init(TMC2130_Handle_t *dev,
                              SPI_HandleTypeDef *hspi,
                              TIM_HandleTypeDef *step_tim,
                              uint32_t step_channel,
                              const TMC2130_Config_t *cfg);

HAL_StatusTypeDef TMC2130_InitEx(TMC2130_Handle_t *dev,
                                SPI_HandleTypeDef *hspi,
                                GPIO_TypeDef *cs_port,
                                uint16_t cs_pin,
                                TIM_HandleTypeDef *step_tim,
                                uint32_t step_channel,
                                const TMC2130_Config_t *cfg);

HAL_StatusTypeDef TMC2130_WriteReg(TMC2130_Handle_t *dev, uint8_t addr, uint32_t value);
HAL_StatusTypeDef TMC2130_ReadReg(TMC2130_Handle_t *dev, uint8_t addr, uint32_t *value);

HAL_StatusTypeDef TMC2130_Enable(TMC2130_Handle_t *dev, bool enable);
void TMC2130_SetDirection(TMC2130_Direction_t dir);

HAL_StatusTypeDef TMC2130_SetStepFrequency(TMC2130_Handle_t *dev, uint32_t frequency_hz);
HAL_StatusTypeDef TMC2130_StartStepClock(TMC2130_Handle_t *dev, uint32_t frequency_hz);
HAL_StatusTypeDef TMC2130_StopStepClock(TMC2130_Handle_t *dev);
bool TMC2130_IsStepClockRunning(const TMC2130_Handle_t *dev);
uint32_t TMC2130_GetStepFrequencyHz(const TMC2130_Handle_t *dev);

HAL_StatusTypeDef TMC2130_MoveBlocking(TMC2130_Handle_t *dev,
                                      int32_t steps,
                                      uint32_t frequency_hz);

HAL_StatusTypeDef TMC2130_ReadGSTAT(TMC2130_Handle_t *dev, uint32_t *gstat);
HAL_StatusTypeDef TMC2130_ClearGSTAT(TMC2130_Handle_t *dev);
HAL_StatusTypeDef TMC2130_ReadDRVStatus(TMC2130_Handle_t *dev, uint32_t *drv_status);

bool TMC2130_ReadDiag0(void);
bool TMC2130_ReadDiag1(void);
bool TMC2130_ReadDcIn(void);

#ifdef __cplusplus
}
#endif

#endif /* TMC2130_H */
