#include <gui/traversescreen_screen/TraverseScreenView.hpp>
#include <math.h>
#include <stdio.h>

using namespace touchgfx;

static const int16_t RING_CX = 136, RING_CY = 150, RING_R = 64;
static const float ROT_MAX = 10.0f; /* ring full scale */

TraverseScreenView::TraverseScreenView() : tick(0), shownFrac(0.0f) {}

void TraverseScreenView::setupScreen()
{
    TraverseScreenViewBase::setupScreen();
    add(backdrop);
    backdrop.setAccent(theme::ACCENT_MINT);
    const theme::AccentPair& p = theme::accent(theme::ACCENT_MINT);

    ringTrackPainter.setColor(theme::rgb(theme::WHITE));
    setupArc(ringTrack, ringTrackPainter, RING_CX, RING_CY, RING_R, 10, -135, 135);
    ringTrack.setAlpha(28);
    add(ringTrack);
    setupArc(ringValue, ringValuePainter, RING_CX, RING_CY, RING_R, 10, -135, -135);
    setGradientArc(ringValue, ringValuePainter, theme::ACCENT_MINT);
    add(ringValue);

    title.init(0, RING_CY - 40, theme::SCREEN_W, 14, T_SMALL_C, theme::TEXT_DIM);
    title.set("TRAVERSE SYNC");
    add(title);
    value.init(0, RING_CY - 28, theme::SCREEN_W, 60, T_BIG_C, theme::TEXT);
    add(value);
    unit.init(0, RING_CY + 34, theme::SCREEN_W, 16, T_SMALL_C, theme::TEXT_DIM);
    unit.set("rot / mm");
    add(unit);
    hint.init(0, RING_CY + 78, theme::SCREEN_W, 16, T_SMALL_C, theme::TEXT_DIM);
    hint.set("turn the knob in 0.05 steps");
    add(hint);

    infoCard.init(12, 256, 248, 76);
    add(infoCard);
    pitchLabel.init(22, 264, 120, 14, T_SMALL_L, theme::TEXT_DIM);
    pitchLabel.set("Pitch per rotation");
    add(pitchLabel);
    pitchValue.init(140, 263, 110, 16, T_BODY_R, theme::TEXT);
    add(pitchValue);
    leadLabel.init(22, 286, 120, 14, T_SMALL_L, theme::TEXT_DIM);
    leadLabel.set("Lead screw  ·  travel");
    add(leadLabel);
    leadValue.init(140, 285, 110, 16, T_BODY_R, theme::TEXT);
    add(leadValue);
    stepsLabel.init(22, 308, 140, 14, T_SMALL_L, theme::TEXT_DIM);
    stepsLabel.set("Steps per encoder count");
    add(stepsLabel);
    stepsValue.init(140, 307, 110, 16, T_BODY_R, theme::TEXT);
    add(stepsValue);

    statePanel.init(12, 342, 248, 40);
    add(statePanel);
    stateCap.setPosition(24, 354, 44, 16);
    stateCap.setColor(theme::rgb(p.a));
    stateCap.setBorderColor(theme::rgb(p.a));
    stateCap.setBorderSize(1);
    stateCap.setAlpha(90);
    add(stateCap);
    stateKey.init(24, 355, 44, 14, T_SMALL_C, theme::TEXT);
    stateKey.set("PRESS");
    add(stateKey);
    stateText.init(76, 353, 176, 18, T_BODY_L, theme::TEXT);
    add(stateText);

    carriageTrack.setPosition(22, 396, 228, 12);
    carriageTrack.setColor(theme::rgb(theme::WHITE));
    carriageTrack.setAlpha(26);
    add(carriageTrack);
    carriage.setPosition(22, 397, 22, 10);
    carriage.setColor(theme::rgb(p.a));
    add(carriage);
    carMin.init(22, 412, 60, 14, T_SMALL_L, theme::TEXT_DIM);
    carMin.set("0 mm");
    add(carMin);
    carPos.init(96, 412, 80, 14, T_SMALL_C, theme::TEXT_DIM);
    add(carPos);
    carMax.init(190, 412, 60, 14, T_SMALL_R, theme::TEXT_DIM);
    add(carMax);

    add(topBar);
    topBar.setLeft("TRAVERSE");
    topBar.setDot(p.a, true);
    add(legend);
    legend.setEntry(0, "KNOB", "rot/mm");
    legend.setEntry(1, "PRESS", "lock / release");
    legend.setEntry(2, "BACK", "menu");
    legend.setEntry(3, "BACK", "hold: stop");
    update();
}

void TraverseScreenView::tearDownScreen()
{
    TraverseScreenViewBase::tearDownScreen();
}

void TraverseScreenView::handleTickEvent()
{
    tick++;
    Model& m = presenter->theModel();
    if (m.traverseEngaged())
    {
        float s = sinf((float)tick * 0.1f);
        statePanel.setFill((uint8_t)(40 + 20 * s));
    }
}

void TraverseScreenView::handleInput(const ui::InputEvent& e)
{
    Model& m = presenter->theModel();
    if (e.type == ui::IN_TURN && e.key == ui::KEY_KNOB)
    {
        float v = m.settings().rotPerMm + 0.05f * (float)e.value;
        v = floorf(v * 20.0f + 0.5f) / 20.0f;
        m.setRotPerMm(v);
        m.feedback(ui::FB_CLICK);
        return;
    }
    if (e.type == ui::IN_RELEASE && e.value == 0)
    {
        if (e.key == ui::KEY_KNOB)
        {
            bool on = !m.traverseEngaged();
            if (m.engageTraverse(on)) m.feedback(on ? ui::FB_CONFIRM : ui::FB_CLICK);
            else m.feedback(ui::FB_WARN);
        }
        else if (e.key == ui::KEY_BACK) { m.feedback(ui::FB_CLICK); m.goTo(Model::SCR_MENU); }
    }
}

void TraverseScreenView::update()
{
    Model& m = presenter->theModel();
    const Model::Settings& cfg = m.settings();
    const theme::AccentPair& p = theme::accent(theme::ACCENT_MINT);
    char buf[80];

    float frac = cfg.rotPerMm / ROT_MAX;
    if (frac > 1.0f) frac = 1.0f;
    shownFrac += (frac - shownFrac) * 0.3f;
    if (fabsf(frac - shownFrac) < 0.002f) shownFrac = frac;
    ringValue.updateArc(-135, -135 + (int)(270.0f * shownFrac + 0.5f));

    value.setFloat(cfg.rotPerMm, 2);
    float pitch = cfg.rotPerMm > 0.0f ? 1.0f / cfg.rotPerMm : 0.0f;
    pitchValue.setFloat(pitch, 3, " mm");
    snprintf(buf, sizeof(buf), "%d.%d mm  ·  %d mm", (int)cfg.leadMm, (int)(cfg.leadMm * 10) % 10, (int)cfg.travelMm);
    leadValue.set(buf);
    const ui::Limits& lim = ui::limits();
    float scale = cfg.rotPerMm > 0.0f ? m.stepsPerMm() / (cfg.rotPerMm * lim.encoderCpr * lim.gearRatio) : 0.0f;
    stepsValue.setFloat(scale, 3);

    if (m.traverseEngaged())
    {
        stateText.set("Locked  ·  press to release");
        stateText.setRgb(theme::GREEN);
        statePanel.setAccent(theme::GREEN, 48);
        stateCap.setColor(theme::rgb(theme::GREEN));
        stateCap.setBorderColor(theme::rgb(theme::GREEN));
    }
    else
    {
        stateText.set(m.traverseAtEnd() ? "At travel end  ·  press to re-lock" : "Released  ·  press to lock");
        stateText.setRgb(theme::TEXT);
        statePanel.setPlain();
        stateCap.setColor(theme::rgb(p.a));
        stateCap.setBorderColor(theme::rgb(p.a));
    }
    stateCap.invalidate();

    float mm = fabsf(m.carriageMm());
    float travel = cfg.travelMm > 0.0f ? cfg.travelMm : 1.0f;
    float cf = mm / travel;
    if (cf > 1.0f) cf = 1.0f;
    int16_t cx = (int16_t)(22 + (int)(cf * (228 - 22)));
    if (carriage.getX() != cx)
    {
        carriage.invalidate();
        carriage.setX(cx);
        carriage.invalidate();
    }
    carPos.setFloat(mm, 1, " mm");
    carMax.setInt((int)cfg.travelMm, " mm");
    topBar.setRight(m.opState() == Model::OP_ARMED ? "ARMED  ·  PEDAL LOCKED" : "DISARMED");
}
