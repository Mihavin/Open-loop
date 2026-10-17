#include <gui/interpscreen_screen/InterpScreenView.hpp>
#include <math.h>
#include <stdio.h>

using namespace touchgfx;

static const int16_t DIAL_CX = 136, DIAL_CY = 136, DIAL_R = 72;
static const int16_t ROWS_Y = 240;
static const int16_t ROW_H = 46;
static const float PI_F = 3.14159265f;
static const char* const PARAM_NAMES[3] = { "Stroke", "Snap pitch", "Hold torque" };
static const char* const PARAM_HELP[3] = {
    "shaft revolutions per full pedal travel",
    "idle spindle clicks into these angles",
    "holding current while the detent is active"
};

InterpScreenView::InterpScreenView() : selected(0), shownTicks(-1), tick(0), shownAngle(0.0f) {}

void InterpScreenView::setupScreen()
{
    InterpScreenViewBase::setupScreen();
    add(backdrop);
    backdrop.setAccent(theme::ACCENT_MINT);
    const theme::AccentPair& p = theme::accent(theme::ACCENT_MINT);

    dialTrackPainter.setColor(theme::rgb(theme::WHITE));
    setupArc(dialTrack, dialTrackPainter, DIAL_CX, DIAL_CY, DIAL_R, 10, 0, 360);
    dialTrack.setAlpha(26);
    add(dialTrack);
    setupArc(dialArc, dialArcPainter, DIAL_CX, DIAL_CY, DIAL_R, 10, 0, 0);
    setGradientArc(dialArc, dialArcPainter, theme::ACCENT_MINT);
    add(dialArc);

    tickPainter.setColor(theme::rgb(theme::WHITE));
    for (int i = 0; i < MAX_TICKS; ++i)
    {
        ticks[i].setPosition(DIAL_CX - DIAL_R - 4, DIAL_CY - DIAL_R - 4, 2 * DIAL_R + 8, 2 * DIAL_R + 8);
        ticks[i].setLineWidth(2);
        ticks[i].setPainter(tickPainter);
        ticks[i].setAlpha(120);
        ticks[i].setVisible(false);
        add(ticks[i]);
    }

    pointerPainter.setColor(theme::rgb(p.a));
    pointer.setPosition(DIAL_CX - DIAL_R - 10, DIAL_CY - DIAL_R - 10, 2 * DIAL_R + 20, 2 * DIAL_R + 20);
    pointer.setCircle(DIAL_R + 10, 10, 6);
    pointer.setPainter(pointerPainter);
    add(pointer);

    angleValue.init(0, DIAL_CY - 30, theme::SCREEN_W, 62, T_BIG_C, theme::TEXT);
    add(angleValue);
    angleUnit.init(0, DIAL_CY + 30, theme::SCREEN_W, 16, T_SMALL_C, theme::TEXT_DIM);
    angleUnit.set("spindle angle");
    add(angleUnit);
    angleHint.init(0, (int16_t)(DIAL_CY + DIAL_R + 12), theme::SCREEN_W, 14, T_SMALL_C, theme::TEXT_DIM);
    add(angleHint);

    for (int i = 0; i < PARAMS; ++i)
    {
        int16_t y = (int16_t)(ROWS_Y + i * ROW_H);
        rowPanel[i].init(12, y, 248, ROW_H - 6);
        add(rowPanel[i]);
        rowName[i].init(24, (int16_t)(y + 12), 120, 18, T_BODY_L, theme::TEXT);
        rowName[i].set(PARAM_NAMES[i]);
        add(rowName[i]);
        rowValue[i].init(130, (int16_t)(y + 7), 118, 28, T_MEDIUM_R, theme::TEXT);
        add(rowValue[i]);
    }
    caption.init(0, (int16_t)(ROWS_Y + PARAMS * ROW_H + 4), theme::SCREEN_W, 16, T_SMALL_C, theme::TEXT_DIM);
    add(caption);

    add(topBar);
    topBar.setLeft("INTERPOLATION");
    topBar.setDot(p.a, true);
    add(legend);
    legend.setEntry(0, "KNOB", "value");
    legend.setEntry(1, "PRESS", "next field");
    legend.setEntry(2, "BACK", "run screen");
    legend.setEntry(3, "SIDE", "hold: arm");

    /* Opening the screen selects the mode when the machine is still. */
    Model& m = presenter->theModel();
    if (m.opMode() != Model::OM_INTERP) m.selectMode(Model::OM_INTERP);
    layoutRows();
    update();
}

void InterpScreenView::tearDownScreen()
{
    InterpScreenViewBase::tearDownScreen();
}

void InterpScreenView::layoutTicks()
{
    Model& m = presenter->theModel();
    int pitch = m.settings().snapDeg;
    int n = pitch > 0 ? 360 / pitch : 0;
    if (n > MAX_TICKS) n = MAX_TICKS;
    if (n == shownTicks) return;
    shownTicks = n;
    const float c = (float)(DIAL_R + 4);
    for (int i = 0; i < MAX_TICKS; ++i)
    {
        bool on = i < n;
        if (on)
        {
            float a = (float)i * 2.0f * PI_F / (float)n;
            float sx = c + (DIAL_R - 12) * sinf(a), sy = c - (DIAL_R - 12) * cosf(a);
            float ex = c + (DIAL_R + 2) * sinf(a), ey = c - (DIAL_R + 2) * cosf(a);
            ticks[i].setLine(sx, sy, ex, ey);
        }
        ticks[i].setVisible(on);
        ticks[i].invalidate();
    }
}

void InterpScreenView::layoutRows()
{
    const theme::AccentPair& p = theme::accent(theme::ACCENT_MINT);
    for (int i = 0; i < PARAMS; ++i)
    {
        if (i == selected) rowPanel[i].setAccent(p.a, 56);
        else rowPanel[i].setPlain();
        rowName[i].setRgb(i == selected ? theme::TEXT : theme::TEXT_DIM);
        rowValue[i].setRgb(i == selected ? p.a : theme::TEXT);
    }
    caption.set(PARAM_HELP[selected]);
}

void InterpScreenView::handleTickEvent()
{
    tick++;
    Model& m = presenter->theModel();
    /* Ease the pointer round the dial. */
    float target = m.spindleAngleDeg();
    float diff = target - shownAngle;
    while (diff > 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    if (fabsf(diff) > 0.3f)
    {
        shownAngle += diff * 0.3f;
        while (shownAngle < 0.0f) shownAngle += 360.0f;
        while (shownAngle >= 360.0f) shownAngle -= 360.0f;
        float a = shownAngle * PI_F / 180.0f;
        pointer.invalidate();
        pointer.setCircle((DIAL_R + 10) + DIAL_R * sinf(a), (DIAL_R + 10) - DIAL_R * cosf(a), 6);
        pointer.invalidate();
        dialArc.updateArc(0, (int)(shownAngle + 0.5f));
    }
}

void InterpScreenView::handleInput(const ui::InputEvent& e)
{
    Model& m = presenter->theModel();
    if (e.type == ui::IN_TURN && e.key == ui::KEY_KNOB)
    {
        switch (selected)
        {
        case 0: m.adjustStroke(e.value); break;
        case 1: m.adjustSnap(e.value); break;
        default: m.adjustHoldTorque(e.value); break;
        }
        m.feedback(ui::FB_CLICK);
        update();
        return;
    }
    if (e.type == ui::IN_RELEASE && e.value == 0)
    {
        if (e.key == ui::KEY_KNOB)
        {
            selected = (selected + 1) % PARAMS;
            m.feedback(ui::FB_CLICK);
            layoutRows();
        }
        else if (e.key == ui::KEY_BACK)
        {
            m.feedback(ui::FB_CLICK);
            m.goTo(Model::SCR_RUN);
        }
    }
}

void InterpScreenView::update()
{
    Model& m = presenter->theModel();
    const Model::Settings& cfg = m.settings();
    char buf[80];
    angleValue.setInt((int)(m.spindleAngleDeg() + 0.5f) % 360);
    if (m.opState() == Model::OP_ARMED)
        angleHint.set(cfg.snapDeg > 0 ? "turn the spindle: it clicks into the detents" : "detent off: spindle holds anywhere");
    else
        angleHint.set("hold SIDE 1 s to arm");

    snprintf(buf, sizeof(buf), "%d.%d rev", (int)m.setpointForMode(Model::OM_INTERP), (int)(m.setpointForMode(Model::OM_INTERP) * 10 + 0.5f) % 10);
    rowValue[0].set(buf);
    if (cfg.snapDeg > 0) snprintf(buf, sizeof(buf), "%d°", cfg.snapDeg);
    else snprintf(buf, sizeof(buf), "off");
    rowValue[1].set(buf);
    snprintf(buf, sizeof(buf), "%d.%02d A", (int)cfg.holdTorqueA, (int)(cfg.holdTorqueA * 100 + 0.5f) % 100);
    rowValue[2].set(buf);
    layoutTicks();

    if (m.opMode() != Model::OM_INTERP) topBar.setRight("STOP TO SELECT");
    else topBar.setRight(m.opState() == Model::OP_ARMED ? "ARMED" : "DISARMED");
}
