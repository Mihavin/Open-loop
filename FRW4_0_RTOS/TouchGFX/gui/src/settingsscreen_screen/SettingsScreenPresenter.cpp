#include <gui/settingsscreen_screen/SettingsScreenView.hpp>
#include <gui/settingsscreen_screen/SettingsScreenPresenter.hpp>

SettingsScreenPresenter::SettingsScreenPresenter(SettingsScreenView& v)
    : view(v)
{
}

void SettingsScreenPresenter::activate()
{
}

void SettingsScreenPresenter::deactivate()
{
}

void SettingsScreenPresenter::onInput(const ui::InputEvent& event)
{
    view.handleInput(event);
}

void SettingsScreenPresenter::onUpdate()
{
    view.update();
}
