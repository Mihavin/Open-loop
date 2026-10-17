#ifndef DIAGNOSTICSSCREENVIEW_HPP
#define DIAGNOSTICSSCREENVIEW_HPP

#include <gui_generated/diagnosticsscreen_screen/DiagnosticsScreenViewBase.hpp>
#include <gui/diagnosticsscreen_screen/DiagnosticsScreenPresenter.hpp>
#include <gui/common/Widgets.hpp>
#include <touchgfx/widgets/graph/GraphScroll.hpp>
#include <touchgfx/widgets/graph/GraphElements.hpp>

/* Live trend (knob picks speed, current or carriage) over a table of states. */
class DiagnosticsScreenView : public DiagnosticsScreenViewBase
{
public:
    static const int POINTS = 120;
    static const int ROWS = 8;
    DiagnosticsScreenView();
    virtual ~DiagnosticsScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
    void handleInput(const ui::InputEvent& event);
    void update();

private:
    void selectSource(int source);

    Backdrop backdrop;
    TopBar topBar;
    KeyLegend legend;
    GlassPanel graphCard;
    Label graphTitle, graphValue, graphUnit;
    touchgfx::GraphScroll<POINTS> graph;
    touchgfx::GraphElementArea area;
    touchgfx::GraphElementLine line;
    touchgfx::PainterARGB8888 areaPainter, linePainter;
    GlassPanel table;
    Label rowNames[ROWS];
    Label rowValues[ROWS];
    int source;
    int tick;
};

#endif // DIAGNOSTICSSCREENVIEW_HPP
