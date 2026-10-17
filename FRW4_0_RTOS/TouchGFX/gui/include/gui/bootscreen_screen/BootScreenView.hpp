#ifndef BOOTSCREENVIEW_HPP
#define BOOTSCREENVIEW_HPP

#include <gui_generated/bootscreen_screen/BootScreenViewBase.hpp>
#include <gui/bootscreen_screen/BootScreenPresenter.hpp>
#include <gui/common/Widgets.hpp>

/* Power-up self-test: brand, spinner, five checks revealed one by one, progress bar. */
class BootScreenView : public BootScreenViewBase
{
public:
    static const int CHECKS = 5;
    BootScreenView();
    virtual ~BootScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleTickEvent();
    void handleInput(const ui::InputEvent& event);
    void update();

protected:
    Backdrop backdrop;
    Label brand, product, firmware, stage;
    touchgfx::Circle spinnerTrack, spinner;
    touchgfx::PainterARGB8888 spinnerTrackPainter;
    touchgfx::PainterARGB8888LinearGradient spinnerPainter;
    GlassPanel panel;
    Label checkNames[CHECKS];
    Label checkStates[CHECKS];
    Bar progress;
    int tick;
};

#endif // BOOTSCREENVIEW_HPP
