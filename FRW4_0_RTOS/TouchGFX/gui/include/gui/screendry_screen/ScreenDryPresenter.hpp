#ifndef SCREENDRYPRESENTER_HPP
#define SCREENDRYPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class ScreenDryView;

class ScreenDryPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    ScreenDryPresenter(ScreenDryView& v);

    /**
     * The activate function is called automatically when this screen is "switched in"
     * (ie. made active). Initialization logic can be placed here.
     */
    virtual void activate();

    /**
     * The deactivate function is called automatically when this screen is "switched out"
     * (ie. made inactive). Teardown functionality can be placed here.
     */
    virtual void deactivate();

    virtual ~ScreenDryPresenter() {}

private:
    ScreenDryPresenter();

    ScreenDryView& view;
};

#endif // SCREENDRYPRESENTER_HPP
