/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    telemetry.c
  * @brief   Simple on-screen telemetry overlay for LTDC (no TouchGFX required).
  *
  * Draws text directly into LTDC Layer0 framebuffer (FMC SDRAM).
  *
  * Shows:
  *   - Encoder count + mechanical angle (deg)
  *   - ia, ib, ic, isum (A)
  *   - speed (RPM)
  *
  * Notes:
  *   - This implementation targets ARGB8888 @ 0xC0000000 (480x272) by default.
  *   - If you make the SDRAM region CACHEABLE and enable D-Cache, you must clean
  *     the cache for the area you draw (this file does that when D-Cache is on).
  ******************************************************************************
  */
/* USER CODE END Header */

#include "telemetry.h"

#include "main.h"
#include "ltdc.h"
#include "tim.h"
#include "motor_foc.h"
#include "fmc.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ================= Configuration ================= */
#ifndef TELEMETRY_LAYER_INDEX
#define TELEMETRY_LAYER_INDEX 0
#endif

#ifndef TELEMETRY_DEFAULT_FB_ADDR
#define TELEMETRY_DEFAULT_FB_ADDR 0xC0000000u
#endif

#ifndef TELEMETRY_FB_WIDTH
#define TELEMETRY_FB_WIDTH  480u
#endif

#ifndef TELEMETRY_FB_HEIGHT
#define TELEMETRY_FB_HEIGHT 272u
#endif

#ifndef TELEMETRY_FB_BPP
#define TELEMETRY_FB_BPP    4u /* ARGB8888 */
#endif

#ifndef TELEMETRY_FB_STRIDE_BYTES
#define TELEMETRY_FB_STRIDE_BYTES (TELEMETRY_FB_WIDTH * TELEMETRY_FB_BPP)
#endif

#ifndef TELEMETRY_X0
#define TELEMETRY_X0  8
#endif
#ifndef TELEMETRY_Y0
#define TELEMETRY_Y0  8
#endif

/* Overlay area we touch (keep it small) */
#ifndef TELEMETRY_W
#define TELEMETRY_W  450
#endif

#ifndef TELEMETRY_H
#define TELEMETRY_H  80
#endif

#define TELEM_COLOR_BG   0xA0000000u /* semi transparent black */
#define TELEM_COLOR_FG   0xFFFFFFFFu
#define TELEM_COLOR_DIM  0xFFB0B0B0u

#ifndef TELEMETRY_CLEAN_DCACHE
#define TELEMETRY_CLEAN_DCACHE 1
#endif

/* ================= 5x7 font (ASCII 32..127) =================
 * Each char is 5 columns, LSB at top (7 bits used).
 * Rendered as 6x8 with 1px spacing.
 */
static const uint8_t font5x7[96][5] = {
    /* 0x20 ' ' */ {0x00,0x00,0x00,0x00,0x00},
    /* 0x21 '!' */ {0x00,0x00,0x5F,0x00,0x00},
    /* 0x22 '"' */ {0x00,0x07,0x00,0x07,0x00},
    /* 0x23 '#' */ {0x14,0x7F,0x14,0x7F,0x14},
    /* 0x24 '$' */ {0x24,0x2A,0x7F,0x2A,0x12},
    /* 0x25 '%' */ {0x23,0x13,0x08,0x64,0x62},
    /* 0x26 '&' */ {0x36,0x49,0x55,0x22,0x50},
    /* 0x27 '\'' */ {0x00,0x05,0x03,0x00,0x00},
    /* 0x28 '(' */ {0x00,0x1C,0x22,0x41,0x00},
    /* 0x29 ')' */ {0x00,0x41,0x22,0x1C,0x00},
    /* 0x2A '*' */ {0x14,0x08,0x3E,0x08,0x14},
    /* 0x2B '+' */ {0x08,0x08,0x3E,0x08,0x08},
    /* 0x2C ',' */ {0x00,0x50,0x30,0x00,0x00},
    /* 0x2D '-' */ {0x08,0x08,0x08,0x08,0x08},
    /* 0x2E '.' */ {0x00,0x60,0x60,0x00,0x00},
    /* 0x2F '/' */ {0x20,0x10,0x08,0x04,0x02},
    /* 0x30 '0' */ {0x3E,0x51,0x49,0x45,0x3E},
    /* 0x31 '1' */ {0x00,0x42,0x7F,0x40,0x00},
    /* 0x32 '2' */ {0x42,0x61,0x51,0x49,0x46},
    /* 0x33 '3' */ {0x21,0x41,0x45,0x4B,0x31},
    /* 0x34 '4' */ {0x18,0x14,0x12,0x7F,0x10},
    /* 0x35 '5' */ {0x27,0x45,0x45,0x45,0x39},
    /* 0x36 '6' */ {0x3C,0x4A,0x49,0x49,0x30},
    /* 0x37 '7' */ {0x01,0x71,0x09,0x05,0x03},
    /* 0x38 '8' */ {0x36,0x49,0x49,0x49,0x36},
    /* 0x39 '9' */ {0x06,0x49,0x49,0x29,0x1E},
    /* 0x3A ':' */ {0x00,0x36,0x36,0x00,0x00},
    /* 0x3B ';' */ {0x00,0x56,0x36,0x00,0x00},
    /* 0x3C '<' */ {0x08,0x14,0x22,0x41,0x00},
    /* 0x3D '=' */ {0x14,0x14,0x14,0x14,0x14},
    /* 0x3E '>' */ {0x00,0x41,0x22,0x14,0x08},
    /* 0x3F '?' */ {0x02,0x01,0x51,0x09,0x06},
    /* 0x40 '@' */ {0x32,0x49,0x79,0x41,0x3E},
    /* 0x41 'A' */ {0x7E,0x11,0x11,0x11,0x7E},
    /* 0x42 'B' */ {0x7F,0x49,0x49,0x49,0x36},
    /* 0x43 'C' */ {0x3E,0x41,0x41,0x41,0x22},
    /* 0x44 'D' */ {0x7F,0x41,0x41,0x22,0x1C},
    /* 0x45 'E' */ {0x7F,0x49,0x49,0x49,0x41},
    /* 0x46 'F' */ {0x7F,0x09,0x09,0x09,0x01},
    /* 0x47 'G' */ {0x3E,0x41,0x49,0x49,0x7A},
    /* 0x48 'H' */ {0x7F,0x08,0x08,0x08,0x7F},
    /* 0x49 'I' */ {0x00,0x41,0x7F,0x41,0x00},
    /* 0x4A 'J' */ {0x20,0x40,0x41,0x3F,0x01},
    /* 0x4B 'K' */ {0x7F,0x08,0x14,0x22,0x41},
    /* 0x4C 'L' */ {0x7F,0x40,0x40,0x40,0x40},
    /* 0x4D 'M' */ {0x7F,0x02,0x04,0x02,0x7F},
    /* 0x4E 'N' */ {0x7F,0x04,0x08,0x10,0x7F},
    /* 0x4F 'O' */ {0x3E,0x41,0x41,0x41,0x3E},
    /* 0x50 'P' */ {0x7F,0x09,0x09,0x09,0x06},
    /* 0x51 'Q' */ {0x3E,0x41,0x51,0x21,0x5E},
    /* 0x52 'R' */ {0x7F,0x09,0x19,0x29,0x46},
    /* 0x53 'S' */ {0x46,0x49,0x49,0x49,0x31},
    /* 0x54 'T' */ {0x01,0x01,0x7F,0x01,0x01},
    /* 0x55 'U' */ {0x3F,0x40,0x40,0x40,0x3F},
    /* 0x56 'V' */ {0x1F,0x20,0x40,0x20,0x1F},
    /* 0x57 'W' */ {0x3F,0x40,0x38,0x40,0x3F},
    /* 0x58 'X' */ {0x63,0x14,0x08,0x14,0x63},
    /* 0x59 'Y' */ {0x07,0x08,0x70,0x08,0x07},
    /* 0x5A 'Z' */ {0x61,0x51,0x49,0x45,0x43},
    /* 0x5B '[' */ {0x00,0x7F,0x41,0x41,0x00},
    /* 0x5C '\\' */ {0x02,0x04,0x08,0x10,0x20},
    /* 0x5D ']' */ {0x00,0x41,0x41,0x7F,0x00},
    /* 0x5E '^' */ {0x04,0x02,0x01,0x02,0x04},
    /* 0x5F '_' */ {0x40,0x40,0x40,0x40,0x40},
    /* 0x60 '`' */ {0x00,0x01,0x02,0x04,0x00},
    /* 0x61 'a' */ {0x20,0x54,0x54,0x54,0x78},
    /* 0x62 'b' */ {0x7F,0x48,0x44,0x44,0x38},
    /* 0x63 'c' */ {0x38,0x44,0x44,0x44,0x20},
    /* 0x64 'd' */ {0x38,0x44,0x44,0x48,0x7F},
    /* 0x65 'e' */ {0x38,0x54,0x54,0x54,0x18},
    /* 0x66 'f' */ {0x08,0x7E,0x09,0x01,0x02},
    /* 0x67 'g' */ {0x0C,0x52,0x52,0x52,0x3E},
    /* 0x68 'h' */ {0x7F,0x08,0x04,0x04,0x78},
    /* 0x69 'i' */ {0x00,0x44,0x7D,0x40,0x00},
    /* 0x6A 'j' */ {0x20,0x40,0x44,0x3D,0x00},
    /* 0x6B 'k' */ {0x7F,0x10,0x28,0x44,0x00},
    /* 0x6C 'l' */ {0x00,0x41,0x7F,0x40,0x00},
    /* 0x6D 'm' */ {0x7C,0x04,0x18,0x04,0x78},
    /* 0x6E 'n' */ {0x7C,0x08,0x04,0x04,0x78},
    /* 0x6F 'o' */ {0x38,0x44,0x44,0x44,0x38},
    /* 0x70 'p' */ {0x7C,0x14,0x14,0x14,0x08},
    /* 0x71 'q' */ {0x08,0x14,0x14,0x18,0x7C},
    /* 0x72 'r' */ {0x7C,0x08,0x04,0x04,0x08},
    /* 0x73 's' */ {0x48,0x54,0x54,0x54,0x20},
    /* 0x74 't' */ {0x04,0x3F,0x44,0x40,0x20},
    /* 0x75 'u' */ {0x3C,0x40,0x40,0x20,0x7C},
    /* 0x76 'v' */ {0x1C,0x20,0x40,0x20,0x1C},
    /* 0x77 'w' */ {0x3C,0x40,0x30,0x40,0x3C},
    /* 0x78 'x' */ {0x44,0x28,0x10,0x28,0x44},
    /* 0x79 'y' */ {0x0C,0x50,0x50,0x50,0x3C},
    /* 0x7A 'z' */ {0x44,0x64,0x54,0x4C,0x44},
    /* 0x7B '{' */ {0x00,0x08,0x36,0x41,0x00},
    /* 0x7C '|' */ {0x00,0x00,0x7F,0x00,0x00},
    /* 0x7D '}' */ {0x00,0x41,0x36,0x08,0x00},
    /* 0x7E '~' */ {0x08,0x04,0x08,0x10,0x08},
    /* 0x7F DEL */ {0x00,0x06,0x09,0x09,0x06}
};

/* ================= Internal framebuffer access ================= */
typedef struct
{
  uint32_t fb_addr;
  uint32_t stride_bytes;
  uint16_t width_px;
  uint16_t height_px;
} fb_info_t;

static fb_info_t g_fb = {0};

static inline uint32_t fb_bytes_per_pixel(void)
{
  return TELEMETRY_FB_BPP;
}

static inline void* fb_ptr_xy(int x, int y)
{
  uint8_t* base = (uint8_t*)(uintptr_t)g_fb.fb_addr;
  return base + (uint32_t)y * g_fb.stride_bytes + (uint32_t)x * fb_bytes_per_pixel();
}

static inline void put_pixel(int x, int y, uint32_t argb8888)
{
  if ((uint32_t)x >= g_fb.width_px || (uint32_t)y >= g_fb.height_px) return;
  uint32_t* p = (uint32_t*)fb_ptr_xy(x, y);
  *p = argb8888;
}

static void fill_rect(int x, int y, int w, int h, uint32_t color)
{
  if (w <= 0 || h <= 0) return;

  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > (int)g_fb.width_px)  w = (int)g_fb.width_px  - x;
  if (y + h > (int)g_fb.height_px) h = (int)g_fb.height_px - y;
  if (w <= 0 || h <= 0) return;

  for (int yy = 0; yy < h; yy++)
  {
    uint32_t* row = (uint32_t*)((uint8_t*)(uintptr_t)g_fb.fb_addr + (uint32_t)(y + yy) * g_fb.stride_bytes);
    row += x;
    for (int xx = 0; xx < w; xx++)
    {
      row[xx] = color;
    }
  }
}

static void draw_char(int x, int y, char ch, uint32_t fg, uint32_t bg)
{
  if (ch < 32 || ch > 127) ch = '?';
  const uint8_t* cols = font5x7[(int)ch - 32];

  for (int col = 0; col < 5; col++)
  {
    uint8_t bits = cols[col];
    for (int row = 0; row < 7; row++)
    {
      put_pixel(x + col, y + row, (bits & (1u << row)) ? fg : bg);
    }
  }
  /* spacing column */
  for (int row = 0; row < 7; row++) put_pixel(x + 5, y + row, bg);
}

static void draw_text(int x, int y, const char* s, uint32_t fg, uint32_t bg)
{
  int cx = x;
  while (*s)
  {
    draw_char(cx, y, *s, fg, bg);
    cx += 6;
    s++;
  }
}

static void dcache_clean_range(void* addr, uint32_t len)
{
#if TELEMETRY_CLEAN_DCACHE
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
  /* Only do cache maintenance if D-Cache is actually enabled */
  if ((SCB->CCR & SCB_CCR_DC_Msk) == 0U)
  {
    (void)addr; (void)len;
    return;
  }

  uintptr_t a  = (uintptr_t)addr;
  uintptr_t a0 = a & ~(uintptr_t)31u;
  uintptr_t a1 = (a + len + 31u) & ~(uintptr_t)31u;
  SCB_CleanDCache_by_Addr((uint32_t*)a0, (int32_t)(a1 - a0));
#else
  (void)addr; (void)len;
#endif
#else
  (void)addr; (void)len;
#endif
}

static void fmt_f2(char* out, size_t n, float v)
{
  int neg = (v < 0.0f) ? 1 : 0;
  if (neg) v = -v;

  int iv = (int)v;
  int frac = (int)((v - (float)iv) * 100.0f + 0.5f);
  if (frac >= 100) { iv++; frac -= 100; }

  if (neg) (void)snprintf(out, n, "-%d.%02d", iv, frac);
  else     (void)snprintf(out, n, "%d.%02d", iv, frac);
}

static int sdram_ready(void)
{
  return (hsdram1.State == HAL_SDRAM_STATE_READY);
}

/* ================= Public API ================= */

void Telemetry_SetFrameBufferAddress(uint32_t fb_addr)
{
  if (fb_addr == 0u) return;
  g_fb.fb_addr = fb_addr;

  /* Also point LTDC to it (Layer0 by default) */
  (void)HAL_LTDC_SetAddress(&hltdc, fb_addr, TELEMETRY_LAYER_INDEX);
  (void)HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_IMMEDIATE);
}

void Telemetry_Init(void)
{
  /* 1) Ensure SDRAM init finished (avoid touching 0xC0000000 too early) */
  uint32_t t0 = HAL_GetTick();
  while (!sdram_ready())
  {
    if ((HAL_GetTick() - t0) > 200u)
    {
      /* Don't hang forever if something is wrong */
      return;
    }
  }

  /* 2) Fill fixed FB info (matches your requirement) */
  g_fb.fb_addr      = TELEMETRY_DEFAULT_FB_ADDR;
  g_fb.stride_bytes = TELEMETRY_FB_STRIDE_BYTES;
  g_fb.width_px     = (uint16_t)TELEMETRY_FB_WIDTH;
  g_fb.height_px    = (uint16_t)TELEMETRY_FB_HEIGHT;

  /* 3) Make sure LTDC layer uses this FB address */
  Telemetry_SetFrameBufferAddress(g_fb.fb_addr);

  /* Force full-screen solid color test (opaque red) */
  fill_rect(0, 00, 480, 272, 0xFFFF0000u);

  /* If D-Cache is enabled, flush framebuffer */
  dcache_clean_range((void*)(uintptr_t)g_fb.fb_addr, 480u * 272u * 4u);

  HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_IMMEDIATE);

  /* 4) Clear ONLY the overlay area (do not wipe the whole screen) */
  fill_rect(TELEMETRY_X0, TELEMETRY_Y0, TELEMETRY_W, TELEMETRY_H, TELEM_COLOR_BG);
  draw_text(TELEMETRY_X0, TELEMETRY_Y0, "FOC TELEMETRY", TELEM_COLOR_FG, TELEM_COLOR_BG);

  /* 5) Cache maintenance (only if D-Cache is enabled) */
  uint32_t bytes = (uint32_t)g_fb.stride_bytes * (uint32_t)(TELEMETRY_Y0 + TELEMETRY_H);
  if (bytes > (uint32_t)g_fb.stride_bytes * (uint32_t)g_fb.height_px)
  {
    bytes = (uint32_t)g_fb.stride_bytes * (uint32_t)g_fb.height_px;
  }
  dcache_clean_range((void*)(uintptr_t)g_fb.fb_addr, bytes);
}

void Telemetry_Update(void)
{
  if (g_fb.fb_addr == 0u) return;

  int32_t enc_cnt = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);

  int32_t cpr = (int32_t)MOTOR_FOC_ENCODER_CPR;
  int32_t cnt_mod = (cpr > 0) ? (enc_cnt % cpr) : 0;
  if (cnt_mod < 0) cnt_mod += cpr;

  int32_t deg100 = 0;
  if (cpr > 0)
  {
    deg100 = (cnt_mod * 36000 + (cpr / 2)) / cpr;
    if (deg100 >= 36000) deg100 -= 36000;
  }

  float ia = MotorFOC_GetIa();
  float ib = MotorFOC_GetIb();
  float ic = MotorFOC_GetIc();
  float isum = ia + ib + ic;
  float rpm = MotorFOC_GetSpeedRPM();
  uint32_t mode = MotorFOC_GetMode();
  uint32_t state = MotorFOC_GetState();
  uint32_t enc = __HAL_TIM_GET_COUNTER(&htim3);
  enc = enc*0.5;
  char s_ia[16], s_ib[16], s_ic[16], s_isum[16], s_rpm[16], s_mode[16], s_state[16], s_enc[16];
  fmt_f2(s_ia, sizeof(s_ia), ia);
  fmt_f2(s_ib, sizeof(s_ib), ib);
  fmt_f2(s_ic, sizeof(s_ic), ic);
  fmt_f2(s_isum, sizeof(s_isum), isum);
  fmt_f2(s_rpm, sizeof(s_rpm), rpm);
  fmt_f2(s_mode, sizeof(s_mode), mode);
  fmt_f2(s_state, sizeof(s_state), state);
  fmt_f2(s_enc, sizeof(s_enc), enc);

  char line[80];

  const int lh = 10;
  int x = TELEMETRY_X0;
  int y = TELEMETRY_Y0 + 12;

  /* Clear just the text lines area */
  fill_rect(x, y, TELEMETRY_W, (lh * 4), TELEM_COLOR_BG);

  (void)snprintf(line, sizeof(line), "ENC:%ld  ANG:%ld.%02ld deg",
                 (long)enc_cnt,
                 (long)(deg100 / 100),
                 (long)(deg100 % 100));
  draw_text(x, y, line, TELEM_COLOR_FG, TELEM_COLOR_BG);
  y += lh;

  (void)snprintf(line, sizeof(line), "IA:%sA  IB:%sA  IC:%sA", s_ia, s_ib, s_ic);
  draw_text(x, y, line, TELEM_COLOR_FG, TELEM_COLOR_BG);
  y += lh;

  (void)snprintf(line, sizeof(line), "ISUM:%sA", s_isum);
  draw_text(x, y, line, TELEM_COLOR_FG, TELEM_COLOR_BG);
  y += lh;

  (void)snprintf(line, sizeof(line), "SPEED:%s RPM", s_rpm);
  draw_text(x, y, line, TELEM_COLOR_FG, TELEM_COLOR_BG);
  y += lh;
  (void)snprintf(line, sizeof(line), "MODE: %s", s_mode);
  draw_text(x, y, line, TELEM_COLOR_FG, TELEM_COLOR_BG);
  y += lh;
  (void)snprintf(line, sizeof(line), "STATE: %s", s_state);
  draw_text(x, y, line, TELEM_COLOR_FG, TELEM_COLOR_BG);
  y += lh;
  (void)snprintf(line, sizeof(line), "UI Enc: %s", s_enc);
  draw_text(x, y, line, TELEM_COLOR_FG, TELEM_COLOR_BG);


  /* Cache maintenance for the updated overlay area */
  uint32_t bytes = (uint32_t)g_fb.stride_bytes * (uint32_t)(TELEMETRY_Y0 + TELEMETRY_H);
  if (bytes > (uint32_t)g_fb.stride_bytes * (uint32_t)g_fb.height_px)
  {
    bytes = (uint32_t)g_fb.stride_bytes * (uint32_t)g_fb.height_px;
  }
  dcache_clean_range((void*)(uintptr_t)g_fb.fb_addr, bytes);
}
