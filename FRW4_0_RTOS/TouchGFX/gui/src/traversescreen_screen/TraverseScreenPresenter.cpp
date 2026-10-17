#include <gui/traversescreen_screen/TraverseScreenView.hpp>
#include <gui/traversescreen_screen/TraverseScreenPresenter.hpp>

TraverseScreenPresenter::TraverseScreenPresenter(TraverseScreenView& v)
    : view(v)
{
}

void TraverseScreenPresenter::activate()
{
}

void TraverseScreenPresenter::deactivate()
{
}

void TraverseScreenPresenter::onInput(const ui::InputEvent& event)
{
    view.handleInput(event);
}

void TraverseScreenPresenter::onUpdate()
{
    view.update();
}
