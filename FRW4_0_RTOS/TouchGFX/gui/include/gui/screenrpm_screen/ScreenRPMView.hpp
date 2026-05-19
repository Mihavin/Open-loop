#ifndef SCREENRPMVIEW_HPP
#define SCREENRPMVIEW_HPP

#include <gui_generated/screenrpm_screen/ScreenRPMViewBase.hpp>
#include <gui/screenrpm_screen/ScreenRPMPresenter.hpp>

class ScreenRPMView : public ScreenRPMViewBase
{
public:
    ScreenRPMView();
    virtual ~ScreenRPMView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
protected:
};

#endif // SCREENRPMVIEW_HPP
