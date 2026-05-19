#ifndef LOADSCREENPRESENTER_HPP
#define LOADSCREENPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class LoadScreenView;

class LoadScreenPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    LoadScreenPresenter(LoadScreenView& v);

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

    virtual ~LoadScreenPresenter() {}

private:
    LoadScreenPresenter();

    LoadScreenView& view;
};

#endif // LOADSCREENPRESENTER_HPP
