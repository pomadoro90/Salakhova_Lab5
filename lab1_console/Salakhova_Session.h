#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <boost/asio.hpp>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <string>
#include "Salakhova_Message.h"

using namespace std;

class Session
{
    queue<Message> messages;
    std::mutex mtx;
    std::condition_variable cv;

    // Время последней активности для таймаута
    std::chrono::steady_clock::time_point lastActivity;

public:
    int sessionID;
    wstring clientName;

    Session(int sessionID, const wstring& name = L"Client");
    ~Session();

    void addMessage(Message& m);
    bool getMessage(Message& m);

    void updateActivity();
    bool isTimedOut(int timeoutSeconds) const;
public:
    // Указатель на сокет клиента
    std::shared_ptr<boost::asio::ip::tcp::socket> sock;

    // Метод для сохранения сокета
    void setSocket(std::shared_ptr<boost::asio::ip::tcp::socket> s)
    {
        sock = s;
    }

    // Метод для безопасного закрытия сокета при таймауте
    void closeSocket()
    {
        if (sock)
        {
            boost::system::error_code ec;
            sock->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            sock->close(ec);
        }
    }
};