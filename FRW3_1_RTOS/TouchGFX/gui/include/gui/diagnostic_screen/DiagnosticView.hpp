#ifndef DIAGNOSTICVIEW_HPP
#define DIAGNOSTICVIEW_HPP

#include <gui_generated/diagnostic_screen/DiagnosticViewBase.hpp>
#include <gui/diagnostic_screen/DiagnosticPresenter.hpp>

extern "C" {
#include "motor_foc.h"
}
// gui/include/gui/diagnostics_screen/DiagnosticsView.hpp
class DiagnosticView : public DiagnosticViewBase
{
public:
    DiagnosticView();
    virtual ~DiagnosticView() {}

    void updateMotorDiag(float rpm,
                         float ia, float ib, float ic, float isum,
                         float iq,
                         MotorFOC_Mode_t mode,
                         MotorFOC_State_t state);

    virtual void setupScreen();
    virtual void tearDownScreen();
};


#endif // DIAGNOSTICVIEW_HPP
