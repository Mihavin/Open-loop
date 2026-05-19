#ifndef SCREENRPMPRESENTER_HPP
#define SCREENRPMPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class ScreenRPMView;

class ScreenRPMPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    ScreenRPMPresenter(ScreenRPMView& v);

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

    virtual ~ScreenRPMPresenter() {}

private:
    ScreenRPMPresenter();

    ScreenRPMView& view;
};

#endif // SCREENRPMPRESENTER_HPP
