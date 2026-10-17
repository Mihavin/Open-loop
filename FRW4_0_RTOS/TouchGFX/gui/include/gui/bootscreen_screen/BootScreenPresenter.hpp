#ifndef BOOTSCREENPRESENTER_HPP
#define BOOTSCREENPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

class BootScreenView;

/* Thin presenter: the model drives the screen through onInput/onUpdate and the
   view reads the model directly through theModel(). */
class BootScreenPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    BootScreenPresenter(BootScreenView& v);
    virtual ~BootScreenPresenter() {}

    virtual void activate();
    virtual void deactivate();
    virtual void onInput(const ui::InputEvent& event);
    virtual void onUpdate();

    Model& theModel() { return *model; }

private:
    BootScreenPresenter();
    BootScreenView& view;
};

#endif // BOOTSCREENPRESENTER_HPP
