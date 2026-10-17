#ifndef INTERPSCREENPRESENTER_HPP
#define INTERPSCREENPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

class InterpScreenView;

/* Thin presenter: the model drives the screen through onInput/onUpdate and the
   view reads the model directly through theModel(). */
class InterpScreenPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    InterpScreenPresenter(InterpScreenView& v);
    virtual ~InterpScreenPresenter() {}

    virtual void activate();
    virtual void deactivate();
    virtual void onInput(const ui::InputEvent& event);
    virtual void onUpdate();

    Model& theModel() { return *model; }

private:
    InterpScreenPresenter();
    InterpScreenView& view;
};

#endif // INTERPSCREENPRESENTER_HPP
