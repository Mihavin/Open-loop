#ifndef OPERATOR_INPUT_H
#define OPERATOR_INPUT_H
#include <stdint.h>
#include <math.h>

/* Counter period is 9,999, so int16_t casting alone cannot unwrap it.
   Call at least once per half-range of movement. */
static inline int32_t Operator_EncoderDelta(uint32_t now, uint32_t before, uint32_t modulus)
{
    int32_t delta = (int32_t)now - (int32_t)before;
    if (delta > (int32_t)(modulus / 2u)) delta -= (int32_t)modulus;
    if (delta < -(int32_t)(modulus / 2u)) delta += (int32_t)modulus;
    return delta;
}

static inline float Operator_AddBounded(float value, float change, float limit)
{
    if (!isfinite(value) || !isfinite(change)) return 0.0f;
    value += change;
    if (value > limit) return limit;
    if (value < -limit) return -limit;
    return value;
}
#endif
