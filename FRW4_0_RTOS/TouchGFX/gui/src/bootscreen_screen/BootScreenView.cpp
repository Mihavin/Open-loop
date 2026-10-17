#include <gui/bootscreen_screen/BootScreenView.hpp>

using namespace touchgfx;

static const char* const CHECK_NAMES[BootScreenView::CHECKS] = {
    "Motor driver", "Current sensing", "Stepper driver", "Pedal encoder", "Task watchdog"
};

BootScreenView::BootScreenView() : tick(0) {}

void BootScreenView::setupScreen()
{
    BootScreenViewBase::setupScreen();
    add(backdrop);

    brand.init(0, 118, 272, 40, T_LARGE_C, theme::TEXT);
    brand.set("FRW4");
    add(brand);
    product.init(0, 160, 272, 20, T_TITLE_C, theme::TEXT, 200);
    product.set("Rod wrapper");
    add(product);
    firmware.init(0, 182, 272, 16, T_SMALL_C, theme::TEXT_DIM);
    firmware.set("Firmware 4.0  ·  FOC + traverse");
    add(firmware);

    spinnerTrackPainter.setColor(theme::rgb(theme::WHITE));
    setupArc(spinnerTrack, spinnerTrackPainter, 136, 72, 22, 4, 0, 360);
    spinnerTrack.setAlpha(30);
    add(spinnerTrack);
    setupArc(spinner, spinnerPainter, 136, 72, 22, 4, 0, 100);
    setGradientArc(spinner, spinnerPainter, theme::ACCENT_AMBER);
    add(spinner);

    panel.init(16, 222, 240, 5 * 26 + 16);
    add(panel);
    for (int i = 0; i < CHECKS; ++i)
    {
        int16_t y = (int16_t)(222 + 8 + i * 26);
        checkNames[i].init(30, y, 150, 18, T_BODY_L, theme::TEXT, 210);
        checkNames[i].set(CHECK_NAMES[i]);
        checkNames[i].setVisible(false);
        add(checkNames[i]);
        checkStates[i].init(160, y, 82, 18, T_BODY_R, theme::TEXT_DIM);
        checkStates[i].setVisible(false);
        add(checkStates[i]);
    }

    progress.init(16, 392, 240, 4);
    progress.setAccent(theme::accent(theme::ACCENT_AMBER).a);
    add(progress);
    stage.init(0, 404, 272, 16, T_SMALL_C, theme::TEXT_DIM);
    stage.set("Self-test");
    add(stage);
}

void BootScreenView::tearDownScreen()
{
    BootScreenViewBase::tearDownScreen();
}

void BootScreenView::handleTickEvent()
{
    tick++;
    int start = (tick * 6) % 360;
    spinner.updateArc(start, start + 100);
}

void BootScreenView::handleInput(const ui::InputEvent&) {}

void BootScreenView::update()
{
    const Model& m = presenter->theModel();
    const ui::Telemetry& t = m.telemetry();
    uint32_t elapsed = m.bootElapsedMs();
    progress.setPermille((int)(elapsed * 1000 / 2200));

    bool ok[CHECKS];
    const char* txt[CHECKS];
    ok[0] = !(t.faults & (ui::FAULT_BLDC_DRIVER | ui::FAULT_SPI));
    txt[0] = ok[0] ? "OK" : "FAULT";
    ok[1] = !(t.faults & (ui::FAULT_ADC | ui::FAULT_CURRENT)) && t.bldcState != ui::BLDC_CALIB;
    txt[1] = ok[1] ? "OK" : (t.bldcState == ui::BLDC_CALIB ? "calibrating" : "FAULT");
    ok[2] = !(t.faults & ui::FAULT_STEPPER_DRIVER);
    txt[2] = ok[2] ? "OK" : "FAULT";
    ok[3] = true;
    txt[3] = t.pedalHomed ? "homed" : "not homed";
    ok[4] = !(t.faults & ui::FAULT_CPU);
    txt[4] = ok[4] ? "OK" : "FAULT";

    for (int i = 0; i < CHECKS; ++i)
    {
        bool show = elapsed >= (uint32_t)(250 + i * 300);
        if (show != checkNames[i].isVisible())
        {
            checkNames[i].setVisible(show);
            checkNames[i].invalidate();
            checkStates[i].setVisible(show);
            checkStates[i].invalidate();
        }
        if (!show) continue;
        checkStates[i].set(txt[i]);
        uint32_t c = !ok[i] ? theme::DANGER : (i == 3 && !t.pedalHomed) ? theme::accent(theme::ACCENT_AMBER).a : theme::GREEN;
        checkStates[i].setRgb(c);
    }
    stage.set(elapsed < 1600 ? "Self-test" : (t.faults & ui::FAULT_CRITICAL) ? "Fault found" : "Ready");
}
