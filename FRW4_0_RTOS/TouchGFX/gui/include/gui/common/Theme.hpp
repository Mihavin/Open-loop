#ifndef UI_THEME_HPP
#define UI_THEME_HPP

#include <touchgfx/Color.hpp>
#include <touchgfx/hal/Types.hpp>

/* Palette and gradient textures shared by every screen (from the approved concepts). */
namespace theme
{
using touchgfx::colortype;

inline colortype rgb(uint32_t hex)
{
    return touchgfx::Color::getColorFromRGB((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

static const uint32_t GROUND = 0x0E1116;
static const uint32_t BG_TOP = 0x18202C;
static const uint32_t BG_MID = 0x0C1017;
static const uint32_t BG_BOTTOM = 0x06080B;
static const uint32_t TEXT = 0xEEF2F6;
static const uint32_t TEXT_DIM = 0x9AA6B2;
static const uint32_t WHITE = 0xFFFFFF;
static const uint32_t GREEN = 0x3DD68C;
static const uint32_t DANGER = 0xE5484D;
static const uint32_t INFO = 0x7FD3EC;
static const uint32_t INK = 0x0B0E13; /* text on an accent fill */

enum Accent
{
    ACCENT_AMBER = 0,  /* speed mode */
    ACCENT_CYAN,       /* torque mode */
    ACCENT_MINT,       /* position mode */
    ACCENT_PURPLE,     /* menus and editing */
    ACCENT_RED,        /* faults */
    ACCENT_COUNT
};

struct AccentPair
{
    uint32_t a;
    uint32_t b;
};
const AccentPair& accent(Accent id);
Accent accentForMode(int mode);

/* 1024x1 ARGB8888 texture of accent b -> a for PainterARGB8888LinearGradient. */
const uint32_t* gradientTexture(Accent id);

static const int16_t SCREEN_W = 272;
static const int16_t SCREEN_H = 480;
static const int16_t TOPBAR_H = 30;
static const int16_t LEGEND_H = 44;
} // namespace theme

#endif // UI_THEME_HPP
