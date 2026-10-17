#ifndef FAULTSCREENVIEW_HPP
#define FAULTSCREENVIEW_HPP

#include <gui_generated/faultscreen_screen/FaultScreenViewBase.hpp>
#include <gui/faultscreen_screen/FaultScreenPresenter.hpp>
#include <gui/common/Widgets.hpp>

/* Output is cut. Lists the active faults and whether the cause is still
   present; the knob confirms the clear once it is gone. */
class FaultScreenView : public FaultScreenViewBase
{
public:
    static const int LINES = 5;
    FaultScreenView();
    virtual ~FaultScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
    void handleInput(const ui::InputEvent& event);
    void update();

private:
    Backdrop backdrop;
    TopBar topBar;
    KeyLegend legend;
    touchgfx::Circle badgeRing, badgeFill;
    touchgfx::PainterARGB8888 badgeFillPainter;
    touchgfx::PainterARGB8888LinearGradient badgeRingPainter;
    Label badgeMark, title, subtitle;
    GlassPanel listCard;
    Label lines[LINES];
    GlassPanel causePanel;
    touchgfx::BoxWithBorder causeCap;
    Label causeKey, causeText;
    Label hint;
    int tick;
};

#endif // FAULTSCREENVIEW_HPP
