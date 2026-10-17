#ifndef RUNSCREENPRESENTER_HPP
#define RUNSCREENPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

class RunScreenView;

/* Thin presenter: the model drives the screen through onInput/onUpdate and the
   view reads the model directly through theModel(). */
class RunScreenPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    RunScreenPresenter(RunScreenView& v);
    virtual ~RunScreenPresenter() {}

    virtual void activate();
    virtual void deactivate();
    virtual void onInput(const ui::InputEvent& event);
    virtual void onUpdate();

    Model& theModel() { return *model; }

private:
    RunScreenPresenter();
    RunScreenView& view;
};

#endif // RUNSCREENPRESENTER_HPP
