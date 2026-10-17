#ifndef FRONTENDAPPLICATION_HPP
#define FRONTENDAPPLICATION_HPP

#include <gui_generated/common/FrontendApplicationBase.hpp>

class FrontendHeap;

using namespace touchgfx;

class FrontendApplication : public FrontendApplicationBase
{
public:
    /* Screen ids, in the order of Model::ScreenId. */
    enum ScreenIndex { SCREEN_BOOT = 0, SCREEN_RUN, SCREEN_MENU, SCREEN_TRAVERSE, SCREEN_INTERP, SCREEN_DIAGNOSTICS, SCREEN_SETTINGS, SCREEN_FAULT };

    FrontendApplication(Model& m, FrontendHeap& heap);
    virtual ~FrontendApplication() { }

    virtual void handleTickEvent();
    /* Keyboard input only exists in the PC simulator; on the target the
       operator controls arrive through ui::popInput(). */
    virtual void handleKeyEvent(uint8_t c);

    /* Switch screens at the end of the current tick. All transitions live here,
       not in the generated base: the Designer only emits goto methods for the
       interactions drawn in the .touchgfx file, and this UI has none. */
    void showScreen(ScreenIndex screen);

private:
    void showScreenImpl();
    touchgfx::Callback<FrontendApplication> screenCallback;
    ScreenIndex pendingScreen;
};

#endif // FRONTENDAPPLICATION_HPP
