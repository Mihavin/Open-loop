#include <stdint.h>

/* 480x272 ARGB8888 framebuffer in SDRAM (placed by linker in .framebuffer at 0xC0000000) */
#ifndef LCD_WIDTH
#define LCD_WIDTH  480
#endif
#ifndef LCD_HEIGHT
#define LCD_HEIGHT 272
#endif

__attribute__((section(".framebuffer"), aligned(32)))
uint32_t g_ltdc_fb0[LCD_WIDTH * LCD_HEIGHT];
