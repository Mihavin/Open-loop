#ifndef LOADSCREENVIEW_HPP
#define LOADSCREENVIEW_HPP

#include <gui_generated/loadscreen_screen/LoadScreenViewBase.hpp>
#include <gui/loadscreen_screen/LoadScreenPresenter.hpp>

class LoadScreenView : public LoadScreenViewBase
{
public:
    LoadScreenView();
    virtual ~LoadScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
protected:
};

#endif // LOADSCREENVIEW_HPP
