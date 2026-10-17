#ifndef MODELLISTENER_HPP
#define MODELLISTENER_HPP

#include <gui/model/Model.hpp>

class ModelListener
{
public:
    ModelListener() : model(0) {}
    virtual ~ModelListener() {}

    void bind(Model* m) { model = m; }

    /* Operator input not consumed by the model's global handling (SIDE/BACK holds, fault confirm). */
    virtual void onInput(const ui::InputEvent& /*event*/) {}
    /* Fresh telemetry and state, once per frame. */
    virtual void onUpdate() {}

protected:
    Model* model;
};

#endif // MODELLISTENER_HPP
