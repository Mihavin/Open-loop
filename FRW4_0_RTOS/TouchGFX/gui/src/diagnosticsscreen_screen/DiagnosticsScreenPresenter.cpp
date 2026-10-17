#include <gui/diagnosticsscreen_screen/DiagnosticsScreenView.hpp>
#include <gui/diagnosticsscreen_screen/DiagnosticsScreenPresenter.hpp>

DiagnosticsScreenPresenter::DiagnosticsScreenPresenter(DiagnosticsScreenView& v)
    : view(v)
{
}

void DiagnosticsScreenPresenter::activate()
{
}

void DiagnosticsScreenPresenter::deactivate()
{
}

void DiagnosticsScreenPresenter::onInput(const ui::InputEvent& event)
{
    view.handleInput(event);
}

void DiagnosticsScreenPresenter::onUpdate()
{
    view.update();
}
