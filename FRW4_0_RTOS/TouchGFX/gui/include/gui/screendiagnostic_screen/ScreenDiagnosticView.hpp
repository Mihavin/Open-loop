#ifndef SCREENDIAGNOSTICVIEW_HPP
#define SCREENDIAGNOSTICVIEW_HPP

#include <gui_generated/screendiagnostic_screen/ScreenDiagnosticViewBase.hpp>
#include <gui/screendiagnostic_screen/ScreenDiagnosticPresenter.hpp>

class ScreenDiagnosticView : public ScreenDiagnosticViewBase
{
public:
    ScreenDiagnosticView();
    virtual ~ScreenDiagnosticView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
protected:
};

#endif // SCREENDIAGNOSTICVIEW_HPP
