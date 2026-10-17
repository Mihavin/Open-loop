#ifndef MENUSCREENVIEW_HPP
#define MENUSCREENVIEW_HPP

#include <gui_generated/menuscreen_screen/MenuScreenViewBase.hpp>
#include <gui/menuscreen_screen/MenuScreenPresenter.hpp>
#include <gui/common/Widgets.hpp>

/* Knob-driven wheel menu: the selected item sits in an accent band, the
   neighbours fade out above and below, a dot strip shows the position. */
class MenuScreenView : public MenuScreenViewBase
{
public:
    static const int ITEMS = 8;
    static const int SLOTS = 5; /* visible rows, selected in the middle */
    MenuScreenView();
    virtual ~MenuScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
    void handleInput(const ui::InputEvent& event);
    void update();

private:
    void layout();
    void activate(int item);

    Backdrop backdrop;
    TopBar topBar;
    KeyLegend legend;
    GlassPanel band;
    touchgfx::Box bandTop, bandBottom;
    Label rows[SLOTS];
    Label caption;
    touchgfx::Box dots[ITEMS];
    int selected;
    int slide;      /* pixels of remaining slide animation, signed */
};

#endif // MENUSCREENVIEW_HPP
