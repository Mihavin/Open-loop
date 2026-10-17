#include <gui/runscreen_screen/RunScreenView.hpp>
#include <gui/runscreen_screen/RunScreenPresenter.hpp>

RunScreenPresenter::RunScreenPresenter(RunScreenView& v)
    : view(v)
{
}

void RunScreenPresenter::activate()
{
}

void RunScreenPresenter::deactivate()
{
}

void RunScreenPresenter::onInput(const ui::InputEvent& event)
{
    view.handleInput(event);
}

void RunScreenPresenter::onUpdate()
{
    view.update();
}
