#include <gui/bootscreen_screen/BootScreenView.hpp>
#include <gui/bootscreen_screen/BootScreenPresenter.hpp>

BootScreenPresenter::BootScreenPresenter(BootScreenView& v)
    : view(v)
{
}

void BootScreenPresenter::activate()
{
}

void BootScreenPresenter::deactivate()
{
}

void BootScreenPresenter::onInput(const ui::InputEvent& event)
{
    view.handleInput(event);
}

void BootScreenPresenter::onUpdate()
{
    view.update();
}
