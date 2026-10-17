#ifndef INTERPSCREENVIEW_HPP
#define INTERPSCREENVIEW_HPP

#include <gui_generated/interpscreen_screen/InterpScreenViewBase.hpp>
#include <gui/interpscreen_screen/InterpScreenPresenter.hpp>
#include <gui/common/Widgets.hpp>

/* Interpolation setup: a dial showing the spindle angle with the snap detents,
   and three parameters edited with the knob (press moves between them):
   stroke (shaft revolutions per full pedal travel), snap pitch, hold torque. */
class InterpScreenView : public InterpScreenViewBase
{
public:
    InterpScreenView();
    virtual ~InterpScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
    void handleInput(const ui::InputEvent& event);
    void update();

private:
    static const int PARAMS = 3;
    static const int MAX_TICKS = 24;
    void layoutTicks();
    void layoutRows();

    Backdrop backdrop;
    TopBar topBar;
    KeyLegend legend;
    touchgfx::Circle dialTrack, dialArc, pointer;
    touchgfx::PainterARGB8888 dialTrackPainter, pointerPainter, tickPainter;
    touchgfx::PainterARGB8888LinearGradient dialArcPainter;
    touchgfx::Line ticks[MAX_TICKS];
    Label angleValue, angleUnit, angleHint;
    GlassPanel rowPanel[PARAMS];
    Label rowName[PARAMS], rowValue[PARAMS];
    Label caption;
    int selected;
    int shownTicks;
    int tick;
    float shownAngle;
};

#endif // INTERPSCREENVIEW_HPP
