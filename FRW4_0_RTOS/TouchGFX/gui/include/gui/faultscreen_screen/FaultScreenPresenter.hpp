#ifndef FAULTSCREENPRESENTER_HPP
#define FAULTSCREENPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

class FaultScreenView;

/* Thin presenter: the model drives the screen through onInput/onUpdate and the
   view reads the model directly through theModel(). */
class FaultScreenPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    FaultScreenPresenter(FaultScreenView& v);
    virtual ~FaultScreenPresenter() {}

    virtual void activate();
    virtual void deactivate();
    virtual void onInput(const ui::InputEvent& event);
    virtual void onUpdate();

    Model& theModel() { return *model; }

private:
    FaultScreenPresenter();
    FaultScreenView& view;
};

#endif // FAULTSCREENPRESENTER_HPP
