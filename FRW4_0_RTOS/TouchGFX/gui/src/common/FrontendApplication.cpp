#include <gui/common/FrontendApplication.hpp>
#include <gui/common/FrontendHeap.hpp>
#include <gui/bootscreen_screen/BootScreenView.hpp>
#include <gui/bootscreen_screen/BootScreenPresenter.hpp>
#include <gui/runscreen_screen/RunScreenView.hpp>
#include <gui/runscreen_screen/RunScreenPresenter.hpp>
#include <gui/menuscreen_screen/MenuScreenView.hpp>
#include <gui/menuscreen_screen/MenuScreenPresenter.hpp>
#include <gui/traversescreen_screen/TraverseScreenView.hpp>
#include <gui/traversescreen_screen/TraverseScreenPresenter.hpp>
#include <gui/interpscreen_screen/InterpScreenView.hpp>
#include <gui/interpscreen_screen/InterpScreenPresenter.hpp>
#include <gui/diagnosticsscreen_screen/DiagnosticsScreenView.hpp>
#include <gui/diagnosticsscreen_screen/DiagnosticsScreenPresenter.hpp>
#include <gui/settingsscreen_screen/SettingsScreenView.hpp>
#include <gui/settingsscreen_screen/SettingsScreenPresenter.hpp>
#include <gui/faultscreen_screen/FaultScreenView.hpp>
#include <gui/faultscreen_screen/FaultScreenPresenter.hpp>
#include <touchgfx/canvas_widget_renderer/CanvasWidgetRenderer.hpp>
#include <touchgfx/transitions/NoTransition.hpp>

#ifdef SIMULATOR
namespace ui
{
void simulatorKey(uint8_t c);
void simulatorFrameDone();
}
#endif

/* One canvas buffer for every screen (arcs, needles, rings). Kept here rather
   than in the generated view bases so a Designer regeneration cannot drop it. */
static uint8_t g_canvasBuffer[9600]; /* RAM_D1 is nearly full; 7200 is the Designer default */

FrontendApplication::FrontendApplication(Model& m, FrontendHeap& heap)
    : FrontendApplicationBase(m, heap), screenCallback(this, &FrontendApplication::showScreenImpl), pendingScreen(SCREEN_BOOT)
{
    CanvasWidgetRenderer::setupBuffer(g_canvasBuffer, sizeof(g_canvasBuffer));
}

void FrontendApplication::handleTickEvent()
{
    model.tick();
    FrontendApplicationBase::handleTickEvent();
#ifdef SIMULATOR
    ui::simulatorFrameDone();
#endif
}

void FrontendApplication::handleKeyEvent(uint8_t c)
{
#ifdef SIMULATOR
    ui::simulatorKey(c);
#else
    (void)c;
#endif
}

void FrontendApplication::showScreen(ScreenIndex screen)
{
    pendingScreen = screen;
    pendingScreenTransitionCallback = &screenCallback;
}

void FrontendApplication::showScreenImpl()
{
    switch (pendingScreen)
    {
    case SCREEN_RUN:
        makeTransition<RunScreenView, RunScreenPresenter, NoTransition, Model>(&currentScreen, &currentPresenter, frontendHeap, &currentTransition, &model);
        break;
    case SCREEN_MENU:
        makeTransition<MenuScreenView, MenuScreenPresenter, NoTransition, Model>(&currentScreen, &currentPresenter, frontendHeap, &currentTransition, &model);
        break;
    case SCREEN_TRAVERSE:
        makeTransition<TraverseScreenView, TraverseScreenPresenter, NoTransition, Model>(&currentScreen, &currentPresenter, frontendHeap, &currentTransition, &model);
        break;
    case SCREEN_INTERP:
        makeTransition<InterpScreenView, InterpScreenPresenter, NoTransition, Model>(&currentScreen, &currentPresenter, frontendHeap, &currentTransition, &model);
        break;
    case SCREEN_DIAGNOSTICS:
        makeTransition<DiagnosticsScreenView, DiagnosticsScreenPresenter, NoTransition, Model>(&currentScreen, &currentPresenter, frontendHeap, &currentTransition, &model);
        break;
    case SCREEN_SETTINGS:
        makeTransition<SettingsScreenView, SettingsScreenPresenter, NoTransition, Model>(&currentScreen, &currentPresenter, frontendHeap, &currentTransition, &model);
        break;
    case SCREEN_FAULT:
        makeTransition<FaultScreenView, FaultScreenPresenter, NoTransition, Model>(&currentScreen, &currentPresenter, frontendHeap, &currentTransition, &model);
        break;
    default:
        makeTransition<BootScreenView, BootScreenPresenter, NoTransition, Model>(&currentScreen, &currentPresenter, frontendHeap, &currentTransition, &model);
        break;
    }
}
