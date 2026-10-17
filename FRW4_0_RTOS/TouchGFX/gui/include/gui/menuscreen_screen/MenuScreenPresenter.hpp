#ifndef MENUSCREENPRESENTER_HPP
#define MENUSCREENPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

class MenuScreenView;

/* Thin presenter: the model drives the screen through onInput/onUpdate and the
   view reads the model directly through theModel(). */
class MenuScreenPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    MenuScreenPresenter(MenuScreenView& v);
    virtual ~MenuScreenPresenter() {}

    virtual void activate();
    virtual void deactivate();
    virtual void onInput(const ui::InputEvent& event);
    virtual void onUpdate();

    Model& theModel() { return *model; }

private:
    MenuScreenPresenter();
    MenuScreenView& view;
};

#endif // MENUSCREENPRESENTER_HPP
