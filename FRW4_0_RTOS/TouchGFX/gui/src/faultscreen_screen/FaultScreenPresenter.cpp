#include <gui/faultscreen_screen/FaultScreenView.hpp>
#include <gui/faultscreen_screen/FaultScreenPresenter.hpp>

FaultScreenPresenter::FaultScreenPresenter(FaultScreenView& v)
    : view(v)
{
}

void FaultScreenPresenter::activate()
{
}

void FaultScreenPresenter::deactivate()
{
}

void FaultScreenPresenter::onInput(const ui::InputEvent& event)
{
    view.handleInput(event);
}

void FaultScreenPresenter::onUpdate()
{
    view.update();
}
