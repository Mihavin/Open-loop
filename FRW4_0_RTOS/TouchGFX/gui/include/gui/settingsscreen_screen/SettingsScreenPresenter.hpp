#ifndef SETTINGSSCREENPRESENTER_HPP
#define SETTINGSSCREENPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

class SettingsScreenView;

/* Thin presenter: the model drives the screen through onInput/onUpdate and the
   view reads the model directly through theModel(). */
class SettingsScreenPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    SettingsScreenPresenter(SettingsScreenView& v);
    virtual ~SettingsScreenPresenter() {}

    virtual void activate();
    virtual void deactivate();
    virtual void onInput(const ui::InputEvent& event);
    virtual void onUpdate();

    Model& theModel() { return *model; }

private:
    SettingsScreenPresenter();
    SettingsScreenView& view;
};

#endif // SETTINGSSCREENPRESENTER_HPP
