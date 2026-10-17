#include <gui/common/Widgets.hpp>
#include <touchgfx/hal/HAL.hpp>
#include <touchgfx/lcd/LCD.hpp>
#include <touchgfx/Matrix3x3.hpp>
#include <stdio.h>
#include <string.h>

using namespace touchgfx;

int clampInt(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
float clampFloat(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ---------------- Label ---------------- */

Label::Label()
{
    buffer[0] = 0;
    current[0] = 0;
}

void Label::init(int16_t x, int16_t y, int16_t w, int16_t h, TEXTS typo, uint32_t rgbHex, uint8_t alpha)
{
    setPosition(x, y, w, h);
    setTypedText(TypedText(typo));
    setColor(theme::rgb(rgbHex));
    setAlpha(alpha);
    setWildcard(buffer);
}

void Label::set(const char* text)
{
    if (!text) text = "";
    if (strncmp(current, text, BYTES - 1) == 0 && current[0] == text[0]) return;
    strncpy(current, text, BYTES - 1);
    current[BYTES - 1] = 0;
    Unicode::fromUTF8(reinterpret_cast<const uint8_t*>(current), buffer, CAPACITY);
    buffer[CAPACITY - 1] = 0; /* fromUTF8 does not terminate a truncated string */
    invalidate();
}

void Label::setInt(int value, const char* suffix)
{
    char tmp[BYTES];
    snprintf(tmp, sizeof(tmp), "%d%s", value, suffix ? suffix : "");
    set(tmp);
}

void Label::setFloat(float value, int decimals, const char* suffix)
{
    char tmp[BYTES];
    /* No float printf on the target's newlib-nano by default: scale by hand. */
    int scale = 1;
    for (int i = 0; i < decimals; ++i) scale *= 10;
    bool neg = value < 0;
    if (neg) value = -value;
    long whole = (long)value;
    long frac = (long)((value - (float)whole) * (float)scale + 0.5f);
    if (frac >= scale) { whole++; frac -= scale; }
    if (decimals > 0)
        snprintf(tmp, sizeof(tmp), "%s%ld.%0*ld%s", neg ? "-" : "", whole, decimals, frac, suffix ? suffix : "");
    else
        snprintf(tmp, sizeof(tmp), "%s%ld%s", neg ? "-" : "", whole, suffix ? suffix : "");
    set(tmp);
}

void Label::setRgb(uint32_t rgbHex)
{
    colortype c = theme::rgb(rgbHex);
    if (c == getColor()) return;
    setColor(c);
    invalidate();
}

void Label::setTypo(TEXTS typo)
{
    setTypedText(TypedText(typo));
    invalidate();
}

/* ---------------- GradientBackground ---------------- */

GradientBackground::GradientBackground()
{
    setPosition(0, 0, theme::SCREEN_W, theme::SCREEN_H);
}

static uint32_t mix(uint32_t a, uint32_t b, int t, int span)
{
    uint32_t r = (((a >> 16) & 0xFF) * (span - t) + ((b >> 16) & 0xFF) * t) / span;
    uint32_t g = (((a >> 8) & 0xFF) * (span - t) + ((b >> 8) & 0xFF) * t) / span;
    uint32_t bl = ((a & 0xFF) * (span - t) + (b & 0xFF) * t) / span;
    return (r << 16) | (g << 8) | bl;
}

void GradientBackground::draw(const Rect& invalidatedArea) const
{
    const int16_t h = getHeight();
    const int16_t mid = (int16_t)(h * 55 / 100);
    const int16_t band = 2;
    int16_t y0 = (int16_t)(invalidatedArea.y - (invalidatedArea.y % band));
    for (int16_t y = y0; y < invalidatedArea.bottom(); y = (int16_t)(y + band))
    {
        uint32_t c = y < mid ? mix(theme::BG_TOP, theme::BG_MID, y, mid) : mix(theme::BG_MID, theme::BG_BOTTOM, y - mid, h - mid);
        Rect r(invalidatedArea.x, y, invalidatedArea.width, band);
        r = r & invalidatedArea;
        if (r.isEmpty()) continue;
        translateRectToAbsolute(r);
        HAL::lcd().fillRect(r, theme::rgb(c), 255);
    }
}

/* ---------------- Backdrop ---------------- */

Backdrop::Backdrop()
{
    setPosition(0, 0, theme::SCREEN_W, theme::SCREEN_H);
    add(gradient);

    /* Soft glows: filled discs at very low alpha, top-right accent and bottom-left cyan. */
    glowPainterA.setColor(theme::rgb(theme::accent(theme::ACCENT_AMBER).a));
    glowA.setPosition(102, -160, 320, 320);
    glowA.setCircle(160, 160, 150);
    glowA.setPainter(glowPainterA);
    glowA.setAlpha(26);
    add(glowA);

    glowPainterB.setColor(theme::rgb(0x3FB6D9));
    glowB.setPosition(-170, 300, 320, 320);
    glowB.setCircle(160, 160, 150);
    glowB.setPainter(glowPainterB);
    glowB.setAlpha(16);
    add(glowB);

    ringPainter.setColor(theme::rgb(theme::WHITE));
    static const int16_t radii[4] = { 60, 95, 130, 165 };
    for (int i = 0; i < 4; ++i)
    {
        rings[i].setPosition(50, 240, 222, 240);
        rings[i].setCircle(200, 200, radii[i]);
        rings[i].setLineWidth(1);
        rings[i].setPainter(ringPainter);
        rings[i].setAlpha(14);
        add(rings[i]);
    }
}

void Backdrop::setAccent(theme::Accent accent)
{
    glowPainterA.setColor(theme::rgb(theme::accent(accent).a));
    glowA.invalidate();
}

/* ---------------- GlassPanel ---------------- */

GlassPanel::GlassPanel() {}

void GlassPanel::init(int16_t x, int16_t y, int16_t w, int16_t h)
{
    setPosition(x, y, w, h);
    frame.setPosition(0, 0, w, h);
    frame.setBorderSize(1);
    setPlain();
    add(frame);
    highlight.setPosition(1, 1, (int16_t)(w - 2), 1);
    highlight.setColor(theme::rgb(theme::WHITE));
    highlight.setAlpha(22);
    add(highlight);
}

void GlassPanel::setPlain()
{
    frame.setColor(theme::rgb(theme::WHITE));
    frame.setBorderColor(theme::rgb(theme::WHITE));
    frame.setAlpha(18);
    frame.invalidate();
}

void GlassPanel::setFill(uint8_t alpha)
{
    frame.setAlpha(alpha);
    frame.invalidate();
}

void GlassPanel::setAccent(uint32_t rgbHex, uint8_t fillAlpha, uint8_t borderAlpha)
{
    /* BoxWithBorder has one alpha: tint the fill with the accent at a mid alpha
       and let the border carry the same colour; close to the concept's tinted card. */
    (void)borderAlpha;
    frame.setColor(theme::rgb(rgbHex));
    frame.setBorderColor(theme::rgb(rgbHex));
    frame.setAlpha(fillAlpha);
    frame.invalidate();
}

/* ---------------- TopBar ---------------- */

TopBar::TopBar()
{
    setPosition(0, 0, theme::SCREEN_W, theme::TOPBAR_H);
    fill.setPosition(0, 0, theme::SCREEN_W, theme::TOPBAR_H);
    fill.setColor(theme::rgb(theme::WHITE));
    fill.setAlpha(14);
    add(fill);
    line.setPosition(0, theme::TOPBAR_H - 1, theme::SCREEN_W, 1);
    line.setColor(theme::rgb(theme::WHITE));
    line.setAlpha(24);
    add(line);
    dotPainter.setColor(theme::rgb(theme::TEXT_DIM));
    dot.setPosition(12, 11, 8, 8);
    dot.setCircle(4, 4, 4);
    dot.setPainter(dotPainter);
    add(dot);
    left.init(26, 8, 150, 16, T_SMALL_L, theme::TEXT);
    add(left);
    right.init(120, 8, 140, 16, T_SMALL_R, theme::TEXT, 160);
    add(right);
}

void TopBar::setDot(uint32_t rgbHex, bool visible)
{
    dotPainter.setColor(theme::rgb(rgbHex));
    dot.setVisible(visible);
    dot.invalidate();
}

void TopBar::setDotAlpha(uint8_t alpha)
{
    if (dot.getAlpha() == alpha) return;
    dot.setAlpha(alpha);
    dot.invalidate();
}

/* ---------------- KeyLegend ---------------- */

KeyLegend::KeyLegend()
{
    setPosition(0, theme::SCREEN_H - theme::LEGEND_H, theme::SCREEN_W, theme::LEGEND_H);
    fill.setPosition(0, 0, theme::SCREEN_W, theme::LEGEND_H);
    fill.setColor(theme::rgb(0x06080C));
    fill.setAlpha(150);
    add(fill);
    line.setPosition(0, 0, theme::SCREEN_W, 1);
    line.setColor(theme::rgb(theme::WHITE));
    line.setAlpha(24);
    add(line);
    for (int i = 0; i < SLOTS; ++i)
    {
        int16_t x = (int16_t)(10 + (i % 2) * 131);
        int16_t y = (int16_t)(7 + (i / 2) * 19);
        caps[i].setPosition(x, y, 34, 15);
        caps[i].setColor(theme::rgb(theme::WHITE));
        caps[i].setBorderColor(theme::rgb(theme::WHITE));
        caps[i].setBorderSize(1);
        caps[i].setAlpha(60);
        add(caps[i]);
        keys[i].init(x, (int16_t)(y + 1), 34, 14, T_SMALL_C, theme::TEXT);
        add(keys[i]);
        meanings[i].init((int16_t)(x + 39), (int16_t)(y + 1), 90, 14, T_SMALL_L, theme::TEXT, 170);
        add(meanings[i]);
        clearEntry(i);
    }
}

void KeyLegend::setEntry(int slot, const char* key, const char* meaning)
{
    if (slot < 0 || slot >= SLOTS) return;
    caps[slot].setVisible(true);
    caps[slot].invalidate();
    keys[slot].set(key);
    meanings[slot].set(meaning);
}

void KeyLegend::clearEntry(int slot)
{
    if (slot < 0 || slot >= SLOTS) return;
    caps[slot].setVisible(false);
    caps[slot].invalidate();
    keys[slot].set("");
    meanings[slot].set("");
}

/* ---------------- arcs ---------------- */

void setupArc(Circle& circle, AbstractPainter& painter, int16_t x, int16_t y, int16_t radius, int16_t lineWidth, int startAngle, int endAngle)
{
    int16_t size = (int16_t)(2 * radius + lineWidth + 2);
    circle.setPosition((int16_t)(x - size / 2), (int16_t)(y - size / 2), size, size);
    circle.setCircle(size / 2, size / 2, radius);
    circle.setLineWidth(lineWidth);
    circle.setArc(startAngle, endAngle);
    circle.setCapPrecision(20);
    circle.setPainter(painter);
}

void setGradientArc(Circle& circle, PainterARGB8888LinearGradient& painter, theme::Accent accent)
{
    Matrix3x3 identity;
    painter.setGradientTexture(theme::gradientTexture(accent), false);
    painter.setWidgetWidth(circle.getWidth());
    /* Diagonal gradient across the widget box, b at the lower left and a at the top right. */
    painter.setGradientEndPoints(0.0f, (float)circle.getHeight(), (float)circle.getWidth(), 0.0f,
                                 (float)circle.getWidth(), (float)circle.getHeight(), identity);
    circle.setPainter(painter);
}

void setSolidPainter(PainterARGB8888& painter, uint32_t rgbHex)
{
    painter.setColor(theme::rgb(rgbHex));
}

/* ---------------- Bar ---------------- */

Bar::Bar() {}

void Bar::init(int16_t x, int16_t y, int16_t w, int16_t h)
{
    setPosition(x, y, w, h);
    track.setPosition(0, 0, w, h);
    track.setColor(theme::rgb(theme::WHITE));
    track.setAlpha(26);
    add(track);
    fillBox.setPosition(0, 0, 0, h);
    fillBox.setColor(theme::rgb(theme::accent(theme::ACCENT_AMBER).a));
    add(fillBox);
}

void Bar::setAccent(uint32_t rgbHex)
{
    fillBox.setColor(theme::rgb(rgbHex));
    fillBox.invalidate();
}

void Bar::setPermille(int permille)
{
    permille = clampInt(permille, 0, 1000);
    int16_t w = (int16_t)((int32_t)getWidth() * permille / 1000);
    if (w == fillBox.getWidth()) return;
    fillBox.invalidate();
    fillBox.setWidth(w);
    fillBox.invalidate();
}
