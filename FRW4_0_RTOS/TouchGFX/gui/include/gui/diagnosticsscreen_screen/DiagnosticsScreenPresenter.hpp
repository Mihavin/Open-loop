#ifndef DIAGNOSTICSSCREENPRESENTER_HPP
#define DIAGNOSTICSSCREENPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

class DiagnosticsScreenView;

/* Thin presenter: the model drives the screen through onInput/onUpdate and the
   view reads the model directly through theModel(). */
class DiagnosticsScreenPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    DiagnosticsScreenPresenter(DiagnosticsScreenView& v);
    virtual ~DiagnosticsScreenPresenter() {}

    virtual void activate();
    virtual void deactivate();
    virtual void onInput(const ui::InputEvent& event);
    virtual void onUpdate();

    Model& theModel() { return *model; }

private:
    DiagnosticsScreenPresenter();
    DiagnosticsScreenView& view;
};

#endif // DIAGNOSTICSSCREENPRESENTER_HPP
