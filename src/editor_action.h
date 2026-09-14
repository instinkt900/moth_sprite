#pragma once

#include <functional>
#include <utility>

class IEditorAction {
public:
    virtual ~IEditorAction() = default;

    virtual void Do() = 0;
    virtual void Undo() = 0;
};

class BasicAction : public IEditorAction {
public:
    BasicAction(std::function<void()> doAction, std::function<void()> undoAction)
        : m_doAction(std::move(doAction))
        , m_undoAction(std::move(undoAction)) {}

    void Do() override {
        m_doAction();
    }

    void Undo() override {
        m_undoAction();
    }

private:
    std::function<void()> m_doAction;
    std::function<void()> m_undoAction;
};
