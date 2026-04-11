#include <gui/diagnostic_screen/DiagnosticView.hpp>
extern "C" {
#include "motor_foc.h"
}
#include <gui/diagnostic_screen/DiagnosticPresenter.hpp>

DiagnosticPresenter::DiagnosticPresenter(DiagnosticView& v)
    : view(v)
{

}




void DiagnosticPresenter::tick()
{
    // Reduce update rate so you don’t waste CPU (60Hz -> 10Hz)
    static uint8_t div = 0;
    if (++div < 6) return;
    div = 0;

    const float rpm  = MotorFOC_GetSpeedRPM();

    const float ia   = MotorFOC_GetIa();
    const float ib   = MotorFOC_GetIb();
    const float ic   = MotorFOC_GetIc();
    const float isum = ia + ib + ic;
    const float iq   = MotorFOC_GetIq();  // "torque current"

    const MotorFOC_Mode_t  mode  = MotorFOC_GetMode();
    const MotorFOC_State_t state = MotorFOC_GetState();

    view.updateMotorDiag(rpm, ia, ib, ic, isum, iq, mode, state);
}
void DiagnosticPresenter::activate()
{

}

void DiagnosticPresenter::deactivate()
{

}
