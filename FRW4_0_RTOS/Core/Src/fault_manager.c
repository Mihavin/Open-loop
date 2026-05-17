#include "fault_manager.h"

static volatile uint32_t g_fault_bits = FAULT_NONE;

void Fault_Report(uint32_t fault_bits)
{
    (void)__atomic_fetch_or(&g_fault_bits, fault_bits, __ATOMIC_SEQ_CST);
}

void Fault_Clear(uint32_t fault_bits)
{
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
