#include <gui/faultscreen_screen/FaultScreenView.hpp>
#include <math.h>

using namespace touchgfx;

FaultScreenView::FaultScreenView() : tick(0) {}

void FaultScreenView::setupScreen()
{
    FaultScreenViewBase::setupScreen();
    add(backdrop);
    backdrop.setAccent(theme::ACCENT_RED);

    badgeFillPainter.setColor(theme::rgb(theme::DANGER));
    badgeFill.setPosition(136 - 34, 96 - 34, 68, 68);
    badgeFill.setCircle(34, 34, 34);
    badgeFill.setPainter(badgeFillPainter);
    badgeFill.setAlpha(60);
    add(badgeFill);
    setupArc(badgeRing, badgeRingPainter, 136, 96, 38, 4, 0, 360);
    setGradientArc(badgeRing, badgeRingPainter, theme::ACCENT_RED);
    add(badgeRing);
    badgeMark.init(0, 72, theme::SCREEN_W, 48, T_LARGE_C, theme::TEXT);
    badgeMark.set("!");
    add(badgeMark);

    title.init(0, 148, theme::SCREEN_W, 30, T_MEDIUM_C, theme::TEXT);
    title.set("Output cut");
    add(title);
    subtitle.init(0, 178, theme::SCREEN_W, 16, T_BODY_C, theme::TEXT_DIM);
    subtitle.set("Motor and traverse are disarmed");
    add(subtitle);

    listCard.init(12, 206, 248, LINES * 24 + 14);
    add(listCard);
    for (int i = 0; i < LINES; ++i)
    {
        lines[i].init(24, (int16_t)(206 + 8 + i * 24), 224, 18, T_BODY_L, theme::TEXT);
        add(lines[i]);
    }

    causePanel.init(12, 350, 248, 52);
    add(causePanel);
    causeCap.setPosition(24, 368, 44, 16);
    causeCap.setBorderSize(1);
    causeCap.setAlpha(90);
    add(causeCap);
    causeKey.init(24, 369, 44, 14, T_SMALL_C, theme::TEXT);
    add(causeKey);
    causeText.init(76, 358, 176, 36, T_BODY_L, theme::TEXT);
    add(causeText);
    hint.init(0, 410, theme::SCREEN_W, 16, T_SMALL_C, theme::TEXT_DIM);
    hint.set("clears with a confirm, not a power cycle");
    add(hint);

    add(topBar);
    topBar.setLeft("FAULT");
    topBar.setDot(theme::DANGER, true);
    add(legend);
    legend.setEntry(0, "PRESS", "confirm clear");
    legend.clearEntry(1);
    legend.clearEntry(2);
    legend.clearEntry(3);
    update();
}

void FaultScreenView::tearDownScreen()
{
    FaultScreenViewBase::tearDownScreen();
}

void FaultScreenView::handleTickEvent()
{
    tick++;
    float s = sinf((float)tick * 0.1f);
    badgeFill.setAlpha((uint8_t)(60 + 40 * s));
    badgeFill.invalidate();
    topBar.setDotAlpha((uint8_t)(170 + 85 * s));
}

void FaultScreenView::handleInput(const ui::InputEvent&)
{
    /* The model handles the knob confirm itself; nothing else is allowed here. */
}

void FaultScreenView::update()
{
    Model& m = presenter->theModel();
    const ui::Telemetry& t = m.telemetry();
    int n = 0;
    for (uint32_t bit = 1; bit && n < LINES; bit <<= 1)
    {
        if (t.faults & bit)
        {
            lines[n].set(ui::faultName(bit));
            lines[n].setRgb((bit & ui::FAULT_CRITICAL) ? theme::TEXT : theme::accent(theme::ACCENT_AMBER).a);
            n++;
        }
    }
    if (n == 0) { lines[0].set("Clearing"); n = 1; }
    for (int i = n; i < LINES; ++i) lines[i].set("");

    if (t.faultCausePresent)
    {
        causePanel.setAccent(theme::DANGER, 48);
        causeCap.setColor(theme::rgb(theme::DANGER));
        causeCap.setBorderColor(theme::rgb(theme::DANGER));
        causeKey.set("WAIT");
        causeText.set("Cause still present, fix it");
        causeText.setRgb(theme::DANGER);
    }
    else
    {
        causePanel.setAccent(theme::GREEN, 40);
        causeCap.setColor(theme::rgb(theme::GREEN));
        causeCap.setBorderColor(theme::rgb(theme::GREEN));
        causeKey.set("PRESS");
        causeText.set(m.faultClearRefused() ? "Refused, check the cause, press again" : "Cause gone, press the knob to confirm");
        causeText.setRgb(theme::GREEN);
    }
    causeCap.invalidate();
    topBar.setRight(m.faultClearRefused() ? "CLEAR REFUSED" : "OUTPUT OFF");
}
