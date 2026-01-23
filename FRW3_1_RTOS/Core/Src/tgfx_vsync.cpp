#include <touchgfx/hal/OSWrappers.hpp>

extern "C" void TGFX_SignalVSync(void)
{
    touchgfx::OSWrappers::signalVSync();
}
