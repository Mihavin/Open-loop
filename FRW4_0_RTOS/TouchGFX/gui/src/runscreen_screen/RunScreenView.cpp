#include <gui/runscreen_screen/RunScreenView.hpp>
#include <math.h>
#include <stdio.h>

using namespace touchgfx;

static const int16_t RING_CX = 136, RING_CY = 190, RING_R = 66;
static const int16_t GAUGE_CX = 136, GAUGE_CY = 148, GAUGE_R = 78;
static const float PI_F = 3.14159265f;
static const int16_t TAB_W = 62;

RunScreenView::RunScreenView()
    : accent(theme::ACCENT_AMBER), tick(0), enterTick(0), shownFrac(0.0f), shownNeedle(-90.0f), armedPane(false), legendMode(-1)
{
}

void RunScreenView::setupScreen()
{
    RunScreenViewBase::setupScreen();
    add(backdrop);
    buildDisarmed();
    buildArmed();
    add(topBar);
    add(legend);

    Model& m = presenter->theModel();
    accent = theme::accentForMode(m.opMode());
    applyAccent();
    showPane(m.opState() == Model::OP_ARMED);
    enterTick = 0;
    shownFrac = 0.0f;
    update();
}

void RunScreenView::tearDownScreen()
{
    RunScreenViewBase::tearDownScreen();
}

/* ---------------- construction ---------------- */

void RunScreenView::buildDisarmed()
{
    disarmed.setPosition(0, 0, theme::SCREEN_W, theme::SCREEN_H);
    add(disarmed);

    tabTrack.init(12, 38, 248, 32);
    disarmed.add(tabTrack);
    tabPill.setPosition(14, 40, TAB_W, 28);
    disarmed.add(tabPill);
    static const char* const names[4] = { "RPM", "TORQUE", "INTERP", "DRY" };
    for (int i = 0; i < 4; ++i)
    {
        tabs[i].init((int16_t)(14 + i * TAB_W), 46, TAB_W, 16, T_SMALL_C, theme::TEXT, 140);
        tabs[i].set(names[i]);
        disarmed.add(tabs[i]);
    }

    ringTrackPainter.setColor(theme::rgb(theme::WHITE));
    setupArc(ringTrack, ringTrackPainter, RING_CX, RING_CY, RING_R, 12, -135, 135);
    ringTrack.setAlpha(28);
    disarmed.add(ringTrack);
    setupArc(ringValue, ringValuePainter, RING_CX, RING_CY, RING_R, 12, -135, -135);
    disarmed.add(ringValue);
    ringInnerPainter.setColor(theme::rgb(theme::WHITE));
    ringInner.setPosition(RING_CX - 54, RING_CY - 54, 108, 108);
    ringInner.setCircle(54, 54, 54);
    ringInner.setPainter(ringInnerPainter);
    ringInner.setAlpha(12);
    disarmed.add(ringInner);
    ringInnerLinePainter.setColor(theme::rgb(theme::WHITE));
    setupArc(ringInnerLine, ringInnerLinePainter, RING_CX, RING_CY, 54, 1, 0, 360);
    ringInnerLine.setAlpha(30);
    disarmed.add(ringInnerLine);

    setLabel.init(0, 94, theme::SCREEN_W, 14, T_SMALL_C, theme::TEXT_DIM);
    disarmed.add(setLabel);
    setValue.init(0, RING_CY - 30, theme::SCREEN_W, 62, T_BIG_C, theme::TEXT);
    disarmed.add(setValue);
    setSub.init(0, 250, theme::SCREEN_W, 16, T_SMALL_C, theme::TEXT_DIM);
    disarmed.add(setSub);

    turnsCardD.init(12, 278, 121, 48);
    disarmed.add(turnsCardD);
    turnsTitleD.init(22, 284, 100, 14, T_SMALL_L, theme::TEXT_DIM);
    turnsTitleD.set("TURNS");
    disarmed.add(turnsTitleD);
    turnsValueD.init(22, 297, 100, 28, T_MEDIUM_L, theme::TEXT);
    disarmed.add(turnsValueD);

    travCard.init(139, 278, 121, 48);
    disarmed.add(travCard);
    travTitle.init(149, 284, 60, 14, T_SMALL_L, theme::TEXT_DIM);
    travTitle.set("TRAVERSE");
    disarmed.add(travTitle);
    travStatus.init(205, 284, 46, 14, T_SMALL_R, theme::GREEN);
    disarmed.add(travStatus);
    travValue.init(149, 297, 60, 28, T_MEDIUM_L, theme::TEXT);
    disarmed.add(travValue);
    travUnit.init(205, 306, 48, 14, T_SMALL_R, theme::TEXT_DIM);
    travUnit.set("rot/mm");
    disarmed.add(travUnit);

    turnsHintD.init(0, 334, theme::SCREEN_W, 14, T_SMALL_C, theme::TEXT_DIM, 160);
    turnsHintD.set("hold BACK 2 s to zero the turns");
    disarmed.add(turnsHintD);

    hintPanel.init(12, 380, 248, 44);
    hintPanel.setAccent(theme::GREEN, 40);
    disarmed.add(hintPanel);
    hintCap.setPosition(24, 394, 38, 16);
    hintCap.setColor(theme::rgb(theme::GREEN));
    hintCap.setBorderColor(theme::rgb(theme::GREEN));
    hintCap.setBorderSize(1);
    hintCap.setAlpha(90);
    disarmed.add(hintCap);
    hintKey.init(24, 395, 38, 14, T_SMALL_C, theme::TEXT);
    hintKey.set("SIDE");
    disarmed.add(hintKey);
    hintText.init(70, 393, 182, 18, T_BODY_L, theme::GREEN);
    hintText.set("Hold 1 s to arm");
    disarmed.add(hintText);
}

void RunScreenView::buildArmed()
{
    armed.setPosition(0, 0, theme::SCREEN_W, theme::SCREEN_H);
    add(armed);

    setupArc(gaugeHalo, gaugeHaloPainter, GAUGE_CX, GAUGE_CY, GAUGE_R, 28, -90, -90);
    gaugeHalo.setAlpha(60);
    armed.add(gaugeHalo);
    gaugeTrackPainter.setColor(theme::rgb(theme::WHITE));
    setupArc(gaugeTrack, gaugeTrackPainter, GAUGE_CX, GAUGE_CY, GAUGE_R, 14, -90, 90);
    gaugeTrack.setAlpha(28);
    armed.add(gaugeTrack);
    setupArc(gaugeValue, gaugeValuePainter, GAUGE_CX, GAUGE_CY, GAUGE_R, 14, -90, -90);
    armed.add(gaugeValue);

    needlePainter.setColor(theme::rgb(theme::WHITE));
    needle.setPosition(GAUGE_CX - 70, GAUGE_CY - 70, 140, 80);
    needle.setStart(70, 70);
    needle.setEnd(10, 70);
    needle.setLineWidth(3);
    needle.setLineEndingStyle(Line::ROUND_CAP_ENDING);
    needle.setPainter(needlePainter);
    armed.add(needle);
    capOuterPainter.setColor(theme::rgb(theme::WHITE));
    capOuter.setPosition(GAUGE_CX - 8, GAUGE_CY - 8, 16, 16);
    capOuter.setCircle(8, 8, 8);
    capOuter.setPainter(capOuterPainter);
    armed.add(capOuter);
    capInnerPainter.setColor(theme::rgb(theme::BG_MID));
    capInner.setPosition(GAUGE_CX - 6, GAUGE_CY - 6, 12, 12);
    capInner.setCircle(6, 6, 6);
    capInner.setPainter(capInnerPainter);
    armed.add(capInner);

    gaugeMin.init(GAUGE_CX - GAUGE_R - 16, GAUGE_CY + 6, 32, 14, T_SMALL_C, theme::TEXT_DIM);
    gaugeMin.set("0");
    armed.add(gaugeMin);
    gaugeMax.init(GAUGE_CX + GAUGE_R - 16, GAUGE_CY + 6, 32, 14, T_SMALL_C, theme::TEXT_DIM);
    armed.add(gaugeMax);

    actValue.init(0, GAUGE_CY + 12, theme::SCREEN_W, 62, T_BIG_C, theme::TEXT);
    armed.add(actValue);
    actUnit.init(0, GAUGE_CY + 70, theme::SCREEN_W, 16, T_BODY_C, theme::TEXT_DIM);
    armed.add(actUnit);
    actSub.init(0, GAUGE_CY + 88, 190, 16, T_SMALL_R, theme::TEXT_DIM);
    armed.add(actSub);
    actCurrent.init(194, GAUGE_CY + 88, 70, 16, T_SMALL_L, theme::INFO);
    armed.add(actCurrent);

    cardA.init(12, 258, 121, 50);
    armed.add(cardA);
    cardATitle.init(22, 264, 100, 14, T_SMALL_L, theme::TEXT_DIM);
    armed.add(cardATitle);
    cardAValue.init(22, 278, 104, 28, T_MEDIUM_L, theme::TEXT);
    armed.add(cardAValue);
    cardB.init(139, 258, 121, 50);
    armed.add(cardB);
    cardBTitle.init(149, 264, 100, 14, T_SMALL_L, theme::TEXT_DIM);
    armed.add(cardBTitle);
    cardBValue.init(149, 278, 60, 28, T_MEDIUM_L, theme::TEXT);
    armed.add(cardBValue);
    pedalBar.init(208, 289, 44, 6);
    armed.add(pedalBar);

    turnsCard.init(12, 314, 121, 50);
    armed.add(turnsCard);
    turnsTitle.init(22, 320, 100, 14, T_SMALL_L, theme::TEXT_DIM);
    turnsTitle.set("TURNS");
    armed.add(turnsTitle);
    turnsValue.init(22, 334, 104, 28, T_MEDIUM_L, theme::TEXT);
    armed.add(turnsValue);

    syncCard.init(139, 314, 121, 50);
    armed.add(syncCard);
    syncTitle.init(149, 320, 60, 14, T_SMALL_L, theme::TEXT_DIM);
    syncTitle.set("TRAVERSE");
    armed.add(syncTitle);
    syncStatus.init(205, 320, 46, 14, T_SMALL_R, theme::GREEN);
    armed.add(syncStatus);
    syncValue.init(149, 334, 60, 28, T_MEDIUM_L, theme::TEXT);
    armed.add(syncValue);
    syncUnit.init(205, 343, 48, 14, T_SMALL_R, theme::TEXT_DIM);
    syncUnit.set("rot/mm");
    armed.add(syncUnit);

    carriageTrack.setPosition(22, 378, 228, 12);
    carriageTrack.setColor(theme::rgb(theme::WHITE));
    carriageTrack.setAlpha(26);
    armed.add(carriageTrack);
    carriage.setPosition(22, 379, 22, 10);
    armed.add(carriage);
    carMin.init(22, 394, 60, 14, T_SMALL_L, theme::TEXT_DIM);
    carMin.set("0 mm");
    armed.add(carMin);
    carPos.init(96, 394, 80, 14, T_SMALL_C, theme::TEXT_DIM);
    armed.add(carPos);
    carMax.init(190, 394, 60, 14, T_SMALL_R, theme::TEXT_DIM);
    armed.add(carMax);
}

void RunScreenView::applyAccent()
{
    const theme::AccentPair& p = theme::accent(accent);
    backdrop.setAccent(accent);
    tabPill.setColor(theme::rgb(p.a));
    tabPill.invalidate();
    setGradientArc(ringValue, ringValuePainter, accent);
    setGradientArc(gaugeValue, gaugeValuePainter, accent);
    setGradientArc(gaugeHalo, gaugeHaloPainter, accent);
    cardA.setAccent(p.a, 60);
    pedalBar.setAccent(p.a);
    carriage.setColor(theme::rgb(p.a));
    carriage.invalidate();
    travValue.setRgb(p.a);
    syncValue.setRgb(p.a);
    turnsValue.setRgb(p.a);
    turnsValueD.setRgb(p.a);
}

void RunScreenView::showPane(bool armedNow)
{
    armedPane = armedNow;
    disarmed.setVisible(!armedNow);
    armed.setVisible(armedNow);
    legendMode = -1;
    setLegend();
    disarmed.invalidate();
    armed.invalidate();
}

void RunScreenView::setLegend()
{
    Model& m = presenter->theModel();
    int mode = (int)m.opMode() + (armedPane ? 10 : 0);
    if (mode == legendMode) return;
    legendMode = mode;
    if (!armedPane)
    {
        legend.setEntry(0, "KNOB", "setpoint");
        legend.setEntry(1, "PRESS", m.opMode() == Model::OM_TORQUE ? "rpm mode" : "torque mode");
        legend.setEntry(2, "SIDE", "hold: arm");
        legend.setEntry(3, "HOLD", "knob: menu");
        return;
    }
    switch (m.opMode())
    {
    case Model::OM_TORQUE:
        legend.setEntry(0, "KNOB", "brake amps");
        legend.setEntry(1, "PRESS", "rpm mode");
        legend.setEntry(2, "F+R", "traverse");
        legend.setEntry(3, "SIDE", "hold: disarm");
        break;
    case Model::OM_INTERP:
        legend.setEntry(0, "KNOB", "stroke");
        legend.setEntry(1, "SIDE", "reverse");
        legend.setEntry(2, "F+R", "traverse");
        legend.setEntry(3, "BACK", "hold: stop");
        break;
    case Model::OM_DRY:
        legend.setEntry(0, "PRESS", "start / stop");
        legend.setEntry(1, "SIDE", "reverse");
        legend.setEntry(2, "BACK", "brake");
        legend.setEntry(3, "F+R", "traverse");
        break;
    default:
        legend.setEntry(0, "KNOB", "rpm");
        legend.setEntry(1, "SIDE", "reverse");
        legend.setEntry(2, "BACK", "brake");
        legend.setEntry(3, "F+R", "traverse");
        break;
    }
}

/* ---------------- input ---------------- */

void RunScreenView::handleInput(const ui::InputEvent& e)
{
    Model& m = presenter->theModel();
    if (e.type == ui::IN_TURN && e.key == ui::KEY_KNOB)
    {
        if (m.adjustSetpoint(e.value)) m.feedback(ui::FB_CLICK);
        else m.feedback(ui::FB_WARN);
        return;
    }
    if (e.type == ui::IN_RELEASE && e.value == 0 && e.key == ui::KEY_KNOB)
    {
        if (m.opMode() == Model::OM_DRY) m.toggleDryTimer();
        else m.toggleRpmTorque();
    }
}

/* ---------------- per frame ---------------- */

void RunScreenView::handleTickEvent()
{
    tick++;
    enterTick++;
    Model& m = presenter->theModel();
    bool live = m.opState() == Model::OP_ARMED;
    /* Pulse the status dot and the arm hint. */
    float s = sinf((float)tick * 0.12f);
    topBar.setDotAlpha(live ? (uint8_t)(170 + 85 * s) : 255);
    if (!armedPane)
    {
        uint8_t a = (uint8_t)(40 + 22 * s);
        hintPanel.setFill(m.opState() == Model::OP_ARMING ? 70 : a);
    }
    else if (m.brakeReleased() || m.reversing())
    {
        cardA.setAccent(theme::accent(theme::ACCENT_AMBER).a, (uint8_t)(50 + 30 * s));
    }
}

void RunScreenView::updateTopBar()
{
    Model& m = presenter->theModel();
    const ui::Telemetry& t = m.telemetry();
    char buf[48];
    switch (m.opState())
    {
    case Model::OP_ARMED:
        snprintf(buf, sizeof(buf), "ARMED  ·  %s", m.modeName());
        topBar.setDot(theme::GREEN, true);
        break;
    case Model::OP_ARMING:
        snprintf(buf, sizeof(buf), "ARMING  ·  %s", m.modeName());
        topBar.setDot(theme::accent(theme::ACCENT_AMBER).a, true);
        break;
    default:
        snprintf(buf, sizeof(buf), "DISARMED  ·  %s", m.modeName());
        topBar.setDot(theme::TEXT_DIM, true);
        break;
    }
    topBar.setLeft(buf);
    if (t.faults & ui::FAULT_FAN) topBar.setRight("FAN WARNING");
    else if (m.controlLocked()) topBar.setRight("PEDAL LOCKED");
    else if (!t.pedalHomed) topBar.setRight("PEDAL NOT HOMED");
    else topBar.setRight("NO FAULTS");
}

void RunScreenView::updateTraverse(Label& value, Label& status)
{
    Model& m = presenter->theModel();
    value.setFloat(m.settings().rotPerMm, 2);
    if (m.traverseEngaged()) { status.set(m.traverseDirection() > 0 ? "LOCK F" : "LOCK R"); status.setRgb(theme::GREEN); }
    else if (m.traverseAtEnd()) { status.set("END"); status.setRgb(theme::accent(theme::ACCENT_AMBER).a); }
    else { status.set(m.traverseDirection() > 0 ? "OFF F" : "OFF R"); status.setRgb(theme::TEXT_DIM); }
}

void RunScreenView::updateTurns(Label& value)
{
    Model& m = presenter->theModel();
    if (m.turnsCounted()) value.setFloat(m.turnsCount(), 1);
    else value.set("off");
}

void RunScreenView::formatTime(char* out, int size, uint32_t ms)
{
    uint32_t s = (ms + 999) / 1000;
    snprintf(out, size, "%u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
}

void RunScreenView::update()
{
    Model& m = presenter->theModel();
    bool live = m.opState() == Model::OP_ARMED;
    if (live != armedPane) { showPane(live); enterTick = 0; shownFrac = 0.0f; }
    theme::Accent a = theme::accentForMode(m.opMode());
    if (a != accent) { accent = a; applyAccent(); }
    setLegend();
    updateTopBar();
    if (live) updateArmed();
    else updateDisarmed();
}

void RunScreenView::updateDisarmed()
{
    Model& m = presenter->theModel();
    int sel = (int)m.opMode();
    int16_t px = (int16_t)(14 + sel * TAB_W);
    if (tabPill.getX() != px)
    {
        tabPill.invalidate();
        tabPill.setX(px);
        tabPill.invalidate();
    }
    for (int i = 0; i < 4; ++i)
    {
        uint32_t c = i == sel ? theme::INK : theme::TEXT;
        tabs[i].setRgb(c);
        uint8_t alpha = i == sel ? 255 : 140;
        if (tabs[i].getAlpha() != alpha) { tabs[i].setAlpha(alpha); tabs[i].invalidate(); }
    }

    float max = m.setpointMax();
    float target = max > 0.0f ? m.setpoint() / max : 0.0f;
    if (m.opState() == Model::OP_ARMING)
    {
        /* rotor alignment: a short arc chasing round the ring */
        int start = -135 + (tick * 9) % 270;
        ringValue.updateArc(start, start + 40);
        hintText.set("Aligning rotor");
        hintKey.set("WAIT");
    }
    else
    {
        shownFrac += (target - shownFrac) * (enterTick < 40 ? 0.12f : 0.3f);
        if (fabsf(target - shownFrac) < 0.002f) shownFrac = target;
        ringValue.updateArc(-135, -135 + (int)(270.0f * shownFrac + 0.5f));
        const char* n = m.notice();
        if (n[0])
        {
            hintText.set(n);
            hintText.setRgb(theme::accent(theme::ACCENT_AMBER).a);
            hintKey.set("INFO");
        }
        else
        {
            hintText.set("Hold 1 s to arm");
            hintText.setRgb(theme::GREEN);
            hintKey.set("SIDE");
        }
    }

    char buf[48];
    const char* d = m.direction() > 0 ? "FWD" : "REV";
    switch (m.opMode())
    {
    case Model::OM_TORQUE:
        setLabel.set("BRAKE AT FULL PEDAL");
        setValue.setFloat(m.setpoint(), 2);
        snprintf(buf, sizeof(buf), "amps  ·  speed limit %d rpm", m.settings().torqueModeMaxRpm);
        setSub.set(buf);
        break;
    case Model::OM_INTERP:
        setLabel.set("STROKE");
        setValue.setFloat(m.setpoint(), 1);
        snprintf(buf, sizeof(buf), "rev per pedal stroke  ·  snap %d°  ·  %s", m.settings().snapDeg, d);
        setSub.set(buf);
        break;
    case Model::OM_DRY:
        setLabel.set("DRY RUN SPEED");
        setValue.setInt((int)(m.setpoint() + 0.5f));
        snprintf(buf, sizeof(buf), "shaft rpm  ·  %d min  ·  %s", m.settings().dryMinutes, d);
        setSub.set(buf);
        break;
    default:
        setLabel.set("SPEED AT FULL PEDAL");
        setValue.setInt((int)(m.setpoint() + 0.5f));
        snprintf(buf, sizeof(buf), "shaft rpm  ·  %s  ·  torque limit %d.%02d A", d, (int)m.settings().rpmModeTorqueA, (int)(m.settings().rpmModeTorqueA * 100 + 0.5f) % 100);
        setSub.set(buf);
        break;
    }
    updateTurns(turnsValueD);
    updateTraverse(travValue, travStatus);
}

void RunScreenView::updateArmed()
{
    Model& m = presenter->theModel();
    const ui::Telemetry& t = m.telemetry();
    const Model::Settings& cfg = m.settings();
    float rpm = fabsf(m.shaftRpm());
    char buf[48];
    float frac = 0.0f;

    switch (m.opMode())
    {
    case Model::OM_TORQUE:
    {
        /* Gauge: braking force actually applied (pedal x setpoint), out of the max. */
        float max = ui::limits().maxTorqueA;
        frac = max > 0.0f ? m.currentA() / max : 0.0f;
        gaugeMax.setFloat(max, 1);
        actValue.setFloat(m.currentA(), 2);
        actUnit.set("brake amps");
        snprintf(buf, sizeof(buf), "full pedal %d.%02d A  ·  %d of max %d rpm", (int)m.setpoint(), (int)(m.setpoint() * 100) % 100, (int)(rpm + 0.5f), cfg.torqueModeMaxRpm);
        cardATitle.set("BRAKE");
        cardAValue.set(t.pedalPermille > 30 ? "ON" : "FREE");
        break;
    }
    case Model::OM_INTERP:
    {
        /* Gauge: the pedal stroke; the big number is the spindle angle. */
        frac = (float)t.pedalPermille / 1000.0f;
        gaugeMax.set("stroke");
        actValue.setInt((int)(m.spindleAngleDeg() + 0.5f) % 360);
        actUnit.set("degrees");
        snprintf(buf, sizeof(buf), "%d.%d rev per stroke  ·  snap %d°", (int)m.setpoint(), (int)(m.setpoint() * 10 + 0.5f) % 10, cfg.snapDeg);
        cardATitle.set("DIRECTION");
        cardAValue.set(m.direction() > 0 ? "FWD" : "REV");
        break;
    }
    case Model::OM_DRY:
    {
        uint32_t total = (uint32_t)cfg.dryMinutes * 60000u;
        uint32_t left = m.dryRemainingMs();
        frac = total > 0 ? 1.0f - (float)left / (float)total : 0.0f;
        gaugeMax.set("end");
        formatTime(buf, sizeof(buf), left);
        actValue.set(buf);
        actUnit.set(m.dryRunning() ? "min:sec left" : "press the knob to start");
        snprintf(buf, sizeof(buf), "%d rpm  ·  actual %d", (int)(m.setpoint() + 0.5f), (int)(rpm + 0.5f));
        cardATitle.set("TIMER");
        cardAValue.set(m.dryRunning() ? (m.reversing() ? "REVERSING" : "RUNNING") : "READY");
        break;
    }
    default:
    {
        float maxRpm = (float)cfg.maxShaftRpm;
        frac = maxRpm > 0.0f ? rpm / maxRpm : 0.0f;
        gaugeMax.setInt((int)maxRpm);
        actValue.setInt((int)(rpm + 0.5f));
        actUnit.set("shaft rpm");
        snprintf(buf, sizeof(buf), "full pedal %d rpm  ·  max %d.%02d A", (int)(m.setpoint() + 0.5f), (int)cfg.rpmModeTorqueA, (int)(cfg.rpmModeTorqueA * 100 + 0.5f) % 100);
        cardATitle.set("DIRECTION");
        if (m.brakeReleased()) cardAValue.set("FREE");
        else if (m.reversing()) cardAValue.set("REVERSING");
        else cardAValue.set(m.direction() > 0 ? "FWD" : "REV");
        break;
    }
    }
    if (frac > 1.0f) frac = 1.0f;
    if (frac < 0.0f) frac = 0.0f;
    shownFrac += (frac - shownFrac) * (enterTick < 40 ? 0.1f : 0.35f);
    if (fabsf(frac - shownFrac) < 0.002f) shownFrac = frac;
    int end = -90 + (int)(180.0f * shownFrac + 0.5f);
    gaugeValue.updateArc(-90, end);
    gaugeHalo.updateArc(-90, end);

    /* Needle: -90 deg points left (0), +90 right (max). */
    float ang = -90.0f + 180.0f * shownFrac;
    if (fabsf(ang - shownNeedle) > 0.2f)
    {
        shownNeedle = ang;
        float rad = shownNeedle * PI_F / 180.0f; /* 0 deg = up, clockwise */
        float ex = 70.0f + 62.0f * sinf(rad);
        float ey = 70.0f - 62.0f * cosf(rad);
        needle.updateEnd(ex, ey);
    }

    actSub.set(buf);
    snprintf(buf, sizeof(buf), "%d.%02d A", (int)m.currentA(), (int)(m.currentA() * 100) % 100);
    actCurrent.set(buf);

    if (!(m.brakeReleased() || m.reversing())) cardA.setAccent(theme::accent(accent).a, 60);

    cardBTitle.set("PEDAL");
    int pct = (t.pedalPermille + 5) / 10;
    cardBValue.setInt(pct, "%");
    pedalBar.setPermille(t.pedalPermille);

    updateTurns(turnsValue);
    updateTraverse(syncValue, syncStatus);
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
}
