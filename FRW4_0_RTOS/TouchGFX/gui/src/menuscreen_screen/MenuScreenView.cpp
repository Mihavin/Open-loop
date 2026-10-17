#include <gui/menuscreen_screen/MenuScreenView.hpp>
#include <stdio.h>
#include <math.h>

using namespace touchgfx;

static const char* const ITEM_NAMES[MenuScreenView::ITEMS] = {
    "Run  ·  RPM", "Run  ·  Torque", "Interpolation", "Dry run", "Traverse sync", "Reset turns", "Diagnostics", "Settings"
};
static const int16_t CENTER_Y = 236;
static const int16_t ROW_H = 54;
static const int16_t BAND_H = 74;

MenuScreenView::MenuScreenView() : selected(0), slide(0) {}

void MenuScreenView::setupScreen()
{
    MenuScreenViewBase::setupScreen();
    add(backdrop);
    backdrop.setAccent(theme::ACCENT_BLUE);

    const theme::AccentPair& p = theme::accent(theme::ACCENT_BLUE);
    band.init(12, CENTER_Y - BAND_H / 2, 248, BAND_H);
    band.setFill(26);
    add(band);
    bandTop.setPosition(12, CENTER_Y - BAND_H / 2, 248, 2);
    bandTop.setColor(theme::rgb(p.a));
    add(bandTop);
    bandBottom.setPosition(12, CENTER_Y + BAND_H / 2 - 2, 248, 2);
    bandBottom.setColor(theme::rgb(p.b));
    add(bandBottom);

    for (int i = 0; i < SLOTS; ++i)
    {
        rows[i].init(0, 0, theme::SCREEN_W, 36, T_MEDIUM_C, theme::TEXT);
        add(rows[i]);
    }
    caption.init(0, CENTER_Y + 10, theme::SCREEN_W, 16, T_SMALL_C, theme::TEXT_DIM);
    add(caption);

    for (int i = 0; i < ITEMS; ++i)
    {
        dots[i].setPosition((int16_t)(136 - (ITEMS * 10) / 2 + i * 10), 418, 5, 5);
        dots[i].setColor(theme::rgb(theme::WHITE));
        dots[i].setAlpha(60);
        add(dots[i]);
    }

    add(topBar);
    topBar.setLeft("MENU");
    topBar.setDot(theme::accent(theme::ACCENT_BLUE).a, true);
    add(legend);
    legend.setEntry(0, "KNOB", "scroll");
    legend.setEntry(1, "PRESS", "open");
    legend.setEntry(2, "BACK", "run screen");
    legend.setEntry(3, "BACK", "hold: stop");

    Model& m = presenter->theModel();
    selected = (int)m.opMode(); /* the current run mode is the natural starting point */
    layout();
    update();
}

void MenuScreenView::tearDownScreen()
{
    MenuScreenViewBase::tearDownScreen();
}

void MenuScreenView::layout()
{
    const theme::AccentPair& p = theme::accent(theme::ACCENT_BLUE);
    for (int s = 0; s < SLOTS; ++s)
    {
        int offset = s - SLOTS / 2;
        int item = selected + offset;
        Label& l = rows[s];
        l.invalidate();
        if (item < 0 || item >= ITEMS)
        {
            l.setVisible(false);
            continue;
        }
        l.setVisible(true);
        int16_t y = (int16_t)(CENTER_Y + offset * ROW_H + slide);
        if (offset == 0)
        {
            l.setTypo(T_LARGE_C);
            l.setPosition(0, (int16_t)(y - 30), theme::SCREEN_W, 40);
            l.setAlpha(255);
            l.setRgb(theme::TEXT);
        }
        else
        {
            l.setTypo(offset == -1 || offset == 1 ? T_MEDIUM_C : T_BODY_C);
            l.setPosition(0, (int16_t)(y - 14), theme::SCREEN_W, 30);
            l.setAlpha(offset == -1 || offset == 1 ? 150 : 80);
            l.setRgb(theme::TEXT);
        }
        l.set(ITEM_NAMES[item]);
        l.invalidate();
    }
    for (int i = 0; i < ITEMS; ++i)
    {
        dots[i].invalidate();
        bool on = i == selected;
        dots[i].setPosition((int16_t)(136 - (ITEMS * 10 + 10) / 2 + i * 10), 418, on ? 15 : 5, 5);
        dots[i].setColor(theme::rgb(on ? p.a : theme::WHITE));
        dots[i].setAlpha(on ? 255 : 60);
        dots[i].invalidate();
    }
}

void MenuScreenView::handleTickEvent()
{
    if (slide != 0)
    {
        int step = slide > 0 ? (slide + 2) / 3 : (slide - 2) / 3;
        if (step == 0) step = slide > 0 ? 1 : -1;
        slide -= step;
        layout();
    }
}

void MenuScreenView::activate(int item)
{
    Model& m = presenter->theModel();
    switch (item)
    {
    case 0: case 1: case 3:
        if (m.selectMode((Model::OpMode)item)) m.goTo(Model::SCR_RUN);
        break;
    case 2: m.feedback(ui::FB_CLICK); m.goTo(Model::SCR_INTERP); break;
    case 4: m.feedback(ui::FB_CLICK); m.goTo(Model::SCR_TRAVERSE); break;
    case 5: m.resetTurns(); m.goTo(Model::SCR_RUN); break;
    case 6: m.feedback(ui::FB_CLICK); m.goTo(Model::SCR_DIAGNOSTICS); break;
    default: m.feedback(ui::FB_CLICK); m.goTo(Model::SCR_SETTINGS); break;
    }
}

void MenuScreenView::handleInput(const ui::InputEvent& e)
{
    Model& m = presenter->theModel();
    if (e.type == ui::IN_TURN && e.key == ui::KEY_KNOB)
    {
        int next = clampInt(selected + (e.value > 0 ? 1 : -1), 0, ITEMS - 1);
        if (next != selected)
        {
            slide = (next > selected) ? ROW_H : -ROW_H;
            selected = next;
            m.feedback(ui::FB_CLICK);
            layout();
        }
        return;
    }
    if (e.type == ui::IN_RELEASE && e.value == 0)
    {
        if (e.key == ui::KEY_KNOB) activate(selected);
        else if (e.key == ui::KEY_BACK) { m.feedback(ui::FB_CLICK); m.goTo(Model::SCR_RUN); }
    }
}

void MenuScreenView::update()
{
    Model& m = presenter->theModel();
    char buf[80];
    switch (selected)
    {
    case 0: caption.set((int)m.opMode() == selected ? "current mode  ·  press to run" : "pedal sets the speed"); break;
    case 1: caption.set((int)m.opMode() == selected ? "current mode  ·  press to run" : "pedal sets the braking force"); break;
    case 2:
        snprintf(buf, sizeof(buf), "stroke %d.%d rev  ·  snap %d°  ·  press to open", (int)m.setpointForMode(Model::OM_INTERP),
                 (int)(m.setpointForMode(Model::OM_INTERP) * 10 + 0.5f) % 10, m.settings().snapDeg);
        caption.set(buf);
        break;
    case 3:
        snprintf(buf, sizeof(buf), "%d rpm for %d min, then a beep", (int)(m.setpointForMode(Model::OM_DRY) + 0.5f), m.settings().dryMinutes);
        caption.set(buf);
        break;
    case 4:
        snprintf(buf, sizeof(buf), "%d.%02d rot/mm  ·  %s  ·  press to open", (int)m.settings().rotPerMm,
                 (int)(m.settings().rotPerMm * 100) % 100, m.traverseEngaged() ? "locked" : "off");
        caption.set(buf);
        break;
    case 5:
        snprintf(buf, sizeof(buf), "counter at %d.%d turns  ·  press to zero", (int)m.turnsCount(), (int)(fabsf(m.turnsCount()) * 10 + 0.5f) % 10);
        caption.set(buf);
        break;
    case 6: caption.set("live trends, currents and faults"); break;
    default: caption.set("lead screw, travel, pedal, dry time, sounds"); break;
    }
    topBar.setRight(m.opState() == Model::OP_ARMED ? "ARMED  ·  PEDAL LOCKED" : "DISARMED");
}
