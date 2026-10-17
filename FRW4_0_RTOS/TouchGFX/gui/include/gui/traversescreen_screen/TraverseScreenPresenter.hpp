#ifndef TRAVERSESCREENPRESENTER_HPP
#define TRAVERSESCREENPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

class TraverseScreenView;

/* Thin presenter: the model drives the screen through onInput/onUpdate and the
   view reads the model directly through theModel(). */
class TraverseScreenPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    TraverseScreenPresenter(TraverseScreenView& v);
    virtual ~TraverseScreenPresenter() {}

    virtual void activate();
    virtual void deactivate();
    virtual void onInput(const ui::InputEvent& event);
    virtual void onUpdate();

    Model& theModel() { return *model; }

private:
    TraverseScreenPresenter();
    TraverseScreenView& view;
};

#endif // TRAVERSESCREENPRESENTER_HPP
