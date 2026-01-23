#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>

Model::Model() : modelListener(0)
{

}

void Model::tick()
{
    if (modelListener)
    {
        modelListener->tick();   // ✅ This calls DiagnosticPresenter::tick()
    }
}
