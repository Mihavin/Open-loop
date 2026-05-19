#ifndef SCREENTORQUEVIEW_HPP
#define SCREENTORQUEVIEW_HPP

#include <gui_generated/screentorque_screen/ScreenTorqueViewBase.hpp>
#include <gui/screentorque_screen/ScreenTorquePresenter.hpp>

class ScreenTorqueView : public ScreenTorqueViewBase
{
public:
    ScreenTorqueView();
    virtual ~ScreenTorqueView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
protected:
};

#endif // SCREENTORQUEVIEW_HPP
