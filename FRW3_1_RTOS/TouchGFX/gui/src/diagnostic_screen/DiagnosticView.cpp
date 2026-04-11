#include <gui/diagnostic_screen/DiagnosticView.hpp>
#include <touchgfx/Unicode.hpp>

extern "C" {
#include "motor_foc.h"
}


static const char* modeToStr(MotorFOC_Mode_t m)
{
    switch (m)
    {
    case MOTOR_FOC_MODE_TORQUE:    return "TORQUE";
    case MOTOR_FOC_MODE_SPEED:     return "SPEED";
    case MOTOR_FOC_MODE_POSITION:  return "POSITION";
    default:                       return "?";
    }
}

static const char* stateToStr(MotorFOC_State_t s)
{
    switch (s)
    {
    case MOTOR_FOC_STATE_IDLE:  return "IDLE";
    case MOTOR_FOC_STATE_CALIB: return "CALIB";
    case MOTOR_FOC_STATE_ALIGN: return "ALIGN";
    case MOTOR_FOC_STATE_RUN:   return "RUN";
    case MOTOR_FOC_STATE_FAULT: return "FAULT";
    default:                    return "?";
    }
}

void DiagnosticView::updateMotorDiag(float rpm,
                                      float ia, float ib, float ic, float isum,
                                      float iq,
                                      MotorFOC_Mode_t mode,
                                      MotorFOC_State_t state)
{
    // Floats: use snprintfFloat (TouchGFX supports it)
    touchgfx::Unicode::snprintfFloat(txtRpmBuffer,  TXTRPM_SIZE,  "%.0f",  rpm);
    touchgfx::Unicode::snprintfFloat(txtIaBuffer,   TXTIA_SIZE,   "%+.2f", ia);
    touchgfx::Unicode::snprintfFloat(txtIbBuffer,   TXTIB_SIZE,   "%+.2f", ib);
    touchgfx::Unicode::snprintfFloat(txtIcBuffer,   TXTIC_SIZE,   "%+.2f", ic);
    touchgfx::Unicode::snprintfFloat(txtIsumBuffer, TXTISUM_SIZE, "%+.2f", isum);
    touchgfx::Unicode::snprintfFloat(txtIqBuffer,   TXTIQ_SIZE,   "%+.2f", iq);

    // Strings: use snprintf (char* format is fine)
    touchgfx::Unicode::strncpy(txtModeBuffer,  modeToStr(mode),  TXTMODE_SIZE);
    txtModeBuffer[TXTMODE_SIZE - 1] = 0;

    touchgfx::Unicode::strncpy(txtStateBuffer, stateToStr(state), TXTSTATE_SIZE);
    txtStateBuffer[TXTSTATE_SIZE - 1] = 0;


    // Force redraw of updated widgets
    txtRpm.invalidate();
    txtIa.invalidate();
    txtIb.invalidate();
    txtIc.invalidate();
    txtIsum.invalidate();
    txtIq.invalidate();
    txtMode.invalidate();
    txtState.invalidate();
}

DiagnosticView::DiagnosticView()
{

}

void DiagnosticView::setupScreen()
{
    DiagnosticViewBase::setupScreen();
}

void DiagnosticView::tearDownScreen()
{
    DiagnosticViewBase::tearDownScreen();
}
