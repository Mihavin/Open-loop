#ifndef SETTINGSSCREENVIEW_HPP
#define SETTINGSSCREENVIEW_HPP

#include <gui_generated/settingsscreen_screen/SettingsScreenViewBase.hpp>
#include <gui/settingsscreen_screen/SettingsScreenPresenter.hpp>
#include <gui/common/Widgets.hpp>

/* Settings list: knob scrolls, press edits a value with the knob, press again
   confirms, BACK cancels an edit or returns to the menu. */
class SettingsScreenView : public SettingsScreenViewBase
{
public:
    static const int ITEMS = 11;
    static const int VISIBLE = 7;
    SettingsScreenView();
    virtual ~SettingsScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
    void handleInput(const ui::InputEvent& event);
    void update();

private:
    void formatValue(int item, char* out, int size) const;
    void adjust(int item, int detents);
    void layout();

    Backdrop backdrop;
    TopBar topBar;
    KeyLegend legend;
    GlassPanel highlight;
    Label names[VISIBLE];
    Label values[VISIBLE];
    Label caption;
    int selected;
    int first;      /* first visible item */
    bool editing;
    Model::Settings draft;
    int tick;
};

#endif // SETTINGSSCREENVIEW_HPP
