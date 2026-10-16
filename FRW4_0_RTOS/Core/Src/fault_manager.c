#include "fault_manager.h"
#include "main.h"

static volatile uint32_t g_fault_bits = FAULT_NONE;
static volatile uint32_t g_health[FAULT_HEALTH_COUNT];
static bool g_watchdog_started;

void Fault_OutputsOff(void)
{
    /* Check clocks: this routine is also used during early startup failures. */
    if (RCC->AHB4ENR & RCC_AHB4ENR_GPIOFEN)
        DRV_EN_GPIO_Port->BSRR = (uint32_t)DRV_EN_Pin << 16;
    if (RCC->AHB4ENR & RCC_AHB4ENR_GPIOGEN)
        DRV_ENN_GPIO_Port->BSRR = DRV_ENN_Pin;
    if (RCC->APB2ENR & RCC_APB2ENR_TIM1EN)
        TIM1->BDTR &= ~TIM_BDTR_MOE;
    if (RCC->APB1LENR & RCC_APB1LENR_TIM12EN)
    {
        TIM12->DIER = 0;
        TIM12->CCER &= ~TIM_CCER_CC2E;
        TIM12->CR1 &= ~TIM_CR1_CEN;
    }
    __DSB();
}

void Fault_Report(uint32_t fault_bits)
{
    (void)__atomic_fetch_or(&g_fault_bits, fault_bits, __ATOMIC_SEQ_CST);
    if (fault_bits & FAULT_CRITICAL_MASK) Fault_OutputsOff();
}

void Fault_Clear(uint32_t fault_bits)
{
    fault_bits &= ~FAULT_CRITICAL_MASK;
    (void)__atomic_fetch_and(&g_fault_bits, ~fault_bits, __ATOMIC_SEQ_CST);
}

uint32_t Fault_GetActive(void)
{
    return __atomic_load_n(&g_fault_bits, __ATOMIC_SEQ_CST);
}

bool Fault_IsActive(void)
{
    return Fault_GetActive() != FAULT_NONE;
}

bool Fault_IsCriticalActive(void)
{
    return (Fault_GetActive() & FAULT_CRITICAL_MASK) != FAULT_NONE;
}

void Fault_Heartbeat(Fault_Health_t source)
{
    if ((unsigned)source < FAULT_HEALTH_COUNT)
        __atomic_store_n(&g_health[source], HAL_GetTick(), __ATOMIC_RELEASE);
}

void Fault_WatchdogStart(void)
{
    /* Self-contained clock setup survives CubeMX regeneration. */
    __HAL_RCC_LSI_ENABLE();
    uint32_t ready_start = HAL_GetTick();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) == 0u)
    {
        if ((uint32_t)(HAL_GetTick() - ready_start) > 100u)
        { Fault_Report(FAULT_CPU); return; }
    }
    /* Nominal timeout: 2 seconds at 32 kHz.
       Never freeze this watchdog for a production debugger session. */
    IWDG1->KR = 0xCCCCu;
    IWDG1->KR = 0x5555u;
    IWDG1->PR = 4u; /* /64 */
    IWDG1->RLR = 999u;
    uint32_t start = HAL_GetTick();
    while (IWDG1->SR != 0u)
    {
        if ((uint32_t)(HAL_GetTick() - start) > 100u)
        {
            Fault_Report(FAULT_CPU);
            return;
        }
    }
    IWDG1->KR = 0xAAAAu;
    for (unsigned i = 0; i < FAULT_HEALTH_COUNT; ++i) g_health[i] = HAL_GetTick();
    g_watchdog_started = true;
}

void Fault_WatchdogPoll(void)
{
    if (!g_watchdog_started) return;
    uint32_t now = HAL_GetTick();
    for (unsigned i = 0; i < FAULT_HEALTH_COUNT; ++i)
    {
        if ((uint32_t)(now - __atomic_load_n(&g_health[i], __ATOMIC_ACQUIRE)) > 300u)
        {
            Fault_Report(FAULT_CPU);
            return; /* Leave watchdog unfed: reset cannot auto-arm. */
        }
    }
    IWDG1->KR = 0xAAAAu;
}
