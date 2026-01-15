#pragma once
#include <stdint.h>

#ifndef LCD_WIDTH
#define LCD_WIDTH  480
#endif
#ifndef LCD_HEIGHT
#define LCD_HEIGHT 272
#endif

extern uint32_t g_ltdc_fb0[LCD_WIDTH * LCD_HEIGHT];
#define LTDC_FB0_ADDR ((uint32_t)&g_ltdc_fb0[0])
