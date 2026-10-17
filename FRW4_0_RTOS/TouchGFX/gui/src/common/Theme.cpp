#include <gui/common/Theme.hpp>
#include <gui/common/UiBackend.hpp>

namespace theme
{
static const AccentPair g_pairs[ACCENT_COUNT] = {
    { 0xF2A33A, 0xFF5F6D },
    { 0x3FB6D9, 0x7B61FF },
    { 0x3DD68C, 0x2BB3C0 },
    { 0x7D9BFF, 0x3D5AF1 },
    { 0xE5484D, 0xFF8A65 },
};

const AccentPair& accent(Accent id)
{
    return g_pairs[(unsigned)id < ACCENT_COUNT ? id : 0];
}

Accent accentForMode(int mode)
{
    /* Operator modes: 0 RPM, 1 TORQUE, 2 INTERP, 3 DRY (Model::OpMode). */
    switch (mode)
    {
    case 1: return ACCENT_CYAN;
    case 2: return ACCENT_MINT;
    case 3: return ACCENT_BLUE;
    default: return ACCENT_AMBER;
    }
}

static uint32_t g_textures[ACCENT_COUNT][1024];
static bool g_built[ACCENT_COUNT];

const uint32_t* gradientTexture(Accent id)
{
    unsigned i = (unsigned)id < ACCENT_COUNT ? id : 0;
    if (!g_built[i])
    {
        const AccentPair& p = g_pairs[i];
        for (int t = 0; t < 1024; ++t)
        {
            uint32_t r = (((p.b >> 16) & 0xFF) * (1023 - t) + ((p.a >> 16) & 0xFF) * t) / 1023;
            uint32_t g = (((p.b >> 8) & 0xFF) * (1023 - t) + ((p.a >> 8) & 0xFF) * t) / 1023;
            uint32_t b = ((p.b & 0xFF) * (1023 - t) + (p.a & 0xFF) * t) / 1023;
            g_textures[i][t] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
        g_built[i] = true;
    }
    return g_textures[i];
}
} // namespace theme
