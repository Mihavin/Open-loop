#ifndef FRONTENDAPPLICATION_HPP
#define FRONTENDAPPLICATION_HPP

#include <gui_generated/common/FrontendApplicationBase.hpp>

class FrontendHeap;

using namespace touchgfx;

class FrontendApplication : public FrontendApplicationBase
{
public:
    FrontendApplication(Model& m, FrontendHeap& heap);
    virtual ~FrontendApplication() { }

    virtual void handleTickEvent();
    /* Keyboard input only exists in the PC simulator; on the target the
       operator controls arrive through ui::popInput(). */
    virtual void handleKeyEvent(uint8_t c);
private:
};

#endif // FRONTENDAPPLICATION_HPP
