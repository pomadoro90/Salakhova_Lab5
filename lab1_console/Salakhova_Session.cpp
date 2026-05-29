#include "Salakhova_Session.h"

Session::Session(int sessionID, const wstring& name)
    : sessionID(sessionID), clientName(name)
{
    updateActivity(); // Инициализируем при создании
}

Session::~Session() {}

void Session::addMessage(Message& m)
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        messages.push(m);
    }
    cv.notify_one();
}

bool Session::getMessage(Message& m)
{
    std::unique_lock<std::mutex> lock(mtx);
    // Добавляем таймаут ожидания, чтобы поток мог проверять флаги завершения
    if (cv.wait_for(lock, std::chrono::milliseconds(100), [this]() { return !messages.empty(); }))
    {
        m = messages.front();
        messages.pop();
        return true;
    }
    return false;
}

void Session::updateActivity()
{
    lastActivity = std::chrono::steady_clock::now();
}

bool Session::isTimedOut(int timeoutSeconds) const
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastActivity).count();
    return elapsed >= timeoutSeconds;
}