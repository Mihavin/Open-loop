#include "tmc2130.h"

#include "main.h"
#include "tim.h"
#include <string.h>

/* GCONF bits used here */
#define TMC2130_GCONF_EN_PWM_MODE       (1UL << 2)

/* GSTAT bits: write 1 to clear */
#define TMC2130_GSTAT_RESET             (1UL << 0)
#define TMC2130_GSTAT_DRV_ERR           (1UL << 1)
#define TMC2130_GSTAT_UV_CP             (1UL << 2)

/* CHOPCONF bit positions */
#define TMC2130_CHOPCONF_TOFF_POS       0u
#define TMC2130_CHOPCONF_HSTRT_POS      4u
#define TMC2130_CHOPCONF_HEND_POS       7u
#define TMC2130_CHOPCONF_TBL_POS        15u
#define TMC2130_CHOPCONF_MRES_POS       24u
#define TMC2130_CHOPCONF_INTPOL         (1UL << 28)

/* Conservative bring-up defaults. Tune current/chopper values on hardware. */
#define TMC2130_DEFAULT_PWMCONF         0x00050480UL

static inline void tmc2130_delay_short(void)
{
    for (volatile uint32_t i = 0; i < 80u; ++i) { __NOP(); }
}

static inline void tmc2130_cs_low(TMC2130_Handle_t *dev)
{
    if (dev->cs_port != NULL)
        HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
}

static inline void tmc2130_cs_high(TMC2130_Handle_t *dev)
{
    if (dev->cs_port != NULL)
        HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
}

static HAL_StatusTypeDef tmc2130_validate_spi(TMC2130_Handle_t *dev)
{
    if ((dev == NULL) || (dev->hspi == NULL))
        return HAL_ERROR;

    /* TMC2130 uses 5 bytes per SPI frame. STM32 SPI must be byte-oriented. */
    if (dev->hspi->Init.DataSize != SPI_DATASIZE_8BIT)
        return HAL_ERROR;

    return HAL_OK;
}

static HAL_StatusTypeDef tmc2130_xfer40(TMC2130_Handle_t *dev,
                                        const uint8_t tx[5],
                                        uint8_t rx[5])
{
    if (tmc2130_validate_spi(dev) != HAL_OK)
        return HAL_ERROR;

    uint8_t tmp_rx[5] = {0};
    if (rx == NULL)
        rx = tmp_rx;

    tmc2130_cs_low(dev);
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(dev->hspi,
                                                   (uint8_t *)tx,
                                                   rx,
                                                   5u,
                                                   dev->spi_timeout_ms);
    tmc2130_cs_high(dev);
    tmc2130_delay_short();

    return st;
}

static uint8_t tmc2130_mres_from_microsteps(uint16_t microsteps)
{
    switch (microsteps)
    {
        case 256u: return 0u;
        case 128u: return 1u;
        case 64u:  return 2u;
        case 32u:  return 3u;
        case 16u:  return 4u;
        case 8u:   return 5u;
        case 4u:   return 6u;
        case 2u:   return 7u;
        case 1u:   return 8u;
        default:   return 4u; /* fallback: 16 microsteps */
    }
}

static uint32_t tmc2130_make_ihold_irun(const TMC2130_Config_t *cfg)
{
    return ((uint32_t)(cfg->ihold & 0x1Fu) << 0) |
           ((uint32_t)(cfg->irun & 0x1Fu) << 8) |
           ((uint32_t)(cfg->ihold_delay & 0x0Fu) << 16);
}

static uint32_t tmc2130_make_chopconf(const TMC2130_Config_t *cfg)
{
    uint32_t chop = 0u;
    const uint8_t mres = tmc2130_mres_from_microsteps(cfg->microsteps);

    chop |= ((uint32_t)(cfg->toff  & 0x0Fu) << TMC2130_CHOPCONF_TOFF_POS);
    chop |= ((uint32_t)(cfg->hstrt & 0x07u) << TMC2130_CHOPCONF_HSTRT_POS);
    chop |= ((uint32_t)(cfg->hend  & 0x0Fu) << TMC2130_CHOPCONF_HEND_POS);
    chop |= ((uint32_t)(cfg->tbl   & 0x03u) << TMC2130_CHOPCONF_TBL_POS);
    chop |= ((uint32_t)mres << TMC2130_CHOPCONF_MRES_POS);

    if (cfg->interpolate_256)
        chop |= TMC2130_CHOPCONF_INTPOL;

    return chop;
}

static uint32_t tmc2130_get_tim_clk_hz(TIM_HandleTypeDef *htim)
{
    RCC_ClkInitTypeDef clk = {0};
    uint32_t flash_latency = 0u;
    HAL_RCC_GetClockConfig(&clk, &flash_latency);

    if ((htim->Instance == TIM1) || (htim->Instance == TIM8))
    {
        uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
        return (clk.APB2CLKDivider == RCC_HCLK_DIV1) ? pclk2 : (pclk2 * 2u);
    }
    else
    {
        uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
        return (clk.APB1CLKDivider == RCC_HCLK_DIV1) ? pclk1 : (pclk1 * 2u);
    }
}

void TMC2130_ConfigDefaults(TMC2130_Config_t *cfg)
{
    if (cfg == NULL)
        return;

    memset(cfg, 0, sizeof(*cfg));

    cfg->ihold = 8u;
    cfg->irun = 16u;
    cfg->ihold_delay = 6u;

    cfg->microsteps = 16u;
    cfg->interpolate_256 = true;
    cfg->stealthchop_enable = false;

    cfg->toff = 3u;
    cfg->hstrt = 4u;
    cfg->hend = 1u;
    cfg->tbl = 2u;

    cfg->tpwmthrs = 0u;
    cfg->tcoolthrs = 0u;
    cfg->thigh = 0u;
    cfg->pwmconf = TMC2130_DEFAULT_PWMCONF;

    cfg->dcstep_enable = false;
}

HAL_StatusTypeDef TMC2130_WriteReg(TMC2130_Handle_t *dev, uint8_t addr, uint32_t value)
{
    uint8_t tx[5];
    uint8_t rx[5];

    tx[0] = (uint8_t)(0x80u | (addr & 0x7Fu));
    tx[1] = (uint8_t)(value >> 24);
    tx[2] = (uint8_t)(value >> 16);
    tx[3] = (uint8_t)(value >> 8);
    tx[4] = (uint8_t)(value >> 0);

    return tmc2130_xfer40(dev, tx, rx);
}

HAL_StatusTypeDef TMC2130_ReadReg(TMC2130_Handle_t *dev, uint8_t addr, uint32_t *value)
{
    if (value == NULL)
        return HAL_ERROR;

    uint8_t tx[5] = { (uint8_t)(addr & 0x7Fu), 0, 0, 0, 0 };
    uint8_t rx[5] = {0};

    /* TMC read data is returned by the next SPI transaction. */
    if (tmc2130_xfer40(dev, tx, rx) != HAL_OK)
        return HAL_ERROR;
    if (tmc2130_xfer40(dev, tx, rx) != HAL_OK)
        return HAL_ERROR;

    *value = ((uint32_t)rx[1] << 24) |
             ((uint32_t)rx[2] << 16) |
             ((uint32_t)rx[3] << 8) |
             ((uint32_t)rx[4]);

    return HAL_OK;
}

HAL_StatusTypeDef TMC2130_Init(TMC2130_Handle_t *dev,
                              SPI_HandleTypeDef *hspi,
                              TIM_HandleTypeDef *step_tim,
                              uint32_t step_channel,
                              const TMC2130_Config_t *cfg)
{
    return TMC2130_InitEx(dev, hspi, NULL, 0u, step_tim, step_channel, cfg);
}

HAL_StatusTypeDef TMC2130_InitEx(TMC2130_Handle_t *dev,
                                SPI_HandleTypeDef *hspi,
                                GPIO_TypeDef *cs_port,
                                uint16_t cs_pin,
                                TIM_HandleTypeDef *step_tim,
                                uint32_t step_channel,
                                const TMC2130_Config_t *cfg)
{
    if ((dev == NULL) || (hspi == NULL) || (step_tim == NULL))
        return HAL_ERROR;

    TMC2130_Config_t local_cfg;
    if (cfg == NULL)
    {
        TMC2130_ConfigDefaults(&local_cfg);
        cfg = &local_cfg;
    }

    memset(dev, 0, sizeof(*dev));
    dev->hspi = hspi;
    dev->cs_port = cs_port;
    dev->cs_pin = cs_pin;
    dev->step_tim = step_tim;
    dev->step_channel = step_channel;
    dev->spi_timeout_ms = TMC2130_SPI_TIMEOUT_MS;

    if (tmc2130_validate_spi(dev) != HAL_OK)
        return HAL_ERROR;

    if (dev->cs_port != NULL)
        tmc2130_cs_high(dev);

    /* Board pins from schematic. */
    HAL_GPIO_WritePin(SPI_MODE_GPIO_Port, SPI_MODE_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DC_EN_GPIO_Port, DC_EN_Pin,
                      cfg->dcstep_enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DRV_ENN_GPIO_Port, DRV_ENN_Pin, GPIO_PIN_SET); /* disabled */

    (void)TMC2130_StopStepClock(dev);

    uint32_t gconf = 0u;
    if (cfg->stealthchop_enable)
        gconf |= TMC2130_GCONF_EN_PWM_MODE;

    if (TMC2130_WriteReg(dev, TMC2130_REG_GCONF, gconf) != HAL_OK)
        return HAL_ERROR;

    if (TMC2130_ClearGSTAT(dev) != HAL_OK)
        return HAL_ERROR;

    if (TMC2130_WriteReg(dev, TMC2130_REG_IHOLD_IRUN, tmc2130_make_ihold_irun(cfg)) != HAL_OK)
        return HAL_ERROR;

    if (TMC2130_WriteReg(dev, TMC2130_REG_TPOWERDOWN, 10u) != HAL_OK)
        return HAL_ERROR;

    if (TMC2130_WriteReg(dev, TMC2130_REG_TPWMTHRS, cfg->tpwmthrs) != HAL_OK)
        return HAL_ERROR;

    if (TMC2130_WriteReg(dev, TMC2130_REG_TCOOLTHRS, cfg->tcoolthrs) != HAL_OK)
        return HAL_ERROR;

    if (TMC2130_WriteReg(dev, TMC2130_REG_THIGH, cfg->thigh) != HAL_OK)
        return HAL_ERROR;

    if (cfg->stealthchop_enable)
    {
        if (TMC2130_WriteReg(dev, TMC2130_REG_PWMCONF, cfg->pwmconf) != HAL_OK)
            return HAL_ERROR;
    }

    if (TMC2130_WriteReg(dev, TMC2130_REG_CHOPCONF, tmc2130_make_chopconf(cfg)) != HAL_OK)
        return HAL_ERROR;

    return HAL_OK;
}

HAL_StatusTypeDef TMC2130_Enable(TMC2130_Handle_t *dev, bool enable)
{
    (void)dev;
    HAL_GPIO_WritePin(DRV_ENN_GPIO_Port, DRV_ENN_Pin,
                      enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
    return HAL_OK;
}

void TMC2130_SetDirection(TMC2130_Direction_t dir)
{
    HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin,
                      (dir == TMC2130_DIR_REVERSE) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

HAL_StatusTypeDef TMC2130_SetStepFrequency(TMC2130_Handle_t *dev, uint32_t frequency_hz)
{
    if ((dev == NULL) || (dev->step_tim == NULL) || (frequency_hz == 0u))
        return HAL_ERROR;

    uint32_t tim_clk = dev->step_timer_clock_hz;
    if (tim_clk == 0u)
        tim_clk = tmc2130_get_tim_clk_hz(dev->step_tim);

    if (tim_clk == 0u)
        return HAL_ERROR;

    uint64_t ticks = ((uint64_t)tim_clk + ((uint64_t)frequency_hz / 2u)) / (uint64_t)frequency_hz;
    if (ticks < 2u)
        ticks = 2u;

    uint64_t presc_factor = (ticks + TMC2130_MAX_TIMER_ARR) / ((uint64_t)TMC2130_MAX_TIMER_ARR + 1u);
    if (presc_factor < 1u)
        presc_factor = 1u;
    if (presc_factor > 65536u)
        presc_factor = 65536u;

    uint64_t arr_plus1 = ticks / presc_factor;
    if (arr_plus1 < 2u)
        arr_plus1 = 2u;
    if (arr_plus1 > ((uint64_t)TMC2130_MAX_TIMER_ARR + 1u))
        arr_plus1 = (uint64_t)TMC2130_MAX_TIMER_ARR + 1u;

    uint32_t psc = (uint32_t)(presc_factor - 1u);
    uint32_t arr = (uint32_t)(arr_plus1 - 1u);
    uint32_t ccr = (arr + 1u) / 2u;

    (void)HAL_TIM_PWM_Stop(dev->step_tim, dev->step_channel);
    __HAL_TIM_DISABLE(dev->step_tim);

    __HAL_TIM_SET_PRESCALER(dev->step_tim, psc);
    __HAL_TIM_SET_AUTORELOAD(dev->step_tim, arr);
    __HAL_TIM_SET_COMPARE(dev->step_tim, dev->step_channel, ccr);
    __HAL_TIM_SET_COUNTER(dev->step_tim, 0u);

    HAL_TIM_GenerateEvent(dev->step_tim, TIM_EVENTSOURCE_UPDATE);
    __HAL_TIM_ENABLE(dev->step_tim);

    dev->step_frequency_hz = frequency_hz;
    return HAL_OK;
}

HAL_StatusTypeDef TMC2130_StartStepClock(TMC2130_Handle_t *dev, uint32_t frequency_hz)
{
    if (TMC2130_SetStepFrequency(dev, frequency_hz) != HAL_OK)
        return HAL_ERROR;

    if (HAL_TIM_PWM_Start(dev->step_tim, dev->step_channel) != HAL_OK)
        return HAL_ERROR;

    dev->step_clock_running = true;
    return HAL_OK;
}

HAL_StatusTypeDef TMC2130_StopStepClock(TMC2130_Handle_t *dev)
{
    if ((dev == NULL) || (dev->step_tim == NULL))
        return HAL_ERROR;

    (void)HAL_TIM_PWM_Stop(dev->step_tim, dev->step_channel);
    __HAL_TIM_SET_COMPARE(dev->step_tim, dev->step_channel, 0u);
    dev->step_clock_running = false;
    dev->step_frequency_hz = 0u;
    return HAL_OK;
}

HAL_StatusTypeDef TMC2130_MoveBlocking(TMC2130_Handle_t *dev,
                                      int32_t steps,
                                      uint32_t frequency_hz)
{
    if ((dev == NULL) || (steps == 0) || (frequency_hz == 0u))
        return HAL_ERROR;

    uint32_t abs_steps;
    if (steps < 0)
    {
        TMC2130_SetDirection(TMC2130_DIR_REVERSE);
        abs_steps = (uint32_t)(-steps);
    }
    else
    {
        TMC2130_SetDirection(TMC2130_DIR_FORWARD);
        abs_steps = (uint32_t)steps;
    }

    if (TMC2130_StartStepClock(dev, frequency_hz) != HAL_OK)
        return HAL_ERROR;

    uint64_t duration_ms = (((uint64_t)abs_steps * 1000u) + frequency_hz - 1u) / frequency_hz;
    while (duration_ms > 0u)
    {
        uint32_t chunk = (duration_ms > 100u) ? 100u : (uint32_t)duration_ms;
        HAL_Delay(chunk);
        duration_ms -= chunk;
    }

    return TMC2130_StopStepClock(dev);
}

HAL_StatusTypeDef TMC2130_ReadGSTAT(TMC2130_Handle_t *dev, uint32_t *gstat)
{
    return TMC2130_ReadReg(dev, TMC2130_REG_GSTAT, gstat);
}

HAL_StatusTypeDef TMC2130_ClearGSTAT(TMC2130_Handle_t *dev)
{
    return TMC2130_WriteReg(dev, TMC2130_REG_GSTAT,
                            TMC2130_GSTAT_RESET |
                            TMC2130_GSTAT_DRV_ERR |
                            TMC2130_GSTAT_UV_CP);
}

HAL_StatusTypeDef TMC2130_ReadDRVStatus(TMC2130_Handle_t *dev, uint32_t *drv_status)
{
    return TMC2130_ReadReg(dev, TMC2130_REG_DRV_STATUS, drv_status);
}

bool TMC2130_ReadDiag0(void)
{
    return HAL_GPIO_ReadPin(DIAG0_GPIO_Port, DIAG0_Pin) == GPIO_PIN_SET;
}

bool TMC2130_ReadDiag1(void)
{
    return HAL_GPIO_ReadPin(DIAG1_GPIO_Port, DIAG1_Pin) == GPIO_PIN_SET;
}

bool TMC2130_ReadDcIn(void)
{
    return HAL_GPIO_ReadPin(DC_IN_GPIO_Port, DC_IN_Pin) == GPIO_PIN_SET;
}
