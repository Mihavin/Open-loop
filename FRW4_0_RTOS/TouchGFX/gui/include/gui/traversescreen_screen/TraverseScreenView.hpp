#ifndef TRAVERSESCREENVIEW_HPP
#define TRAVERSESCREENVIEW_HPP

#include <gui_generated/traversescreen_screen/TraverseScreenViewBase.hpp>
#include <gui/traversescreen_screen/TraverseScreenPresenter.hpp>
#include <gui/common/Widgets.hpp>

/* Traverse sync editor: knob sets rotations per mm, press engages or releases
   the stepper lock to the main motor, the carriage bar shows where it is. */
class TraverseScreenView : public TraverseScreenViewBase
{
public:
    TraverseScreenView();
    virtual ~TraverseScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
    void handleInput(const ui::InputEvent& event);
    void update();

private:
    Backdrop backdrop;
    TopBar topBar;
    KeyLegend legend;
    touchgfx::Circle ringTrack, ringValue;
    touchgfx::PainterARGB8888 ringTrackPainter;
    touchgfx::PainterARGB8888LinearGradient ringValuePainter;
    Label title, value, unit, hint;
    GlassPanel infoCard;
    Label pitchLabel, pitchValue, leadLabel, leadValue, stepsLabel, stepsValue;
    GlassPanel statePanel;
    touchgfx::BoxWithBorder stateCap;
    Label stateKey, stateText;
    touchgfx::Box carriageTrack, carriage;
    Label carMin, carPos, carMax;
    int tick;
    float shownFrac;
};

#endif // TRAVERSESCREENVIEW_HPP
