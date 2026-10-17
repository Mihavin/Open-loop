#ifndef RUNSCREENVIEW_HPP
#define RUNSCREENVIEW_HPP

#include <gui_generated/runscreen_screen/RunScreenViewBase.hpp>
#include <gui/runscreen_screen/RunScreenPresenter.hpp>
#include <gui/common/Widgets.hpp>

/* The operating screen. Disarmed: mode tabs + setpoint ring + turns and traverse
   cards + arm hint. Arming: the same with the rotor-alignment spinner. Armed: a
   gauge whose meaning follows the mode (speed, brake force, pedal stroke, timer),
   two status cards, turns and traverse cards and the carriage bar. */
class RunScreenView : public RunScreenViewBase
{
public:
    RunScreenView();
    virtual ~RunScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
    void handleInput(const ui::InputEvent& event);
    void update();

private:
    void buildDisarmed();
    void buildArmed();
    void showPane(bool armed);
    void updateTopBar();
    void updateDisarmed();
    void updateArmed();
    void updateTraverse(Label& value, Label& status);
    void updateTurns(Label& value);
    void applyAccent();
    void setLegend();
    void formatTime(char* out, int size, uint32_t ms);

    Backdrop backdrop;
    TopBar topBar;
    KeyLegend legend;
    theme::Accent accent;
    int tick;
    int enterTick;
    float shownFrac;     /* eased arc fraction */
    float shownNeedle;   /* eased needle angle */
    bool armedPane;
    int legendMode;

    /* disarmed pane */
    touchgfx::Container disarmed;
    GlassPanel tabTrack;
    touchgfx::Box tabPill;
    Label tabs[4];
    touchgfx::Circle ringTrack, ringValue, ringInner, ringInnerLine;
    touchgfx::PainterARGB8888 ringTrackPainter, ringInnerPainter, ringInnerLinePainter;
    touchgfx::PainterARGB8888LinearGradient ringValuePainter;
    Label setLabel, setValue, setSub;
    GlassPanel turnsCardD;
    Label turnsTitleD, turnsValueD, turnsHintD;
    GlassPanel travCard;
    Label travTitle, travValue, travUnit, travStatus;
    GlassPanel hintPanel;
    touchgfx::BoxWithBorder hintCap;
    Label hintKey, hintText;

    /* armed pane */
    touchgfx::Container armed;
    touchgfx::Circle gaugeHalo, gaugeTrack, gaugeValue, capOuter, capInner;
    touchgfx::PainterARGB8888 gaugeTrackPainter, capOuterPainter, capInnerPainter;
    touchgfx::PainterARGB8888LinearGradient gaugeValuePainter, gaugeHaloPainter;
    touchgfx::Line needle;
    touchgfx::PainterARGB8888 needlePainter;
    Label gaugeMin, gaugeMax, actValue, actUnit, actSub, actCurrent;
    GlassPanel cardA, cardB;
    Label cardATitle, cardAValue, cardBTitle, cardBValue;
    Bar pedalBar;
    GlassPanel turnsCard, syncCard;
    Label turnsTitle, turnsValue, syncTitle, syncStatus, syncValue, syncUnit;
    touchgfx::Box carriageTrack, carriage;
    Label carMin, carPos, carMax;
};

#endif // RUNSCREENVIEW_HPP
