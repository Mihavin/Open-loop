#include <gui/interpscreen_screen/InterpScreenView.hpp>
#include <gui/interpscreen_screen/InterpScreenPresenter.hpp>

InterpScreenPresenter::InterpScreenPresenter(InterpScreenView& v)
    : view(v)
{
}

void InterpScreenPresenter::activate()
{
}

void InterpScreenPresenter::deactivate()
{
}

void InterpScreenPresenter::onInput(const ui::InputEvent& event)
{
    view.handleInput(event);
}

void InterpScreenPresenter::onUpdate()
{
    view.update();
}
