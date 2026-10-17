#ifndef UI_WIDGETS_HPP
#define UI_WIDGETS_HPP

#include <gui/common/Theme.hpp>
#include <touchgfx/containers/Container.hpp>
#include <touchgfx/widgets/Box.hpp>
#include <touchgfx/widgets/BoxWithBorder.hpp>
#include <touchgfx/widgets/TextAreaWithWildcard.hpp>
#include <touchgfx/widgets/canvas/Circle.hpp>
#include <touchgfx/widgets/canvas/Line.hpp>
#include <touchgfx/widgets/canvas/PainterARGB8888.hpp>
#include <touchgfx/widgets/canvas/PainterARGB8888LinearGradient.hpp>
#include <texts/TextKeysAndLanguages.hpp>

/* Small building blocks shared by the screens. All text is produced at runtime
   through Label, so the text database only holds wildcard strings (texts.xml). */

/* A text area with its own buffer; redraws only when the string changes. */
class Label : public touchgfx::TextAreaWithOneWildcard
{
public:
    static const int CAPACITY = 56;  /* unicode characters, incl. terminator */
    static const int BYTES = 80;     /* UTF-8 source bytes */
    Label();
    /* typo is one of the T_<TYPOGRAPHY>_<L|C|R> wildcard texts. */
    void init(int16_t x, int16_t y, int16_t w, int16_t h, TEXTS typo, uint32_t rgbHex, uint8_t alpha = 255);
    void set(const char* text);
    void setInt(int value, const char* suffix = 0);
    void setFloat(float value, int decimals, const char* suffix = 0);
    void setRgb(uint32_t rgbHex);
    void setTypo(TEXTS typo);
    const char* text() const { return current; }

private:
    touchgfx::Unicode::UnicodeChar buffer[CAPACITY];
    char current[BYTES];
};

/* Full-screen vertical gradient drawn with LCD fills (DMA2D on target). */
class GradientBackground : public touchgfx::Widget
{
public:
    GradientBackground();
    virtual void draw(const touchgfx::Rect& invalidatedArea) const;
    virtual touchgfx::Rect getSolidRect() const { return touchgfx::Rect(0, 0, getWidth(), getHeight()); }
};

/* Gradient background with the faint ring art and two soft accent glows. */
class Backdrop : public touchgfx::Container
{
public:
    Backdrop();
    void setAccent(theme::Accent accent);

private:
    GradientBackground gradient;
    touchgfx::Circle rings[4];
    touchgfx::PainterARGB8888 ringPainter;
    touchgfx::Circle glowA, glowB;
    touchgfx::PainterARGB8888 glowPainterA, glowPainterB;
};

/* Translucent panel with a hairline border; optional accent border and tint. */
class GlassPanel : public touchgfx::Container
{
public:
    GlassPanel();
    void init(int16_t x, int16_t y, int16_t w, int16_t h);
    void setAccent(uint32_t rgbHex, uint8_t fillAlpha = 48, uint8_t borderAlpha = 140);
    void setPlain();
    void setFill(uint8_t alpha);

private:
    touchgfx::BoxWithBorder frame;
    touchgfx::Box highlight; /* 1 px top highlight */
};

/* 30 px status bar: state dot, left and right captions. */
class TopBar : public touchgfx::Container
{
public:
    TopBar();
    void setLeft(const char* text) { left.set(text); }
    void setRight(const char* text) { right.set(text); }
    void setDot(uint32_t rgbHex, bool visible);
    void setDotAlpha(uint8_t alpha);

private:
    touchgfx::Box fill, line;
    touchgfx::Circle dot;
    touchgfx::PainterARGB8888 dotPainter;
    Label left, right;
};

/* 44 px bottom strip with up to four key caps and their meaning. */
class KeyLegend : public touchgfx::Container
{
public:
    static const int SLOTS = 4;
    KeyLegend();
    void setEntry(int slot, const char* key, const char* meaning);
    void clearEntry(int slot);

private:
    touchgfx::Box fill, line;
    touchgfx::BoxWithBorder caps[SLOTS];
    Label keys[SLOTS];
    Label meanings[SLOTS];
};

/* Arc helpers (angles in degrees, 0 = up, clockwise). */
void setupArc(touchgfx::Circle& circle, touchgfx::AbstractPainter& painter, int16_t x, int16_t y, int16_t radius,
              int16_t lineWidth, int startAngle, int endAngle);
void setGradientArc(touchgfx::Circle& circle, touchgfx::PainterARGB8888LinearGradient& painter, theme::Accent accent);
void setSolidPainter(touchgfx::PainterARGB8888& painter, uint32_t rgbHex);

/* Horizontal progress bar with a rounded look: track + accent fill. */
class Bar : public touchgfx::Container
{
public:
    Bar();
    void init(int16_t x, int16_t y, int16_t w, int16_t h);
    void setAccent(uint32_t rgbHex);
    void setPermille(int permille);

private:
    touchgfx::Box track, fillBox;
};

/* Formats helpers shared by views. */
int clampInt(int v, int lo, int hi);
float clampFloat(float v, float lo, float hi);

#endif // UI_WIDGETS_HPP
