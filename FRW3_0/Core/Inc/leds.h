#ifndef LEDS_H
#define LEDS_H

#include "stm32h7xx_hal.h"

// ----------------------------------------------------------------------
// EDIT THESE DEFINES TO MATCH YOUR BOARD!
// ----------------------------------------------------------------------
// Example only – change GPIOx and GPIO_PIN_y to your real LED pins.

// ----------------------------------------------------------------------

void LEDs_Init(void);

void LED0_On(void);
void LED0_Off(void);

void LED1_On(void);
void LED1_Off(void);

void LED2_On(void);
void LED2_Off(void);

void LED0_Toggle(void);
void LED1_Toggle(void);
void LED2_Toggle(void);

#endif // LEDS_H
