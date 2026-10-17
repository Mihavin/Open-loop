#include <gui/settingsscreen_screen/SettingsScreenView.hpp>
#include <math.h>
#include <stdio.h>

using namespace touchgfx;

static const char* const ITEM_NAMES[SettingsScreenView::ITEMS] = {
    "Lead screw lead", "Carriage travel", "Pedal full travel", "Max shaft speed",
    "Knob step", "Dry run time", "Buzzer", "Vibration", "Backlight"
};
static const char* const ITEM_HELP[SettingsScreenView::ITEMS] = {
    "mm the carriage moves per stepper turn", "usable travel before sync releases",
    "pedal encoder counts at full press", "limit for the speed setpoint",
    "shaft rpm per knob detent", "minutes the dry run turns before it beeps", "key clicks and alerts",
    "vibration on arm, warn and fault", "display brightness"
};
static const int16_t LIST_Y = 44;
static const int16_t ROW_H = 46;

SettingsScreenView::SettingsScreenView() : selected(0), first(0), editing(false), tick(0) {}

void SettingsScreenView::setupScreen()
{
    SettingsScreenViewBase::setupScreen();
    add(backdrop);
    backdrop.setAccent(theme::ACCENT_PURPLE);

    highlight.init(12, LIST_Y, 248, ROW_H - 4);
    highlight.setAccent(theme::accent(theme::ACCENT_PURPLE).a, 40);
    add(highlight);
    for (int i = 0; i < VISIBLE; ++i)
    {
        int16_t y = (int16_t)(LIST_Y + i * ROW_H);
        names[i].init(22, (int16_t)(y + 7), 150, 16, T_BODY_L, theme::TEXT);
        add(names[i]);
        values[i].init(120, (int16_t)(y + 5), 130, 24, T_TITLE_R, theme::TEXT);
        add(values[i]);
    }
    caption.init(0, 410, theme::SCREEN_W, 16, T_SMALL_C, theme::TEXT_DIM);
    add(caption);

    add(topBar);
    topBar.setLeft("SETTINGS");
    topBar.setDot(theme::accent(theme::ACCENT_PURPLE).a, true);
    add(legend);
    draft = presenter->theModel().settings();
    layout();
    update();
}

void SettingsScreenView::tearDownScreen()
{
    SettingsScreenViewBase::tearDownScreen();
}

void SettingsScreenView::formatValue(int item, char* out, int size) const
{
    const Model::Settings& s = draft;
    switch (item)
    {
    case 0: snprintf(out, size, "%d.%d mm", (int)s.leadMm, (int)(s.leadMm * 10 + 0.5f) % 10); break;
    case 1: snprintf(out, size, "%d mm", (int)s.travelMm); break;
    case 2: snprintf(out, size, "%u counts", (unsigned)s.pedalFullCounts); break;
    case 3: snprintf(out, size, "%d rpm", s.maxShaftRpm); break;
    case 4: snprintf(out, size, "%d rpm", s.rpmPerDetent); break;
    case 5: snprintf(out, size, "%d min", s.dryMinutes); break;
    case 6: snprintf(out, size, "%s", s.buzzer ? "on" : "off"); break;
    case 7: snprintf(out, size, "%s", s.vibration ? "on" : "off"); break;
    default: snprintf(out, size, "%d %%", s.backlightPercent); break;
    }
}

void SettingsScreenView::adjust(int item, int d)
{
    static const int knobSteps[5] = { 1, 2, 5, 10, 20 };
    Model::Settings& s = draft;
    switch (item)
    {
    case 0: s.leadMm = clampFloat(floorf(s.leadMm * 2.0f + 0.5f + (float)d) / 2.0f, 0.5f, 10.0f); break;
    case 1: s.travelMm = clampFloat(s.travelMm + (float)d, 5.0f, 300.0f); break;
    case 2: s.pedalFullCounts = (uint16_t)clampInt((int)s.pedalFullCounts + 10 * d, 20, 2000); break;
    case 3: s.maxShaftRpm = clampInt(s.maxShaftRpm + 50 * d, 50, 1000); break;
    case 4:
    {
        int idx = 0;
        for (int i = 0; i < 5; ++i) if (knobSteps[i] == s.rpmPerDetent) idx = i;
        s.rpmPerDetent = knobSteps[clampInt(idx + (d > 0 ? 1 : -1), 0, 4)];
        break;
    }
    case 5: s.dryMinutes = clampInt(s.dryMinutes + d, 1, 240); break;
    case 6: s.buzzer = !s.buzzer; break;
    case 7: s.vibration = !s.vibration; break;
    default:
        s.backlightPercent = clampInt(s.backlightPercent + 5 * d, 10, 100);
        ui::setBacklight((uint8_t)s.backlightPercent); /* live preview */
        break;
    }
}

void SettingsScreenView::layout()
{
    if (selected < first) first = selected;
    if (selected >= first + VISIBLE) first = selected - VISIBLE + 1;
    char buf[32];
    for (int i = 0; i < VISIBLE; ++i)
    {
        int item = first + i;
        bool show = item < ITEMS;
        names[i].setVisible(show);
        values[i].setVisible(show);
        if (!show) { names[i].invalidate(); values[i].invalidate(); continue; }
        names[i].set(ITEM_NAMES[item]);
        formatValue(item, buf, sizeof(buf));
        if (editing && item == selected)
        {
            char edit[40];
            snprintf(edit, sizeof(edit), "<  %s  >", buf);
            values[i].set(edit);
            values[i].setRgb(theme::accent(theme::ACCENT_PURPLE).a);
        }
        else
        {
            values[i].set(buf);
            values[i].setRgb(item == selected ? theme::TEXT : theme::TEXT_DIM);
        }
        names[i].setAlpha(item == selected ? 255 : 170);
        names[i].invalidate();
    }
    int16_t hy = (int16_t)(LIST_Y + (selected - first) * ROW_H);
    if (highlight.getY() != hy)
    {
        highlight.invalidate();
        highlight.setY(hy);
        highlight.invalidate();
    }
    caption.set(ITEM_HELP[selected]);
    legend.setEntry(0, "KNOB", editing ? "change" : "select");
    legend.setEntry(1, "PRESS", editing ? "confirm" : "edit");
    legend.setEntry(2, "BACK", editing ? "cancel" : "menu");
    legend.setEntry(3, "BACK", "hold: stop");
}

void SettingsScreenView::handleTickEvent()
{
    tick++;
    if (editing) highlight.setFill((uint8_t)(50 + 24 * sinf((float)tick * 0.15f)));
}

void SettingsScreenView::handleInput(const ui::InputEvent& e)
{
    Model& m = presenter->theModel();
    if (e.type == ui::IN_TURN && e.key == ui::KEY_KNOB)
    {
        if (editing) adjust(selected, e.value);
        else selected = clampInt(selected + (e.value > 0 ? 1 : -1), 0, ITEMS - 1);
        m.feedback(ui::FB_CLICK);
        layout();
        return;
    }
    if (e.type != ui::IN_RELEASE || e.value != 0) return;
    if (e.key == ui::KEY_KNOB)
    {
        if (editing)
        {
            m.settings() = draft;
            m.applySettings();
            draft = m.settings();
            editing = false;
            highlight.setAccent(theme::accent(theme::ACCENT_PURPLE).a, 40);
            m.feedback(ui::FB_CONFIRM);
        }
        else
        {
            editing = true;
            m.feedback(ui::FB_CLICK);
        }
        layout();
    }
    else if (e.key == ui::KEY_BACK)
    {
        if (editing)
        {
            draft = m.settings();
            ui::setBacklight((uint8_t)draft.backlightPercent);
            editing = false;
            highlight.setAccent(theme::accent(theme::ACCENT_PURPLE).a, 40);
            m.feedback(ui::FB_CLICK);
            layout();
        }
        else { m.feedback(ui::FB_CLICK); m.goTo(Model::SCR_MENU); }
    }
}

void SettingsScreenView::update()
{
    Model& m = presenter->theModel();
    topBar.setRight(m.opState() == Model::OP_ARMED ? "ARMED  ·  PEDAL LOCKED" : "DISARMED");
}
