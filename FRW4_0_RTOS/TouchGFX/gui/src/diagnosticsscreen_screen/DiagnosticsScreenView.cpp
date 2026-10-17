#include <gui/diagnosticsscreen_screen/DiagnosticsScreenView.hpp>
#include <math.h>
#include <stdio.h>

using namespace touchgfx;

static const char* const ROW_NAMES[DiagnosticsScreenView::ROWS] = {
    "Motor", "Mode", "Shaft speed", "Phase current", "Position", "Stepper", "Pedal", "Faults"
};
static const char* const SOURCE_NAMES[3] = { "SHAFT SPEED", "PHASE CURRENT", "CARRIAGE" };
static const char* const SOURCE_UNITS[3] = { "rpm", "A", "mm" };

DiagnosticsScreenView::DiagnosticsScreenView() : source(0), tick(0) {}

void DiagnosticsScreenView::setupScreen()
{
    DiagnosticsScreenViewBase::setupScreen();
    add(backdrop);
    backdrop.setAccent(theme::ACCENT_CYAN);
    const theme::AccentPair& p = theme::accent(theme::ACCENT_CYAN);

    graphCard.init(12, 40, 248, 150);
    add(graphCard);
    graphTitle.init(22, 48, 130, 14, T_SMALL_L, theme::TEXT_DIM);
    add(graphTitle);
    graphValue.init(130, 44, 90, 24, T_MEDIUM_R, theme::TEXT);
    add(graphValue);
    graphUnit.init(224, 50, 30, 14, T_SMALL_L, theme::TEXT_DIM);
    add(graphUnit);

    graph.setPosition(14, 70, 244, 116);
    graph.setGraphAreaMargin(4, 4, 4, 4);
    graph.setGraphAreaPadding(2, 0, 0, 2);
    areaPainter.setColor(theme::rgb(p.a));
    area.setPainter(areaPainter);
    area.setAlpha(50);
    area.setBaseline(0);
    graph.addGraphElement(area);
    linePainter.setColor(theme::rgb(p.a));
    line.setPainter(linePainter);
    line.setLineWidth(2);
    graph.addGraphElement(line);
    add(graph);

    table.init(12, 198, 248, ROWS * 27 + 10);
    add(table);
    for (int i = 0; i < ROWS; ++i)
    {
        int16_t y = (int16_t)(198 + 7 + i * 27);
        rowNames[i].init(22, y, 110, 16, T_BODY_L, theme::TEXT_DIM);
        rowNames[i].set(ROW_NAMES[i]);
        add(rowNames[i]);
        rowValues[i].init(110, y, 140, 16, T_BODY_R, theme::TEXT);
        add(rowValues[i]);
    }

    add(topBar);
    topBar.setLeft("DIAGNOSTICS");
    topBar.setDot(p.a, true);
    add(legend);
    legend.setEntry(0, "KNOB", "trend source");
    legend.setEntry(1, "BACK", "menu");
    legend.setEntry(2, "BACK", "hold: stop");
    selectSource(0);
    update();
}

void DiagnosticsScreenView::tearDownScreen()
{
    DiagnosticsScreenViewBase::tearDownScreen();
}

void DiagnosticsScreenView::selectSource(int s)
{
    source = ((s % 3) + 3) % 3;
    graphTitle.set(SOURCE_NAMES[source]);
    graphUnit.set(SOURCE_UNITS[source]);
    graph.clear();
    switch (source)
    {
    case 1: graph.setGraphRange(0, POINTS - 1, 0, 2); graph.setScale(100); break;
    case 2: graph.setGraphRange(0, POINTS - 1, 0, (int)presenter->theModel().settings().travelMm); graph.setScale(10); break;
    default: graph.setGraphRange(0, POINTS - 1, 0, presenter->theModel().settings().maxShaftRpm); graph.setScale(1); break;
    }
    graph.invalidate();
}

void DiagnosticsScreenView::handleTickEvent()
{
    tick++;
    if (tick % 2) return; /* 30 samples per second, 4 s across */
    Model& m = presenter->theModel();
    float v;
    switch (source)
    {
    case 1: v = m.currentA(); break;
    case 2: v = fabsf(m.carriageMm()); break;
    default: v = fabsf(m.shaftRpm()); break;
    }
    graph.addDataPoint(v);
}

void DiagnosticsScreenView::handleInput(const ui::InputEvent& e)
{
    Model& m = presenter->theModel();
    if (e.type == ui::IN_TURN && e.key == ui::KEY_KNOB)
    {
        selectSource(source + (e.value > 0 ? 1 : -1));
        m.feedback(ui::FB_CLICK);
        return;
    }
    if (e.type == ui::IN_RELEASE && e.value == 0 && (e.key == ui::KEY_BACK || e.key == ui::KEY_KNOB))
    {
        m.feedback(ui::FB_CLICK);
        m.goTo(Model::SCR_MENU);
    }
}

static const char* bldcStateName(uint8_t s)
{
    switch (s)
    {
    case ui::BLDC_CALIB: return "calibrating";
    case ui::BLDC_ALIGN: return "aligning";
    case ui::BLDC_RUN: return "running";
    case ui::BLDC_FAULT: return "FAULT";
    default: return "idle";
    }
}

static const char* stepperStateName(uint8_t s)
{
    switch (s)
    {
    case ui::STEP_MOVING: return "moving";
    case ui::STEP_SYNC: return "synced";
    case ui::STEP_STOPPING: return "stopping";
    case ui::STEP_FAULT: return "FAULT";
    default: return "idle";
    }
}

void DiagnosticsScreenView::update()
{
    Model& m = presenter->theModel();
    const ui::Telemetry& t = m.telemetry();
    char buf[80];

    switch (source)
    {
    case 1: graphValue.setFloat(m.currentA(), 2); break;
    case 2: graphValue.setFloat(fabsf(m.carriageMm()), 1); break;
    default: graphValue.setInt((int)(fabsf(m.shaftRpm()) + 0.5f)); break;
    }

    rowValues[0].set(bldcStateName(t.bldcState));
    rowValues[0].setRgb(t.bldcState == ui::BLDC_FAULT ? theme::DANGER : theme::TEXT);
    snprintf(buf, sizeof(buf), "%s  ·  %s", m.modeName(), m.direction() > 0 ? "FWD" : "REV");
    rowValues[1].set(buf);
    snprintf(buf, sizeof(buf), "%d rpm  (set %d)", (int)(m.shaftRpm() + (m.shaftRpm() < 0 ? -0.5f : 0.5f)), (int)(m.setpoint() + 0.5f));
    rowValues[2].set(buf);
    int ia = (int)(fabsf(t.ia) * 100), ib = (int)(fabsf(t.ib) * 100), ic = (int)(fabsf(t.ic) * 100);
    snprintf(buf, sizeof(buf), "%d.%02d  %d.%02d  %d.%02d A", ia / 100 % 100, ia % 100, ib / 100 % 100, ib % 100, ic / 100 % 100, ic % 100);
    rowValues[3].set(buf);
    float rev = (float)t.positionCounts / ui::limits().encoderCpr / ui::limits().gearRatio;
    rowValues[4].setFloat(rev, 2, " shaft rev");
    snprintf(buf, sizeof(buf), "%s  ·  %d.%d mm", stepperStateName(t.stepperState), (int)fabsf(m.carriageMm()), (int)(fabsf(m.carriageMm()) * 10) % 10);
    rowValues[5].set(buf);
    rowValues[5].setRgb(t.stepperState == ui::STEP_FAULT ? theme::DANGER : theme::TEXT);
    snprintf(buf, sizeof(buf), "%d %%  ·  %s", (t.pedalPermille + 5) / 10, t.pedalHomed ? "homed" : "not homed");
    rowValues[6].set(buf);
    if (t.faults == 0) { rowValues[7].set("none"); rowValues[7].setRgb(theme::GREEN); }
    else
    {
        snprintf(buf, sizeof(buf), "0x%03X  %s", (unsigned)t.faults, ui::faultName(t.faults & (~t.faults + 1)));
        rowValues[7].set(buf);
        rowValues[7].setRgb((t.faults & ui::FAULT_CRITICAL) ? theme::DANGER : theme::accent(theme::ACCENT_AMBER).a);
    }
    topBar.setRight(m.opState() == Model::OP_ARMED ? "ARMED  ·  PEDAL LOCKED" : "DISARMED");
}
