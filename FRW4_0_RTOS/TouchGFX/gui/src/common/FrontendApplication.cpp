#include <gui/common/FrontendApplication.hpp>

#ifdef SIMULATOR
namespace ui
{
void simulatorKey(uint8_t c);
void simulatorFrameDone();
}
#endif

FrontendApplication::FrontendApplication(Model& m, FrontendHeap& heap)
    : FrontendApplicationBase(m, heap)
{
}

void FrontendApplication::handleTickEvent()
{
    model.tick();
    FrontendApplicationBase::handleTickEvent();
#ifdef SIMULATOR
    ui::simulatorFrameDone();
#endif
}

void FrontendApplication::handleKeyEvent(uint8_t c)
{
#ifdef SIMULATOR
    ui::simulatorKey(c);
#else
    (void)c;
#endif
}
