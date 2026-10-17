#include "tmc2130.h"

#include "main.h"
#include "tim.h"
#include "fault_manager.h"
#include <limits.h>

static TMC2130_Handle_t *g_step_dev;
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
    if (dev->hspi->Init.DataSize != SPI_DATASIZE_8BIT ||
        dev->hspi->Init.CLKPolarity != SPI_POLARITY_HIGH ||
        dev->hspi->Init.CLKPhase != SPI_PHASE_2EDGE)
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
    return TMC2130_InitEx(dev,
                          hspi,
                          EXT_NSS_GPIO_Port,
                          EXT_NSS_Pin,
                          step_tim,
                          step_channel,
                          cfg);
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

    /* TIM12 is a free-running 1 MHz timebase. CH2 toggles at scheduled
       compare deadlines; changing speed never resets timer phase. */
    if (step_tim->Instance != TIM12 || step_channel != TIM_CHANNEL_2) return HAL_ERROR;
    uint32_t timer_hz = tmc2130_get_tim_clk_hz(step_tim);
    uint32_t divider = (timer_hz + 999999u) / 1000000u;
    if (divider == 0u || divider > 65536u) return HAL_ERROR;
    dev->step_timer_clock_hz = timer_hz / divider;
    __HAL_TIM_DISABLE(step_tim);
    __HAL_TIM_SET_PRESCALER(step_tim, divider - 1u);
    __HAL_TIM_SET_AUTORELOAD(step_tim, 65535u);
    HAL_TIM_GenerateEvent(step_tim, TIM_EVENTSOURCE_UPDATE);
    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode = TIM_OCMODE_FORCED_INACTIVE;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    if (HAL_TIM_OC_ConfigChannel(step_tim, &oc, TIM_CHANNEL_2) != HAL_OK) return HAL_ERROR;
    g_step_dev = dev;
    dev->step_direction = 1;
    dev->step_target = INT32_MAX;
    HAL_NVIC_SetPriority(TIM8_BRK_TIM12_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(TIM8_BRK_TIM12_IRQn);

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

    uint32_t readback;
    if (TMC2130_ReadReg(dev, TMC2130_REG_IOIN, &readback) != HAL_OK ||
        (readback >> 24) != 0x11u) return HAL_ERROR;
    if (TMC2130_ReadReg(dev, TMC2130_REG_GCONF, &readback) != HAL_OK || readback != gconf) return HAL_ERROR;
    if (TMC2130_ReadReg(dev, TMC2130_REG_CHOPCONF, &readback) != HAL_OK ||
        readback != tmc2130_make_chopconf(cfg)) return HAL_ERROR;
    return HAL_OK;
}

HAL_StatusTypeDef TMC2130_Enable(TMC2130_Handle_t *dev, bool enable)
{
    if (!dev) return HAL_ERROR;
    uint32_t key = __get_PRIMASK(); __disable_irq();
    if (enable && Fault_IsCriticalActive()) { __set_PRIMASK(key); return HAL_ERROR; }
    HAL_GPIO_WritePin(DRV_ENN_GPIO_Port, DRV_ENN_Pin, enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
    __set_PRIMASK(key);
    return HAL_OK;
}

void TMC2130_SetDirection(TMC2130_Direction_t dir)
{
    if (g_step_dev && g_step_dev->step_clock_running) return;
    HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, dir == TMC2130_DIR_REVERSE ? GPIO_PIN_SET : GPIO_PIN_RESET);
    if (g_step_dev) g_step_dev->step_direction = dir == TMC2130_DIR_REVERSE ? -1 : 1;
}

static void step_force_low(TMC2130_Handle_t *dev)
{
    TIM_TypeDef *tim = dev->step_tim->Instance;
    tim->DIER &= ~TIM_DIER_CC2IE;
    tim->CCER &= ~TIM_CCER_CC2E;
    MODIFY_REG(tim->CCMR1, TIM_CCMR1_OC2M | TIM_CCMR1_OC2PE,
               TIM_OCMODE_FORCED_INACTIVE << 8);
    tim->CCER |= TIM_CCER_CC2E; /* Drive the STEP pad low, including at rest. */
    dev->step_clock_running = false;
    dev->step_high = false;
    dev->stop_requested = false;
    dev->step_frequency_hz = 0;
}

void TMC2130_SetStepTarget(TMC2130_Handle_t *dev, int32_t target)
{
    uint32_t key = __get_PRIMASK(); __disable_irq();
    dev->step_target = target;
    __set_PRIMASK(key);
}

int32_t TMC2130_GetEmittedSteps(const TMC2130_Handle_t *dev)
{
    return dev ? dev->emitted_steps : 0;
}

HAL_StatusTypeDef TMC2130_ZeroEmittedSteps(TMC2130_Handle_t *dev)
{
    if (!dev || dev->step_clock_running) return HAL_ERROR;
    dev->emitted_steps = dev->step_target = 0;
    return HAL_OK;
}

HAL_StatusTypeDef TMC2130_SetStepFrequency(TMC2130_Handle_t *dev, uint32_t frequency_hz)
{
    if (!dev || !dev->step_tim || frequency_hz < 8u || frequency_hz > 2000u) return HAL_ERROR;
    uint32_t half = dev->step_timer_clock_hz / (2u * frequency_hz);
    if (half < 5u || half > 65535u) return HAL_ERROR;
    /* Atomic single-word update: the ISR applies it at the next edge. */
    dev->half_period_ticks = half;
    dev->step_frequency_hz = frequency_hz;
    return HAL_OK;
}

HAL_StatusTypeDef TMC2130_StartStepClock(TMC2130_Handle_t *dev, uint32_t frequency_hz)
{
    if (TMC2130_SetStepFrequency(dev, frequency_hz) != HAL_OK) return HAL_ERROR;
    uint32_t key = __get_PRIMASK(); __disable_irq();
    if (Fault_IsCriticalActive()) { __set_PRIMASK(key); return HAL_ERROR; }
    if (dev->step_clock_running) { __set_PRIMASK(key); return HAL_OK; }
    if ((dev->step_direction > 0 && dev->emitted_steps >= dev->step_target) ||
        (dev->step_direction < 0 && dev->emitted_steps <= dev->step_target))
    { __set_PRIMASK(key); return HAL_OK; }
    TIM_TypeDef *tim = dev->step_tim->Instance;
    step_force_low(dev);
    MODIFY_REG(tim->CCMR1, TIM_CCMR1_OC2M | TIM_CCMR1_OC2PE, TIM_OCMODE_TOGGLE << 8);
    tim->CCR2 = (uint16_t)(tim->CNT + 5u); /* >=5 us direction setup */
    __HAL_TIM_CLEAR_FLAG(dev->step_tim, TIM_FLAG_CC2);
    dev->step_clock_running = true;
    dev->step_frequency_hz = frequency_hz;
    tim->DIER |= TIM_DIER_CC2IE;
    tim->CR1 |= TIM_CR1_CEN;
    __set_PRIMASK(key);
    return HAL_OK;
}

HAL_StatusTypeDef TMC2130_StopStepClock(TMC2130_Handle_t *dev)
{
    if (!dev || !dev->step_tim) return HAL_ERROR;
    uint32_t key = __get_PRIMASK(); __disable_irq();
    if (dev->step_clock_running && dev->step_high && !Fault_IsCriticalActive())
        dev->stop_requested = true; /* Complete the pulse before normal stop. */
    else step_force_low(dev);
    __set_PRIMASK(key);
    return HAL_OK;
}

bool TMC2130_IsStepClockRunning(const TMC2130_Handle_t *dev)
{
    return dev && dev->step_clock_running;
}

uint32_t TMC2130_GetStepFrequencyHz(const TMC2130_Handle_t *dev)
{
    return dev ? dev->step_frequency_hz : 0;
}

void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
    TMC2130_Handle_t *dev = g_step_dev;
    if (!dev || htim != dev->step_tim || htim->Channel != HAL_TIM_ACTIVE_CHANNEL_2) return;
    if (Fault_IsCriticalActive()) { step_force_low(dev); return; }
    if (!dev->step_clock_running) return;
    dev->step_high = !dev->step_high; /* Hardware has just toggled CH2. */
    if (dev->step_high) dev->emitted_steps += dev->step_direction;
    else if (dev->stop_requested ||
             (dev->step_direction > 0 && dev->emitted_steps >= dev->step_target) ||
             (dev->step_direction < 0 && dev->emitted_steps <= dev->step_target))
    { step_force_low(dev); return; }
    uint32_t half = dev->half_period_ticks;
    if ((uint16_t)(htim->Instance->CNT - htim->Instance->CCR2) >= half - 2u)
    { Fault_Report(FAULT_MOTION_TIMEOUT); step_force_low(dev); return; }
    htim->Instance->CCR2 = (uint16_t)(htim->Instance->CCR2 + half);
}

void TIM8_BRK_TIM12_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim12);
}

HAL_StatusTypeDef TMC2130_MoveBlocking(TMC2130_Handle_t *dev, int32_t steps, uint32_t frequency_hz)
{
    /* Application motion belongs to StepperTask. Legacy timed moves cannot
       guarantee pulse counts and are deliberately unavailable. */
    (void)dev; (void)steps; (void)frequency_hz;
    return HAL_ERROR;
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
