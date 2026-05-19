#ifndef SCREENDRYVIEW_HPP
#define SCREENDRYVIEW_HPP

#include <gui_generated/screendry_screen/ScreenDryViewBase.hpp>
#include <gui/screendry_screen/ScreenDryPresenter.hpp>

class ScreenDryView : public ScreenDryViewBase
{
public:
    ScreenDryView();
    virtual ~ScreenDryView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
protected:
};

#endif // SCREENDRYVIEW_HPP
